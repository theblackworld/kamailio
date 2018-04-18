#include "aid_function.h"
#include "sha1.h"
/*******************************************************************************
 * 名称: webSocket_getRandomString
 * 功能: 生成随机字符串
 * 形参: *buf：随机字符串存储到
 *              len : 生成随机字符串长度
 * 返回: 无
 * 说明: 无
 ******************************************************************************/
void webSocket_getRandomString(unsigned char *buf, unsigned int len)
{ 
    if(buf == NULL){
       LM_ERR("can not allocate memory");
    }

    unsigned int i;
    unsigned char temp;
    srand((int)time(0));
    for(i = 0; i < len; i++)
    {
        temp = (unsigned char)(rand()%256);
        if(temp == 0)   // 随机数不要0, 0 会干扰对字符串长度的判断
            temp = 128;
        buf[i] = temp;
    }
}
/*******************************************************************************
 * 名称: base64_encode
 * 功能: 把指定字符串进行base64编码,
 * 形参: *bindata你所给的随机串;
         *base64存放编码好之后的字符串;
         binlength你所给的data的长度
 * 返回: 无
 * 说明: 使用的时候需要预先为base64准备足够大的内存空间,具体使用考虑后续补充;
 ******************************************************************************/
int base64_encode( const unsigned char *bindata, char *base64, int binlength)
{
    const char base64char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int i, j;
    unsigned char current;
    for ( i = 0, j = 0 ; i < binlength ; i += 3 )
    {
        current = (bindata[i] >> 2) ;
        current &= (unsigned char)0x3F;
        base64[j++] = base64char[(int)current];
        current = ( (unsigned char)(bindata[i] << 4 ) ) & ( (unsigned char)0x30 ) ;
        if ( i + 1 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+1] >> 4) ) & ( (unsigned char) 0x0F );
        base64[j++] = base64char[(int)current];
        current = ( (unsigned char)(bindata[i+1] << 2) ) & ( (unsigned char)0x3C ) ;
        if ( i + 2 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+2] >> 6) ) & ( (unsigned char) 0x03 );
        base64[j++] = base64char[(int)current];
        current = ( (unsigned char)bindata[i+2] ) & ( (unsigned char)0x3F ) ;
        base64[j++] = base64char[(int)current];
    }
    base64[j] = '\0';
    return j;
}
/*******************************************************************************
 * 名称: webSocket_buildShakeKey
 * 功能: 生成握手的key
 * 形参: *key：实际生成的由随机字符串构成经过base64编码的字符串;
 *              len : 生成随机字符串长度
 * 返回: 无
 * 说明: 使用时候需要注意为key申请足够的内存通常为（随机字符串长度的4-8倍）,这里的内存
 * 管理考虑是不是由我自己做;
 ******************************************************************************/
int webSocket_buildShakeKey(unsigned char *key)
{
    unsigned char tempKey[WEBSOCKET_SHAKE_KEY_LEN] = {0};
    webSocket_getRandomString(tempKey, WEBSOCKET_SHAKE_KEY_LEN);
    return base64_encode((const unsigned char *)tempKey, (char *)key, WEBSOCKET_SHAKE_KEY_LEN);
}
int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + 'a' - 'A';
    }
    else
    {
        return c;
    }
}
int htoi(const char s[], int start, int len)
{
    int i, j;
    int n = 0;
    if (s[0] == '0' && (s[1]=='x' || s[1]=='X')) //判断是否有前导0x或者0X
    {
        i = 2;
    }
    else
    {
        i = 0;
    }
    i+=start;
    j=0;
    for (; (s[i] >= '0' && s[i] <= '9')
       || (s[i] >= 'a' && s[i] <= 'f') || (s[i] >='A' && s[i] <= 'F');++i)
    {
        if(j>=len)
        {
            break;
        }
        if (tolower(s[i]) > '9')
        {
            n = 16 * n + (10 + tolower(s[i]) - 'a');
        }
        else
        {
            n = 16 * n + (tolower(s[i]) - '0');
        }
        j++;
    }
    return n;
}

/*******************************************************************************
 * 名称: webSocket_buildRespondShakeKey
 * 功能: 生成回应的key，这里我使用主要是为了与客户端接收到的key做比较从而确定是否建立连接;
 * 形参: *acceptkey：   从client的获取到的accpetKey;
 *        acceptKeyLen: 获取到key的长度;
 *        *respondKey:  生成的回应key;
 * 返回: 返回生成key的长度;
 * 说明:
 ******************************************************************************/
int webSocket_buildRespondShakeKey(unsigned char *acceptKey, unsigned int acceptKeyLen, unsigned char *respondKey)
{
    char *clientKey;
    char *sha1DataTemp;
    char *sha1Data;
    int i, n;
    const char GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned int GUIDLEN;

    if(acceptKey == NULL)
        return 0;
    GUIDLEN = sizeof(GUID);
    clientKey = (char *)calloc(1, sizeof(char)*(acceptKeyLen + GUIDLEN + 10));
    memset(clientKey, 0, (acceptKeyLen + GUIDLEN + 10));
    //
    memcpy(clientKey, acceptKey, acceptKeyLen);
    memcpy(&clientKey[acceptKeyLen], GUID, GUIDLEN);
    clientKey[acceptKeyLen + GUIDLEN] = '\0';
    //
    sha1DataTemp = sha1_hash(clientKey);
    n = strlen(sha1DataTemp);
    sha1Data = (char *)calloc(1, n / 2 + 1);
    memset(sha1Data, 0, n / 2 + 1);
   //
    for(i = 0; i < n; i += 2)
        sha1Data[ i / 2 ] = htoi(sha1DataTemp, i, 2);
    n = base64_encode((const unsigned char *)sha1Data, (char *)respondKey, (n / 2));
    //
    free(sha1DataTemp);
    free(sha1Data);
    free(clientKey);
    return n;
}
/*******************************************************************************
 * 名称: webSocket_matchShakeKey;
 * 功能: 匹配服务端回送的key;
 * 形参: *mykey：       客户端之前发送出去的key;
 *        myKeyLen:     mykey长度;
 *        *acceptKey:   服务端生成的回应key;
 *        acceptketLen: accept长度
 * 返回:  0 匹配成功，同意连接; -1 匹配失败拒绝连接;
 * 说明:
 ******************************************************************************/

int webSocket_matchShakeKey(unsigned char *myKey, unsigned int myKeyLen, unsigned char *acceptKey, unsigned int acceptKeyLen)
{
    int retLen;
    unsigned char tempKey[256] = {0};
    //
    retLen = webSocket_buildRespondShakeKey(myKey, myKeyLen, tempKey);
    //printf("webSocket_matchShakeKey :\r\n%d : %s\r\n%d : %s\r\n", acceptKeyLen, acceptKey, retLen, tempKey);
    //
    if(retLen != acceptKeyLen)
    {
        printf("webSocket_matchShakeKey : len err\r\n%s\r\n%s\r\n%s\r\n", myKey, tempKey, acceptKey);
        return -1;
    }
    else if(strcmp((const char *)tempKey, (const char *)acceptKey) != 0)
    {
        printf("webSocket_matchShakeKey : str err\r\n%s\r\n%s\r\n", tempKey, acceptKey);
        return -1;
    }
    return 0;
}
