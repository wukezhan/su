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

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "Zend/zend_types.h"
#include "Zend/zend_interfaces.h"

#include "php_su.h"

#include "src/co.h"
#include "src/timer.h"

SU_CLASS_D(timer);

#define SU_TIMER_RUNNING 1
#define SU_TIMER_STOPING 2

static zend_object* su_timer_create(zend_class_entry *ce) {
    su_timer_t *timer = (su_timer_t *)su_ecalloc(su_timer_t);
    SU_OBJ_INIT_STD(timer, ce, su_timer_handlers);
    uv_timer_init(SU_G(loop), &timer->handle);
    timer->handle.data = (zval *)su_malloc(sizeof(zval));

    return &timer->std;
}

void su_timer_free(zend_object *std) {
    su_timer_t *timer = SU_OBJ_FROM_STD(std, su_timer_t);
    SU_OBJ_FREE_STD(timer);
    su_free(timer);
}

void su_timer_close(su_timer_t* timer) {
    if (timer->status == SU_TIMER_STOPING) {
        return ;
    }
    timer->status = SU_TIMER_STOPING;
    uv_timer_stop(&timer->handle);
    SU_TRY_FREE_CBS(timer->cbs);
    zval *self = (zval *)timer->handle.data;
    if (self) {
        zval_ptr_dtor(self);
        su_free(self);
        timer->handle.data = NULL;
    }
}

void su_timer_co(void *v) {
    su_timer_t *st = (su_timer_t *)v;
    zval *self = (zval *)st->handle.data;
    if (self) {
        st->status = SU_TIMER_RUNNING;
        zval retval;
        su_call_cb(st->cbs[EV_TIMER_TIMEOUT], &retval, 0, NULL);
        zval_ptr_dtor(&retval);
        if (st->type == SU_TIMER_AFTER) {
            su_timer_close(st);
        }
    } else {
        SU_TRACE("error!");
    }
}

void su_timer_cb(uv_timer_t *ut) {
    coco_create(su_timer_co, ut, SU_COCO_STACK);
}

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO_EX(su_timer_construct_arginfo, 0, 0, 0)
    ZEND_ARG_INFO(0, loop)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ PHP METHODS */
SU_METHOD(timer, __construct) {
    SU_SET_SELF;
    su_timer_t *timer = SU_OBJ_FROM_SELF(su_timer_t);
    ZVAL_COPY(timer->handle.data, self);
    SU_RET_SELF;
}

SU_METHOD(timer, again) {
    SU_SET_SELF;
    su_timer_t *timer = SU_OBJ_FROM_SELF(su_timer_t);
    timer->status = 0;
    uv_timer_start(&timer->handle, su_timer_cb, timer->timeout, 0);
    SU_RET_SELF;
}

SU_METHOD(timer, after) {
    SU_SET_SELF;
    zend_long time = 0;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lf", &time, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::after($time, $callback) param", SU_STATIC_CLASS());
    }
    if (time<0) {
        php_error(E_ERROR, "parameter 1 must be a non-negative integer");
    }

    su_timer_t *timer = SU_OBJ_FROM_SELF(su_timer_t);
    timer->type = SU_TIMER_AFTER;
    timer->status = 0;
    timer->timeout = time;
    su_set_cb(timer->cbs, EV_TIMER_TIMEOUT, &cb);
    uv_timer_start(&timer->handle, su_timer_cb, time, 0);
}

SU_METHOD(timer, every) {
    SU_SET_SELF;
    zend_long time = 0;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lf", &time, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::after($time, $callback) param", SU_STATIC_CLASS());
    }
    if (time < 0) {
        php_error(E_ERROR, "parameter 1 must be a non-negative integer");
    }

    su_timer_t *timer = SU_OBJ_FROM_SELF(su_timer_t);
    timer->type = SU_TIMER_EVERY;
    timer->status = 0;
    timer->timeout = time;
    su_set_cb(timer->cbs, EV_TIMER_TIMEOUT, &cb);
    uv_timer_start(&timer->handle, su_timer_cb, time, time);
}

SU_METHOD(timer, close) {
    SU_SET_SELF;
    SU_TRACE("close");
    su_timer_t *timer = SU_OBJ_FROM_SELF(su_timer_t);
    su_timer_close(timer);
    SU_RET_SELF;
}

SU_METHOD(timer, unref) {
    SU_SET_SELF;
    su_timer_t *timer = SU_OBJ_FROM_SELF(su_timer_t);
    uv_unref((uv_handle_t *) timer);
    SU_RET_SELF;
}

SU_METHOD(timer, __destruct) {
    SU_SET_SELF;
    su_timer_t *timer = SU_OBJ_FROM_SELF(su_timer_t);
    SU_TRACE("timer dtor %p", timer);
    su_timer_close(timer);
}

/* }}} */

/* {{{ uv_timer_methods */
zend_function_entry su_timer_methods[] = {
    SU_ME(timer, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(timer, after, NULL, ZEND_ACC_PUBLIC)
    SU_ME(timer, again, NULL, ZEND_ACC_PUBLIC)
    SU_ME(timer, every, NULL, ZEND_ACC_PUBLIC)
    SU_ME(timer, close, NULL, ZEND_ACC_PUBLIC)
    SU_ME(timer, unref, NULL, ZEND_ACC_PUBLIC)
    SU_ME(timer, __destruct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(timer) {
    SU_CE_INIT(timer, "su\\timer");
    return SUCCESS;
}
/* }}} */
