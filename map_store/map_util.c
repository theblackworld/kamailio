#include<string.h>
#include "map_util.h"
/*******************************************************************************
 * 名称: char_to_val
 * 功能: 把entry的三条属性存放到伪变量里;
 * 形参: val:伪变量，  entry: 存放房间属性; type: 以下有说明; 
 * 返回: 0成功,  -1失败;
 * 说明: 
 ******************************************************************************/
//   type : 0   callid;
//          1   fromtag;
//          2   totag;
int  char_to_val(pv_value_t* val, struct map_entry_array* entry,int type){ //因为存放的是每一个房间的条目;
  char* result = NULL;

  switch(type){
	case 0:
      result = entry->callid;
	  break;	
	case 1:
      result = entry->fromtag;
      break;
    case 2:
	  result = entry->totag;
	  break;
	default:
      return -1;
	  break;  	
  }  
  val->rs.s = result;            // to store the value;
  
  val->rs.len = strlen(result);
  
  val->flags = PV_VAL_STR;      // STR;
  
  return 0;
}
