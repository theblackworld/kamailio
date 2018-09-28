#include "kms_dlg.h"

void b2b_entities_dump(int no_lock);
int b2b_entities_restore(void);
int b2be_db_insert(b2b_dlg_t* dlg, int type);
int b2be_db_update(b2b_dlg_t* dlg, int type);
void b2be_initialize(void);
void b2b_db_delete(str param);
void b2b_entity_db_delete(int type, b2b_dlg_t* dlg);
