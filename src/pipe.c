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

SU_CLASS_D(pipe);

static zend_object* su_pipe_create(zend_class_entry *ce) {
    su_pipe_t *pipe = (su_pipe_t *)su_ecalloc(su_pipe_t);
    SU_OBJ_INIT_STD(pipe, ce, su_pipe_handlers);
    pipe->handle.data = (zval *)su_malloc(sizeof(zval));

    return &pipe->std;
}

void su_pipe_free(zend_object *std) {
    su_pipe_t *pipe = SU_OBJ_FROM_STD(std, su_pipe_t);
    SU_OBJ_FREE_STD(pipe);
    zval *self = (zval *)pipe->handle.data;
    if (self) {
        zval_ptr_dtor(self);
        su_free(self);
        pipe->handle.data = NULL;
    }
    if (pipe->read_bufs.cap) {
        su_rbuf_free(&pipe->read_bufs);
    }
    su_free(pipe);
}

void su_pipe_close_cb(uv_handle_t *handle) {
    su_pipe_t *pipe = (su_pipe_t *) handle;
    if (pipe->status == SU_OBJ_CLOSED) {
        return ;
    }
    pipe->status = SU_OBJ_CLOSED;
    SU_TRY_FREE_CBS(pipe->cbs);
    zval *self = (zval *)pipe->handle.data;
    if (self) {
        zval_ptr_dtor(self);
        su_free(self);
        pipe->handle.data = NULL;
    }
    if (pipe->req) {
        su_free(pipe->req);
    }
}

void su_pipe_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    su_pipe_t *pipe = (su_pipe_t *) handle;
    buf->base = su_malloc(pipe->read_buf_size);
    buf->len = pipe->read_buf_size;
}

void su_pipe_read_co(void *arg) {
    su_pipe_t *pipe = (su_pipe_t *) arg;
    uv_buf_t *buf;
    while (buf = (uv_buf_t *)su_rbuf_read(&pipe->read_bufs)) {
        zval zbuf;
        ZVAL_STRINGL(&zbuf, buf->base, buf->len);
        zval params[1] = { zbuf };
        zval retval;
        su_call_cb(pipe->cbs[EV_PIPE_DATA], &retval, 1, params);
        zval_ptr_dtor(&retval);
        zval_ptr_dtor(&zbuf);
        su_free(buf->base);
        su_free(buf);
    }
    if (pipe->read_status & SU_PIPE_READ_PAUSE) {
        if (!(pipe->read_status & SU_PIPE_READ_USER_PAUSE)) {
            uv_read_start((uv_stream_t *)pipe, su_pipe_alloc_cb, su_pipe_read_cb);
        }
        pipe->read_status &= ~SU_PIPE_READ_PAUSE;
    }
    pipe->read_status &= ~SU_PIPE_READING;
    SU_OBJ_CHECK_CLOSE(pipe);
}

void su_pipe_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    su_pipe_t *pipe = (su_pipe_t *) stream;
    if (nread <= 0) {
        if (nread == UV_EOF) {
            SU_OBJ_CLOSE(pipe, su_pipe_close_cb);
        } else if(nread == 0) {
            //
        } else {
            SU_OBJ_CLOSE(pipe, su_pipe_close_cb);
        }
        su_free(buf->base);
    } else {
        uv_buf_t *b = (uv_buf_t *)su_malloc(sizeof(uv_buf_t));
        b->base = buf->base;
        b->len = nread;
        su_rbuf_push(&pipe->read_bufs, (void *)&b);
        if (su_rbuf_is_full(&pipe->read_bufs)) {
            pipe->read_status |= SU_PIPE_READ_PAUSE;
            if (!(pipe->read_status & SU_PIPE_READ_USER_PAUSE)) {
                uv_read_stop(stream);
            }
        }
        if (!(pipe->read_status & SU_PIPE_READING)) {
            pipe->read_status |= SU_PIPE_READING;
            coco_create(su_pipe_read_co, (void *)pipe, SU_COCO_STACK);
        }
    }
}

su_pipe_t *su_pipe_init(zval *self, int ipc) {
    su_pipe_t *pipe = SU_OBJ_FROM_SELF(su_pipe_t);
    uv_pipe_init(SU_G(loop), &pipe->handle, ipc);
    ZVAL_COPY(pipe->handle.data, self);
    pipe->read_buf_num = 2;
    pipe->read_buf_size = 65536;

    return pipe;
}

/* {{{ ARG_INFO */
#if 0
SU_BEGIN_ARG_INFO_EX(pipe_construct_arginfo, 0, 0, 0)
    SU_ARG_INFO(0, config)
SU_END_ARG_INFO()
#endif
/* }}} */

/* {{{ PHP METHODS */
SU_METHOD(pipe, __construct) {
    SU_SET_SELF;
    zend_long ipc = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &ipc) == FAILURE) {
        php_error(E_ERROR, "invalid %s::__construct($ipc) param", SU_STATIC_CLASS());
    }
    su_pipe_init(self, ipc);
    SU_RET_SELF;
}

static void su_pipe_connect_cb(uv_connect_t* req, int status) {
    su_pipe_t *pipe = (su_pipe_t *) req->data;
    if (status == 0) {
        //pipe->state = 1;
        pipe->status = SU_OBJ_WORKING;
        su_pipe_try_start(pipe);
        coco_create(su_call_co_cb, (void *)pipe->cbs[EV_PIPE_CONNECT], SU_COCO_STACK);
    } else {
        // on error
        SU_OBJ_TRY_ERR(pipe, status);
    }
}

SU_METHOD(pipe, connect) {
    SU_SET_SELF;
    su_pipe_t *pipe = SU_OBJ_FROM_SELF(su_pipe_t);
    if (pipe->handle.ipc) {
        php_error(E_ERROR, "pipe::connect() can not work in ipc mode");
        return;
    }
    zend_string *pipe_server_name = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &pipe_server_name) == FAILURE) {
        php_error(E_ERROR, "invalid params");
        return;
    }

    pipe->req = (uv_connect_t *) su_malloc(sizeof(uv_connect_t));
    pipe->req->data = (void *)pipe;
    uv_pipe_connect(pipe->req, &pipe->handle, ZSTR_VAL(pipe_server_name), su_pipe_connect_cb);
}

SU_METHOD(pipe, open) {
    SU_SET_SELF;
    su_pipe_t *pipe = SU_OBJ_FROM_SELF(su_pipe_t);
    if (!pipe->handle.ipc) {
        php_error(E_ERROR, "pipe::open() can only work in ipc mode");
        return;
    }
    zend_long fd = -1;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &fd) == FAILURE) {
        php_error(E_ERROR, "invalid params");
        return;
    }

    int r = uv_pipe_open(&pipe->handle, fd);
    RETURN_BOOL(r == SUCCESS);
}

SU_METHOD(pipe, on) {
    SU_SET_SELF;
    zend_string *event;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sf", &event, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::after($time, $callback) param", SU_STATIC_CLASS());
    }

    su_pipe_t *pipe = SU_OBJ_FROM_SELF(su_pipe_t);

    if (su_cmp("data", event)) {
        su_set_cb(pipe->cbs, EV_PIPE_DATA, &cb);
        su_pipe_try_start(pipe);
    } else if(su_cmp("error", event)) {
        su_set_cb(pipe->cbs, EV_PIPE_ERROR, &cb);
    } else if(su_cmp("close", event)) {
        su_set_cb(pipe->cbs, EV_PIPE_CLOSE, &cb);
    } else if(su_cmp("end", event)) {
        su_set_cb(pipe->cbs, EV_PIPE_END, &cb);
    } else if(su_cmp("connect", event)) {
        su_set_cb(pipe->cbs, EV_PIPE_CONNECT, &cb);
    } else {
        // throw error
    }
    SU_RET_SELF;
}

SU_METHOD(pipe, end) {
    /* TODO */
}

void su_pipe_write_cb(uv_write_t* req, int status) {
    //SU_OUT("w status %d", status);
    //if there is no data, then shutdown it!
    su_write_req_t *wr = (su_write_req_t *)req;
    uv_stream_t *stream = req->handle;
    zend_string *msg = (zend_string *)req->data;
    if (msg) {
        zend_string_release(msg);
    }
    if (QUEUE_EMPTY(&stream->write_queue)) {
        //SU_UVE("write cb2", status);
        //uv_close((uv_handle_t *) stream, su_pipe_close_cb);
    }
    free(wr);
}

SU_METHOD(pipe, write) {
    SU_SET_SELF;
    zend_string *buf = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &buf) == FAILURE) {
        php_error(E_ERROR, "invalid params");
    }

    su_pipe_t *pipe = SU_OBJ_FROM_SELF(su_pipe_t);
    if (pipe->status != SU_OBJ_WORKING) {
        RETURN_FALSE;
    }

    su_write_req_t *wr = (su_write_req_t *) malloc(sizeof(su_write_req_t));
    zend_string_addref(buf);
    wr->req.data = (void *)buf;
    wr->buf = uv_buf_init(ZSTR_VAL(buf), ZSTR_LEN(buf));
    //SU_INFO("handle %p", pipe);
    int r = uv_write(&wr->req, (uv_stream_t *)&pipe->handle, &wr->buf, 1, su_pipe_write_cb);
    //SU_OUT("[r] %d %s", r, strerror(r));

    SU_RET_SELF;
}

SU_METHOD(pipe, close) {
    SU_SET_SELF;
    SU_OBJ_CLOSE(SU_OBJ_FROM_SELF(su_pipe_t), su_pipe_close_cb);
}

SU_METHOD(pipe, __destruct) {
    SU_SET_SELF;
    SU_OBJ_CLOSE(SU_OBJ_FROM_SELF(su_pipe_t), su_pipe_close_cb);
}

/* }}} */

/* {{{ uv_pipe_methods */
zend_function_entry su_pipe_methods[] = {
    SU_ME(pipe, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(pipe, connect, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe, on, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe, end, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe, write, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe, close, NULL, ZEND_ACC_PUBLIC)
    SU_ME(pipe, __destruct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(pipe) {
    SU_CE_INIT(pipe, "su\\pipe");

    zend_declare_property_long(su_pipe_ce, ZEND_STRL("_ipc"), 0, ZEND_ACC_PROTECTED);
    zend_declare_property_null(su_pipe_ce, ZEND_STRL("_bind"), ZEND_ACC_PROTECTED);
    return SUCCESS;
}
/* }}} */
