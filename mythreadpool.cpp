/*
Copyright (c) 2017, Yomei Otani <yomei.otani@gmai.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mythread.h"
#include "ysleep.h"

#include "mythreadpool.h"



#if defined(_WIN32) && !defined(__GNUC__)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif


#define MAX_THREAD 10


static void** m_func=NULL;
static void** m_arg=NULL;
static void** m_list=NULL;
static int* m_flag=NULL;


class MyThreadPool{
public:
	MyThreadPool();
	virtual ~MyThreadPool();


};

MyThreadPool::MyThreadPool()
{
	m_func=(void**)malloc(sizeof(void*)*MAX_THREAD);
	m_list=(void**)malloc(sizeof(void*)*MAX_THREAD);
	m_arg=(void**)malloc(sizeof(void*)*MAX_THREAD);
	m_flag=(int*)malloc(sizeof(int)*MAX_THREAD);
	int i;
	for(i=0;i<MAX_THREAD;i++){
		m_func[i]=NULL;
		m_list[i]=NULL;
		m_arg[i]=NULL;
		m_flag[i]=0;
	}
}

MyThreadPool::~MyThreadPool()
{
	mytp_endall();

	if(m_func)free(m_func);
	m_func=NULL;
	if(m_arg)free(m_arg);
	m_arg=NULL;
	if(m_list)free(m_list);
	m_list=NULL;
	if(m_flag)free(m_flag);
	m_flag=NULL;

}


int mytp_create(void* func,void* arg)
{
	int i;
	for(i=0;i<MAX_THREAD;i++){
		if(m_flag[i]==0 && m_list[i]!=NULL){
			mythread_join((mythread_t*)(m_list[i]));
			m_list[i]=NULL;
			m_flag[i]=0;
		}
	}
	for(i=0;i<MAX_THREAD;i++){
		if(m_flag[i]==0 && m_list[i]==NULL){
			char* p=NULL;
			m_flag[i]=1;
			m_arg[i]=arg;
			m_func[i]=func;
			p+=i;
			//m_list[i]=mythread_create((void*)run,(void*)i);
			m_list[i]=mythread_create((void*)mytp_run,(void*)p);
			return i;
		}
	}
	return -1;
}


int mytp_join(int i)
{
	if(i==-1)return -1;
	if(m_flag[i]==0 || m_list[i]==NULL)return -1;

	mythread_join((mythread_t*)(m_list[i]));
	m_list[i]=NULL;
	m_flag[i]=0;
	return i;
}

void mytp_sleep(int ms)
{
	ymsleep(ms);
}

void mytp_endall()
{
	int i;
	for(i=0;i<MAX_THREAD;i++){
		if(m_list[i]!=NULL){
			mythread_join((mythread_t*)(m_list[i]));
			m_list[i]=NULL;
			m_flag[i]=0;
		}
	}
}

int mytp_run(void* id)
{
	int i;
	char *p=NULL;
	//i=(int)id;
	i=(int)(((char*)id)-p);
	((void (*)(void*)) (m_func[i]))(m_arg[i]);
	m_flag[i]=0;
	return 0;
}

static MyThreadPool mythreadpool;




#if 0

int f(char* arg)
{
	printf("%s start\n",arg);
	ThreadPool::sleep(5000);
	printf("%s end\n",arg);
	return 0;
}




main()
{
#if defined(_WIN32) && !defined(__GNUC__)
//	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_WNDW);
//	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_WNDW);
//	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif
	printf("main start\n");
	ThreadPool::create((void*)f,"000"); 
	int id=ThreadPool::create((void*)f,"123"); 
	printf("id=%d\n",id);
	ThreadPool::join(id);

	printf("main end\n");
	return 0;
}

#endif
