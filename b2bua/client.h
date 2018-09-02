#ifndef _B2B_CLIENT_H_
#define _B2B_CLIENT_H_

#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "dlg.h"
#include "b2b_entities.h"

str* client_new(client_info_t* ci, b2b_notify_t b2b_cback, b2b_add_dlginfo_t add_dlginfo, str* param);

void b2b_client_tm_cback( struct cell *t, int type, struct tmcb_params *ps);

dlg_t* b2b_client_build_dlg(b2b_dlg_t* dlg, dlg_leg_t* leg);

#endif