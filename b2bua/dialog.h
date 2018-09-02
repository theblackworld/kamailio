#include "../../str.h"
#include "../../lock_ops.h"
#include "../tm/h_table.h"
#include "../tm/dlg.h"
#include "../dialog/dlg_load.h"

#define B2BL_MAX_KEY_LEN	21

#define CALLER_LEG   0
#define CALLEE_LEG   1

#define B2B_REQUEST   0
#define B2B_REPLY     1

#define DLG_ESTABLISHED   1

#define B2B_MAX_KEY_SIZE	(B2B_MAX_PREFIX_LEN+4+10+10+INT2STR_MAX_LEN)

enum b2b_entity_type {B2B_SERVER=0, B2B_CLIENT, B2B_NONE};

/*
 * Dialog state
 */
typedef enum b2b_state {
	B2B_UNDEFINED = 0, /* New dialog, no reply received yet */
	B2B_NEW,        /* New dialog, no reply received yet */
	B2B_NEW_AUTH,   /* New dialog with auth info, no reply received yet */
	B2B_EARLY,      /* Early dialog, provisional response received */
	B2B_CONFIRMED,  /* Confirmed dialog, 2xx received */
    
	B2B_ESTABLISHED,/* Established dialog, sent or received ACK received */
	B2B_MODIFIED,   /* ReInvite inside dialog */
	B2B_DESTROYED,  /* Destroyed dialog */
	B2B_TERMINATED, /* Terminated dialog */
	B2B_LAST_STATE  /* Just to know the number of states */
} b2b_state_t;


typedef struct b2b_dlg_leg {
	int id;
	str tag;            
	unsigned int cseq; 
	str route_set;   //record route, 路由信息;
	str contact;
	struct b2b_dlg_leg* next;
}dlg_leg_t;


/** Definitions for structures used for storing dialogs */
typedef struct b2b_dlg
{
	unsigned int         id;         // local_index；
	
	b2b_state_t          state;      // dialog状态;

    //标记dialog信息	
	
	str                  ruri;       // request Uri    
	str                  callid;      
	str                  from_uri;   
	str                  from_dname; 
	str                  to_uri;     
	str                  to_dname;   
	
	str                  tag[2];           // from and to;  	
	
	unsigned int         cseq[2];          // 两端的事物;

	unsigned int         last_invite_cseq; 
	
	str                  route_set[2];
	
	str                  contact[2];       // 两个方向的contact
	
	enum request_method  last_method;
	
	struct b2b_dlg      *next;
	struct b2b_dlg      *prev;
	
	b2b_notify_t         b2b_cback;        //预留的两个回调函数;
	b2b_add_dlginfo_t    add_dlginfo;
	
	str                  param;
	str                  ack_sdp;
	
	struct cell*         uac_tran;         //这个数据结构还需要好好看看;
	struct cell*         uas_tran;
	struct cell*         update_tran;
	struct cell*         cancel_tm_tran;	
	
	dlg_leg_t*           legs;
	
	struct socket_info*  send_sock;        //
	
	unsigned int         last_reply_code;  // 主要用来来判断是否是重传;
	
	int                  db_flag;          // 是否放入数据库中;
}b2b_dlg_t;

//copied from  opensips
typedef struct b2b_dlginfo
{
	str callid;
	str fromtag;
	str totag;
}b2b_dlginfo_t;

//request data;
typedef struct b2b_req_data
{
	enum b2b_entity_type et;
	str* b2b_key;
	str* method;
	str* extra_headers;
	str* client_headers;
	str* body;
	b2b_dlginfo_t* dlginfo;
	unsigned int no_cb;
}b2b_req_data_t;

//reply data;
typedef struct b2b_rpl_data
{
	enum b2b_entity_type et;
	str* b2b_key;
	
	int method;
	int code;    //reply code;
	str* text;
	str* body;
	str* extra_headers;
	b2b_dlginfo_t* dlginfo;
}b2b_rpl_data_t;


typedef struct client_info
{
	str method;
	str from_uri;
	str from_dname;
	str req_uri;
	str dst_uri;
	str to_uri;
	str to_dname;
	str* extra_headers;
	str* client_headers;
	str* body;
	str* from_tag;
	str local_contact;
	
	unsigned int cseq;   // uac端事物;
	struct socket_info* send_sock;
	struct usr_avp *avps;
}client_info_t;

typedef struct b2b_entry
{
	b2b_dlg_t* first;
	gen_lock_t lock;
	int checked;
}b2b_entry_t;


//table init；
int init_b2b_htables(void);
void destroy_b2b_htables(); 

// key format : B2B.hash_index.local_index.timestamp
str* b2b_generate_key(unsigned int hash_index, unsigned int local_index);
int b2b_parse_key(str* key, unsigned int* hash_index,unsigned int* local_index);
		
			
b2b_dlg_t* b2b_search_htable(b2b_table table, unsigned int hash_index, unsigned int local_index);
b2b_dlg_t* b2b_search_htable_dlg(b2b_table table, unsigned int hash_index, unsigned int local_index, str* to_tag, str* from_tag, str* callid);
str* b2b_htable_insert(b2b_table table, b2b_dlg_t* dlg, int hash_index, int src, int reload);



//send request or reply inside the dialog;
int b2b_send_reply(b2b_rpl_data_t*);

typedef int (*b2b_send_reply_t)(b2b_rpl_data_t*);

typedef int (*b2b_send_request_t)(b2b_req_data_t*);

int b2b_send_request(b2b_req_data_t*);












































































