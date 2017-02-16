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

#define __MYTHREAD_C__VER "20130626"


#ifdef _MSC_VER
#if _MSC_VER >= 1400
#pragma warning( disable : 4996 )
#pragma warning( disable : 4819 )
#endif
#endif


#include <stdio.h>
#include <stdlib.h>
#include "mythread.h"



#ifdef HAVE_PTHREAD_H
#define PTHREAD_CREATE(thread, attr, func, arg)                          \
	(int)pthread_create((pthread_t *)(thread), (pthread_attr_t *)(attr), \
    (void * (*)(void *))(func), (void *)(arg))
#else
#ifdef _WIN32
#define BEGINTHREAD(src, stack, func, arg, flag, id)         \
	(HANDLE)_beginthreadex((void *)(src), (unsigned)(stack), \
	(unsigned (_stdcall *)(void *))(func), (void *)(arg),    \
	(unsigned)(flag), (unsigned *)(id))
#endif
#endif



/* yomei fixed: translate __stdcall to __cdecl  */
#ifdef _WIN32
static unsigned __stdcall internal_start_thread( void * vp)
{
	mythread_t *t;
	if(vp==NULL)return 0;

	t=(mythread_t *)vp;
	return (unsigned)(((void * (*)(void *))(t->func))(t->arg));
}
#endif


mythread_t*
mythread_create(void* func, void* arg)
{
	mythread_t *t;
	
	t = (mythread_t *)malloc(sizeof(mythread_t));
	if (!t)
	{
		goto fail0;
	}

	t->func = func;
	t->arg = arg;

#ifdef HAVE_PTHREAD_H
	if (PTHREAD_CREATE(&t->hthread, 0, func, arg))
	{
		goto fail1;
	}
#else
#ifdef _WIN32
	{
		int id;
		HANDLE hnd;
		/* yomei fixed: this thread function must be a standard call.  */
		hnd = BEGINTHREAD(0, 0, internal_start_thread, t, 0, &id);
		if (!hnd)
		{
			goto fail1;
		}
		t->hthread = hnd;
	}
#endif
#endif
	return t;

fail1:
	if (t)
	{
		free(t);
		t = NULL;
	}
fail0:
	return t;
}



void
mythread_join(mythread_t* t)
{
	if (!t)
	{
		goto fail;
	}
#ifdef HAVE_PTHREAD_H
	pthread_join(t->hthread, NULL);
	free(t);
#else
#ifdef _WIN32
	WaitForSingleObject(t->hthread, INFINITE);
	CloseHandle(t->hthread);
	free(t);
#endif
#endif

fail:
	return;
}




