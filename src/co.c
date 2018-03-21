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

#include "ext/standard/basic_functions.h"

#include "php_su.h"
#include "src/coco/coco.h"
#include "src/co.h"

SU_CLASS_D(co);

void su_co_main(void *v);

/**
 * 默认情况下，设置 timer::every(0)
 * 在 every 中
 * 如果无协程，则取消掉
 * 如果有，则sched
 * 
 * 如何更新？
 * 当 ready 协程时，判断当前 协程数，如果为0，则设置一个 every，否则 协程数++
 * 在 协程 yield 或执行完成时，协程数--
 **/

void su_co_main(void *v) {
    do {
        if (uv_loop_alive(SU_G(loop))) {
            if (SU_G(coco_ready)) {
                uv_run(SU_G(loop), UV_RUN_NOWAIT);
            } else {
                uv_run(SU_G(loop), UV_RUN_ONCE);
            }
        }

        if (SU_G(coco_ready)) {
            coco_yield();
        } else {
            if (!uv_loop_alive(SU_G(loop))) {
                // don't break directly, because we can exit manually
                coco_yield();
            }
        }
    } while(1);
}

int su_co_scheduler() {
    coco_scheduler();
}

/* {{{ ARG_INFO */
SU_BEGIN_ARG_INFO_EX(co_sleep_arginfo, 0, 0, 0)
	SU_ARG_INFO(0, time)
SU_END_ARG_INFO()
/* }}} */

/* {{{ PHP METHODS */
SU_METHOD(co, id) {
    if (SU_G(coco_running)) {
        RETURN_LONG(SU_G(coco_running)->id);
    }
}

SU_METHOD(co, run) {
    su_cb_t tcb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "f", &tcb.fci, &tcb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::after($time, $callback) param", SU_STATIC_CLASS());
    }
    su_cb_t* cb = su_malloc(sizeof(su_cb_t));
    memcpy(cb, &tcb, sizeof(su_cb_t));
    SU_TRY_REF_CB(cb);
    coco_create(su_call_co_cb, (void *)cb, SU_COCO_STACK);
}

SU_METHOD(co, sched) {
    if (UNEXPECTED(!SU_G(coco_running))) {
        php_error(E_ERROR, "su::sched() can only be called in coroutines");
        return;
    }
    su_vmc_t vmc;
    su_vmc_stash2(&vmc);
    coco_sched();
    su_vmc_unstash2(&vmc);
}

void su_co_sleep_cb(uv_timer_t *ut) {
    coco_t *co = ut->data;
    coco_ready(co);
}

SU_METHOD(co, sleep) {
    if (!SU_G(coco_running)) {
        php_error_docref(NULL, E_ERROR, "coroutine is not %s", "running");
        RETURN_FALSE;
    }
    zend_long t = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &t) == FAILURE) {
        php_error(E_ERROR, "invalid %s::sleep($time) param", SU_STATIC_CLASS());
        RETURN_FALSE;
    }
    if (t < 0) {
        php_error(E_ERROR, "parameter 1 must be a non-negative integer");
    }
    su_vmc_t *vmc = su_vmc_stash();
    uv_timer_t *ut = (uv_timer_t *)su_malloc(sizeof(uv_timer_t));
    ut->data = SU_G(coco_running);
    uv_timer_init(SU_G(loop), ut);
    uv_timer_start(ut, su_co_sleep_cb, t, 0);
    coco_yield();
    su_free(ut);
    su_vmc_unstash(vmc);
}
/* }}} */

/* {{{ uv_co_methods */
zend_function_entry su_co_methods[] = {
    SU_ME(co, id, NULL,	ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    SU_ME(co, run, NULL,	ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    SU_ME(co, sched, NULL,	ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    SU_ME(co, sleep, co_sleep_arginfo,	ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(co) {
    SU_CE_LITE_INIT(co, "su\\co");
    SU_CE_ALIAS(co, "su\\coroutine");
    return SUCCESS;
}
/* }}} */
