extern unsigned int hashsize ;       
extern struct hash_table* map_table;  //  map store the information;
extern struct map* current;           //  map current;
extern unsigned int current_size;     //  当前room的大小;  
gen_lock_t *roomlock;                 //  在获取整个表的时候需要加锁;