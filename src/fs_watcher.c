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

#include "src/fs_watcher.h"

SU_CLASS_D(fs_watcher);

static zend_object* su_fs_watcher_create(zend_class_entry *ce) {
    su_fs_watcher_t *fs_watcher = (su_fs_watcher_t *)su_ecalloc(su_fs_watcher_t);
    SU_OBJ_INIT_STD(fs_watcher, ce, su_fs_watcher_handlers);
    uv_fs_event_init(SU_G(loop), &fs_watcher->handle);
    fs_watcher->handle.data = (zval *)su_malloc(sizeof(zval));

    return &fs_watcher->std;
}

void su_fs_watcher_close(su_fs_watcher_t* fs_watcher) {
    if (fs_watcher->status == SU_FS_WATCHER_STOPED) {
        return ;
    }
    fs_watcher->status = SU_FS_WATCHER_STOPED;
    zval *self = (zval *)fs_watcher->handle.data;
    if (fs_watcher->status) {
        uv_fs_event_stop(&fs_watcher->handle);
    }
    if (self) {
        SU_TRY_FREE_CBS(fs_watcher->cbs);
        zval_ptr_dtor(self);
        su_free(self);
        fs_watcher->handle.data = NULL;
    }
}

void su_fs_watcher_free(zend_object *std) {
    su_fs_watcher_t *fs_watcher = SU_OBJ_FROM_STD(std, su_fs_watcher_t);
    su_fs_watcher_close(fs_watcher);
    SU_OBJ_FREE_STD(fs_watcher);
    su_free(fs_watcher);
}

/* {{{ ARG_INFO */
#if 0
SU_BEGIN_ARG_INFO_EX(fs_construct_arginfo, 0, 0, 0)
    SU_ARG_INFO(0, config)
SU_END_ARG_INFO()
#endif
/* }}} */

/* {{{ PHP METHODS */
SU_METHOD(fs_watcher, __construct) {
    SU_SET_SELF;
    su_fs_watcher_t *fw = SU_OBJ_FROM_SELF(su_fs_watcher_t);
    ZVAL_COPY(fw->handle.data, self);
    SU_RET_SELF;
}

SU_METHOD(fs_watcher, __destruct) {
    SU_SET_SELF;
    SU_RET_SELF;
}

void su_fs_watch_co(void* arg) {
    su_fs_watcher_t *fw = (su_fs_watcher_t *)arg;
    zval path;
    ZVAL_STR(&path, fw->path);
    zval events;
    ZVAL_LONG(&events, fw->events);
    zval params[2] = { path, events };
    zval retval;
    su_call_cb(fw->cbs[EV_FW_CHANGE], &retval, 2, params);
    zval_ptr_dtor(&retval);
    fw->events = 0;
}

void su_fs_watch_cb(uv_fs_event_t* handle, const char* filename, int events, int status) {
    su_fs_watcher_t *fw = (su_fs_watcher_t *)handle;
    if (!fw->events) {
        coco_create(su_fs_watch_co, (void *)fw, SU_COCO_STACK);
    }
    fw->events |= events;
}

SU_METHOD(fs_watcher, watch) {
    SU_SET_SELF;
    zend_string *path;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sf", &path, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::watch($path, $callback) param", SU_STATIC_CLASS());
    }

    su_fs_watcher_t *fw = SU_OBJ_FROM_SELF(su_fs_watcher_t);
    zend_string_addref(path);
    fw->path = path;
    su_set_cb(fw->cbs, EV_FW_CHANGE, &cb);
    int status = uv_fs_event_start(&fw->handle, su_fs_watch_cb, ZSTR_VAL(path), 0);
    if (status == SUCCESS) {
        fw->status = SU_FS_WATCHER_STARTED;
    }
    RETURN_BOOL(status == SUCCESS);
}

SU_METHOD(fs_watcher, close) {
    SU_SET_SELF;
    su_fs_watcher_t *fw = SU_OBJ_FROM_SELF(su_fs_watcher_t);
    su_fs_watcher_close(fw);
}

/* }}} */

/* {{{ uv_fs_methods */
zend_function_entry su_fs_watcher_methods[] = {
    SU_ME(fs_watcher, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(fs_watcher, __destruct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    SU_ME(fs_watcher, watch, NULL, ZEND_ACC_PUBLIC)
    SU_ME(fs_watcher, close, NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(fs_watcher) {
    SU_CE_INIT(fs_watcher, "su\\fs\\watcher");
    REGISTER_LONG_CONSTANT("SU\\FS\\EVENT\\RENAME", UV_RENAME, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SU\\FS\\EVENT\\CHANGE", UV_CHANGE, CONST_PERSISTENT);

    return SUCCESS;
}
/* }}} */
