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

//#define I_USE_SAMPLE

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
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

#include "sha1.h"
#include "base64.h"

#ifdef I_USE_SAMPLE
#include "websocketcmd.h"
#endif

#if defined(unix) || defined(ANDROID_NDK) || defined(__APPLE__)
#define closesocket(s) close(s)
#endif	/* unix */

#define MAX_LINE 256

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
	// use ih ttp
	char dummy[16];
};

struct child_websocket{
	int version;
	int buf_sz;
	char key[MAX_LINE];
	char buf[MAX_LINE];
};


static void mystrlwr(char*key)
{
	if (key == NULL)return;
	while (*key)*key++ = tolower(*key);
}
static void mystrupr(char*key)
{
	if (key == NULL)return;
	while (*key)*key++ = toupper(*key);
}


void* smplws_child_websocket_init(int fd, struct child_data* cd)
{
	void* ret = NULL;

	ret = malloc(sizeof(struct child_websocket));
	if (ret == NULL)return ret;
	memset(ret, 0, sizeof(struct child_websocket));

#ifdef I_USE_SAMPLE
	if (websocketcmd_open(cd->url) < 0){
		free(ret);
		return NULL;
	}
#endif
	return ret;
}


int smplws_child_websocket_parse_header(int fd, struct child_data* cd)
{
	char*p;
	char key[MAX_LINE];
	char val[MAX_LINE];
	struct child_websocket* cw = (struct child_websocket*)(cd->vp);

	strcpy(cd->tmp, cd->buf);
	p = strchr(cd->tmp,':');
	if (p == NULL)return 0;
	*p = ' ';
	key[0] = val[0]=0;
	sscanf(cd->tmp, "%s%s", key, val);
	mystrlwr(key);
	if (strcmp(key, "sec-websocket-version") == 0){
		sscanf(val, "%d", &(cw->version));
	}
	if (strcmp(key, "sec-websocket-key") == 0){
		sscanf(val, "%s", &(cw->key));
	}
	return 0;
}

int smplws_child_send_websocket_connection(int fd, struct child_data* cd)
{
	char buf[256];
	char result[64];
	char sha[20];
	SHA1Context ctx;
	char* str;

	struct child_websocket* cw = (struct child_websocket*)(cd->vp);
	if (cw->version == 0)return -1;
	if (strlen(cw->key) > 128)return -1;

	strcpy(buf, cw->key);
	strcat(buf, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	SHA1Reset(&ctx);
	SHA1Input(&ctx, buf, strlen(buf));
	SHA1Result(&ctx, sha);
	base64_encode(result, sha, 20);

	str = "HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: upgrade\r\n"
		//"Sec-WebSocket-Protocol: chat\r\n"
		"Sec-WebSocket-Accept: ";
	send(fd, str, strlen(str), 0);
	send(fd, result, strlen(result), 0);
	send(fd, "\r\n\r\n", 4, 0);

	return 0;
}


static int recv_sz(int fd, void* buf, int sz)
{
	int ret, read = 0;
	while (1){
		ret = recv(fd, (char*)buf + read, sz, 0);
		if (ret < 1)return ret;
		sz -= ret;
		read += ret;
		if (sz == 0)return read;
	}
}


int smplws_child_websocket_ping(int fd, struct child_data* cd)
{
	unsigned char buf[] = { 0x89, 0x04, 0x41, 0x42, 0x43, 0x44 };
	return send(fd, buf, 6, 0);
}

int smplws_child_websocket_idle(int fd, struct child_data* cd)
{
	return 0;
}

int smplws_child_websocket_pong_send(int fd, struct child_data* cd)
{

	int ret = -1;
	struct child_websocket* cw = (struct child_websocket*)(cd->vp);

	unsigned char b[2] = { 0x8a, 0x00 };
	b[1] = cw->buf_sz;

	ret=send(fd, b, 2, 0);
	if (ret < 1)return ret;
	ret = send(fd, cw->buf, cw->buf_sz, 0);
	return ret;
}

int smplws_child_websocket_close_send(int fd, struct child_data* cd)
{

	int ret = -1;
	struct child_websocket* cw = (struct child_websocket*)(cd->vp);

	unsigned char b[2] = { 0x88, 0x05 };
	b[1] = cw->buf_sz;

	ret = send(fd, b, 2, 0);
	if (ret < 1)return ret;
	ret = send(fd,"close", 5, 0);
	return ret;
}

int smplws_child_websocket_frame_send(int fd, struct child_data* cd)
{

	int ret = -1;
	int len = 2;
	struct child_websocket* cw = (struct child_websocket*)(cd->vp);

	unsigned char b[4] = { 0x81, 0x00 };
	b[1] = cw->buf_sz;
	if (cw->buf_sz > 125){
		b[1] = 126;
		b[2] = (cw->buf_sz >> 8) & 255;
		b[3] = cw->buf_sz &255;
		len += 2;
	}
	ret = send(fd, b, len, 0);

	if (ret < 1)return ret;
	ret = send(fd, cw->buf, cw->buf_sz, 0);
	return ret;
}


int smplws_child_websocket_frame_data_recv(int fd, struct child_data* cd)
{
	struct child_websocket* cw = (struct child_websocket*)(cd->vp);
	printf("data >>%s<< \n", cw->buf);
#ifdef I_USE_SAMPLE
	{
		int ret = -1;
		char out[MAX_LINE];
		
		ret = websocketcmd_data(cd->url, cw->buf, out, sizeof(out));
		if (ret < 0)return ret;
		strncpy(cw->buf, out, MAX_LINE);
		cw->buf[MAX_LINE - 1] = 0;
		cw->buf_sz = strlen(cw->buf);
		return smplws_child_websocket_frame_send(fd, cd);
	}
#else
	return smplws_child_websocket_frame_send(fd, cd);
#endif
}




int smplws_child_websocket_frame(int fd, struct child_data* cd)
{
	unsigned char bt1,bt2;
	unsigned char msk[4];
	unsigned char buf[256];
	struct child_websocket* cw = (struct child_websocket*)(cd->vp);
	int ret = -1, len=0, i;

	buf[0] = 0;
	ret = recv(fd, &bt1, 1, 0);
	if (ret < 1)return -1;
	ret = recv(fd, &bt2, 1, 0);
	if (ret < 1)return -1;
	if ((bt2 & 0x80) == 0)return -1;
	len = bt2 & 0x7f;
	if (len >= 127)return -1;
	if (len == 126){
		ret = recv_sz(fd, msk, 4);
		if (ret < 1)return -1;
		len = (msk[0] << 8) | msk[1];
	}
	if (len>250)return -1;

	if (bt2 & 0x80){
		ret = recv_sz(fd, msk, 4);
		if (ret < 1)return -1;
	}
	if (len>0){
		ret = recv_sz(fd, buf, len);
		if (ret < 1)return -1;
		buf[len] = 0;
		for (i = 0; i < len; i++)buf[i] = buf[i] ^ msk[i & 3];
	}
	memcpy(cw->buf, buf,len);
	cw->buf[len] = 0;
	cw->buf_sz = len;

	switch (bt1 & 0xf){
	//data
	case 1:
	case 2:
		smplws_child_websocket_frame_data_recv(fd, cd);
		break;
	//close
	case 8:
		return smplws_child_websocket_close_send(fd, cd);
		return -1;
	//ping
	case 9:
		return smplws_child_websocket_pong_send(fd, cd);
		break;
	//pong
	case 10:
		break;
	default:
		return - 1;

	}
	return 0;
}

int smplws_child_websocket_done(struct child_data* cd)
{
#ifdef I_USE_SAMPLE
	websocketcmd_close(cd->url);
#endif
	return 0;
}


