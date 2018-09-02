/*****************************************************
//主要存储了用于握手时候的功能函数;
******************************************************/
#define WEBSOCKET_SHAKE_KEY_LEN 16 //你可以指定;
/*******************************************************************************
 获取指定长度的随机字符串用于后面握手时候的消息构造
 ******************************************************************************/
void webSocket_getRandomString(unsigned char *buf, unsigned int len);

int  webSocket_buildShakeKey(unsigned char *key);

int base64_encode( const unsigned char *bindata, char *base64, int binlength);

int webSocket_matchShakeKey(unsigned char *myKey, unsigned int myKeyLen, unsigned char *acceptKey, unsigned int acceptKeyLen);

/******************************
*主要是为了match key做服务使用;
*******************************/
int webSocket_buildRespondShakeKey(unsigned char *acceptKey, unsigned int acceptKeyLen, unsigned char *respondKey);

int htoi(const char s[], int start, int len);

const char base64char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
