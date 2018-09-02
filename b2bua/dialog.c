#include <stdio.h>
#include <stdlib.h>

#include "../presence/hash.h"

#include "../../core/parser/parse_rr.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_methods.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_authenticate.h"
#include "../../core/parser/sdp/sdp.h"

#include "../../core/locking.h"
#include "../../core/script_cb.h"
#include "../../core/action.h"
#include "../../core/trim.h"

#include "dialog.h"
#include "b2b_entities.h"
#include "b2be_db.h"


#define BUF_LEN 1024

str ack = str_init(ACK);
str bye = str_init(BYE);

b2b_dlg_t* current_dlg = NULL;
/************************辅助函数************************************/
#define UPDATE_DBFLAG(dlg) do{ \
	if(b2be_db_mode == WRITE_BACK && dlg->db_flag==NO_UPDATEDB_FLAG) \
			dlg->db_flag = UPDATEDB_FLAG; \
	} while(0)
				
void set_dlg_state(b2b_dlg_t* dlg, int meth)
{
	switch(meth)
	{
		case METHOD_INVITE:
			if (dlg->state != B2B_NEW_AUTH)
				dlg->state= B2B_MODIFIED;
			break;
		case METHOD_CANCEL:
		case METHOD_BYE:
			dlg->state= B2B_TERMINATED;
			break;
		case METHOD_ACK:
			dlg->state= B2B_ESTABLISHED;
			break;
		default:
			break;
	}
}		
		
			
str* b2b_generate_key(unsigned int hash_index, unsigned int local_index)
{
	char buf[B2B_MAX_KEY_SIZE];
	
	str* b2b_key;
	
	int len;

	len = sprintf(buf, "%s.%d.%d.%ld",
		b2b_key_prefix.s, hash_index, local_index, startup_time+get_ticks());

	b2b_key = (str*)pkg_malloc(sizeof(str)+ len);
	
	if(b2b_key == NULL)
	{
		LM_ERR("no more private memory\n");
		return NULL;
	}
	
	b2b_key->s = (char*)b2b_key + sizeof(str);
	memcpy(b2b_key->s, buf, len);
	b2b_key->len = len;

	return b2b_key;
}

void print_b2b_dlg(b2b_dlg_t *dlg)
{
	dlg_leg_t *leg = dlg->legs;

	LM_DBG("dlg[%p][%p][%p]: [%.*s] id=[%d] param=[%.*s] state=[%d] db_flag=[%d]\n",
		dlg, dlg->prev, dlg->next, dlg->ruri.len, dlg->ruri.s,
		dlg->id, dlg->param.len, dlg->param.s, dlg->state, dlg->db_flag);
	LM_DBG("  from=[%.*s] [%.*s]\n",
		dlg->from_dname.len, dlg->from_dname.s, dlg->from_uri.len, dlg->from_uri.s);
	LM_DBG("    to=[%.*s] [%.*s]\n",
		dlg->to_dname.len, dlg->to_dname.s, dlg->to_uri.len, dlg->to_uri.s);
	LM_DBG("callid=[%.*s] tag=[%.*s][%.*s]\n",
		dlg->callid.len, dlg->callid.s,
		dlg->tag[0].len, dlg->tag[0].s, dlg->tag[1].len, dlg->tag[1].s);
	while(leg)
	{
		LM_DBG("leg[%p][%p] id=[%d] tag=[%.*s] cseq=[%d]\n",
			leg, leg->next, leg->id, leg->tag.len, leg->tag.s, leg->cseq);
		leg = leg->next;
	}
	return;
}

b2b_dlg_t* b2b_dlg_copy(b2b_dlg_t* dlg){
	b2b_dlg_t* new_dlg;
	int size;

	size = sizeof(b2b_dlg_t) + dlg->callid.len+ dlg->from_uri.len+ dlg->to_uri.len+
		dlg->tag[0].len + dlg->tag[1].len+ dlg->route_set[0].len+ dlg->route_set[1].len+
		dlg->contact[0].len+ dlg->contact[1].len+ dlg->ruri.len+ B2BL_MAX_KEY_LEN+
		dlg->from_dname.len + dlg->to_dname.len;

	new_dlg = (b2b_dlg_t*)shm_malloc(size);
	
	if(new_dlg == 0)
	{
		LM_ERR("No more shared memory\n");
		return 0;
	}
	
	memset(new_dlg, 0, size);
	size = sizeof(b2b_dlg_t);

	if(dlg->ruri.s)
		CONT_COPY(new_dlg, new_dlg->ruri, dlg->ruri);
	CONT_COPY(new_dlg, new_dlg->callid, dlg->callid);
	CONT_COPY(new_dlg, new_dlg->from_uri, dlg->from_uri);
	CONT_COPY(new_dlg, new_dlg->to_uri, dlg->to_uri);
	if(dlg->tag[0].len && dlg->tag[0].s)
		CONT_COPY(new_dlg, new_dlg->tag[0], dlg->tag[0]);
	if(dlg->tag[1].len && dlg->tag[1].s)
		CONT_COPY(new_dlg, new_dlg->tag[1], dlg->tag[1]);
	if(dlg->route_set[0].len && dlg->route_set[0].s)
		CONT_COPY(new_dlg, new_dlg->route_set[0], dlg->route_set[0]);
	if(dlg->route_set[1].len && dlg->route_set[1].s)
		CONT_COPY(new_dlg, new_dlg->route_set[1], dlg->route_set[1]);
	if(dlg->contact[0].len && dlg->contact[0].s)
		CONT_COPY(new_dlg, new_dlg->contact[0], dlg->contact[0]);
	if(dlg->contact[1].len && dlg->contact[1].s)
		CONT_COPY(new_dlg, new_dlg->contact[1], dlg->contact[1]);
	if(dlg->param.s)
	{
		new_dlg->param.s= (char*)new_dlg+ size;
		memcpy(new_dlg->param.s, dlg->param.s, dlg->param.len);
		new_dlg->param.len= dlg->param.len;
		size+= B2BL_MAX_KEY_LEN;
	}

	if(dlg->from_dname.s)
		CONT_COPY(new_dlg, new_dlg->from_dname, dlg->from_dname);
	if(dlg->to_dname.s)
		CONT_COPY(new_dlg, new_dlg->to_dname, dlg->to_dname);

	new_dlg->cseq[0]          = dlg->cseq[0];
	new_dlg->cseq[1]          = dlg->cseq[1];
	new_dlg->id               = dlg->id;
	new_dlg->state            = dlg->state;
	new_dlg->b2b_cback        = dlg->b2b_cback;
	new_dlg->add_dlginfo      = dlg->add_dlginfo;
	new_dlg->last_invite_cseq = dlg->last_invite_cseq;
	new_dlg->db_flag          = dlg->db_flag;
	new_dlg->send_sock        = dlg->send_sock;

	return new_dlg;
}
/***********************************************************辅助函数*******************************/


/***********************************************************辅助发送接收消息函数*******************************//
int b2b_send_indlg_req(b2b_dlg_t* dlg, enum b2b_entity_type et, str* b2b_key, str* method, str* ehdr, str* body, unsigned int no_cb){
	
	str* b2b_key_shm = NULL;
	dlg_t* td = NULL;
	transaction_cb* tm_cback;
	build_dlg_f build_dlg;
	int method_value = dlg->last_method;
	int result;

	if (!no_cb) {
		b2b_key_shm= b2b_key_copy_shm(b2b_key);
		if(b2b_key_shm== NULL)
		{
			LM_ERR("no more shared memory\n");
			return -1;
		}
	}

	if(et == B2B_SERVER)
	{
		build_dlg = b2b_server_build_dlg;
		tm_cback = b2b_server_tm_cback;
	}
	else
	{
		build_dlg = b2b_client_dlg;
		tm_cback = b2b_client_tm_cback;
	}

	/* build structure with dialog information */
	td = build_dlg(dlg);
	if(td == NULL)
	{
		LM_ERR("Failed to build tm dialog structure, was asked to send [%.*s]"
				" request\n", method->len, method->s);
		if (b2b_key_shm)
			shm_free(b2b_key_shm);
		return -1;
	}

	if(method_value == METHOD_ACK)
	{
		td->loc_seq.value = dlg->last_invite_cseq;
		if(et == B2B_SERVER)
			dlg->cseq[CALLEE_LEG]--;
		else
			dlg->cseq[CALLER_LEG]--;

		if(dlg->ack_sdp.s)
		{
			shm_free(dlg->ack_sdp.s);
			dlg->ack_sdp.s=NULL;
			dlg->ack_sdp.len=0;
		}
		if(body && body->s)
		{
			dlg->ack_sdp.s = (char*)shm_malloc(body->len);
			if(dlg->ack_sdp.s == NULL)
			{
				LM_ERR("No more memory\n");
				goto error;
			}
			memcpy(dlg->ack_sdp.s, body->s, body->len);
			dlg->ack_sdp.len = body->len;
		}
	}
	else
	if(method_value == METHOD_INVITE)
	{
		dlg->last_invite_cseq = td->loc_seq.value +1;
		if(dlg->uac_tran)
			tmb.unref_cell(dlg->uac_tran);
		tmb.setlocalTholder(&dlg->uac_tran);
	}

//事物内把消息发送出去;
	if (no_cb){
		result= tmb.t_request_within
			(method,            /* method*/
			ehdr,               /* extra headers*/
			body,               /* body*/
			td,                 /* dialog structure*/
			NULL,               /* callback function*/
			NULL,               /* callback parameter*/
			NULL);
	}
	else
	{
		td->T_flags = T_NO_AUTOACK_FLAG|T_PASS_PROVISIONAL_FLAG;
		result= tmb.t_request_within
			(method,            /* method*/
			ehdr,               /* extra headers*/
			body,               /* body*/
			td,                 /* dialog structure*/
			tm_cback,           /* callback function*/
			b2b_key_shm,        /* callback parameter*/
			shm_free_param);
	}

	tmb.setlocalTholder(0);

	if(result < 0)
	{
		LM_ERR("failed to send request [%.*s] for dlg=[%p] uac_tran=[%p]\n",
			method->len, method->s, dlg, dlg->uac_tran);
		goto error;
	}
	
	free_tm_dlg(td);

	return 0;
error:
	if(td)
		free_tm_dlg(td);
	if(b2b_key_shm)
		shm_free(b2b_key_shm);
	return -1;
}

/***********************************************************辅助发送接收消息函数*******************************/


/*******************************************************************************

 * 名称: init_b2b_htables

 * 功能: init the server dialog table and client dialog table;

 * 形参: 主要数据结构:
 
         类似 current_hashmap ,one slot one lock;
		 
 * 返回: -1意味着当前分配失败, 0分配成功;

 * 说明: server_table,client_table对, server_size, client_size应全局变量,对应存储全局的dialog信息;

 ******************************************************************************/
 int init_b2b_htables(void){
	int i;

	server_htable = (b2b_table)shm_malloc(server_hsize* sizeof(b2b_entry_t));
	client_htable = (b2b_table)shm_malloc(client_hsize* sizeof(b2b_entry_t));
	
	if(!server_htable || !client_htable)
		ERR_MEM(SHARE_MEM);

	memset(server_htable, 0, server_hsize* sizeof(b2b_entry_t));
	memset(client_htable, 0, client_hsize* sizeof(b2b_entry_t));
	
	//每一个hash slot;
	for(i= 0; i< server_hsize; i++)
	{
		lock_init(&server_htable[i].lock);
	}

	for(i= 0; i< client_hsize; i++)
	{
		lock_init(&client_htable[i].lock);
	}

	return 0;

error:
	return -1;
}

/*******************************************************************************

 * 名称: destroy_b2b_htables

 * 功能: destory the dialog table;

 * 形参: 无 

 * 返回: 无

 * 说明: 了解销毁过程,注意dialog相关的数据结构,在dialog.h中对应有注释;

 ******************************************************************************/
 
void destroy_b2b_htables(void){
	int i;

	b2b_dlg_t* dlg, *aux;

	if(server_htable)
	{
		for(i= 0; i< server_hsize; i++)
		{
			lock_destroy(&server_htable[i].lock); 
			dlg = server_htable[i].first;  
			while(dlg)   
			{
				aux = dlg->next;
				if(dlg->tag[CALLEE_LEG].s)
					shm_free(dlg->tag[CALLEE_LEG].s);
				if(dlg->ack_sdp.s)
					shm_free(dlg->ack_sdp.s);
				shm_free(dlg);
				dlg = aux;
			}
		}
		shm_free(server_htable);
	}

	if(client_htable)
	{
		for(i = 0; i< client_hsize; i++)
		{
			lock_destroy(&client_htable[i].lock);
			dlg = client_htable[i].first;
			while(dlg)
			{
				aux = dlg->next;
				b2b_delete_legs(&dlg->legs);
				if(dlg->ack_sdp.s)
					shm_free(dlg->ack_sdp.s);
				shm_free(dlg);
				dlg = aux;
			}
		}
		shm_free(client_htable);
	}
}


/*******************************************************************************

 * 名称: b2b_parse_key

 * 功能: parse the key and get the hash_index and local_index;

 * 形参: key:   特定方式的key : B2B.hash_index.local_index.timestamp.
         
		 hash_index:   dialog module中的hash slot.
		 
		 local_index:  local_index对应为指定dialog的唯一标志;
		 
 * 返回: -1说明传入的key本身存在问题;

 * 说明: key format : B2B.hash_index.local_index.timestamp , B2B可以为任意的固定前缀,解析出其中的hash_index and local
  
   index用于找到指定的dialog条目;
   
   其中b2b_key_prefix为初始化时候设定，
 ******************************************************************************/   
int b2b_parse_key(str* key, unsigned int* hash_index, unsigned int* local_index)
{
	char* p;
	str s;

	if(!key || !key->s)
		return -1;

	if(strncmp(key->s, b2b_key_prefix.s, b2b_key_prefix.len) != 0 ||
			key->len<(b2b_key_prefix.len +4) || key->s[b2b_key_prefix.len]!='.')
	{
		LM_DBG("Does not have b2b_entities prefix\n");
		return -1;
	}

	s.s = key->s + b2b_key_prefix.len+1;
	p= strchr(s.s, '.');
	if(p == NULL || ((p-s.s) > key->len) )
	{
		LM_DBG("Wrong format for b2b key\n");
		return -1;
	}

	s.len = p - s.s;
	if(str2int(&s, hash_index) < 0)
	{
		LM_DBG("Could not extract hash_index [%.*s]\n", key->len, key->s);
		return -1;
	}

	p++;
	s.s = p;
	p= strchr(s.s, '.');
	
	if(p == NULL || ((p-s.s) > key->len) )
	{
		LM_DBG("Wrong format for b2b key\n");
		return -1;
	}

	s.len = p - s.s;
	if(str2int(&s, local_index)< 0)
	{
		LM_DBG("Wrong format for b2b key\n");
		return -1;
	}
	/* we do not really care about the third part of the key */

	LM_DBG("hash_index = [%d]  - local_index= [%d]\n", *hash_index, *local_index);

	return 0;
}

/*******************************************************************************

 * 名称: b2b_search_htable_next

 * 功能: get the dialog entry by the hash_index and local index;

 * 形参: start_dlg: 如果不是null , 则不需要hash_index去定位到特定的hash_slot;
         
		 hash_index:   定位到当前dialog所在表中的位置;(hash_slot的位置);
		 
		 local_index:  定位到具体的dialog_entry;
		 
 * 返回: NULL or 实际获取的指针.

 * 说明: 无
 ******************************************************************************/ 

b2b_dlg_t* b2b_search_htable_next(b2b_dlg_t* start_dlg, b2b_table table, unsigned int hash_index, unsigned int local_index){
	
	b2b_dlg_t* dlg;

	dlg = start_dlg?start_dlg->next:table[hash_index].first;
	
	while(dlg && dlg->id != local_index)
		dlg = dlg->next;

	if(dlg == NULL || dlg->id!=local_index)
	{
		LM_DBG("No dialog with hash_index=[%d] and local_index=[%d] found\n",
				hash_index, local_index);
		return NULL;
	}

	return dlg;
}

//无
b2b_dlg_t* b2b_search_htable(b2b_table table, unsigned int hash_index,
		unsigned int local_index)
{
	return b2b_search_htable_next(NULL, table, hash_index, local_index);
}		

/*******************************************************************************

 * 名称: b2b_search_htable_next_dlg

 * 功能: 根据dialog的信息(callid, fromtag, totag)找到dialog.

 * 形参: start_dlg: 如果不是null , 则不需要hash_index去定位到特定的hash_slot;
         
		 hash_index:   定位到当前dialog所在表中的位置;(hash_slot的位置);
		 
		 local_index:  定位到具体的dialog_entry;
		 
		 to_tag: 
		 
		 from_tag:
		 
		 callid:
		 
 * 返回: NULL or 实际获取的指针.

 * 说明: 涉及到uac端的变换没有搞清楚;
 
         #define CALLER_LEG   0 标定主叫, 被叫方;
         #define CALLEE_LEG   1
 ******************************************************************************/ 
b2b_dlg_t* b2b_search_htable_next_dlg(b2b_dlg_t* start_dlg, b2b_table table, unsigned int hash_index, unsigned int local_index, str* to_tag, str* from_tag, str* callid){

	b2b_dlg_t* dlg;
	str dlg_from_tag={NULL, 0};
	dlg_leg_t* leg;

	LM_DBG("entering with start=%p, table=%p, hash=%d, label=%d \n",
		start_dlg,table,hash_index,local_index);
		
	if(callid)
		LM_DBG("searching  callid %d[%.*s]\n", callid->len,callid->len, callid->s);
	if(to_tag)
		LM_DBG("searching   totag %d[%.*s]\n", to_tag->len,to_tag->len, to_tag->s);
	if(from_tag)
		LM_DBG("searching fromtag %d[%.*s]\n", from_tag->len,from_tag->len, from_tag->s);
	
	dlg = start_dlg?start_dlg->next:table[hash_index].first;
	
	while(dlg)
	{
		if(dlg->id != local_index)
		{
			dlg = dlg->next;
			continue;
			
		}
		/*check if the dialog information correspond */
		if(table == server_htable)
		{
			if(!from_tag || !callid)
				return NULL;
			dlg_from_tag = dlg->tag[CALLER_LEG];  //主叫方;
			/* check from tag and callid */
			 if(dlg_from_tag.len == from_tag->len &&
				strncmp(dlg_from_tag.s, from_tag->s, dlg_from_tag.len)==0
				&& dlg->callid.len==callid->len && strncmp(dlg->callid.s, callid->s, callid->len)==0)
			{
				LM_DBG("Match for server dlg [%p] dlg->uas_tran=[%p]\n",
					dlg, dlg->uas_tran);
				return dlg;
			}

		}
		//如果是 UAC端的 对话;
		else
		{
			/*
			LM_DBG("dialog totag [%.*s] with state %d\n",
				dlg->tag[CALLER_LEG].len, dlg->tag[CALLER_LEG].s, dlg->state);
			*/
			
			/* it is an UAC dialog (callid is the key)*/
			// 用key联系UAS 与 UAC？？？？
			if(to_tag && dlg->tag[CALLER_LEG].len == to_tag->len &&
				strncmp(dlg->tag[CALLER_LEG].s, to_tag->s, to_tag->len)== 0)
			{

				leg = dlg->legs;
				if(dlg->state < B2B_CONFIRMED || dlg->state>=B2B_DESTROYED)
				{
					if(from_tag == NULL || from_tag->len==0 || leg==NULL)
					{
						LM_DBG("Match for client dlg [%p] last_method=%d"
							" dlg->uac_tran=[%p]\n",
							dlg, dlg->last_method, dlg->uac_tran);
						return dlg;
					}
				}
				
				if(from_tag == NULL || from_tag->s==NULL)
				{
					dlg = dlg->next;
					continue;
				}
				/* if it is an already confirmed dialog match the to_tag also*/
				while(leg)
				{
					if(leg->tag.len == from_tag->len && strncmp(leg->tag.s, from_tag->s, from_tag->len)== 0)
						return dlg;
					leg = leg->next;
				}
				
				if(dlg->state < B2B_CONFIRMED || dlg->state>=B2B_DESTROYED) /* state not confirmed yet and a new leg */
					return dlg;
			}
		}
		
		dlg = dlg->next;
	}
	return NULL;
}

b2b_dlg_t* b2b_search_htable_dlg(b2b_table table, unsigned int hash_index, unsigned int local_index, str* to_tag, str* from_tag, str* callid){
	
	return b2b_search_htable_next_dlg(NULL, table, hash_index, local_index, to_tag, from_tag, callid);

}

/*******************************************************************************

 * 名称: b2b_htable_insert

 * 功能: 把dialog条目插入到指定的uas or uac table中;

 * 形参: table: client or server dialog hash table
        
		 dlg:   待插入的对话;
        
		 hash_index:  hash slot;
		 
		 src:  
		 
		 reload:
 
		 
 * 返回: NULL or 实际获取的指针.

 * 说明: 如果对应的是server dialog table则把b2b_key当做totag value,方便uac与uas之间的联系;
 
 ******************************************************************************/ 
 
str* b2b_htable_insert(b2b_table table, b2b_dlg_t* dlg, int hash_index, int src, int reload){
	
	b2b_dlg_t *it, *prev_it= NULL;
	
	str* b2b_key;

	if(!reload)
		lock_get(&table[hash_index].lock);

	dlg->prev = dlg->next = NULL;
	
	it = table[hash_index].first;

	if(it == NULL)
	{
		table[hash_index].first = dlg;
	}
	else
	{
		while(it)
		{
			prev_it = it;
			it = it->next;
		}
		prev_it->next = dlg;
		dlg->prev = prev_it;
	}
	
	/* if an insert in server_htable -> copy the b2b_key in the to_tag */
		
	// 如果对应插入在uas dialog中， 则把b2b_key放在to_tag中;
	
	b2b_key = b2b_generate_key(hash_index, dlg->id);
	
	if(b2b_key == NULL)
	{
		if(!reload)
			lock_release(&table[hash_index].lock);
		LM_ERR("Failed to generate b2b key\n");
		return NULL;
	}

	if(src == B2B_SERVER)
	{
		
		// to tag
		dlg->tag[CALLEE_LEG].s = (char*)shm_malloc(b2b_key->len);
		
		if(dlg->tag[CALLEE_LEG].s == NULL)
		{
			LM_ERR("No more shared memory\n");
			if(!reload)
				lock_release(&table[hash_index].lock);
			return 0;
		}
		
		memcpy(dlg->tag[CALLEE_LEG].s, b2b_key->s, b2b_key->len);
		
		dlg->tag[CALLEE_LEG].len = b2b_key->len;
		if(!reload && b2be_db_mode == WRITE_THROUGH)
			b2be_db_insert(dlg, src);
	}

	if(!reload)
		lock_release(&table[hash_index].lock);

	return b2b_key;
}
/*******************************************************************************

 * 名称: b2b_send_request

 * 功能: 在对话内发送请求消息,对话有req_data->b2b_key标定;
 
 * 形参: req_data
 
         typedef struct b2b_req_data{
	        
			enum b2b_entity_type et;
	        str* b2b_key;          
		
		//用于构造新的请求消息;	
	        str* method;
	        str* extra_headers;
	        str* client_headers;
	        str* body;
	    //找到精确的dialog;    
		
   		    b2b_dlginfo_t* dlginfo; {callid, fromtag, totag};		
	        unsigned int no_cb; 
		  }b2b_req_data_t;
		 
 * 返回: 0 or -1;

 * 说明: 这个函数有可能使用会有问题,注意函数参数req->data的构建,需要在共享内存中构建么？？
   下面注释是对这个函数的一点理解,这个函数大致上是有理解的无非是发送消息与dialog状态之间的关系;
 ******************************************************************************/ 
int b2b_send_request(b2b_req_data_t* req_data){
	
	enum b2b_entity_type et = req_data->et;
	str* b2b_key = req_data->b2b_key;
	str* method = req_data->method;
	b2b_dlginfo_t* dlginfo = req_data->dlginfo;
   
	unsigned int hash_index, local_index;
	
	b2b_dlg_t* dlg;
	str ehdr = {NULL, 0};
	b2b_table table;
	str totag={NULL, 0}, fromtag={NULL, 0};
	unsigned int method_value;
	int ret;

	if(et == B2B_SERVER)
	{
		table = server_htable;
	}
	else
	{
		table = client_htable;
	}

	if(dlginfo)
	{
		totag = dlginfo->totag;
		fromtag = dlginfo->fromtag;
	}

	/* parse the key and find the position in hash table */
	if(b2b_parse_key(b2b_key, &hash_index, &local_index) < 0)
	{
		LM_ERR("Wrong format for b2b key [%.*s]\n", b2b_key->len, b2b_key->s);
		return -1;
	}

	lock_get(&table[hash_index].lock);
	
	if(dlginfo == NULL)
	{
		dlg = b2b_search_htable(table, hash_index, local_index);
	}
	else
	{
		dlg = b2b_search_htable_dlg(table, hash_index, local_index,
		totag.s?&totag:NULL, fromtag.s?&fromtag:NULL, &dlginfo->callid);
	}
	
	if(dlg== NULL)
	{
		LM_ERR("No dialog found for b2b key [%.*s] and dlginfo=[%p]\n",
			b2b_key->len, b2b_key->s, dlginfo);
		goto error;
	}

	parse_method(method->s, method->s+method->len, &method_value);

// 当前状态位terminated 意味着对话结束不能继续发送request信息;
// 理解逻辑;
	
	if(dlg->state == B2B_TERMINATED)
	{
		LM_ERR("Can not send request [%.*s] for entity type [%d] "
			"for dlg[%p]->[%.*s] in terminated state\n",
			method->len, method->s, et,
			dlg, b2b_key->len, b2b_key->s);
		lock_release(&table[hash_index].lock);
	    //如果method为 bye 或者cancel时,不需要发送request当前已经结束;
		if(method_value==METHOD_BYE || method_value==METHOD_CANCEL)
			return 0;
		else
			return -1;
	}
	
//构建ehdr用于request消息的发送！
	if(b2breq_complete_ehdr(req_data->extra_headers, req_data->client_headers,
			&ehdr, req_data->body,
			((et==B2B_SERVER)?&dlg->contact[CALLEE_LEG]:&dlg->contact[CALLER_LEG]))< 0)
	{
		LM_ERR("Failed to complete extra headers\n");
		goto error;
	}
	
//  如果当前的dialog状态小于confirmed, 200之前,
//  修改信令逻辑需要对应修改这里;

	if(dlg->state < B2B_CONFIRMED)
	{
		if(method_value == METHOD_BYE && et == B2B_CLIENT) /* send CANCEL*/
		{
			method_value = METHOD_CANCEL;
		}
		else if(method_value!=METHOD_UPDATE && method_value!=METHOD_PRACK &&
			method_value!=METHOD_CANCEL && (method_value!=METHOD_INVITE /**/))
		{
			LM_ERR("State [%d] not established, can not send request %.*s, [%.*s]\n",
				dlg->state, method->len, method->s, b2b_key->len, b2b_key->s);
			lock_release(&table[hash_index].lock);
			return -1;
		}
	}
// 200收到,对应invite的confirmed;	
	else if(dlg->state == B2B_CONFIRMED && method_value!=METHOD_ACK &&
			dlg->last_method == METHOD_INVITE)
	{
		/* send it ACK so that you can send the new request */
		// 当前状态如果为confirmed,则可以通过发送之前的ack，这样建立会话状态则可以发送新的请求; 
		b2b_send_indlg_req(dlg, et, b2b_key, &ack, &ehdr, req_data->body, req_data->no_cb);
		dlg->state= B2B_ESTABLISHED;
	}

	dlg->last_method = method_value;
	
	LM_DBG("Send request [%.*s] for entity type [%d] for dlg[%p]->[%.*s]\n",
			method->len, method->s, et,
			dlg, b2b_key->len, b2b_key->s);
	
	UPDATE_DBFLAG(dlg); //跟新当前dlg的相关信息;

	/* send request */
	// 如果当前发送的为cancel;
	if(method_value == METHOD_CANCEL)
	{
		if(dlg->state < B2B_CONFIRMED)
		{
			// 取消绑定dialog上的事物;
			// 如果当前dialog上绑定了事物,则直接取消即可,这样后序的send_ reply就会被取消;
			// 并没有uas事物;
			if(dlg->uac_tran)
			{
				struct cell *inv_t;
				LM_DBG("send cancel request\n");
				if (tmb.t_lookup_ident( &inv_t, dlg->uac_tran->hash_index, dlg->uac_tran->label) != 1) {
					LM_ERR("Invite trasaction not found\n");
					goto error;
				}
				ret = tmb.t_cancel_trans( inv_t, &ehdr);
				tmb.unref_cell(inv_t);
			}
			//当前上并没有相应的事物绑定;
			else
			{
				LM_ERR("No transaction saved. Cannot send CANCEL\n");
				goto error;
			}
		}
		else
		{
		// 而当200实际上收到了之后, 为了正确的取消两端事物(confirmed之后如果,依旧按照上述取消事物则只销毁server端事物),
 		// 则先发送把dialog建立起来,然后发送bye取消;
  		    if(dlg->state == B2B_CONFIRMED)
			{
				b2b_send_indlg_req(dlg, et, b2b_key, &ack, &ehdr, 0, req_data->no_cb);
			}
			ret = b2b_send_indlg_req(dlg, et, b2b_key, &bye, &ehdr, req_data->body, req_data->no_cb);
			method_value = METHOD_BYE;
		}
	}
	else
	{   //直接发送相应的请求;
		ret = b2b_send_indlg_req(dlg, et, b2b_key, method, &ehdr, req_data->body, req_data->no_cb);
	}

	if(ret < 0)
	{
		LM_ERR("Failed to send request\n");
		goto error;
	}
	
	//发送成功;
	set_dlg_state(dlg, method_value); //有点像状态机改变;
    	
	//如果设置了之后, 则把当前的dialog信息写入到数据库中;
	if(b2be_db_mode==WRITE_THROUGH && current_dlg!=dlg && dlg->state>B2B_CONFIRMED) {
		if(b2be_db_update(dlg, et) < 0)
			LM_ERR("Failed to update in database\n");
	}

	lock_release(&table[hash_index].lock);
	
	return 0;
error:
	lock_release(&table[hash_index].lock);
	
	return -1;
}
/*******************************************************************************

 * 名称: b2b_send_reply

 * 功能: 在对话内回复请求消息 , 对话有req_data->b2b_key标定;
 
 * 形参: rpl_data
 
      typedef struct b2b_rpl_data{
     	
		enum b2b_entity_type et;
   	    
		str* b2b_key;
	    int method;
	    int code;    //reply code;
	    
		str* text;
	    str* body;
	    str* extra_headers;
	    b2b_dlginfo_t* dlb; 
	  }b2b_req_data_t;
		 
 * 返回: 0 or -1;
 * 说明: 部分逻辑依旧不明确;
 ******************************************************************************/ 
int b2b_send_reply(b2b_rpl_data_t* rpl_data){
	
	enum b2b_entity_type et = rpl_data->et;
	str* b2b_key = rpl_data->b2b_key;
	int sip_method = rpl_data->method;
	int code = rpl_data->code;
	str* extra_headers = rpl_data->extra_headers;
	b2b_dlginfo_t* dlginfo = rpl_data->dlginfo;

	unsigned int hash_index, local_index;
	
	b2b_dlg_t* dlg;
	str* to_tag = NULL;
	struct cell* tm_tran;
	struct sip_msg* msg;
	char buffer[BUF_LEN];  //buff  len
	int len;
	char* p;
	str ehdr;
	b2b_table table;
	str totag, fromtag;
	struct to_body *pto;
	unsigned int method_value = METHOD_UPDATE;
	str local_contact;
	
	//这里是由废步骤, 因为对应的回复比是uas的逻辑;
	if(et == B2B_SERVER)
	{
		LM_DBG("For server entity\n");
		table = server_htable;
		if(dlginfo)
		{
			totag = dlginfo->fromtag;
			fromtag = dlginfo->totag;
		}
	}
	else
	{
		LM_DBG("For client entity\n");
		table = client_htable;
		if(dlginfo)
		{
			totag = dlginfo->fromtag;
			fromtag = dlginfo->totag;
		}
	}

	/* parse the key and find the position in hash table */
	if(b2b_parse_key(b2b_key, &hash_index, &local_index) < 0)
	{
		LM_ERR("Wrong format for b2b key\n");
		return -1;
	}
	
	
	lock_get(&table[hash_index].lock);
	
	if(dlginfo == NULL)
	{
		dlg = b2b_search_htable(table, hash_index, local_index);
    }
	else
	{
		dlg = b2b_search_htable_dlg(table, hash_index, local_index, &fromtag, &totag, &dlginfo->callid);
	}
	
	if(dlg== NULL)
	{
		LM_ERR("No dialog found\n");
		lock_release(&table[hash_index].lock);
		return -1;
	}
		
	if(et == B2B_CLIENT)
		local_contact = dlg->contact[CALLER_LEG];
	else
		local_contact = dlg->contact[CALLEE_LEG];
    
	//update information;
	if(sip_method == METHOD_UPDATE)
		tm_tran = dlg->update_tran;
	else
	{
		tm_tran = dlg->uas_tran;  //实现的有问题;生成响应;
		if(tm_tran)
		{
			if(parse_method(tm_tran->method.s,
					tm_tran->method.s + tm_tran->method.len, &method_value) == 0)
			{
				LM_ERR("Wrong method stored in tm transaction [%.*s]\n",
					tm_tran->method.len, tm_tran->method.s);
				lock_release(&table[hash_index].lock);
				return -1;
			}
			if(sip_method != method_value)
			{
				//这个send reply对应的method  value应该与事物对应的method的回复相一致;
				LM_ERR("Mismatch between the method in tm[%d] and the method to send reply to[%d]\n",
						sip_method, method_value);
				lock_release(&table[hash_index].lock);
				return -1;
			}
		}
	}
	
	if(tm_tran == NULL)  //if not found;
	{
		LM_DBG("code = %d, last_method= %d\n", code, dlg->last_method);
		if(dlg->last_reply_code == code)
		{
			LM_DBG("it is a retransmission - nothing to do\n");
			lock_release(&table[hash_index].lock);
			return 0;
		}
		LM_ERR("Tm transaction not saved!\n");
		lock_release(&table[hash_index].lock);
		return -1;
	}

	LM_DBG("Send reply %d %.*s, for dlg [%p]->[%.*s]\n", code, tm_tran->method.len,
			tm_tran->method.s, dlg, b2b_key->len, b2b_key->s);

			
    //如果对应invite的回复是confirmed or established状态则没有必要继续回复;		
	if(method_value==METHOD_INVITE &&
			(dlg->state==B2B_CONFIRMED || dlg->state==B2B_ESTABLISHED))
	{
		
		//confirmed 是在等待ack, 而established之后除了主叫的bye没有必要回复其它的消息;
		LM_DBG("A retransmission of the reply\n");
		lock_release(&table[hash_index].lock);
		return 0;
	}

	if(code >= 200)
	{
		if(method_value==METHOD_INVITE)
		{
			if(code < 300)  //当收到 2xx消息时 改变当前状态位confirmed;
				dlg->state = B2B_CONFIRMED;
			else
				dlg->state= B2B_TERMINATED;
			UPDATE_DBFLAG(dlg);   // dbflag;
		}
		LM_DBG("Reset transaction- send final reply [%p], uas_tran=0\n", dlg);
		
		if(sip_method == METHOD_UPDATE)
			dlg->update_tran = NULL;
		else
			dlg->uas_tran = NULL;  //非200的取消; ？？？;
	}

	msg = tm_tran->uas.request;
	
	if(msg== NULL)
	{
		LM_DBG("Transaction not valid anymore\n");
		lock_release(&table[hash_index].lock);
		return 0;
	}

	if(parse_headers(msg, HDR_EOH_F, 0) < 0)
	{
		LM_ERR("Failed to parse headers\n");
		return 0;
	}

	pto = get_to(msg);
	
	if (pto == NULL || pto->error != PARSE_OK)
	{
		LM_ERR("'To' header COULD NOT parsed\n");
		return 0;
	}

	/* only for the server replies to the initial INVITE */
	if(!pto || !pto->tag_value.s || !pto->tag_value.len)
	{
		to_tag = b2b_key;
	}
	else
		to_tag = &pto->tag_value;

	/* if sent reply for bye, delete the record */
	if(method_value == METHOD_BYE)
		dlg->state = B2B_TERMINATED;

	dlg->last_reply_code = code;
	
	UPDATE_DBFLAG(dlg);

	
	//更新数据库;
	if(b2be_db_mode==WRITE_THROUGH && current_dlg!=dlg && dlg->state>B2B_CONFIRMED) {
		if(b2be_db_update(dlg, et) < 0)
			LM_ERR("Failed to update in database\n");
	}

	lock_release(&table[hash_index].lock);

	if((extra_headers?extra_headers->len:0) + 14 + local_contact.len
			+ 20 + CRLF_LEN > BUF_LEN)
	{
		LM_ERR("Buffer overflow!\n");
		goto error;
	}
	
	p = buffer;

	if(extra_headers && extra_headers->s && extra_headers->len)
	{
		memcpy(p, extra_headers->s, extra_headers->len);
		p += extra_headers->len;
	}
	len = sprintf(p,"Contact: <%.*s>", local_contact.len, local_contact.s);
	p += len;
	memcpy(p, CRLF, CRLF_LEN);
	p += CRLF_LEN;
	ehdr.len = p -buffer;
	ehdr.s = buffer;

	/* send reply */
	
	// 关键回复代码在这里;
	if(tmb.t_reply_with_body(tm_tran, code, rpl_data->text, rpl_data->body, &ehdr, to_tag) < 0)
	{
		LM_ERR("failed to send reply with tm\n");
		goto error;
	}
	// 大于200需要把tran给去除？？？
	if(code >= 200) 
	{
		LM_DBG("Sent reply [%d] and unreffed the cell %p\n", code, tm_tran);
		tmb.unref_cell(tm_tran);
	}
	else
	{
		LM_DBG("code not >= 200\n");
	}
	return 0;

error:
	if(code >= 200)
	{
		tmb.unref_cell(tm_tran);
	}
	return -1;
}
/*******************************************************************************

 * 名称: b2b_new_dlg

 * 功能: 按照传入的信息, 生成dialog; 
 
 * 形参: msg:
         
		 local_contact:
		 
		 init_dlg: 初始化的init_dlg,在传入的时候基本不用;
		 
		 param:
		 
 * 返回: 0 or -1;
 
 * 说明: 被server_new 所调用，生成dialog; 首次调用生成dialog, 必须以sip_request  :  method invite为起始;
 
 ******************************************************************************/
b2b_dlg_t* b2b_new_dlg(struct sip_msg* msg, str* local_contact, b2b_dlg_t* init_dlg, str* param){
	
	struct to_body *pto, *pfrom = NULL;
	b2b_dlg_t dlg;
	contact_body_t*  b;
	b2b_dlg_t* shm_dlg = NULL;

	memset(&dlg, 0, sizeof(b2b_dlg_t));

	if (parse_headers(msg, HDR_EOH_F, 0) < 0)
	{
		LM_ERR("failed to parse message\n");
		return 0;
	}

	/* reject CANCEL messages */
	if (msg->first_line.type == SIP_REQUEST)
	{
		if(msg->first_line.u.request.method_value != METHOD_INVITE)
		{
			LM_ERR("Called b2b_init on a Cancel message\n");
			return 0;
		}
		dlg.ruri = msg->first_line.u.request.uri;
	}

	pto = get_to(msg); //get to body;
	
	if (pto == NULL || pto->error != PARSE_OK)
	{
		LM_DBG("'To' header COULD NOT parsed\n");
		return 0;
	}
    //如果tag value 不是非0的话,则说明不是起始的请求;
	if(pto->tag_value.s!= 0 && pto->tag_value.len != 0)
	{
		LM_DBG("Not an initial request\n");
		dlg.tag[CALLEE_LEG] = pto->tag_value;  //to tag;
	}
	/* examine the from header */
	if (!msg->from || !msg->from->body.s)
	{
		LM_ERR("cannot find 'from' header!\n");
		return 0;
	}
	if (msg->from->parsed == NULL)
	{
		if ( parse_from_header( msg )<0 )
		{
			LM_ERR("cannot parse From header\n");
			return 0;
		}
	}
	
	pfrom = (struct to_body*)msg->from->parsed;
	
	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{   //如果没有fromtag, 则证明不正确的请求;
		LM_ERR("no from tag value present\n");
		return 0;
	}
	
	dlg.tag[CALLER_LEG] = pfrom->tag_value;

	/* for ACK TO & FROM headers must be the same as in Invite */
	if(init_dlg) {
		dlg.to_uri = init_dlg->to_uri;
		dlg.to_dname = init_dlg->to_dname;
		dlg.from_uri = init_dlg->from_uri;
		dlg.from_dname = init_dlg->from_dname;
	}
	else
	{
		dlg.to_uri= pto->uri;
		dlg.to_dname= pto->display;
		dlg.from_uri = pfrom->uri;
		dlg.from_dname = pfrom->display;
	}

	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LM_ERR("failed to parse callid header\n");
		return 0;
	}
	
	dlg.callid = msg->callid->body;

	if( msg->cseq==NULL || msg->cseq->body.s==NULL)
	{
		LM_ERR("failed to parse cseq header\n");
		return 0;
	}
	
	if (str2int( &(get_cseq(msg)->number), &dlg.cseq[CALLER_LEG])!=0 )
	{
		LM_ERR("failed to parse cseq number - not an integer\n");
		return 0;
	}
	dlg.last_invite_cseq = dlg.cseq[CALLER_LEG];
	dlg.cseq[CALLEE_LEG] = 1;

	if( msg->contact!=NULL && msg->contact->body.s!=NULL)
	{
		if(parse_contact(msg->contact) <0 )
		{
			LM_ERR("failed to parse contact header\n");
			return 0;
		}
		b= (contact_body_t* )msg->contact->parsed;
		if(b == NULL)
		{
			LM_ERR("contact header not parsed\n");
			return 0;
		}
		if(init_dlg) /* called on reply */
			dlg.contact[CALLEE_LEG] = b->contacts->uri;
		else
			dlg.contact[CALLER_LEG] = b->contacts->uri;
	}

	if(msg->record_route!=NULL && msg->record_route->body.s!= NULL)
	{
		if( print_rr_body(msg->record_route, &dlg.route_set[CALLER_LEG], (init_dlg?1:0), 0)!= 0)
		{
			LM_ERR("failed to process record route\n");
		}
	}
    	
	//
	
	dlg.send_sock = msg->rcv.bind_address;

	if(init_dlg) /* called on reply */
		dlg.contact[CALLER_LEG]=*local_contact;
	else
		dlg.contact[CALLEE_LEG]=*local_contact;

	if (!msg->content_length)
	{
		LM_ERR("no Content-Length header found!\n");
		return 0;
	}

	if(!init_dlg) /* called from server_new on initial Invite */
	{
		if(msg->via1->branch)
		{
			dlg.id = core_hash(&dlg.ruri, &msg->via1->branch->value, server_hsize);
		} else {
			dlg.id = core_hash(&dlg.ruri, &msg->callid->body, server_hsize);
		}
	}

	if(param)
		dlg.param = *param;
	dlg.db_flag = INSERTDB_FLAG;

	shm_dlg = b2b_dlg_copy(&dlg);
	
	if(shm_dlg == NULL)
	{
		LM_ERR("failed to copy dialog structure in shared memory\n");
		pkg_free(dlg.route_set[CALLER_LEG].s);
		return 0;
	}
	if(dlg.route_set[CALLER_LEG].s)
		pkg_free(dlg.route_set[CALLER_LEG].s);

	return shm_dlg;
}
/*******************************************************************************

 * 名称: b2breq_complete_ehdr

 * 功能: 按照传入参数构造报头
 
 * 形参: extra_header:    // exitra header,理解成额外字段;
 
         client_headers:  // client 本身构建的报头; 		 
      
	     ehdr_out:        // 构建好的报头;
		 
		 body:        
		 
		 local_contact:
		 
 * 返回: 0 or -1;
 * 说明: 部分逻辑依旧不明确;
 ******************************************************************************/
int b2breq_complete_ehdr(str* extra_headers, str *client_headers,str* ehdr_out, str* body, str* local_contact){
	
	str ehdr= {NULL,0};
	
	static char buf[BUF_LEN];
	static struct sip_msg foo_msg;

	if(((extra_headers?extra_headers->len:0) + 14 + local_contact->len +
		(client_headers?client_headers->len:0))> BUF_LEN)
	{
		LM_ERR("Buffer too small\n");
		return -1;
	}

	ehdr.s = buf;
	
	if(extra_headers && extra_headers->s && extra_headers->len)
	{
		memcpy(ehdr.s, extra_headers->s, extra_headers->len);
		ehdr.len = extra_headers->len;
	}
	
	ehdr.len += sprintf(ehdr.s+ ehdr.len, "Contact: <%.*s>\r\n",
		local_contact->len, local_contact->s);
	
	if (client_headers && client_headers->len && client_headers->s)
	{
		memcpy(ehdr.s + ehdr.len, client_headers->s, client_headers->len);
		ehdr.len += client_headers->len;
	}

	/* if not present and body present add content type */
	if(body) {
		/* build a fake sip_msg to parse the headers sip-wisely */
		memset(&foo_msg, 0, sizeof(struct sip_msg));
		foo_msg.len = ehdr.len;
		foo_msg.buf = foo_msg.unparsed = ehdr.s;

		if (parse_headers( &foo_msg, HDR_EOH_F, 0) == -1) {
			LM_ERR("Failed to parse headers\n");
			return -1;
		}

		if (!is_CT_present(foo_msg.headers)) {
			/* add content type header */
			if(ehdr.len + 32 > BUF_LEN)
			{
				LM_ERR("Buffer too small, can not add Content-Type header\n");
				return -1;
			}
			memcpy(ehdr.s+ ehdr.len, "Content-Type: application/sdp\r\n", 31);
			ehdr.len += 31;
			ehdr.s[ehdr.len]= '\0';
		}

		if (foo_msg.headers) free_hdr_field_lst(foo_msg.headers);
	}
	
	*ehdr_out = ehdr;

	return 0;
}
