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
#include "Zend/zend_interfaces.h"

#include "php_su.h"

#include "src/coco/coco.h"
#include "src/chan.h"

SU_CLASS_D(chan);

static zend_object* su_chan_create(zend_class_entry *ce) {
    su_chan_t *chan = (su_chan_t *)su_ecalloc(su_chan_t);
    SU_OBJ_INIT_STD(chan, ce, su_chan_handlers);
    chan->self = (zval *)su_malloc(sizeof(zval));

    return &chan->std;
}

int su_chan_init(su_chan_t *chan, int cap) {
    chan->cap = cap;
    chan->size = 0;
    chan->head = 0;
    chan->tail = -1;
    chan->data = (zval *)su_malloc(sizeof(zval)*cap);
}

void su_chan_free(zend_object *std) {
    su_chan_t *chan = SU_OBJ_FROM_STD(std, su_chan_t);
    SU_OBJ_FREE_STD(chan);
    zval_ptr_dtor(chan->self);
    su_free(chan->self);
    int i = 0;
    zval *p = NULL;
    do {
        p = chan->data+i;
        if (p) {
            zval_ptr_dtor(p);
            su_free(p);
        }
        i ++;
    } while(i < chan->cap);
    su_free(chan);
}

void su_chan_close(su_chan_t* chan) {
    if (chan->status == SU_CHAN_STOP) {
        return;
    }
    chan->status = SU_CHAN_STOP;
    SU_TRY_FREE_CBS(chan->cbs);
    zval *self = (zval *)chan->self;
    if (self) {
        zval_ptr_dtor(self);
        su_free(self);
        chan->self = NULL;
    }
}

/* su_chan_resize needed here */

int su_chan_send(su_chan_t *chan, zval *data) {
    if (chan->size == chan->cap) {
        return FAILURE;
    }

    chan->size ++;
    chan->tail = (chan->tail+1) % chan->cap;
    if (data) {
        ZVAL_COPY(chan->data+chan->tail, data);
    } else {
        ZVAL_NULL(chan->data+chan->tail);
    }

    return SUCCESS;
}

zval* su_chan_recv(su_chan_t *chan) {
    if (chan->size == 0) {
        return NULL;
    }
    int head = chan->head;
    chan->head = (head+1) % chan->cap;
    chan->size --;
    zval* data = chan->data+head;
    return data;
}

/* {{{ ARG_INFO */
#if 0
SU_BEGIN_ARG_INFO_EX(chan_construct_arginfo, 0, 0, 0)
    SU_ARG_INFO(0, config)
SU_END_ARG_INFO()
#endif
/* }}} */

/* {{{ PHP METHODS */
SU_METHOD(chan, __construct) {
    SU_SET_SELF;
    zend_ulong cap = 1;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &cap) == FAILURE) {
        // throw exception
        return;
    }
    su_chan_t *sc = SU_OBJ_FROM_SELF(su_chan_t);
    if (su_chan_init(sc, cap) == FAILURE) {
        // throw exception
        return ;
    }
    ZVAL_COPY(sc->self, self);
    SU_RET_SELF;
}

SU_METHOD(chan, send) {
    SU_SET_SELF;
    su_chan_t *sc = SU_OBJ_FROM_SELF(su_chan_t);
    if (sc->status == SU_CHAN_STOP) {
        return ;
    }
    zval *data;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(data)
    ZEND_PARSE_PARAMETERS_END();
    while (su_chan_send(sc, data) == FAILURE) {
        sc->sender_status = SU_CHAN_SENDER_YIELD;
        sc->coco_sender = SU_G(coco_running);
        su_vmc_t *vmc = su_vmc_stash();
        coco_yield();
        su_vmc_unstash(vmc);
    }
    if (sc->recver_status != SU_CHAN_RECVER_READY) {
        sc->recver_status = SU_CHAN_RECVER_READY;
        if (sc->coco_recver) {
            coco_ready(sc->coco_recver);
        } else {
            /* defer */
        }
    }
}

SU_METHOD(chan, recv) {
    SU_SET_SELF;
    su_chan_t *sc = SU_OBJ_FROM_SELF(su_chan_t);
    if (SU_G(coco_running) == NULL) {
        php_error(E_ERROR, "chan must recv in coroutine");
    }
    if (sc->status == SU_CHAN_STOP) {
        php_error(E_WARNING, "channel has been stoped");
        return;
    }
    if (sc->status == SU_CHAN_CHAOS) {
        sc->status = 0;
    }
    sc->coco_recver = SU_G(coco_running);
    sc->coco_recver->channel = (void *)sc;
    if (sc->size == 0) {
        su_vmc_t *vmc = su_vmc_stash();
        sc->recver_status = SU_CHAN_RECVER_YIELD;
        coco_yield();
        su_vmc_unstash(vmc);
    }
    zval *data = su_chan_recv(sc);
    if (data) {
        if (Z_TYPE_P(data) == IS_NULL) {
            php_error(E_WARNING, "channel returned null, something may be error");
        }
        RETVAL_ZVAL(data, 1, 1);
        if (sc->sender_status == SU_CHAN_SENDER_YIELD) {
            sc->sender_status = SU_CHAN_SENDER_READY;
            coco_ready(sc->coco_sender);
        }
    } else {
        php_error(E_WARNING, "channel returned null, something may be error");
        RETVAL_NULL();
    }
}

SU_METHOD(chan, is_full) {
    SU_SET_SELF;
    su_chan_t *sc = SU_OBJ_FROM_SELF(su_chan_t);
    RETURN_BOOL(sc->size >= sc->cap);
}

SU_METHOD(chan, cap) {
    SU_SET_SELF;
    su_chan_t *sc = SU_OBJ_FROM_SELF(su_chan_t);
    RETURN_LONG(sc->cap);
}

SU_METHOD(chan, size) {
    SU_SET_SELF;
    su_chan_t *sc = SU_OBJ_FROM_SELF(su_chan_t);
    RETURN_LONG(sc->size);
}

SU_METHOD(chan, select) {
}

SU_METHOD(chan, close) {
}

SU_METHOD(chan, __destruct) {
}

/* }}} */

/* {{{ uv_chan_methods */
zend_function_entry su_chan_methods[] = {
    SU_ME(chan, __construct, NULL,    ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(chan, __destruct, NULL,    ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    //SU_ME(chan, select, NULL,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    SU_ME(chan, send, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(chan, recv, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(chan, is_full, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(chan, cap, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(chan, size, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(chan, close, NULL,    ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(chan) {
    SU_CE_INIT(chan, "su\\chan");
    SU_CE_ALIAS(chan, "su\\channel");

    return SUCCESS;
}
/* }}} */
