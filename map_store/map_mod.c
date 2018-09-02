// auhor : L
// date  :   7.3

#include <string.h>
#include <stdlib.h>#include <stdio.h>
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "../../trim.h"
#include "../../sr_module.h"
//#include "../../parser/hf.h"
#include "../../cfg/cfg_struct.h"

#include "../tm/tm_load.h"

#include "../../locking.h"
// self definition header
#include "map_store.h"
#include "datatype.h"
#include "global.h"
MODULE_VERSION
static int mod_init(void);
static int fixup_map(void** param, int param_no);
static int fixup_map_free(void **param,int param_no);
static int fixup_room(void** param,int param_no);
static int fixup_room_free(void** param, int param_no);
// in global.h
struct hash_table* map_table ; //global variable;
struct map* current = NULL;
unsigned int current_size = 0;

gen_lock_t *roomlock;          // lock the hash table; 
                               // 需要改进需要多把锁;
                               // 可实现定期的把数据down到数据库里
unsigned int hashsize = 100;
/********************
***cmd export
********************/
static cmd_export_t cmds[] = {
  {"room_entry_add", (cmd_function)room_entry_add, 4, fixup_map, fixup_map_free, ANY_ROUTE},		
  {"entry_get_by_roomid", (cmd_function)entry_get_by_roomid, 4, fixup_room , fixup_room_free, ANY_ROUTE},
  {"print_all" , (cmd_function)print_a;;}
  {0, 0, 0, 0, 0, 0}	
};
/***********************
*Script Parameters
***********************/
static param_export_t mod_params[]={
	{"hashsize", INT_PARAM, &hashsize},
	{ 0,0,0 }
};
// 可能需要锁的初始化;
/********************
* module exports
********************/
struct module_exports exports = {
  "map_store",	
  DEFAULT_DLFLAGS,	
  cmds,        // export functions;	
  mod_params,  // export parameters;
  0,           /* exported statistics */
  0,           /* exported MI functions */
  0,           /* exported pseudo-variables */
  0,           /* extra processes */  
  mod_init,    // mod init function;
  0,
  0, // mod destory function
  0  
};
static int mod_init(void){
  //锁的初始化
  if(hashsize <=0 ){
    LM_ERR("hashsize is less than 0");
    return -1;	
  }  
  //hash table初始化;
  if(!(map_table = (struct hash_table*)shm_malloc(sizeof(struct hash_table)))){
	LM_ERR("MOD INIT MAP_TABLE FAILED");
	return -1;
  }   
  map_table->size = 0;   
  if(!(map_table->table = (struct map*)shm_malloc(sizeof(struct map)*hashsize))){
	LM_ERR("MOD INIT MAP FAILED");
	shm_free(map_table);
 	return -1;	   
  }
  
  //针对hash table的初始化;
  for(int i=0; i<hashsize; i++){
	map_table->table[i].roomid = NULL;  
	map_table->table[i].size = 0;       
	map_table->table[i].next = NULL;  	
    map_table->table[i].entry = NULL;
  }  
  
  roomlock = lock_alloc();	
  //锁的初始化;  
  if(roomlock == NULL){
	 LM_ERR("lock alloc failed");
     return -1;	 
  }	  
  if(lock_init(roomlock) == NULL){
	 LM_ERR("lock init failed");
     lock_dealloc(roomlock);
     return -1;	 
  }	
  return 0;	 	
}
/* Fixup Functions */
static int fixup_map(void** param, int param_no){
  if(param_no <=4){
	return fixup_spve_null(param, 1);   
  }	
  LM_ERR("function takes at most 4 parameters.\n");
  return -1;	
}

static int fixup_map_free(void** param, int param_no){
  if (param_no <= 4) {
	return fixup_free_spve_null(param, 1);
  }	
  LM_ERR("function takes at most 4 parameters.\n");
  return -1;			
}
// mabybe is only exists three param;
static int fixup_room(void** param, int param_no){
  if(param_no <=4){
	return fixup_spve_null(param, 1);   
  }
  LM_ERR("function takes at most 4 parameters.\n");
  return -1;	
}

static int fixup_room_free(void** param, int param_no){
  if(param_no <=4){
	return fixup_free_spve_null(param, 1);   
  }
  LM_ERR("function takes at most 4 parameters.\n");
  return -1;	
}
