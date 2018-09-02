/*
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "netstring.h"

#include "../../core/sr_module.h"
#include "../../core/mem/mem.h"
#include "aid_function.h"

int netstring_read_fd(int fd, char **netstring) {
  int i, bytes;
	size_t len = 0;

 	*netstring = NULL;

	char buffer[10]={0};
	
	/* Peek at first 10 bytes, to get length and colon */
	bytes = recv(fd,buffer,10,MSG_PEEK);

	if (bytes<3) return NETSTRING_ERROR_TOO_SHORT;
	
  /* No leading zeros allowed! */
  if (buffer[0] == '0' && isdigit(buffer[1]))
    return NETSTRING_ERROR_LEADING_ZERO;

  /* The netstring must start with a number */
  if (!isdigit(buffer[0])) return NETSTRING_ERROR_NO_LENGTH;

  /* Read the number of bytes */
  for (i = 0; i < bytes && isdigit(buffer[i]); i++) {
    /* Error if more than 9 digits */
    if (i >= 9) return NETSTRING_ERROR_TOO_LONG;
    /* Accumulate each digit, assuming ASCII. */
    len = len*10 + (buffer[i] - '0');
  }

  /* Read the colon */
  if (buffer[i++] != ':') return NETSTRING_ERROR_NO_COLON;
	
	/* Read the whole string from the buffer */
	size_t read_len = i+len+1;
	char *buffer2 = pkg_malloc(read_len);
	if (!buffer2) {
		LM_ERR("Out of memory!");
		return -1;
	}
	bytes = recv(fd,buffer2,read_len,0);

  /* Make sure we got the whole netstring */
  if (read_len > bytes) return NETSTRING_ERROR_TOO_SHORT;
	
  /* Test for the trailing comma */
  if (buffer2[read_len-1] != ',') return NETSTRING_ERROR_NO_COMMA;

	buffer2[read_len-1] = '\0';
	
	int x;
		
	for(x=0;x<=read_len-i-1;x++) {
		buffer2[x]=buffer2[x+i];
	}
	
	*netstring = buffer2;
  return 0;
}


/* Reads a netstring from a `buffer` of length `buffer_length`. Writes
   to `netstring_start` a pointer to the beginning of the string in
   the buffer, and to `netstring_length` the length of the
   string. Does not allocate any memory. If it reads successfully,
   then it returns 0. If there is an error, then the return value will
   be negative. The error values are:

   NETSTRING_ERROR_TOO_LONG      More than 999999999 bytes in a field
   NETSTRING_ERROR_NO_COLON      No colon was found after the number
   NETSTRING_ERROR_TOO_SHORT     Number of bytes greater than buffer length
   NETSTRING_ERROR_NO_COMMA      No comma was found at the end
   NETSTRING_ERROR_LEADING_ZERO  Leading zeros are not allowed
   NETSTRING_ERROR_NO_LENGTH     Length not given at start of netstring

   If you're sending messages with more than 999999999 bytes -- about
   2 GB -- then you probably should not be doing so in the form of a
   single netstring. This restriction is in place partially to protect
   from malicious or erroneous input, and partly to be compatible with
   D. J. Bernstein's reference implementation.

   Example:
      if (netstring_read("3:foo,", 6, &str, &len) < 0) explode_and_die();
 */

int netstring_read(char *buffer, size_t buffer_length,
		   char **netstring_start, size_t *netstring_length) {
  int i;
  size_t len = 0;

  /* Write default values for outputs */
  *netstring_start = NULL; *netstring_length = 0;

  /* Make sure buffer is big enough. Minimum size is 3. */
  if (buffer_length < 3) return NETSTRING_ERROR_TOO_SHORT;

  /* No leading zeros allowed! */
  if (buffer[0] == '0' && isdigit(buffer[1]))
    return NETSTRING_ERROR_LEADING_ZERO;

  /* The netstring must start with a number */
  if (!isdigit(buffer[0])) return NETSTRING_ERROR_NO_LENGTH;

  /* Read the number of bytes */
  for (i = 0; i < buffer_length && isdigit(buffer[i]); i++) {
    /* Error if more than 9 digits */
    if (i >= 9) return NETSTRING_ERROR_TOO_LONG;
    /* Accumulate each digit, assuming ASCII. */
    len = len*10 + (buffer[i] - '0');
  }

  /* Check buffer length once and for all. Specifically, we make sure
     that the buffer is longer than the number we've read, the length
     of the string itself, and the colon and comma. */
  if (i + len + 1 >= buffer_length) return NETSTRING_ERROR_TOO_SHORT;

  /* Read the colon */
  if (buffer[i++] != ':') return NETSTRING_ERROR_NO_COLON;
  
  /* Test for the trailing comma, and set the return values */
  if (buffer[i + len] != ',') return NETSTRING_ERROR_NO_COMMA;
  *netstring_start = &buffer[i]; *netstring_length = len;

  return 0;
}

/* Return the length, in ASCII characters, of a netstring containing
   `data_length` bytes. */
size_t netstring_buffer_size(size_t data_length) {
  if (data_length == 0) return 3;
  return (size_t)ceil(log10((double)data_length + 1)) + data_length + 2;
}

/* Allocate and create a netstring containing the first `len` bytes of
   `data`. This must be manually freed by the client. If `len` is 0
   then no data will be read from `data`, and it may be NULL. */
size_t netstring_encode_new(char **netstring, char *data, size_t len) {
  char *ns;
  size_t num_len = 1;

  if (len == 0) {
    ns = pkg_malloc(3);
	if (!ns) {
		LM_ERR("Out of memory!");
		return 0;
	}
    ns[0] = '0';
    ns[1] = ':';
    ns[2] = ',';
  } else {
    num_len = (size_t)ceil(log10((double)len + 1));
    ns = pkg_malloc(num_len + len + 2);
	if (!ns) {
		LM_ERR("Out of memory!");
		return 0;
	}
    sprintf(ns, "%lu:", (unsigned long)len);
    memcpy(ns + num_len + 1, data, len);
    ns[num_len + len + 1] = ',';
  }

  *netstring = ns;
  return num_len + len + 2;
}
/*******************************************************************************
 * 名称: websocket_encode_new
 * 功能: 将json数据封装成帧;
 * 形参: **netstring用于存储生成的帧；
 *       *data即json数据;
         len传入json数据的长度
 * 返回: 返回封装好的帧的长度
 * 说明: 这里面只有正常的交互帧，只支持文本传输, 并不考虑close结束等操作,没有针对异常做处理;
 * 仅在本地做过测试.
 ******************************************************************************/
size_t websocket_encode_new(unsigned char** netstring, unsigned char* data, size_t len){
  unsigned int i=0, j=0,package_len = 5,flag = 0;  // 1自己固定报头加四字节掩码;
  unsigned char masked[4] = {'0'};	//随机构造四个掩码;
  unsigned char* temp = data; 
  unsigned char* package;
  
  webSocket_getRandomString(mask,4);
  
  //首先构造帧头;
 
  for(i=0;  i<len; i++){
    data[i] = data[i] ^ masked[i%4];
  } 
  //计算包的长度:
  
  if(len < 126){
	package_len += len+1;  //加上json格式的长度以及一字节payloadLen长度;  
  }
  else{
	package_len += len+3;  //因为需要额外的两字节来限定长度;    
  }
  
  package = (unsigned char*)pkg_malloc(package_len);
  
  //内存初始化;
  for(i=0; i<package_len; i++){
    package[i] = package[i] & 0x00;
  }
  //检查是够可能出现内存分配失败;
  
  package[0] = package[0] | 0x81;  //高位置为1 并且只支持json数据格式发送.txt;
  
  package[1] = package[1] | 0x80;  //
  
  if(package_len < 126){
	 package[1] = package[1] | len;
	 flag = 2;
       	 
  }
  else{
	 package[1] = package[1] | 0xfe;
     package[2] = package[2] | (len & 0x0000ff00);
     package[3] = package[3] | (len & 0x000000ff); 	 
     flag = 4;
  }
  
  for(i =flag; i<flag+4; i++){
	 package[i] = masked[i];  
  }
  //
  
  for(int i=flag+4; i<flag+4+len; i++){
	 package[i] = data[i]; 
  }
   
  *netstring =  package;
  
  return flag+4+len;
  //
}
  
