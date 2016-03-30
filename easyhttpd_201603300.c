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
int dualsock_server_create(int port)
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


int dualsock_create(const char* hostname, int port)
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

int dualsock_accept(int sockfd)
{
	int cs;
	struct sockaddr_storage sa;
	socklen_t len = sizeof(sa);

	cs = accept(sockfd, (struct sockaddr*) &sa, &len);
	return cs;
}

int dualsock_select(int sockfd, int sec)
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



int dualsock_select_multi(int* fds, int n, int sec)
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
		//tmp = dualsock_select(fd,0);
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
	char buf[256];
	char tmp[256];
	int state;
	int mode;

	//http
	char content_type[64];
	int content_length;
	char* body;

	//websocket
};


void* smplws_child_init(void)
{
	void* ret = NULL;

	//printf("child_init()\n");
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

int smplws_child_send_websocket_connection(int fd, struct child_data* cd)
{
	return 0;
}


int smplws_child_parse_header(int fd, struct child_data* cd)
{
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
	send(fd, cd->body, cd->content_length, 0);
	return 0;
}


int smplws_child_data(void* vp, int fd)
{
	int ret=0,tmp;
	struct child_data* c = (struct child_data*)vp;

	//printf("child_data()\n");
	switch(c->state){
	case CHILD_STATE_HTTP_REQUEST:
		ret = smplws_readuntil(fd, c->buf, 256, '\n');
		if (ret == -1)return -1;
		if (ret == 0)return 0;
		//printf(">>%s<<\n", c->buf);
		c->tmp[0] = 0;
		sscanf(c->buf, "%s", c->tmp);
		if (strcmp(c->tmp, "GET") != 0){
			smplws_child_send_error(fd);
			return -1;
		}
		c->tmp[0] = 0;
		sscanf(c->buf + 3, "%s", c->tmp);
		if (strstr(c->tmp, "/" DIR_WEBSOCKET "/") == c->tmp)c->mode = CHILD_MODE_WEB_SOCKET;
		else if (strstr(c->tmp, "/") == c->tmp)c->mode = CHILD_MODE_HTTP;
		if(c->mode==CHILD_MODE_UNKNOWN){
			smplws_child_send_error(fd);
			return -1;
		}
		c->state = CHILD_STATE_HTTP_HEADER;
		break;
	case CHILD_STATE_HTTP_HEADER:
		ret = smplws_readuntil(fd, c->buf, 256, '\n');
		if (ret == -1)return -1;
		if (ret == 0)return 0;
		if (strcmp(c->buf, "\r\n") == 0){
			if (c->mode == CHILD_MODE_WEB_SOCKET){
				c->state = CHILD_STATE_WEB_SOCKET;
				smplws_child_send_websocket_connection(fd, c);
				break;
			}
			if (c->mode == CHILD_MODE_HTTP){
				tmp=smplws_child_send_http_response_parse(fd, c);
				if(tmp == -1){
					smplws_child_send_error(fd);
					return -1;
				}
				smplws_child_send_http_response_header(fd, c);
				smplws_child_send_http_response_body(fd, c);
				tmp = smplws_child_send_http_response_dealloc(fd, c);
				return -1;
			}
			smplws_child_send_error(fd);
			return -1;
		}
		smplws_child_parse_header(fd, c);
		break;
	case CHILD_STATE_WEB_SOCKET:
		ret = -1;
		break;
	}

	printf(">>%s<<\n", c->buf);
	c->buf[0] = 0;

	return ret;
}

void smplws_child_fini(void* vp)
{
	//printf("child_fini()\n");
	if (vp)free(vp);
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
	int ret,t;
	int i,j;

	s_server_stop = 0;
	for (i = 0; i < MAX_FD; i++)fd[i] = -1;

	fd[0] = dualsock_server_create(PORT);
	if (fd[0] == -1)return;
	printf("enter main loop\n");
	while (1){
		if (s_server_stop)break;
		dualsock_select_multi(fd,MAX_FD,3);
		t = smplws_gettime();
		// client time out
		for (i = 1; i < MAX_FD; i++){
			if (fd[i] != -1 && (t - tm[i])>RECV_TIMEOUT){
				closesocket(fd[i]);
				smplws_child_fini(vp[i]);
				fd[i] = -1;
				vp[i] = NULL;
				tm[i] = 0;
			}
		}
		// polling sockets
		for (i = 0; i < MAX_FD; i++){
			if (fd[i] == -1)continue;
			ret = dualsock_select(fd[i], 0);
			if (ret == 0)continue;
			if (i == 0){
				int s;
				void* v;

				s = dualsock_accept(fd[0]);
				v = smplws_child_init();
				if (v == NULL){
					closesocket(s);
				}
				for (j = 1; j < MAX_FD; j++){
					if (fd[j] == -1){
						// initial child
						fd[j] = s;
						vp[j] = v;
						tm[j] = t;
						break;
					}
				}
				if (j == MAX_FD){
					smplws_child_fini(v);
					closesocket(s);
				}
			}else{
				//recv child data
				tm[i] = t;
				ret = smplws_child_data(vp[i], fd[i]);
				if (ret == -1){
					closesocket(fd[i]);
					smplws_child_fini(vp[i]);
					fd[i] = -1;
					vp[i] = NULL;
					tm[i] = 0;
				}
			}
		}
	}
	
	// abort
	closesocket(fd[0]);
	for (i = 1; i < MAX_FD;i++){
		if (fd[i]!=-1)smplws_child_fini(vp[i]);
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

