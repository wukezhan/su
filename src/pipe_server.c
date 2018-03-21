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
#include "src/co.h"

#include "src/pipe.h"
#include "src/pipe_server.h"

SU_CLASS_D(pipe_server);

static zend_object* su_pipe_server_create(zend_class_entry *ce) {
    su_pipe_server_t *pipe_server = (su_pipe_server_t *)su_ecalloc(su_pipe_server_t);
    SU_OBJ_INIT_STD(pipe_server, ce, su_pipe_server_handlers);
    uv_pipe_init(SU_G(loop), &pipe_server->handle, 0);
    pipe_server->handle.data = (zval *)su_malloc(sizeof(zval));
    pipe_server->bind = NULL;

    return &pipe_server->std;
}

void su_pipe_server_free(zend_object *std) {
    su_pipe_server_t *pipe_server = SU_OBJ_FROM_STD(std, su_pipe_server_t);
    SU_OBJ_FREE_STD(pipe_server);
    zval *self = (zval *)pipe_server->handle.data;
    if (self) {
        zval_ptr_dtor(self);
        su_free(self);
        pipe_server->handle.data = NULL;
    }
    SU_TRY_FREE_CBS(pipe_server->cbs);
    su_free(pipe_server);
}

void su_pipe_server_close_cb(uv_handle_t *handle) {
    su_pipe_server_t *pipe_server = (su_pipe_server_t *) handle;
    if (pipe_server->status == SU_PIPE_STOPED) {
        return;
    }
    pipe_server->status = SU_PIPE_STOPED;
    if (pipe_server->bind) {
        uv_fs_t req;
        uv_fs_unlink(SU_G(loop), &req, pipe_server->bind, NULL);
    }
    SU_TRY_FREE_CBS(pipe_server->cbs);
    zval *self = (zval *)pipe_server->handle.data;
    if (self) {
        zval_ptr_dtor(self);
        su_free(self);
        pipe_server->handle.data = NULL;
    }
}

static void su_pipe_server_conn_co(void* arg) {
    uv_stream_t *server = (uv_stream_t *)arg;
    zval zp;
    object_init_ex(&zp, su_pipe_ce);
    SU_CALL_OM_ARR(su_pipe_ce, &zp, "__construct", NULL, 0, NULL);
    su_pipe_t *pipe = SU_OBJ_FROM_STD(Z_OBJ_P(&zp), su_pipe_t);
    int r = uv_accept(server, (uv_stream_t *)&pipe->handle);
    if (r == SUCCESS) {
        pipe->status = SU_OBJ_WORKING;
        su_pipe_server_t *sps = (su_pipe_server_t *)server;
        zval retval;
        su_call_cb(sps->cbs[EV_PIPE_SERVER_CONN], &retval, 1, &zp);
        zval_ptr_dtor(&retval);
    }
}

static void su_pipe_server_conn_cb(uv_stream_t* server, int status) {
    if (status == 0) {
        coco_create(su_pipe_server_conn_co, (void *)server, SU_COCO_STACK);
    } else {
        su_pipe_server_t *sps = (su_pipe_server_t *)server;
        SU_OBJ_TRY_ERR(sps, status);
    }
}

/* {{{ ARG_INFO */
#if 0
SU_BEGIN_ARG_INFO_EX(pipe_construct_arginfo, 0, 0, 0)
    SU_ARG_INFO(0, config)
SU_END_ARG_INFO()
#endif
/* }}} */

/* {{{ PHP METHODS */
SU_METHOD(pipe_server, __construct) {
    SU_SET_SELF;
    zend_long ipc = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &ipc) == FAILURE) {
        php_error(E_ERROR, "invalid %s::__construct($ipc) param", SU_STATIC_CLASS());
    }
    su_pipe_server_t *pipe_server = SU_OBJ_FROM_SELF(su_pipe_server_t);
    ZVAL_COPY(pipe_server->handle.data, self);
    Z_TRY_ADDREF_P(self);
    zend_hash_next_index_insert(&SU_G(servers), self);
    SU_RET_SELF;
}

SU_METHOD(pipe_server, on) {
    SU_SET_SELF;
    zend_string *event;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sf", &event, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::after($time, $callback) param", SU_STATIC_CLASS());
    }

    su_pipe_server_t *pipe_server = SU_OBJ_FROM_SELF(su_pipe_server_t);

    if (su_cmp("connection", event)) {
        su_set_cb(pipe_server->cbs, EV_PIPE_SERVER_CONN, &cb);
        su_pipe_server_try_start(self);
    } else if(su_cmp("error", event)) {
        su_set_cb(pipe_server->cbs, EV_PIPE_SERVER_ERROR, &cb);
        SU_OBJ_TRY_ERR(pipe_server, 0);
    } else if(su_cmp("close", event)) {
        su_set_cb(pipe_server->cbs, EV_PIPE_SERVER_CLOSE, &cb);
    } else if(su_cmp("pause", event)) {
        su_set_cb(pipe_server->cbs, EV_PIPE_SERVER_PAUSE, &cb);
    } else if(su_cmp("resume", event)) {
        su_set_cb(pipe_server->cbs, EV_PIPE_SERVER_RESUME, &cb);
    } else {
        // throw error
    }
    SU_RET_SELF;
}

int su_pipe_server_try_start(zval *self) {
    su_pipe_server_t *pipe_server = SU_OBJ_FROM_SELF(su_pipe_server_t);
    zval *bind = zend_read_property(su_pipe_server_ce, self, ZEND_STRL("_bind"), 1, NULL);
    if (!Z_ISNULL_P(bind) && pipe_server->cbs[EV_PIPE_SERVER_CONN]) {
        int r = uv_pipe_bind(&pipe_server->handle, Z_STRVAL_P(bind));
        if (r == SUCCESS) {
            pipe_server->bind = Z_STRVAL_P(bind);
            r = uv_listen((uv_stream_t *)&pipe_server->handle, 128, su_pipe_server_conn_cb);
        } else {
            SU_OBJ_TRY_ERR(pipe_server, r);
        }
    }
}

SU_METHOD(pipe_server, listen) {
    SU_SET_SELF;
    zend_string *pipe_server_name = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &pipe_server_name) == FAILURE) {
        php_error(E_ERROR, "invalid params");
    }
    zend_update_property_stringl(su_pipe_server_ce, self, ZEND_STRL("_bind"), 
                                    ZSTR_VAL(pipe_server_name), ZSTR_LEN(pipe_server_name));
    su_pipe_server_try_start(self);
    SU_RET_SELF;
}

SU_METHOD(pipe_server, pause) {
}

SU_METHOD(pipe_server, resume) {
}

SU_METHOD(pipe_server, close) {
    SU_SET_SELF;
    su_pipe_server_t *pipe_server = SU_OBJ_FROM_SELF(su_pipe_server_t);
    SU_OBJ_CLOSE(pipe_server, su_pipe_server_close_cb);
    SU_RET_SELF;
}

SU_METHOD(pipe_server, __destruct) {
    SU_SET_SELF;
    su_pipe_server_t *pipe_server = SU_OBJ_FROM_SELF(su_pipe_server_t);
    SU_OBJ_CLOSE(pipe_server, su_pipe_server_close_cb);
    SU_RET_SELF;
}

/* }}} */

/* {{{ uv_pipe_server_methods */
zend_function_entry su_pipe_server_methods[] = {
    SU_ME(pipe_server, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(pipe_server, on, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe_server, listen, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe_server, pause, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe_server, resume, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe_server, close, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe_server, __destruct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(pipe_server) {
    SU_CE_INIT(pipe_server, "su\\pipe\\server");

    zend_declare_property_null(su_pipe_server_ce, ZEND_STRL("_bind"), ZEND_ACC_PROTECTED);
    return SUCCESS;
}
/* }}} */
