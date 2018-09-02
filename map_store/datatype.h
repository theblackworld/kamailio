#define ROOMSIZE 50
/*
***
   目前数据结构仅仅适用于一对多的通话,
   每一路通话的属性可以对应在map_entry_array中添加;
***
*/
/*
*  对应存储dialog每一条的信息;
*/
struct map_entry_array{
  char* callid;
  char* fromtag;
  char* totag;
};
/*
* 对应每一个房间的信息,这个结构本身可能需要带锁,目前仅仅是最简单的实现;
*/
struct map{
  char* roomid;     // 房间号;
  int size;         // 当前房间dialog数目;
  struct map* next;        //
  struct map_entry_array** entry; //对应存储房间信息,目前默认每个房间开启roomsize为50;
};
/*
* 对应的hashtable, size仅仅做保留位;
*/
struct hash_table{
  struct map* table;  //  0  1 2 3....;
  int size;
};
