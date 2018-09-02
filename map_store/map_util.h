#ifndef _MAP_UTILS_H_
#define _MAP_UTILS_H_
#include "datatype.h"
#include "../../sr_module.h"
#include "../../lvalue.h"
#define CALLID  0
#define FROMTAG 1
#define TOTAG   2
typedef int (*jansson_to_val_f)(pv_value_t* , struct map_entry_array* , int );
int  char_to_val(pv_value_t* val,struct map_entry_array* entry,int type);//因为存放的是每一个房间的条目; 
#endif
