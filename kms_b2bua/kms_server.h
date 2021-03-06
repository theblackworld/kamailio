#include "../../parser/msg_parser.h"
#include "kms_dlg.h"

str* server_new(struct sip_msg*, str* local_contact, b2b_notify_t, str*);

dlg_t* b2b_server_build_dlg(b2b_dlg_t* dlg);

void b2b_server_tm_cback( struct cell *t, int type, struct tmcb_params *ps);