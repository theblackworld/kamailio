// kamailio version 4.4;
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "../../parser/parse_rr.h"
#include "../../parser/parse_content.h"
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "../presence/hash.h"
#include "../tm/dlg.h"
#include "server.h"
#include "dlg.h"

/*******************************************************************************

 * 名称: server_new(hash index callid && fromtag )

 * 功能: 生成新的server实例:

 * 形参: msg:
 
         local_contact:  //根据接收到的local_contact所构建;
		 
		 b2b_cback:
		 
		 param:
 		 
 * 返回: NULL or 实际获取的指针.

 * 说明: 按照传入参数生产新的dialog, 注意server dialog的to tag 由b2b_key所决定;
 ******************************************************************************/ 
str* server_new(struct sip_msg* msg, str* local_contact, b2b_notify_t b2b_cback, str* param){
	
	b2b_dlg_t* dlg;
	
	unsigned int hash_index;
	
	int ret;

	if(param && param->len > B2BL_MAX_KEY_LEN)
	{
		LM_ERR("parameter too long, received [%d], maximum [%d]\n",
				param->len, B2BL_MAX_KEY_LEN);
		return NULL;
	}

	/* create new entry in hash table */
	// 根据msg local_contact创建;
	dlg = b2b_new_dlg(msg, local_contact, 0, param);
	
	if( dlg == NULL )
	{
		LM_ERR("failed to create new dialog structure entry\n");
		return NULL;
	}
	//global param;
	
	hash_index = core_hash(&dlg->callid, &dlg->tag[CALLER_LEG], server_hsize);

	/* check if record does not exist already */

	dlg->state = B2B_NEW;
	
	dlg->b2b_cback = b2b_cback;

	/* get the pointer to the tm transaction to store it the tuple record */
	dlg->uas_tran = tmb.t_gett();
	
	if(dlg->uas_tran == NULL || dlg->uas_tran == T_UNDEFINED)
	{
		ret = tmb.t_newtran(msg);  
		if(ret < 1)
		{
			if(ret== 0)
			{
				LM_DBG("It is a retransmission, drop\n");
			}
			else
				LM_DBG("Error when creating tm transaction\n");
			goto error;
		}
		dlg->uas_tran = tmb.t_gett();
	}
	
	tmb.ref_cell(dlg->uas_tran);
	tmb.t_setkr(REQ_FWDED);

	LM_DBG("new server entity[%p]: callid=[%.*s] tag=[%.*s] param=[%.*s] dlg->uas_tran=[%p]\n",
		dlg, dlg->callid.len, dlg->callid.s,
		dlg->tag[CALLER_LEG].len, dlg->tag[CALLER_LEG].s,
		dlg->param.len, dlg->param.s, dlg->uas_tran);

	/* add the record in hash table */
	dlg->db_flag = INSERTDB_FLAG;
	
	return b2b_htable_insert(server_htable, dlg, hash_index, B2B_SERVER, 0);
error:
	if(dlg)
		shm_free(dlg);
	return NULL;
}

void b2b_server_tm_cback( struct cell *t, int type, struct tmcb_params *ps){
	b2b_tm_cback(t, server_htable, ps);
}
	
	
	
	
	