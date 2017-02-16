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

#ifndef __MYTHREAD_H__
#define __MYTHREAD_H__

#define __MYTHREAD_H__VER "20130626"


#if defined(unix) || defined(__APPLE__)
/* define if use pthread */
#define HAVE_PTHREAD_H
#endif

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#include <string.h>
#else
#ifdef _WIN32
#include <windows.h>
#include <winbase.h>
#include <process.h>
#endif
#endif


#ifdef HAVE_PTHREAD_H
typedef pthread_t hthread_t;
#else
#ifdef _WIN32
typedef HANDLE hthread_t;
#endif
#endif



typedef struct _thread
{
	hthread_t hthread;
	void* func;
	void* arg;
} mythread_t;


#ifdef __cplusplus
extern "C"
{
#endif

mythread_t* mythread_create(void* func, void *arg);
void mythread_join(mythread_t* t);

#ifdef __cplusplus
}
#endif

#endif /* __MYTHREAD_H__ */
