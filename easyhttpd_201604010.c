/*
Copyright (c) 2016, Yomei Otani <yomei.otani@gmai.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
*/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif	/* _WIN32 */
#if defined(unix) || defined(ANDROID_NDK) || defined(__APPLE__)
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif	/* unix */
#if defined(_WIN32) && !defined(__GNUC__)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#ifdef _MSC_VER
#if _MSC_VER >= 1400
#pragma warning( disable : 4996 )
#pragma warning( disable : 4819 )
#endif
#endif
#if defined(_WIN32) && !defined(__GNUC__)
#pragma comment( lib, "ws2_32.lib" )
#endif

#if defined(unix) || defined(ANDROID_NDK) || defined(__APPLE__)
#define closesocket(s) close(s)
#endif	/* unix */

#define MAX_LISTEN 20
#define MAX_FD 20
#define PORT 12345
#define RECV_TIMEOUT 30
#define PING_TIMEOUT 10
#define TIMEOUT_RECV 10
#define MAX_LINE 256

//#define I_USE_WEBSOCKET
#define DIR_WEBSOCKET "websocket"

void smplws_init()
{
#ifdef _WIN32
	WSADATA wsaData;
	int err;

	err = WSAStartup(MAKEWORD(2, 0), &wsaData);
	if (err != 0)
	{
		exit(1);
	}
#endif
#if defined(unix) || defined(__APPLE__)
	signal(SIGPIPE, SIG_IGN);
#endif
}

void smplws_done()
{
#ifdef _WIN32
	WSACleanup();
#endif
}

//
// socket
//
int smplws_server_create(int port)
{
	int err;
	struct addrinfo hints;
	struct addrinfo* res = NULL;
	struct addrinfo* ai;
	int sockfd;
	char service[16];

	sprintf(service, "%d", port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	err = getaddrinfo(NULL, service, &hints, &res);
	if (err != 0) {
		return -1;
	}
	ai = res;
	sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sockfd < 0)return -1;

#if defined(_WIN32) || defined(__CYGWIN__)
	{
		int one = 0;
		err = setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&one, sizeof one);
	}
#endif

	if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0){
		closesocket(sockfd);
		return -1;
	}
	if (listen(sockfd, 20) < 0){
		closesocket(sockfd);
		return -1;
	}
	return sockfd;
}


int smplws_create(const char* hostname, int port)
{
	int sockfd;
	int err;
	struct addrinfo hints;
	struct addrinfo* res = NULL;
	struct addrinfo* ai;
	char service[16];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;

	sprintf(service, "%d", port);

	err = getaddrinfo(hostname, service, &hints, &res);
	if (err != 0) {
		return -1;
	}
	for (ai = res; ai; ai = ai->ai_next) {
		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sockfd < 0){
			freeaddrinfo(res);
			return -1;
		}
		if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
			closesocket(sockfd);
			sockfd = -1;
			continue;
		}
		break;
	}
	freeaddrinfo(res);
	return sockfd;
}

int smplws_accept(int sockfd)
{
	int cs;
	struct sockaddr_storage sa;
	socklen_t len = sizeof(sa);

	cs = accept(sockfd, (struct sockaddr*) &sa, &len);
	return cs;
}

int smplws_select(int sockfd, int sec)
{
	fd_set readfds;
	struct timeval t;
	int ret;
	//int tmp;

	t.tv_sec = sec;
	t.tv_usec = 0;

	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);
	ret = select(sockfd+1, &readfds, NULL, NULL, &t);
	return ret;
}



int smplws_select_multi(int* fds, int n, int sec)
{
	fd_set readfds;
	struct timeval t;
	int ret;
	int i;
	int maxfd = 0;
	t.tv_sec = sec;
	t.tv_usec = 0;

	FD_ZERO(&readfds);
	for (i = 0; i < n; i++){
		if (fds[i] != -1){
			if (maxfd < fds[i])maxfd = fds[i];
			FD_SET(fds[i], &readfds);
		}
	}
	ret = select(maxfd+1, &readfds, NULL, NULL, &t);
	return ret;
}


void smplws_setsockopt_timeout(int fd,int tt)
{
	int timeout = tt;
	int ret;
	struct timeval tv;

	tv.tv_sec = tt / 1000;
	tv.tv_usec = (tt % 1000) * 1000;
#if defined(_WIN32) || defined(__CYGWIN__)
	ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tt, (socklen_t) sizeof(tt));
#endif	
#ifdef unix
	ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, (socklen_t) sizeof(tv));
#endif
	tv.tv_sec = tt / 1000;
	tv.tv_usec = (tt % 1000) * 1000;
#if defined(_WIN32) || defined(__CYGWIN__)
	ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tt, (socklen_t)sizeof(tt));
#endif
#ifdef unix
	ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, (socklen_t)sizeof(tv));
#endif	
}

//
// smplws utils
//


int smplws_readch(int fd)
{
	int ret;
	unsigned char buf[4];
	if (fd == -1)return -1;
	ret=recv(fd, buf, 1, 0);
	if (ret < 1)return -1;
	return buf[0];
}

int smplws_readuntil(int fd,char* buf, int sz, int endch)
{
	int ret = 0;
	int ch;
	//int tmp;

	if (fd==-1 || buf == NULL || sz < 1)return -1;
	
	ret = strlen(buf);

	while (ret <= sz - 2){
		//tmp = smplws_select(fd,0);
		//if (tmp == 0)return 0;
		ch = smplws_readch(fd);
		if (ch == -1)return -1;
		buf[ret] = ch;
		buf[ret + 1] = 0;
		ret++;
		if (ch == endch)break;
	}
	return ret;
}

int smplws_gettime()
{
	return (int)time(NULL);
}

//
// dhild connection
//

enum{
	CHILD_STATE_CLOSED = 0,
	CHILD_STATE_HTTP_REQUEST = 1,
	CHILD_STATE_HTTP_HEADER  = 2,
	CHILD_STATE_WEB_SOCKET   = 3,
};

enum{
	CHILD_MODE_UNKNOWN = 0,
	CHILD_MODE_HTTP = 1,
	CHILD_MODE_WEB_SOCKET = 2,
};

struct child_data
{
	//common
	char url[MAX_LINE];
	char buf[MAX_LINE];
	char tmp[MAX_LINE];
	int state;
	int mode;
	//use by user
	void* vp;

	//http
	char content_type[64];
	int content_length;
	char* body;
	
};

//
// websocket
//

#ifdef I_USE_WEBSOCKET
void* smplws_child_websocket_init(int fd, struct child_data* cd);
int smplws_child_websocket_parse_header(int fd, struct child_data* cd);
int smplws_child_send_websocket_connection(int fd, struct child_data* cd);
int smplws_child_websocket_ping(int fd, struct child_data* cd);
int smplws_child_websocket_frame(int fd, struct child_data* cd);
#else
void* smplws_child_websocket_init(int fd, struct child_data* cd)
{
	return NULL;
}
int smplws_child_websocket_parse_header(int fd, struct child_data* cd)
{
	return 0;
}
int smplws_child_send_websocket_connection(int fd, struct child_data* cd)
{
	return 0;
}
int smplws_child_websocket_ping(int fd, struct child_data* cd)
{
	return 0;
}
int smplws_child_websocket_frame(int fd, struct child_data* cd)
{
	return 0;
}
#endif

//
// http child
//
void* smplws_child_init(void)
{
	void* ret = NULL;

	ret = malloc(sizeof(struct child_data));
	if (ret == NULL)return NULL;
	memset(ret, 0, sizeof(struct child_data));
	((struct child_data*)ret)->state = CHILD_STATE_HTTP_REQUEST;
	return ret;

}

int smplws_child_send_error(int fd)
{
	int ret;
	char* str = "HTTP/1.1 403 Error\r\n"
		"Cnnection: close\r\n"
		"Content-Length: 0\r\n"
		"\r\n";
	ret=send(fd, str, strlen(str), 0);
	if (ret == -1)return -1;
	return ret;
}



int smplws_child_parse_header(int fd, struct child_data* cd)
{
	if (cd->mode == CHILD_MODE_WEB_SOCKET)return smplws_child_websocket_parse_header(fd, cd);
	// TODO parse http request if you want to parse http header
	return 0;
}

int smplws_child_ping(int fd, struct child_data* cd)
{
	if (cd->mode == CHILD_MODE_WEB_SOCKET)return smplws_child_websocket_ping(fd, cd);
	return 0;
}

int smplws_child_send_http_response_parse(int fd, struct child_data* cd)
{
	cd->body = malloc(1024);
	if (cd->body == NULL)return -1;
	cd->body[0] = 0;
	strcpy(cd->content_type, "text/html\r\n");
	strcpy(cd->body, "<HTML><BODY>It wroks!</BODY></HTML>");
	cd->content_length = strlen(cd->body);
	return 0;
}

int smplws_child_send_http_response_dealloc(int fd, struct child_data* cd)
{
	if (cd->body)free(cd->body);
	return 0;
}


int smplws_child_send_http_response_header(int fd, struct child_data* cd)
{
	char* str;
	int ret;
	char tmp[64];

	str = "HTTP/1.1 200 OK\r\n"
		"Cnnection: close\r\n"
		"Pragma: no-cache\r\n"
		"Cache-Control: no-cache\r\n";
	ret = send(fd, str, strlen(str), 0);
	if (ret == -1)return -1;

	sprintf(tmp, "Content-Length: %d\r\n\r\n", cd->content_length);
	ret = send(fd, tmp, strlen(tmp), 0);
	if (ret == -1)return -1;

	return 0;
}

int smplws_child_send_http_response_body(int fd, struct child_data* cd)
{
	int ret=-1;
	ret=send(fd, cd->body, cd->content_length, 0);
	if (ret == -1)return -1;
	return 0;
}


int smplws_child_data(void* vp, int fd)
{
	int ret=0,tmp;
	struct child_data* c = (struct child_data*)vp;

	switch(c->state){
	case CHILD_STATE_HTTP_REQUEST:
		ret = smplws_readuntil(fd, c->buf, MAX_LINE, '\n');
		if (ret <1)return ret;
		c->tmp[0] = 0;
		sscanf(c->buf, "%s", c->tmp);
		if (strcmp(c->tmp, "GET") != 0){
			smplws_child_send_error(fd);
			return -1;
		}
		c->url[0] = 0;
		sscanf(c->buf + 3, "%s", c->url);
		if (0){}
#ifdef I_USE_WEBSOCKET
		else if (strstr(c->url, "/" DIR_WEBSOCKET "/") == c->url){
			c->mode = CHILD_MODE_WEB_SOCKET;
			c->vp = smplws_child_websocket_init(fd, c);
			if (c->vp == NULL){
				smplws_child_send_error(fd);
				return -1;
			}
		}
#endif
		else if (strstr(c->url, "/") == c->url)c->mode = CHILD_MODE_HTTP;
		if(c->mode==CHILD_MODE_UNKNOWN){
			smplws_child_send_error(fd);
			return -1;
		}
		c->state = CHILD_STATE_HTTP_HEADER;
		c->buf[0] = 0;
		break;
	case CHILD_STATE_HTTP_HEADER:
		ret = smplws_readuntil(fd, c->buf, MAX_LINE, '\n');
		if (ret <1)return ret;

		printf(">>%s<<\n",c->buf);

		if (strcmp(c->buf, "\r\n") != 0){
			if (smplws_child_parse_header(fd, c)<0)return -1;
			c->buf[0] = 0;
			break;
		}
		if (c->mode == CHILD_MODE_WEB_SOCKET){
			c->state = CHILD_STATE_WEB_SOCKET;
			if (smplws_child_send_websocket_connection(fd, c) < 0){
				smplws_child_send_error(fd);
				return -1;
			}
			ret = 0;
			break;
		}
		if (c->mode == CHILD_MODE_HTTP){
			tmp=smplws_child_send_http_response_parse(fd, c);
			if(tmp == -1){
				smplws_child_send_error(fd);
				return -1;
			}
			tmp=smplws_child_send_http_response_header(fd, c);
			if(tmp!=-1)tmp=smplws_child_send_http_response_body(fd, c);
			smplws_child_send_http_response_dealloc(fd, c);
			return -1;
		}
		smplws_child_send_error(fd);
		return -1;
	case CHILD_STATE_WEB_SOCKET:
		ret = smplws_child_websocket_frame(fd,c);
		break;
	}
	return ret;
}

void smplws_child_fini(void* vp)
{
	struct child_data* cd = (struct child_data*)vp;
	if (cd){
		if(cd->vp)free(cd->vp);
		free(cd);
	}
}

//
// smplws signal
//

static int s_server_stop = 0;

void smplws_server_stop()
{
	s_server_stop = 1;
}



//
// smplws_main loop
//
void smplws_server_main(){

	int fd[MAX_FD];
	void* vp[MAX_FD];
	int tm[MAX_FD];
	int tm_pg[MAX_FD];
	int i, j,ret, t;
	int f_close = 0;


	s_server_stop = 0;
	for (i = 0; i < MAX_FD; i++)fd[i] = -1;

	fd[0] = smplws_server_create(PORT);
	if (fd[0] == -1)return;
	printf("enter main loop\n");

	while (1){
		if (s_server_stop)break;
		smplws_select_multi(fd,MAX_FD,3);
		t = smplws_gettime();
	
		for (i = 0; i < MAX_FD; i++){
		do{
			f_close = 0;
			if (fd[i] == -1)break;
			if (i != 0 && fd[i] != -1 && (t - tm_pg[i])>PING_TIMEOUT){
				ret = smplws_child_ping(fd[i], vp[i]);
				tm_pg[i] = t;
				if (ret < 0){
					f_close = 1;
					break;
				}
			}
			if (i != 0 && fd[i] != -1 && (t - tm[i])>RECV_TIMEOUT){
				f_close = 1;
				break;
			}
			ret = smplws_select(fd[i], 0);
			if (ret == 0)break;
			if (i == 0){
				int s;
				void* v;

				s = smplws_accept(fd[0]);
				v = smplws_child_init();
				smplws_setsockopt_timeout(s, TIMEOUT_RECV);
				if (v == NULL){
					closesocket(s);
					break;
				}
				for (j = 1; j < MAX_FD; j++)if (fd[j] == -1){
					// initial child
					fd[j] = s;
					vp[j] = v;
					tm[j] = tm_pg[j] = t;
					break;
				}
				if (j == MAX_FD){
					smplws_child_fini(v);
					closesocket(s);
				}
				break;
			}
			//recv child data
			tm[i] = t;
			ret = smplws_child_data(vp[i], fd[i]);
			if (ret == -1)f_close = 1;
		} while (0);
		if (f_close){
			closesocket(fd[i]);
			smplws_child_fini(vp[i]);
			fd[i] = -1;
			vp[i] = NULL;
			tm[i] = tm_pg[i] = 0;
		}
		}
	}
	// abort
	closesocket(fd[0]);
	for (i = 1; i < MAX_FD;i++)if (fd[i]!=-1){
		smplws_child_fini(vp[i]);
		closesocket(fd[i]);
		fd[i] = -1;
		vp[i] = NULL;
	}
	return;
}


//
// main
//
#if 1

int main()
{
	smplws_init();
	smplws_server_main();
	smplws_done();
	return 0;
}
#endif

