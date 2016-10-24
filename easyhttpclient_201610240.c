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

//
// init
//

void myhttp_init()
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

void myhttp_done()
{
#ifdef _WIN32
	WSACleanup();
#endif
}

//
// socket
//


static int dualsock_create(const char* hostname, int port)
{
	int sockfd;
	int err;
	struct addrinfo hints;
	struct addrinfo* res = NULL;
	struct addrinfo* ai;
	char service[16];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;     // IPv4/IPv6—¼‘Î‰ž
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

//
// string
//

static void mystrlwr(char*key)
{
	if (key == NULL)return;
	while (*key){
		*key = tolower(*key);
		key++;
	}
}
static void mystrupr(char*key)
{
	if (key == NULL)return;
	while (*key){
		*key = toupper(*key);
		key++;
	}
}


//
// url parser
//

static int is_http(const char* url)
{
	if (url == NULL)return 0;
	if (strncmp(url, "http://", 7) != 0)return 0;
	return 1;
}

static int get_port(const char* url)
{
	int port = 80;
	char server[256];
	char *s, *p;

	if (url == NULL)return port;

	if (strncmp(url, "http:", 7) == 0)port = 80;
	if (strncmp(url, "https:", 7) == 0)port = 443;
	s = strstr(url, "//");
	if (s == NULL)return port;
	s += 2;

	if (strlen(s) > 200)return port;
	strcpy(server, s);

	p = strchr(server, '/');
	if (p) {
		*p = 0;
	}
	p = strchr(server, ':');
	if (p) {
		sscanf(p + 1, "%d", &port);
		*p = 0;
	}
	if (port < 1)port = 80;
	return port;
}

static int get_server(const char* url, char *server, int sz)
{
	char *s, *p;
	if (url == NULL || server == NULL || sz <1)return -1;
	server[0] = 0;

	s = strstr(url, "//");
	if (s == NULL)return -1;
	s += 2;

	strncpy(server, s, sz);
	server[sz - 1] = 0;
	p = strchr(server, '/');
	if (p)*p = 0;
	p = strchr(server, ':');
	if (p)*p = 0;
	return 0;
}

static int get_location(const char* url, char *server, int sz)
{
	char *s, *p;

	if (url == NULL || server == NULL || sz <1)return -1;
	server[0] = 0;

	s = strstr(url, "//");
	if (s == NULL)return -1;
	s += 2;


	p = strchr(s, '/');
	if (p == NULL)
	{
		strncpy(server, "/", sz);
		server[sz - 1] = 0;
		return 0;
	}
	strncpy(server, p, sz);
	server[sz - 1] = 0;
	return 0;
}


//
// recv
//

static int recv_until(int fd, char* buf, int sz, int endch)
{
	int ret = 0, r;
	if (fd == -1 || buf == NULL || sz < 1)return -1;
	buf[0] = 0;
	while (ret <= sz - 2) {
		r = recv(fd, buf + ret, 1, 0);
		if (r <1)return ret;
		buf[ret + 1] = 0;
		if (buf[ret] == endch)return ret;
		ret++;
	}
	return ret;
}

static int recv_sz(int fd, void* buf, int sz)
{
	int ret, read = 0;
	while (1) {
		ret = recv(fd, (char*)buf + read, sz, 0);
		if (ret < 1)return read;
		sz -= ret;
		read += ret;
		if (sz == 0)return read;
	}
}

static int recv_sz_null(int fd, int sz)
{
	char buf[1024];
	int ret = 0, r;

	if (fd == -1)return -1;
	while (sz) {
		r = recv(fd, buf, sizeof(buf), 0);
		if (r <1)return ret;
		ret += r;
		sz -= r;
	}
	return ret;
}

static int recv_sz_callback(int fd, long long sz, void*vp, int(*f)(void*vp, void*data, int sz))
{
	int ret, read = 0,ret2;
	char buf[1024 * 64];
	long long bsz;

	if (f == NULL)return -1;

	while (1) {
		bsz = sizeof(buf);
		if (sz < bsz)bsz = sz;

		ret = recv(fd, buf,(int)bsz, 0);
		if (ret < 1)return read;
		sz -= ret;
		read += ret;


		ret2=f(vp, buf, ret);
		if (ret2 < ret)return read;

		if (sz == 0)return read;
	}
}



static int recv_chunked(int fd, void* vp, int max_sz)
{
	int ret = 0;
	int rd, sz;
	char buf[256];
	while (max_sz) {
		buf[0] = 0;
		sz = 0;
		rd = recv_until(fd, buf, sizeof(buf), '\n');
		if (rd < 1)break;
		sscanf(buf, "%x", &sz);
		if (sz == 0) {
			rd = recv_until(fd, buf, sizeof(buf), '\n');
			break;
		}
		if (sz < 1)break;
		if (max_sz < sz)break;
		rd = recv_sz(fd, ((char*)vp) + ret, sz);
		if (rd < 1)break;
		ret += rd;
		max_sz -= rd;
		rd = recv_until(fd, buf, sizeof(buf), '\n');
		if (rd < 1)break;
	}
	return ret;
}

static int recv_chunked_null(int fd, int max_sz)
{
	int ret = 0;
	int rd, sz;
	char buf[256];
	while (max_sz) {
		buf[0] = 0;
		sz = 0;
		rd = recv_until(fd, buf, sizeof(buf), '\n');
		if (rd < 1)break;
		sscanf(buf, "%x", &sz);
		if (sz == 0) {
			rd = recv_until(fd, buf, sizeof(buf), '\n');
			break;
		}
		if (sz < 1)break;
		if (max_sz < sz)break;
		rd = recv_sz_null(fd, sz);
		if (rd < 1)break;
		ret += rd;
		max_sz -= rd;
		rd = recv_until(fd, buf, sizeof(buf), '\n');
		if (rd < 1)break;
	}
	return ret;
}

static int recv_chunked_callback(int fd, long long max_sz, void* vp, int(*f)(void*vp, void* data, int sz))
{
	int ret = 0;
	int rd, sz;
	char buf[256];

	if (f == NULL)return -1;

	while (max_sz) {
		buf[0] = 0;
		sz = 0;
		rd = recv_until(fd, buf, sizeof(buf), '\n');
		if (rd < 1)break;
		sscanf(buf, "%x", &sz);
		if (sz == 0) {
			rd = recv_until(fd, buf, sizeof(buf), '\n');
			break;
		}
		if (sz < 1)break;
		if (max_sz < sz)break;
		rd = recv_sz_callback(fd, sz,vp,f);
		if (rd < 1)break;
		ret += rd;
		max_sz -= rd;
		rd = recv_until(fd, buf, sizeof(buf), '\n');
		if (rd < 1)break;
	}
	return ret;
}

static int recv_null(int fd)
{
	char buf[1024];
	int ret = 0, r;

	if (fd == -1)return -1;
	while (1) {
		r = recv(fd, buf, sizeof(buf), 0);
		if (r <1)return ret;
		ret += r;
	}
	return ret;
}


//
//
//

static int myhttp_get_data__(const char* url, void** data, int* sz, int* rescode, int ct)
{
	int ret = -1;
	int port = 80;
	char server[256];
	char loc[256];
	char redirect[1024 * 2];
	char tmp[1024 * 2];
	char key[1024];
	char val[1024];
	char* p;

	int s;
	int read, st, clen = -1, wri;
	int chunk = 0;

	if (url == NULL || data == NULL || sz == NULL)return ret;
	if (ct >= 10)return -1;

	if (strlen(url)>800)return ret;
	*data = NULL;
	*sz = 0;
	*rescode = 0;

	if (is_http(url) == 0)return ret;
	port = get_port(url);
	get_server(url, server, sizeof(server));
	get_location(url, loc, sizeof(loc));

	if (server[0] == 0 || loc[0] == 0)return ret;

	//printf("port=%d\n",port);
	//printf("server=>>%s<<\n", server);
	//printf("dir=>>%s<<\n", loc);
	//printf("\n");


	sprintf(tmp, "GET %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n"
		"\r\n"
		, loc, server);

	s = dualsock_create(server, port);

	if (s == -1)return ret;

	//printf("connect!!\n");
	//printf("%s", tmp);

	wri = send(s, tmp, strlen(tmp), 0);
	if (wri < 1) {
		closesocket(s);
		return ret;
	}
	read = recv_until(s, tmp, sizeof(tmp), '\n');
	if (read < 1) {
		closesocket(s);
		return ret;
	}
	//printf("%s",tmp);

	st = 0;
	sscanf(tmp, "%*s%d", &st);

	redirect[0] = 0;

	while (1) {
		read = recv_until(s, tmp, sizeof(tmp), '\n');
		if (read < 1)break;
		if (tmp[0] == '\r' || tmp[0] == '\n')break;
		p = strchr(tmp, ':');
		if (p == NULL)continue;

		//printf("%s", tmp);

		*p = 0;
		p++;
		key[0] = 0;
		val[0] = 0;
		sscanf(tmp, "%s", key);
		sscanf(p, "%s", val);
		mystrupr(key);
		if (strcmp(key, "LOCATION") == 0) {
			strcpy(redirect, val);
		}
		else if (strcmp(key, "CONTENT-LENGTH") == 0) {
			sscanf(val, "%d", &clen);
		}
		else if (strcmp(key, "TRANSFER-ENCODING") == 0) {
			strcpy(redirect, val);
			if (strstr(val, "chunked"))chunk = 1;
		}
	}
	if (read < 1) {
		closesocket(s);
		return ret;
	}
	if (clen > 1024 * 1024 - 100) {
		closesocket(s);
		return ret;
	}
	if (rescode)*rescode = st;
	if (clen < 0)clen = 1024 * 1024 - 100;
	ret = 0;

	//printf("body\n");
	*data = malloc(clen + 16);
	if (*data == NULL) {
		closesocket(s);
		return ret;
	}

	read = 0;
	if (clen > 0) {
		if (chunk) {
			read = recv_chunked(s, *data, clen);
		}
		else {
			read = recv_sz(s, *data, clen);
		}
	}

	((char*)(*data))[read] = 0;
	if (sz)*sz = read;

	if (st >= 300 && st <= 399) {
		free(*data);
		*data = NULL;
		closesocket(s);

		if (redirect[0] == 0 || strlen(redirect) > 500 || strlen(loc)>500 || strlen(server)>500) {
			return ret;
		}
		if (strstr(redirect, "http://") == NULL) {
			if (redirect[0] == '/') {
				strcpy(tmp, redirect);
				sprintf(redirect, "http://%s:%d%s", server, port, tmp);
			}
			else {
				strcpy(tmp, redirect);
				sprintf(redirect, "http://%s:%d%s", server, port, loc);
				p = strrchr(redirect, '/');
				if (p) {
					p++;
					*p = 0;
					strcat(redirect, tmp);
				}
			}
		}
		ret = myhttp_get_data__(redirect, data, sz, rescode, ct + 1);
		return ret;
	}
	closesocket(s);
	ret = 0;
	return ret;
}

int myhttp_get_data(const char* url, void** data, int* sz, int* rescode)
{
	return myhttp_get_data__(url, data, sz, rescode, 0);
}

static int myhttp_get_data_callback__(const char* url,long long *sz,int* rescode,void* vp, int (*f)(void* vp,void* data,int sz),int ct )
{
	int ret = -1;
	int port = 80;
	char server[256];
	char loc[256];
	char redirect[1024 * 2];
	char tmp[1024 * 2];
	char key[1024];
	char val[1024];
	char* p;

	int s;
	
	int st,wri;

	long long read,clen = -1;
	long long  chunk = 0;

	if (url == NULL || f==NULL)return ret;
	if (ct >= 10)return -1;

	if (strlen(url)>800)return ret;


	if (is_http(url) == 0)return ret;
	port = get_port(url);
	get_server(url, server, sizeof(server));
	get_location(url, loc, sizeof(loc));

	if (server[0] == 0 || loc[0] == 0)return ret;

	//printf("port=%d\n",port);
	//printf("server=>>%s<<\n", server);
	//printf("dir=>>%s<<\n", loc);
	//printf("\n");


	sprintf(tmp, "GET %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n"
		"\r\n"
		, loc, server);

	s = dualsock_create(server, port);

	if (s == -1)return ret;

	//printf("connect!!\n");
	//printf("%s", tmp);

	wri = send(s, tmp, strlen(tmp), 0);
	if (wri < 1) {
		closesocket(s);
		return ret;
	}
	read = recv_until(s, tmp, sizeof(tmp), '\n');
	if (read < 1) {
		closesocket(s);
		return ret;
	}
	//printf("%s",tmp);

	st = 0;
	sscanf(tmp, "%*s%d", &st);

	redirect[0] = 0;

	while (1) {
		read = recv_until(s, tmp, sizeof(tmp), '\n');
		if (read < 1)break;
		if (tmp[0] == '\r' || tmp[0] == '\n')break;
		p = strchr(tmp, ':');
		if (p == NULL)continue;

		//printf("%s", tmp);

		*p = 0;
		p++;
		key[0] = 0;
		val[0] = 0;
		sscanf(tmp, "%s", key);
		sscanf(p, "%s", val);
		mystrupr(key);
		if (strcmp(key, "LOCATION") == 0) {
			strcpy(redirect, val);
		}
		else if (strcmp(key, "CONTENT-LENGTH") == 0) {
			sscanf(val, "%lld", &clen);
		}
		else if (strcmp(key, "TRANSFER-ENCODING") == 0) {
			strcpy(redirect, val);
			if (strstr(val, "chunked"))chunk = 1;
		}
	}
	if (read < 1) {
		closesocket(s);
		return ret;
	}
	
	if(rescode)*rescode = st;
	if (clen < 0)clen = 1024 * 1024 - 100;
	ret = 0;

	//printf("body\n");
	if (rescode)*rescode = st;


	read = 0;
	if (st >= 200 && st<300){
		if (clen > 0) {
			if (chunk) {
				read = recv_chunked_callback(s, clen, vp, f);
			}
			else {
				read = recv_sz_callback(s, clen, vp, f);
			}
		}
	}
	else{
		if (clen > 0) {
			if (chunk) {
				read = recv_chunked_null(s,(int)clen);
			}
			else {
				read = recv_sz_null(s,(int)clen);
			}
		}
	}

	if (sz)*sz = read;


	if (st >= 300 && st <= 399) {
		closesocket(s);

		if (redirect[0] == 0 || strlen(redirect) > 500 || strlen(loc)>500 || strlen(server)>500) {
			return ret;
		}
		if (strstr(redirect, "http://") == NULL) {
			if (redirect[0] == '/') {
				strcpy(tmp, redirect);
				sprintf(redirect, "http://%s:%d%s", server, port, tmp);
			}
			else {
				strcpy(tmp, redirect);
				sprintf(redirect, "http://%s:%d%s", server, port, loc);
				p = strrchr(redirect, '/');
				if (p) {
					p++;
					*p = 0;
					strcat(redirect, tmp);
				}
			}
		}
		ret = myhttp_get_data_callback__(redirect, sz,rescode,vp,f, ct + 1);
		return ret;
	}
	closesocket(s);
	ret = 0;
	return ret;
}

int myhttp_get_data_callback(const char* url,long long *sz,int* rescode,void* vp,int (*f)(void*vp,void* data,int sz))
{
	return myhttp_get_data_callback__(url,sz,rescode,vp,f,0);
}

static int myhttp_post_data__(const char* url, void* sdata, int ssz, const char* ctype, void** data, int* sz, int* rescode, int ct)
{
	int ret = -1;
	int port = 80;
	char server[256];
	char loc[256];
	char redirect[1024 * 2];
	char tmp[1024 * 2];
	char key[1024];
	char val[1024];
	char* p;

	int s;
	int read, st, clen = -1, wri;
	int chunk = 0;

	if (url == NULL || data == NULL || sz == NULL)return ret;
	if (sdata == NULL || ctype == NULL || ssz <1)return ret;
	if (ct >= 10)return -1;

	if (strlen(url)>800)return ret;
	*data = NULL;
	*sz = 0;
	*rescode = 0;

	if (is_http(url) == 0)return ret;
	port = get_port(url);
	get_server(url, server, sizeof(server));
	get_location(url, loc, sizeof(loc));

	if (server[0] == 0 || loc[0] == 0)return ret;

	//printf("port=%d\n",port);
	//printf("server=>>%s<<\n", server);
	//printf("dir=>>%s<<\n", loc);
	//printf("\n");


	sprintf(tmp, "POST %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		, loc, server, ctype, ssz);

	s = dualsock_create(server, port);

	if (s == -1)return ret;

	//printf("connect!!\n");
	//printf("%s", tmp);

	wri = send(s, tmp, strlen(tmp), 0);
	if (wri < 1) {
		closesocket(s);
		return ret;
	}
	wri = send(s, sdata, ssz, 0);
	if (wri < 1) {
		closesocket(s);
		return ret;
	}
	read = recv_until(s, tmp, sizeof(tmp), '\n');
	if (read < 1) {
		closesocket(s);
		return ret;
	}
	//printf("%s",tmp);

	st = 0;
	sscanf(tmp, "%*s%d", &st);

	redirect[0] = 0;

	while (1) {
		read = recv_until(s, tmp, sizeof(tmp), '\n');
		if (read < 1)break;
		if (tmp[0] == '\r' || tmp[0] == '\n')break;
		p = strchr(tmp, ':');
		if (p == NULL)continue;

		//printf("%s", tmp);

		*p = 0;
		p++;
		key[0] = 0;
		val[0] = 0;
		sscanf(tmp, "%s", key);
		sscanf(p, "%s", val);
		mystrupr(key);
		if (strcmp(key, "LOCATION") == 0) {
			strcpy(redirect, val);
		}
		else if (strcmp(key, "CONTENT-LENGTH") == 0) {
			sscanf(val, "%d", &clen);
		}
		else if (strcmp(key, "TRANSFER-ENCODING") == 0) {
			strcpy(redirect, val);
			if (strstr(val, "chunked"))chunk = 1;
		}
	}
	if (read < 1) {
		closesocket(s);
		return ret;
	}
	if (clen > 1024 * 1024 - 100) {
		closesocket(s);
		return ret;
	}
	if (rescode)*rescode = st;
	if (clen < 0)clen = 1024 * 1024 - 100;
	ret = 0;

	//printf("body\n");
	*data = malloc(clen + 16);
	if (*data == NULL) {
		closesocket(s);
		return ret;
	}

	read = 0;
	if (clen > 0) {
		if (chunk) {
			read = recv_chunked(s, *data, clen);
		}
		else {
			read = recv_sz(s, *data, clen);
		}
	}

	((char*)(*data))[read] = 0;
	if (sz)*sz = read;

	if (st >= 300 && st <= 399) {
		free(*data);
		*data = NULL;
		closesocket(s);


		if (redirect[0] == 0 || strlen(redirect) > 500 || strlen(loc)>500 || strlen(server)>500) {
			return ret;
		}
		if (strstr(redirect, "http://") == NULL) {
			if (redirect[0] == '/') {
				strcpy(tmp, redirect);
				sprintf(redirect, "http://%s:%d%s", server, port, tmp);
			}
			else {
				strcpy(tmp, redirect);
				sprintf(redirect, "http://%s:%d%s", server, port, loc);
				p = strrchr(redirect, '/');
				if (p) {
					p++;
					*p = 0;
					strcat(redirect, tmp);
				}
			}
		}
		ret = myhttp_post_data__(redirect, sdata, ssz, ctype, data, sz, rescode, ct + 1);
		return ret;
	}
	closesocket(s);
	ret = 0;
	return ret;
}


int myhttp_post_data(const char* url, void* sdata, int ssz, const char* ctype, void** data, int* sz, int* rescode)
{
	return myhttp_post_data__(url, sdata, ssz, ctype, data, sz, rescode, 0);
}


void myhttp_free_data(void* vp)
{
	if (vp == NULL)return;
	free(vp);
}




//#ifdef _CONSOLE
#if 0

int main()
{
	void* data;
	int sz, ret, res;


#if defined(_WIN32) && !defined(__GNUC__)
	//	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_WNDW);
	//	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_WNDW);
	//	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	myhttp_init();

	//ret = myhttp_get_data("http://127.0.0.1", &data, &sz, &res);
	//ret = myhttp_get_data("http://127.0.0.1:12345", &data, &sz, &res);
	//ret = myhttp_get_data("http://127.0.0.1:0", &data, &sz, &res);
	//ret = myhttp_get_data("http://127.0.0.1:12345/test/happy", &data, &sz, &res);

	//ret=myhttp_get_data("http://127.0.0.1/test/happy",&data, &sz,&res);
	//ret = myhttp_get_data("http://127.0.0.1/test/happy/", &data, &sz, &res);

	//ret = myhttp_post_data("http://127.0.0.1/test/happy/nph-connection-check.exe", "1234567890",10,"application/octet-steram",&data, &sz, &res);
	//ret = myhttp_get_data("http://127.0.0.1/test/happy/checklogin.exe",  &data, &sz, &res);
	ret = myhttp_get_data("http://127.0.0.1:11228/301/aaa", &data, &sz, &res);
	printf(">>%s<<\n", (char*)data);

	myhttp_free_data(data);

	myhttp_done();

	return 0;
}

#endif


