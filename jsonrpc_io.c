/*
 *
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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <event.h>
#include <sys/timerfd.h>

#include "../../core/sr_module.h"
#include "../../core/route.h"
#include "../../core/route_struct.h"
#include "../../core/lvalue.h"
#include "../tm/tm_load.h"

#include "jsonrpc_io.h"
#include "jsonrpc.h"
#include "netstring.h"
#include "aid_function.h"

#define CHECK_MALLOC_VOID(p)  if(!p) {LM_ERR("Out of memory!"); return;}
#define CHECK_MALLOC(p)  if(!p) {LM_ERR("Out of memory!"); return -1;}

struct jsonrpc_server {
	char *host;
	int  port, socket, status, conn_attempts;
	struct jsonrpc_server *next;
	struct event *ev;
	struct itimerspec *timer;
};

struct jsonrpc_server_group {
	struct jsonrpc_server *next_server;
	int    priority;
	struct jsonrpc_server_group *next_group;
};

struct tm_binds tmb;

struct jsonrpc_server_group *server_group;

void socket_cb(int fd, short event, void *arg);     // 接收网络字节流;
void cmd_pipe_cb(int fd, short event, void *arg);   // 发送的cmd_pipe_cb;
int  set_non_blocking(int fd);                      // time out 超时的回调;  //result_cb???
int  parse_servers(char *_servers, struct jsonrpc_server_group **group_ptr);
int  connect_servers(struct jsonrpc_server_group *group);
int  connect_server(struct jsonrpc_server *server);
int  handle_server_failure(struct jsonrpc_server *server);

/* module config from jsonrpc_mod.c */
int _jsonrpcc_max_conn_retry = 0; /* max retries to connect. -1 forever 0 none */

int jsonrpc_io_child_process(int cmd_pipe, char* _servers)
{
	if (parse_servers(_servers, &server_group) != 0)
	{
		LM_ERR("servers parameter could not be parsed\n");
		return -1;
	}

	event_init();
	
	struct event pipe_ev;
	set_non_blocking(cmd_pipe);
	event_set(&pipe_ev, cmd_pipe, EV_READ | EV_PERSIST, cmd_pipe_cb, &pipe_ev);
	event_add(&pipe_ev, NULL);

	if (!connect_servers(server_group))
	{
		LM_WARN("failed to connect to any servers\n");
	}

	event_dispatch();
	return 0;
}

void timeout_cb(int fd, short event, void *arg) 
{
	LM_ERR("message timeout\n");
	jsonrpc_request_t *req = (jsonrpc_request_t*)arg;
	json_object *error = json_object_new_string("timeout");
	void_jsonrpc_request(req->id);
	close(req->timerfd);
	event_del(req->timer_ev);
	pkg_free(req->timer_ev);
	req->cbfunc(error, req->cbdata, 1);
	pkg_free(req);
}

int result_cb(json_object *result, char *data, int error) 
{
	struct jsonrpc_pipe_cmd *cmd = (struct jsonrpc_pipe_cmd*)data;

	pv_spec_t *dst = cmd->cb_pv;
	pv_value_t val;

	const char* res = json_object_get_string(result);

	val.rs.s = (char*)res;
	val.rs.len = strlen(res);
	val.flags = PV_VAL_STR;

	dst->setf(0, &dst->pvp, (int)EQ_T, &val);

	int n;
	if (error) {
		n = route_get(&main_rt, cmd->err_route);
	} else {
		n = route_get(&main_rt, cmd->cb_route);
	}

	struct action *a = main_rt.rlist[n];
	tmb.t_continue(cmd->t_hash, cmd->t_label, a);	

	free_pipe_cmd(cmd);
	return 0;
}


int (*res_cb)(json_object*, char*, int) = &result_cb;


void cmd_pipe_cb(int fd, short event, void *arg)
{
	struct jsonrpc_pipe_cmd *cmd;
	char *ns = 0;
	size_t bytes;
	json_object *payload = NULL;
	jsonrpc_request_t *req = NULL;
	json_object *params;
	/* struct event *ev = (struct event*)arg; */

//获取管道文件上的数据;

	if (read(fd, &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to read from command pipe: %s\n", strerror(errno));
		return;
	}
//这个就是我所写入的json数据;
	params = json_tokener_parse(cmd->params);

	if (cmd->notify_only) {
		payload = build_jsonrpc_notification(cmd->method, params);
	} else {
		req = build_jsonrpc_request(cmd->method, params, (char*)cmd, res_cb);
		if (req)
			payload = req->payload;
	}

	if (!payload) {
		LM_ERR("Failed to build jsonrpc_request_t (method: %s, params: %s)\n", cmd->method, cmd->params);	
		goto error;
	}
	
//获取实际编写的json数据;	

	char *json = (char*)json_object_get_string(payload);
	unsigned char *json = (unsigned char*)json_object_get_string(payload);

//把数据写到ns里面去这个时候就是我构造的最佳时机,需要更改ns的消息格式;

//	bytes = netstring_encode_new(&ns, json, (size_t)strlen(json));
    
	bytes = websocket_encode_new(&ns, json, (size_t)strlen(json)); 
    
	struct jsonrpc_server_group *g;
	int sent = 0;
	for (g = server_group; g != NULL; g = g->next_group)
	{
		struct jsonrpc_server *s, *first = NULL;
		for (s = g->next_server; s != first; s = s->next)
		{
			if (first == NULL) first = s;
			if (s->status == JSONRPC_SERVER_CONNECTED) {
				if (send(s->socket, ns, bytes, 0) == bytes)
				{
					sent = 1;
					break;
				} else {
					handle_server_failure(s);
				}
			}
			g->next_server = s->next;
		}
		if (sent) {
			break;
		} else {
			LM_WARN("Failed to send on priority group %d... proceeding to next priority group.\n", g->priority);
		}
	}

	if (sent && req) {
		int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

		if (timerfd == -1) {
			LM_ERR("Could not create timerfd.");
			goto error;
		}

		req->timerfd = timerfd;
		struct itimerspec *itime = pkg_malloc(sizeof(struct itimerspec));
		CHECK_MALLOC_VOID(itime);
		itime->it_interval.tv_sec = 0;
		itime->it_interval.tv_nsec = 0;

		itime->it_value.tv_sec = JSONRPC_TIMEOUT/1000;
		itime->it_value.tv_nsec = (JSONRPC_TIMEOUT % 1000) * 1000000;
		if (timerfd_settime(timerfd, 0, itime, NULL) == -1) 
		{
			LM_ERR("Could not set timer.");
			pkg_free(itime);
			goto error;
		}
		pkg_free(itime);
		struct event *timer_ev = pkg_malloc(sizeof(struct event));
		CHECK_MALLOC_VOID(timer_ev);
		event_set(timer_ev, timerfd, EV_READ, timeout_cb, req); 
		if(event_add(timer_ev, NULL) == -1) {
			LM_ERR("event_add failed while setting request timer (%s).", strerror(errno));
			goto error;
		}
		req->timer_ev = timer_ev;
	} else if (!sent) {
		LM_ERR("Request could not be sent... no more failover groups.\n");
		if (req) {
			json_object *error = json_object_new_string("failure");
			void_jsonrpc_request(req->id);
			req->cbfunc(error, req->cbdata, 1);
		}
	}

	pkg_free(ns);
	json_object_put(payload);
	if (cmd->notify_only) free_pipe_cmd(cmd);
	return;

error:
	if(ns) pkg_free(ns);
	if(payload) json_object_put(payload);
	if (cmd->notify_only) free_pipe_cmd(cmd);
	return;
}

void socket_cb(int fd, short event, void *arg)
{	
	struct jsonrpc_server *server = (struct jsonrpc_server*)arg;

	if (event != EV_READ) {
		LM_ERR("unexpected socket event (%d)\n", event);
		handle_server_failure(server);	
		return;
	}

	char *netstring;

	int retval = netstring_read_fd(fd, &netstring);

	if (retval != 0) {
		LM_ERR("bad netstring (%d)\n", retval);
		handle_server_failure(server);
		return;
	}	
	
	struct json_object *res = json_tokener_parse(netstring);

	if (res) {
		handle_jsonrpc_response(res);
		json_object_put(res);
	} else {
		LM_ERR("netstring could not be parsed: (%s)\n", netstring);
		handle_server_failure(server);
	}
	pkg_free(netstring);
}

int set_non_blocking(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -1;

	return 0;
}

int parse_servers(char *_servers, struct jsonrpc_server_group **group_ptr)
{
	char cpy[strlen(_servers)+1];
	char *servers = strcpy(cpy, _servers);

	struct jsonrpc_server_group *group = NULL;

	/* parse servers string */
	char *token = strtok(servers, ":");
	while (token != NULL) 
	{
		char *host, *port_s, *priority_s, *tail;
		int port, priority;
		host = token;

		/* validate domain */
		if (!(isalpha(host[0]) || isdigit(host[0]))) {
			LM_ERR("invalid domain (1st char is '%c')\n", host[0]);
			return -1;
		}
		int i;
		for (i=1; i<strlen(host)-1; i++)
		{
			if(!(isalpha(host[i]) || isdigit(host[i]) || host[i] == '-' || host[i] == '.'))
			{
				LM_ERR("invalid domain (char %d is %c)\n", i, host[i]);
				return -1;
			}
		}
		if (!(isalpha(host[i]) || isdigit(host[i]))) {
			LM_ERR("invalid domain (char %d (last) is %c)\n", i, host[i]);
			return -1;
		}

		/* convert/validate port */
		port_s = strtok(NULL, ",");
		if (port_s == NULL || !(port = strtol(port_s, &tail, 0)) || strlen(tail)) 
		{
			LM_ERR("invalid port: %s\n", port_s);
			return -1;
		}

		/* convert/validate priority */
		priority_s = strtok(NULL, " ");
		if (priority_s == NULL || !(priority = strtol(priority_s, &tail, 0)) || strlen(tail)) 
		{
			LM_ERR("invalid priority: %s\n", priority_s);
			return -1;
		}

	
		struct jsonrpc_server *server = pkg_malloc(sizeof(struct jsonrpc_server));
		CHECK_MALLOC(server);
		memset(server, 0, sizeof(struct jsonrpc_server));
		char *h = pkg_malloc(strlen(host)+1);
		CHECK_MALLOC(h);

		strcpy(h,host);
		server->host = h;
		server->port = port;
		server->status = JSONRPC_SERVER_DISCONNECTED;
		server->socket = 0;
		server->conn_attempts = _jsonrpcc_max_conn_retry;

		int group_cnt = 0;

		/* search for a server group with this server's priority */
		struct jsonrpc_server_group *selected_group = NULL;
		for (selected_group=group; selected_group != NULL; selected_group=selected_group->next_group)
		{
			if (selected_group->priority == priority) break;
		}
		
		if (selected_group == NULL) {
			group_cnt++;
			LM_INFO("Creating group for priority %d\n", priority);

			/* this is the first server for this priority... link it to itself */
			server->next = server;
			
			selected_group = pkg_malloc(sizeof(struct jsonrpc_server_group));
			CHECK_MALLOC(selected_group);
			memset(selected_group, 0, sizeof(struct jsonrpc_server_group));
			selected_group->priority = priority;
			selected_group->next_server = server;
			
			/* insert the group properly in the linked list */
			struct jsonrpc_server_group *x, *pg;
			pg = NULL;
			if (group == NULL) 
			{
				group = selected_group;
				group->next_group = NULL;
			} else {
				for (x = group; x != NULL; x = x->next_group) 
				{
					if (priority > x->priority)
					{
						if (pg == NULL)
						{
							group = selected_group;
						} else {
							pg->next_group = selected_group;
						}
						selected_group->next_group = x;
						break;
					} else if (x->next_group == NULL) {
						x->next_group = selected_group;
						break;
					} else {
						pg = x;
					}
				}
			}
		} else {
			LM_ERR("Using existing group for priority %d\n", priority);
			server->next = selected_group->next_server->next;
			selected_group->next_server->next = server;
		}

		token = strtok(NULL, ":");
	}

	*group_ptr = group;
	return 0;
}

int connect_server(struct jsonrpc_server *server) 
{	
	struct sockaddr_in  server_addr;
	struct hostent      *hp;
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port   = htons(server->port);

	hp = gethostbyname(server->host);
	if (hp == NULL) {
		LM_ERR("gethostbyname(%s) failed with h_errno=%d.\n", server->host, h_errno);
		handle_server_failure(server);
		return -1;
	}
	memcpy(&(server_addr.sin_addr.s_addr), hp->h_addr, hp->h_length);

	int sockfd = socket(AF_INET,SOCK_STREAM,0);

	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in))) {
		LM_WARN("Failed to connect to %s on port %d... %s\n", server->host, server->port, strerror(errno));
		handle_server_failure(server);
		return -1;
	}

	if (set_non_blocking(sockfd) != 0)
	{
		LM_WARN("Failed to set socket (%s:%d) to non blocking.\n", server->host, server->port);
		handle_server_failure(server);
		return -1;
	}	
/**************************************
*  这里是新添加的函数;
*  函数 client_link_to_server(主要是为了完成与服务器的连接建立);
*
**************************************/
	if(client_link_to_server(server,sockfd)==0);
	else{
	    LM_ERR("Failed to handle to server(%s:%d)\n", server->host, server->port);
		handle_server_failure(server);	
	}
	
/////////////////////////////////////////////////////	
	server->socket = sockfd;
	server->status = JSONRPC_SERVER_CONNECTED;
	server->conn_attempts = _jsonrpcc_max_conn_retry;

	struct event *socket_ev = pkg_malloc(sizeof(struct event));
	CHECK_MALLOC(socket_ev);
	event_set(socket_ev, sockfd, EV_READ | EV_PERSIST, socket_cb, server);
	event_add(socket_ev, NULL);
	server->ev = socket_ev;
	return 0;
}

int  connect_servers(struct jsonrpc_server_group *group)
{
	int connected_servers = 0;
	for (;group != NULL; group = group->next_group)
	{
		struct jsonrpc_server *s, *first = NULL;
		LM_INFO("Connecting to servers for priority %d:\n", group->priority);
		for (s=group->next_server;s!=first;s=s->next)
		{
			if (connect_server(s) == 0) 
			{
				connected_servers++;
				LM_INFO("Connected to host %s on port %d\n", s->host, s->port);
			}
			if (first == NULL) first = s;
		}
	}
	return connected_servers;
}

void reconnect_cb(int fd, short event, void *arg)
{
	LM_INFO("Attempting to reconnect now.");
	struct jsonrpc_server *server = (struct jsonrpc_server*)arg;
	
	if (server->status == JSONRPC_SERVER_CONNECTED) {
		LM_WARN("Trying to connect an already connected server.");
		return;
	}

	if (server->ev != NULL) {
		event_del(server->ev);
		pkg_free(server->ev);
		server->ev = NULL;
	}

	close(fd);
	pkg_free(server->timer);

	connect_server(server);
}

int handle_server_failure(struct jsonrpc_server *server)
{
	LM_INFO("Setting timer to reconnect to %s on port %d in %d seconds.\n", server->host, server->port, JSONRPC_RECONNECT_INTERVAL);

	if (server->socket)
		close(server->socket);
	server->socket = 0;
	if (server->ev != NULL) {
		event_del(server->ev);
		pkg_free(server->ev);
		server->ev = NULL;
	}
	server->status = JSONRPC_SERVER_FAILURE;
	server->conn_attempts--;
	if(_jsonrpcc_max_conn_retry!=-1 && server->conn_attempts<0) {
		LM_ERR("max reconnect attempts. No further attempts will be made to reconnect this server.");
		return -1;
	}

	int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	
	if (timerfd == -1) {
		LM_ERR("Could not create timerfd to reschedule connection. No further attempts will be made to reconnect this server.");
		return -1;
	}

	struct itimerspec *itime = pkg_malloc(sizeof(struct itimerspec));
	CHECK_MALLOC(itime);
	itime->it_interval.tv_sec = 0;
	itime->it_interval.tv_nsec = 0;
	
	itime->it_value.tv_sec = JSONRPC_RECONNECT_INTERVAL;
	itime->it_value.tv_nsec = 0;
	
	if (timerfd_settime(timerfd, 0, itime, NULL) == -1) 
	{
		LM_ERR("Could not set timer to reschedule connection. No further attempts will be made to reconnect this server.");
		return -1;
	}
	LM_INFO("timerfd value is %d\n", timerfd);
	struct event *timer_ev = pkg_malloc(sizeof(struct event));
	CHECK_MALLOC(timer_ev);
	event_set(timer_ev, timerfd, EV_READ, reconnect_cb, server);  //当接收出现问题的时候会尝试重新连接;
	if(event_add(timer_ev, NULL) == -1) {
		LM_ERR("event_add failed while rescheduling connection (%s). No further attempts will be made to reconnect this server.", strerror(errno));
		return -1;
	}
	server->ev = timer_ev;
	server->timer = itime;
	return 0;
}


void free_pipe_cmd(struct jsonrpc_pipe_cmd *cmd) 
{
	if (cmd->method) 
		shm_free(cmd->method);
	if (cmd->params)
		shm_free(cmd->params);
	if (cmd->cb_route)
		shm_free(cmd->cb_route);
	if (cmd->err_route)
		shm_free(cmd->err_route);
	if (cmd->cb_pv)
		shm_free(cmd->cb_pv);
	shm_free(cmd);
}



int construct_http_package(char* host, unsigned int port , const char *shakeKey, char *package){

  int len;

  package = (char*)pkg_malloc(500);   //内存管理问题2;

  const char httpRequest[] = "GET %s HTTP/1.1\r\n"
                             "Connection: Upgrade\r\n"
                             "Host: %s\r\n"
                             "Sec-WebSocket-Key: %s\r\n"
                             "Sec-WebSocket-Version: 13\r\n"
                             "Upgrade: websocket\r\n\r\n";
   
  len = sprintf(package, httpRequest, "/kamailio", ip_and_port, shakeKey);

  if(len == -1){
	 pkg_free(package); 
	 return -1;
  }
  else{
	 return len;  //代表打包的数据长度;
  }
}


int client_link_to_server(struct jsonrpc_server *server,int sockfd){
	
  	unsigned  int key_len;    //新增代码;
    unsigned  int package_len;
    int ret;	
	unsigned  char  recBuf[512];	
	unsigned  char* shaKey;  	//新增代码;
    unsigned  char* package;
	unsigned  char* temp;
	unsigned  char* p;
	unsigned  char  status[10];
    int flag = 0;
	
	shaKey  = (char*)pkg_malloc(100); //预先申请的内存大小,存放base64编码之后的shaKey;  
	   
    key_len = webSocket_buildShakeKey(shaKey);
	
    package_len = construct_http_package(server->host,server->port,interface,shaKey,package); //package内存需要在合适的时候释放;
  	
    if(package_len == -1) {			
	  LM_ERR("handle package engage error");
      return -1;
	}
	
	ret = send(sockfd, package, package_len, MSG_NOSIGNAL);	//防止崩溃情况发生;
	   
	if(ret < 0) {
		
		pkg_free(package);
		
		LM_ERR(" can not send handle shakey package");   
		  //代表发送成功
	    return -1;
	}
		
	while(1){
		  memset(recBuf , 0 , sizeof(recBuf));
		  memset(status, 0, sizeof(status));
		  
          ret = recv(fd, recBuf, sizeof(recBuf), MSG_NOSIGNAL);
		  
		  temp = recBuf; //由此操作status; 
		  if(ret > 0){
            if(strncmp((const char *)recBuf, (const char *)"HTTP/1.1 ", strlen((const char *)"HTTP/1.1 ")) == 0)    // 返回的是http回应信息
            {
                //先看服务端发送回来的状态码;
				temp = temp+9;
				if(strncmp((const char *)temp, (const char *)"101", strlen((const char *)"101")) !=0){
					//最好可以把状态码直接打印出来;
					LM_ERR("server can not accept such link");
                    break;					
				}                	
                if((p = (unsigned char *)strstr((const char *)recBuf, (const char *)"Sec-WebSocket-Accept: ")) != NULL)    // 检查握手信号
                {
                    p += strlen((const char *)"Sec-WebSocket-Accept: ");;
                    if(webSocket_matchShakeKey(shakeKey, strlen((const char *)shakeKey), p, strlen((const char *)p)) == 0){  // 握手成功, 发送登录数据包
                       flag = 1; 
					   break;
					}
                    else{
					   break;
					}					
                }
            }	
		  }
	   }
	   //集中释放资源：
	   pkg_free(shaKey);
	   pkg_free(package);
       
	   if(flag  == 1) return 0;
       else return -1;	   
}
