/* Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT */
/*
  +----------------------------------------------------------------------+
  | su framework                                                         |
  +----------------------------------------------------------------------+
  | Copyright (c) wukezhan<wukezhan@gmail.com>                           |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: wukezhan<wukezhan@gmail.com>                                 |
  +----------------------------------------------------------------------+
*/
#ifndef COCO_H
#define COCO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ctx.h>


#define nil ((void*)0)
#define nelem(x) (sizeof(x)/sizeof((x)[0]))

typedef unsigned long coco_ulong;
typedef unsigned int coco_uint;
typedef unsigned char coco_uchar;
typedef unsigned short coco_ushort;
typedef unsigned long long coco_uvlong;
typedef long long coco_vlong;

typedef struct Context Context;

struct Context
{
    ucontext_t uc;
};


typedef struct coco_s coco_t;
struct coco_s
{
    /*char    name[256];    // offset known to acid*/
    /*char    state[256];*/
    void    *channel;
    coco_t *next;
    coco_t *prev;
    coco_t *allnext;
    coco_t *allprev;
    Context context;
    coco_uvlong alarmtime;
    coco_uint id;
    coco_uchar *stk;
    coco_uint stksize;
    int exiting;
    int allcoco_slot;
    int system;
    int ready;
    void (*startfn)(void*);
    void *startarg;
    void *udata;
};

typedef struct coco_list_s {
    coco_t *head;
    coco_t *tail;
} coco_list_t;

void coco_ready(coco_t*);
void coco_switch(void);

void coco_add(coco_list_t*, coco_t*);
void coco_del(coco_list_t*, coco_t*);

extern coco_t *coco_running;
extern int coco_count;

/*
 * basic procs and threads
 */
int coco_anyready(void);
coco_uint coco_create(void (*f)(void *arg), void *arg, uint stacksize);
void coco_exit(int);
void coco_exitall(int);
//void coco_main(int argc, char *argv[]);
int coco_sched(void);
int coco_yield(void);
void** coco_data(void);
void needstack(int);
void coco_name(char*, ...);
void coco_state(char*, ...);
char* coco_getname(void);
char* coco_getstate(void);
void coco_system(void);
unsigned int coco_delay(unsigned int);
coco_uint coco_id(void);
void coco_scheduler(void);
void coco_info(int s);
#ifdef __cplusplus
}
#endif

#endif