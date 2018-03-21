/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>

#include "php.h"
#include "php_ini.h"
#include "Zend/zend_interfaces.h"

#include "php_su.h"
#include "src/chan.h"
#include "src/co.h"
#include "coco.h"

int coco_debug_level;
int coco_count;
int coco_nswitch;
int coco_exit_val;

Context coco_sched_context;
coco_list_t coco_run_queue;

coco_t** coco_store;
int coco_total_count;

static char* argv0;
static void contextswitch(Context* from, Context* to);

#define coco_ready_incr() ++ SU_G(coco_ready)
#define coco_ready_decr() -- SU_G(coco_ready)

static void
coco_start(coco_uint y, coco_uint x)
{
    coco_t* t;
    coco_ulong z;

    z = x << 16; /* hide undefined 32-bit shift from 32-bit compilers */
    z <<= 16;
    z |= y;
    t = (coco_t*)z;

    t->startfn(t->startarg);
    coco_exit(0);
}

static unsigned int coco_idgen;

static coco_t*
coco_alloc(void (*fn)(void*), void* arg, coco_uint stack)
{
    coco_t* t;
    sigset_t zero;
    coco_uint x, y;
    coco_ulong z;

    /* allocate the task and stack together */
    t = malloc(sizeof *t + stack);
    if (t == nil) {
        abort();
    }
    memset(t, 0, sizeof *t);
    t->stk = (coco_uchar*)(t + 1);
    t->stksize = stack;
    t->id = ++coco_idgen;
    t->startfn = fn;
    t->startarg = arg;

    /* do a reasonable initialization */
    memset(&t->context.uc, 0, sizeof t->context.uc);
    sigemptyset(&zero);
    sigprocmask(SIG_BLOCK, &zero, &t->context.uc.uc_sigmask);

    /* must initialize with current context */
    if (getcontext(&t->context.uc) < 0) {
        abort();
    }

    /* call makecontext to do the real work. */
    /* leave a few words open on both ends */
    t->context.uc.uc_stack.ss_sp = t->stk + 8;
    t->context.uc.uc_stack.ss_size = t->stksize - 64;
#if defined(__sun__) && !defined(__MAKECONTEXT_V2_SOURCE) /* sigh */
#warning "doing sun thing"
    /* can avoid this with __MAKECONTEXT_V2_SOURCE but only on SunOS 5.9 */
    t->context.uc.uc_stack.ss_sp = (char*)t->context.uc.uc_stack.ss_sp
        + t->context.uc.uc_stack.ss_size;
#endif
    /*
	 * All this magic is because you have to pass makecontext a
	 * function that takes some number of word-sized variables,
	 * and on 64-bit machines pointers are bigger than words.
	 */
    z = (coco_ulong)t;
    y = z;
    z >>= 16; /* hide undefined 32-bit shift from 32-bit compilers */
    x = z >> 16;
    makecontext(&t->context.uc, (void (*)())coco_start, 2, y, x);

    return t;
}

coco_uint coco_create(void (*fn)(void*), void* arg, coco_uint stack)
{
    coco_t* t;

    t = coco_alloc(fn, arg, stack);
    coco_count++;
    if (coco_total_count % 64 == 0) {
        coco_store = realloc(coco_store, (coco_total_count + 64) * sizeof(coco_store[0]));
        if (coco_store == nil) {
            php_error(E_WARNING, "out of memory");
            abort();
        }
    }
    t->allcoco_slot = coco_total_count;
    coco_store[coco_total_count++] = t;
    coco_ready(t);
    if (t->id == 1) {
        SU_G(coco_io) = t;
    }
    return t->id;
}

void coco_system(void)
{
    if (!SU_G(coco_running)->system) {
        SU_G(coco_running)->system = 1;
        --coco_count;
    }
}

void coco_switch(void)
{
    needstack(0);
    contextswitch(&SU_G(coco_running)->context, &coco_sched_context);
}

void coco_ready(coco_t* t)
{
    t->ready = 1;
    if (t->id > 1) {
        /* 只对非 main coco，执行 renew 逻辑；
         * 否则对于 coco main 执行 renew 的话，会导致无限循环
         */
        coco_ready_incr();
    }
    coco_add(&coco_run_queue, t);
}

int coco_sched(void)
{
    int n = 0;

    coco_ready(SU_G(coco_running));
    coco_switch();
    return 0;
}

int coco_yield(void)
{
    int n;

    n = coco_nswitch;
    coco_switch();
    return 0;
}

int coco_anyready(void)
{
    return coco_run_queue.head != nil;
}

void coco_exitall(int val)
{
    exit(val);
}

void coco_exit(int val)
{
    coco_exit_val = val;
    if (SU_G(coco_running)->channel) {
        su_chan_t* chan = (su_chan_t*)SU_G(coco_running)->channel;
        chan->status = SU_CHAN_CHAOS;
        SU_G(coco_running)->channel = NULL;
    }
    SU_G(coco_running)->exiting = 1;
    coco_switch();
}

static void
contextswitch(Context* from, Context* to)
{
    if (swapcontext(&from->uc, &to->uc) < 0) {
        php_error(E_ERROR, "swapcontext failed: %r\n");
        assert(0);
    }
}

static inline int coco_check_io() {
    if (!SU_G(coco_io)) {
        return 0;
    }
    if (uv_loop_alive(SU_G(loop))) {
        if (!SU_G(coco_io)->ready) {
            coco_ready(SU_G(coco_io));
        }
        return 1;
    }
    return 0;
}

void coco_scheduler(void)
{
    int i = 0;
    coco_t* t;

    int n = 0;
    for (;;) {
        ++ n;
        coco_check_io();
        t = coco_run_queue.head;
        if (UNEXPECTED(t == nil)) {
            /*for (i=0; i<coco_total_count; i++) {
                coco_t *cc = coco_store[i];
                su_chan_t *sc = (su_chan_t *) cc->channel;
                if (sc && sc->recver_status & SU_CHAN_RECVER_YIELD) {
                    su_chan_send(sc, NULL);
                    coco_ready(sc->coco_recver);
                }
            }*/
            if (coco_run_queue.head == nil) {
                break;
            } else {
                continue;
            }
        }
        coco_del(&coco_run_queue, t);
        t->ready = 0;
        SU_G(coco_running) = t;
        if (EXPECTED(t->id > 1)) {
            coco_ready_decr();
        }
        contextswitch(&coco_sched_context, &t->context);
        SU_G(coco_running) = nil;
        if (UNEXPECTED(t->exiting)) {
            if (t->id == 1) {
                SU_G(coco_io) = NULL;
            }
            if (!t->system) {
                -- coco_count;
            }
            i = t->allcoco_slot;
            coco_store[i] = coco_store[-- coco_total_count];
            coco_store[i]->allcoco_slot = i;
            free(t);
        }
    }

    coco_idgen = 0;
    int j = 0;
    for(; j<coco_total_count; j++) {
        coco_t *t = coco_store[j];
        //php_error(E_WARNING, "coco %d need to be exit", t->id);
        if (t->id == 1) {
            SU_G(coco_io) = NULL;
        }
        if (!t->system) {
            -- coco_count;
        }
        j = t->allcoco_slot;
        coco_store[j] = coco_store[-- coco_total_count];
        coco_store[j]->allcoco_slot = j;
        free(t);
    }
    SU_TRACE("total count %d", coco_total_count);
}

void**
coco_data(void)
{
    return &SU_G(coco_running)->udata;
}

void needstack(int n)
{
    coco_t* t;

    t = SU_G(coco_running);

    if ((char*)&t <= (char*)t->stk
        || (char*)&t - (char*)t->stk < 256 + n) {
        /* 此处协程栈溢出，需加上高效栈自动增长逻辑 */
        php_error(E_ERROR, "task stack overflow: &t=%p tstk=%p n=%d\n", &t, t->stk, 256+n);
        abort();
    }
}

/*
 * hooray for linked lists
 */
void coco_add(coco_list_t* l, coco_t* t)
{
    if (l->tail) {
        l->tail->next = t;
        t->prev = l->tail;
    } else {
        l->head = t;
        t->prev = nil;
    }
    l->tail = t;
    t->next = nil;
}

void coco_del(coco_list_t* l, coco_t* t)
{
    if (t->prev) {
        t->prev->next = t->next;
    } else {
        l->head = t->next;
    }
    if (t->next) {
        t->next->prev = t->prev;
    } else {
        l->tail = t->prev;
    }
}

coco_uint coco_id(void)
{
    return SU_G(coco_running)->id;
}