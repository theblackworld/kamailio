#include "../../parser/msg_parser.h"
//export function in mod;
int room_entry_add(struct sip_msg* _m,char* _roomid, char* _callid, char* _fromtag, char* _totag);
//the function has not been reliazed;int room_entry_delete(struct sip_msg* _m, char* _roomid, char* _callid, char* _fromtag, char* _totag); //好像不太好删除;
int entry_get_by_roomid(struct sip_msg* _m, char* roomid,char* callid, char* fromtag, char* totag); //result store each call;
//only for debugging;
int print_by_room(struct sip_mag* _m, char* roomnew);
int print_all(struct sip_mas* _m);
