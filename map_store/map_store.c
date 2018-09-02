#include<stdlib.h>
#include<string.h>
//伪变量的操作部分;
#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../sr_module.h"
//#include "../../lvalue.h" 
#include "../../hashes.h"
//共享内存部分;
#include "../../mem/shm_mem.h"


//#include "datatype.h"
#include "global.h"
#include "map_util.h"
//
static void mem_release(struct map* current){
  unsigned int i=0;	
  for(i=0; i<ROOMSIZE; i++){
	shm_free(current->entry[i]); //release the entry memory;  	  
  }	
  shm_free(current->entry);
  shm_free(current);
}
// copy from jsonrpc module;
// private;
static char* shm_strdup(str *src)
{
	char *res;
    if (!src || !src->s)
		return NULL;		
	if (!(res = (char *) shm_malloc(src->len + 1)))
		return NULL;
	strncpy(res, src->s, src->len);
	res[src->len] = 0;	
	return res;
}
/*******************************************************************************
 * 名称: room_get_by_roomid
 * 功能: 在首次调用时候找到当前房间信息;
 * 形参: roomid 
 * 返回: room pointer， NULL失败意味着不存在这样的roomid。
 * 说明: 
 ******************************************************************************/
static struct map* room_get_by_roomid(const char* roomid){
  if(roomid == NULL){
	LM_ERR("room id si NULL");
    return NULL;	
  }
  unsigned int i = 0;  
  struct map* room = NULL;
  for(i=0; i<hashsize; i++){
    room = map_table->table[i].next;
    if(room == NULL) continue;		
	else{	  
	  while(room!=NULL){
		//find the target room;
		if(strcmp(roomid,room->roomid) == 0){
	     break;			
		}
		room = room->next;
	  }	  
      if(room != NULL) break; //find it;	  
	}  
  }
  return room;  
}
/*******************************************************************************
 * 名称: room_entry_add
 * 功能: 往hash_table指定的room_id中添加dialog信息;
 * 形参: _roomid:    $rU;
 *       _callid:    $dlg(callid);
 *       _fromtag:   $dlg(fromtag);
 *       _totag:     $dlg(totag);
 *
 * 返回: 0成功， -1失败。
 * 说明: 中间的资源释放可能尚存在问题;
 ******************************************************************************/
int room_entry_add(struct sip_msg* _m,char* _roomid, char* _callid, char* _fromtag, char* _totag){	

  str key;       //roomid;
  str callid;	
  str fromtag;
  str totag;  
 
  if(fixup_get_svalue(_m,(gparam_p)_roomid,&key)!=0){
	LM_ERR("can not get key value");
    return -1;	
  }
  if(fixup_get_svalue(_m,(gparam_p)_callid,&callid)!=0){
	LM_ERR("can not get callid value");
    return -1;	
  }
  if(fixup_get_svalue(_m,(gparam_p)_roomid,&fromtag)!=0){
	LM_ERR("can not get fromtag value");
    return -1;	
  }
  if(fixup_get_svalue(_m,(gparam_p)_roomid,&totag)!=0){
	LM_ERR("can not get totag value");
    return -1;	
  }	
  struct map_entry_array* entry;
  if(!(entry = (struct map_entry_array*)shm_malloc(sizeof (struct map_entry_array)))){
	LM_ERR("Out of memory!");
	return -1;
  }
  // shared memory alloc
  entry->callid = shm_strdup(&callid);
  entry->fromtag = shm_strdup(&fromtag);
  entry->totag = shm_strdup(&totag);
  // 需要根据key找到table对应entry;
  unsigned int hash = get_hash1_case_raw(key.s,key.len); 
  LM_INFO("hash size is%d",hash);  
  unsigned int slot = hash % hashsize;  
  struct map* room;  
  struct map* start = map_table->table[slot].next;  
  struct map* prev = start;
  //锁的问题,这样直接用可能显得过于笨重;  
  lock_get(roomlock);
  while(start!=NULL &&  strcmp(start->roomid, key.s) != 0){
	prev = start;	
	start = start->next;				
  }  	
  //roomlock;
  if(start == NULL){	  
	if(!(room = (struct map*) shm_malloc (sizeof (struct map)))){
	  LM_ERR("Out of memory!");
	  return -1;
    }
	room->size = 0;	
	room->next = NULL;
	room->roomid = shm_strdup(&key);	
	if(room->roomid == NULL){		
	  LM_ERR("roomid allocate failed ");	  
	  return -1;
    }	
	// key的问题
	room->entry = (struct map_entry_array**)shm_malloc(sizeof(struct map_entry_array*) * ROOMSIZE);
	if(room->entry == NULL){
	  LM_ERR("Room allocate fail");
      return -1;	  
	}
	room->entry[room->size] = entry;  //  
    room->size++;  
	if(prev == NULL){
	  map_table->table[slot].next = room;
	}
	else{ 
	  prev->next = room;
    }
  }
  else{
    if(start->size == ROOMSIZE){		
	  LM_ERR("the room is full");
	  return -1;	  
	}   	
	start->entry[start->size] = entry; 	
    start->size++;  	
  }	
  lock_release(roomlock);  
  return 0;
}
/*******************************************************************************
 * 名称: entry_get_by_roomid
 * 功能: 根据roomid找到当前信息,需要调用room_get_by_roomid;
 * 形参: roomid ,  result暂时定位存放结果的魔法变量;
 * 返回: 
 * 说明: 由于是循环调用, 仅仅在首次调用的时候找当前的房间号; 后序操作都不需要再找当前的房间号;
   其中 current 与 current_size由于是全局变量所以在实际的调用中会随着循环调用而改变当前值;
 * 注意： 可能缺少魔法变量的具体的数据结构;  
 ******************************************************************************/
//这个函数存在优化的可能性;
int entry_get_by_roomid(struct sip_msg* msg, const char* roomid,char* callid,char* fromtag,char* totag){
  //current == NULL means the first call function;
  str key;   
  pv_spec_t* dst_pv1; 
  pv_spec_t* dst_pv2; 
  pv_spec_t* dst_pv3; 
  
  pv_value_t dst_val1;
  pv_value_t dst_val2;
  pv_value_t dst_val3;
  if(fixup_get_svalue(msg, (gparam_p)roomid, &key) != 0){  
    LM_ERR("room get failed");
	return -1;
  } 
  dst_pv1 = (pv_spec_t *)callid;
  dst_pv2 = (pv_spec_t *)fromtag;
  dst_pv3 = (pv_spec_t *)totag;
  //首次调用时, current为NULL;
  if(current == NULL){	  
    current = room_get_by_roomid(roomid);
	if(current == NULL){
	  LM_ERR("the roomid is not exsits so can not find it");
      return -1;	  
	}	
	current_size = current->size;  //the current room_size ;
	if(current_size < 0){		
	  mem_release(current);  // 意味着当前房间信息全部获取完毕，需要释放当前资源;
      current = NULL; 	     // 为了下一次的首次调用;	  
	  current_size = 0;	  
	  return -1;			
	}
	else{
	  // 仍需要一个转化过程;
	  // store the dialog info in the $var(callid) , $var(fromtag), $var(totag);
      if(char_to_val(&dst_val1, current->entry[current_size], CALLID) == -1 ||	  
		 char_to_val(&dst_val2, current->entry[current_size], FROMTAG) == -1 ||
	     char_to_val(&dst_val3, current->entry[current_size], TOTAG) == -1){
		 LM_ERR("result pv value set error");	
		 //可能存在内存泄漏问题;
		 return -1; 	 
	  }
      dst_pv1->setf(msg, &dst_pv1->pvp, (int)EQ_T, &dst_val1);	  
      dst_pv2->setf(msg, &dst_pv2->pvp, (int)EQ_T, &dst_val2);      
	  dst_pv3->setf(msg, &dst_pv3->pvp, (int)EQ_T, &dst_val3);
	  current_size--;	  
	  return 0;
	}	 
  }
  else{
    //需要遍历整个room;   	  
	if(current_size <0){
	  mem_release(current);  // 意味着当前房间信息全部获取完毕，需要释放当前资源;
	  
      current = NULL; 	     // 为了下一次的首次调用;
	  current_size = 0;
	  return -1;		
	}
	else{
	  //store the dialog info;
      if(char_to_val(&dst_val1, current->entry[current_size], CALLID) == -1 ||
		 char_to_val(&dst_val2, current->entry[current_size], FROMTAG) == -1 ||
		 
	     char_to_val(&dst_val3, current->entry[current_size], TOTAG) == -1){
		 
		 LM_ERR("result pv value set error");			 
		 //可能存在内存泄漏问题;
		 return -1; 	 
	  }
	  
      dst_pv1->setf(msg, &dst_pv1->pvp, (int)EQ_T, &dst_val1);	  
      dst_pv2->setf(msg, &dst_pv2->pvp, (int)EQ_T, &dst_val2);
	  dst_pv3->setf(msg, &dst_pv3->pvp, (int)EQ_T, &dst_val3);
	  current_size--;	  
	  return 0;			
	}
  }
}
/*******************************************************************************
 * 名称: print_by_name
 * 功能: 打印指定房间的所有dlg信息;
 * 形参: s:  roomid(房间名称);
 *
 
 * 返回: 0成功， -1失败。
 * 说明: 仅在debug中调用查看当前module是否调用成功;
 ******************************************************************************/
void print_by_room(const char* s){
  unsigned int hash = get_hash1_case_raw(s,strlen(s)); 
  unsigned int slot = hash % hashsize;
  struct map* start = map_table->table[slot].next;
  if(start == NULL){
    LM_INFO("the room size if 0\n");
  }
  for(int i=0; i<start->size; i++){
	LM_INFO("the callid is:%s,the fromtag is:%s, the totag is: %s\n",start->entry[i]->callid,start->entry[i]->fromtag,start->entry[i]->totag);  
  }
}
/*******************************************************************************
 * 名称: print_all
 * 功能: 打印所有房间的信息;
 * 形参: 无
 * 返回: 0成功， -1失败。
 * 说明: 仅在debug中调用查看当前module是否调用成功;
 ******************************************************************************/
void print_all(){
  unsigned int i=0;  
  for(i=0; i<hashsize; i++){	  
    struct map* room = map_table->table[i].next;	
    while(room != NULL){
      LM_INFO("the room  is:%s\n",room->roomid);
      for(int j=0; j<room->size; j++){ 
        LM_INFO("the callid is:%s,the fromtag is:%s, the totag is: %s\n",room->entry[j]->callid,room->entry[j]->fromtag,room->entry[j]->totag);
      }
      room = room->next;
    }
  }
}