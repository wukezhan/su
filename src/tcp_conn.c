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

#include <sys/socket.h>

#include "php.h"
#include "php_ini.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_smart_str.h"

#include "php_su.h"
#include "src/co.h"

#include "src/process.h"
#include "src/rbuf/rbuf.h"
#include "src/tcp_conn.h"

SU_CLASS_D(tcp_conn);

void su_tcp_conn_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void su_tcp_conn_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

#define su_tcp_read_start(conn) do {                        \
    if (conn->read_bufs.cap == 0) {                         \
        su_rbuf_new(&conn->read_bufs, conn->read_buf_num);  \
    }                                                       \
    uv_read_start((uv_stream_t *)&conn->handle, su_tcp_conn_alloc_cb, su_tcp_conn_read_cb);\
}while(0)

static zend_object* su_tcp_conn_create(zend_class_entry *ce) {
    su_tcp_conn_t *conn = (su_tcp_conn_t *)su_ecalloc(su_tcp_conn_t);
    SU_OBJ_INIT_STD(conn, ce, su_tcp_conn_handlers);
    /* here can be inited in __construct */
    uv_tcp_init(SU_G(loop), &conn->handle);
    conn->handle.data = (zval *)su_malloc(sizeof(zval));

    return &conn->std;
}

void su_tcp_conn_try_free(su_tcp_conn_t *conn) {
    /** 释放事件 **/
    if (conn->status == SU_OBJ_FREEED) {
        return;
    }
    conn->status = SU_OBJ_FREEED;
    SU_TRY_FREE_CBS(conn->cbs);
    zval *self = (zval *)conn->handle.data;
    if (self) {
        /** 释放关联对象 **/
        zval_ptr_dtor(self);
        su_free(self);
        conn->handle.data = NULL;
    }
    if (conn->req) {
        /** 释放请求句柄 **/
        su_free(conn->req);
    }
    /** clean the write queue **/
    zend_long h;
    zend_string *key;
    su_write_req_t *wr;
    ZEND_HASH_FOREACH_KEY_PTR(&conn->wrs, h, key, wr) {
        zend_string *buf = (zend_string *)wr->req.data;
        zend_string_release(buf);
        efree(wr);
    } ZEND_HASH_FOREACH_END();
    zend_hash_destroy(&conn->wrs);
}

//tcp close callback
void su_tcp_conn_close_cb(uv_handle_t *handle) {
    su_tcp_conn_t *conn = (su_tcp_conn_t *) handle;
    SU_TRACE("CONN CLOSE %p->%d", conn, conn->status);
    /* 所有与 uv_tcp_t 相关的工作在此处做 */
    conn->status = SU_OBJ_CLOSED;
    SU_TRACE("CONN CLOSED %p->%d", conn, conn->status);
    su_tcp_conn_try_free(conn);
}

void su_tcp_conn_free(zend_object *std) {
    /* 真正与 su_tcp_conn_t 相关的工作在此处做 */
    su_tcp_conn_t *conn = SU_OBJ_FROM_STD(std, su_tcp_conn_t);
    SU_TRACE("CONN FREE %p %d", conn, conn->status);
    if (conn->read_bufs.cap) {
        su_rbuf_free(&conn->read_bufs);
    }
    su_tcp_conn_try_free(conn);
    SU_OBJ_FREE_STD(conn);
    SU_TRACE("fd %d", conn->handle.io_watcher.fd);
    su_free(conn);
}

void su_tcp_conn_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    su_tcp_conn_t *conn = (su_tcp_conn_t *) handle;
    buf->base = su_malloc(conn->read_buf_size);
    buf->len = conn->read_buf_size;
}

void su_tcp_conn_read_co(void *arg) {
    su_tcp_conn_t *conn = (su_tcp_conn_t *) arg;
    uv_buf_t *buf;
    while (buf = (uv_buf_t *)su_rbuf_read(&conn->read_bufs)) {
        SU_TRACE("BUF POP %p", buf);
        zval zbuf;
        ZVAL_STRINGL(&zbuf, buf->base, buf->len);
        zval params[1] = { zbuf };
        zval retval;
        su_call_cb(conn->cbs[EV_TCP_CONN_DATA], &retval, 1, params);
        zval_ptr_dtor(&retval);
        zval_ptr_dtor(&zbuf);
        su_free(buf->base);
        su_free(buf);
    }
    if (conn->read_status & SU_TCP_CONN_READ_PAUSE) {
        /* only restart if SU_TCP_CONN_READ_PAUSE */
        if (!(conn->read_status & SU_TCP_CONN_READ_USER_PAUSE)) {
            su_tcp_read_start(conn);
        }
        conn->read_status &= ~SU_TCP_CONN_READ_PAUSE;
    }
    conn->read_status &= ~SU_TCP_CONN_READING;
    SU_OBJ_CHECK_CLOSE(conn);
}

void su_tcp_conn_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    su_tcp_conn_t *conn = (su_tcp_conn_t *) stream;
    if (nread <= 0) {
        if (nread == UV_EOF) {
            SU_OBJ_CLOSE(conn, su_tcp_conn_close_cb);
        } else if(nread == 0) {
            //
        } else {
            SU_OBJ_CLOSE(conn, su_tcp_conn_close_cb);
        }
        su_free(buf->base);
    } else {
        uv_buf_t *b = (uv_buf_t *)su_malloc(sizeof(uv_buf_t));
        b->base = buf->base;
        b->len = nread;
        SU_TRACE("BUF PUSH %p", b);
        su_rbuf_push(&conn->read_bufs, (void *)&b);
        if (su_rbuf_is_full(&conn->read_bufs)) {
            conn->read_status |= SU_TCP_CONN_READ_PAUSE;
            if (!(conn->read_status & SU_TCP_CONN_READ_USER_PAUSE)) {
                uv_read_stop(stream);
            }
        }
        if (!(conn->read_status & SU_TCP_CONN_READING)) {
            conn->read_status |= SU_TCP_CONN_READING;
            coco_create(su_tcp_conn_read_co, (void *)conn, SU_COCO_STACK);
        }
    }
}

SU_METHOD(tcp_conn, __construct) {
    SU_SET_SELF;
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);
    ZVAL_COPY(conn->handle.data, self);
    /* here can be set by user for later */
    conn->read_buf_num = 2;
    conn->read_buf_size = 65536;
    zend_hash_init(&conn->wrs, 8, NULL, NULL, 0);
    SU_RET_SELF;
}

SU_METHOD(tcp_conn, __destruct) {
    SU_SET_SELF;
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);
    SU_OBJ_CLOSE(conn, su_tcp_conn_close_cb);
}

static void su_tcp_conn_connect_cb(uv_connect_t* req, int status) {
    su_tcp_conn_t *conn = (su_tcp_conn_t *) req->data;
    if (status == 0) {
        conn->status = SU_OBJ_WORKING;
        su_tcp_read_start(conn);
        coco_create(su_call_co_cb, (void *)conn->cbs[EV_TCP_CONN_CONNECT], SU_COCO_STACK);
    } else {
        SU_OBJ_TRY_ERR(conn, status);
    }
}

SU_METHOD(tcp_conn, connect) {
    SU_SET_SELF;
    zval *host;
    zend_long port;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "zl", &host, &port) == FAILURE) {
        php_error(E_ERROR, "connect param error");
        return ;
    }
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);
    conn->req = (uv_connect_t *) su_malloc(sizeof(uv_connect_t));
    conn->req->data = (void *)conn;
    struct sockaddr_in addr;
    uv_ip4_addr(Z_STRVAL_P(host), (int) port, &addr);
    uv_tcp_connect(
        conn->req,
        &conn->handle,
        (const struct sockaddr*) &addr,
        su_tcp_conn_connect_cb
    );
}

SU_METHOD(tcp_conn, on) {
    SU_SET_SELF;
    zend_string *event;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sf", &event, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::on($event, $callback) param", SU_STATIC_CLASS());
    }

    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);

    if (su_cmp("data", event)) {
        su_set_cb(conn->cbs, EV_TCP_CONN_DATA, &cb);
        if (conn->status == SU_OBJ_WORKING) {
            su_tcp_read_start(conn);
        }
    } else if(su_cmp("error", event)) {
        su_set_cb(conn->cbs, EV_TCP_CONN_ERROR, &cb);
    } else if(su_cmp("close", event)) {
        su_set_cb(conn->cbs, EV_TCP_CONN_CLOSE, &cb);
    } else if(su_cmp("connect", event)) {
        su_set_cb(conn->cbs, EV_TCP_CONN_CONNECT, &cb);
    } else if(su_cmp("end", event)) {
        su_set_cb(conn->cbs, EV_TCP_CONN_END, &cb);
    } else if(su_cmp("lookup", event)) {
        su_set_cb(conn->cbs, EV_TCP_CONN_LOOKUP, &cb);
    } else if(su_cmp("timeout", event)) {
        su_set_cb(conn->cbs, EV_TCP_CONN_TIMEOUT, &cb);
    } else {
        // throw error
    }
    SU_RET_SELF;
}

void su_tcp_conn_end_cb(uv_write_t* req, int status) {
    //if there is no data, then shutdown it!
    su_write_req_t *wr = (su_write_req_t *)req;
    uv_stream_t *stream = req->handle;
    zend_string *msg = (zend_string *)req->data;
    if (msg) {
        zend_string_release(msg);
    }
    if (QUEUE_EMPTY(&stream->write_queue)) {
        su_tcp_conn_t *conn = (su_tcp_conn_t *) stream;
        SU_OBJ_CHECK_CLOSE(conn);
        // uv_close((uv_handle_t *) stream, su_tcp_conn_close_cb);
    }
    free(wr);
}

void su_tcp_conn_write_cb(uv_write_t* req, int status) {
    su_write_req_t *wr = (su_write_req_t *)req;
    uv_stream_t *stream = req->handle;
    su_tcp_conn_t *conn = (su_tcp_conn_t *)stream;
    zend_string *msg = (zend_string *)req->data;
    if (msg) {
        zend_hash_index_del(&conn->wrs, msg->h);
        zend_string_release(msg);
    }
    zend_string *buf = (zend_string *)wr->req.data;
    efree(wr);
    SU_OBJ_CHECK_CLOSE(conn);
}

SU_METHOD(tcp_conn, write) {
    SU_SET_SELF;
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);
    if (conn->status != SU_OBJ_WORKING) {
        RETURN_FALSE;
    }
    //SU_INFO("WRITE CONN OBJ %p", self);
    zend_string *buf = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &buf) == FAILURE) {
        php_error(E_ERROR, "invalid params");
    }

    su_write_req_t *wr = (su_write_req_t *) emalloc(sizeof(su_write_req_t));
    zend_string_addref(buf);
    wr->req.data = (void *)buf;
    wr->buf = uv_buf_init(ZSTR_VAL(buf), ZSTR_LEN(buf));
    SU_TRACE("write %p", buf);
    int r = uv_write(&wr->req, (uv_stream_t *)&conn->handle, &wr->buf, 1, su_tcp_conn_write_cb);
    zend_hash_index_add_ptr(&conn->wrs, zend_string_hash_val(buf), (void *)wr);
    SU_RET_SELF;
}

SU_METHOD(tcp_conn, end) {
    SU_SET_SELF;
    zend_string *buf = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &buf) == FAILURE) {
        php_error(E_ERROR, "invalid params");
    }
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);

    su_write_req_t *wr = (su_write_req_t *) malloc(sizeof(su_write_req_t));
    wr->buf = uv_buf_init(ZSTR_VAL(buf), ZSTR_LEN(buf));
    int r = uv_write(&wr->req, (uv_stream_t *)&conn->handle, &wr->buf, 1, su_tcp_conn_end_cb);
    //here should have a judge, to decide if the conn need to be closed
    SU_RET_SELF;
}

SU_METHOD(tcp_conn, pause) {
    SU_SET_SELF;
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);
    if (!(conn->read_status & (SU_TCP_CONN_READ_PAUSE|SU_TCP_CONN_READ_USER_PAUSE))) {
        uv_read_stop((uv_stream_t *)&conn->handle);
    }
    conn->read_status |= SU_TCP_CONN_READ_USER_PAUSE;
    SU_RET_SELF;
}

SU_METHOD(tcp_conn, resume) {
    SU_SET_SELF;
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);
    if ((conn->read_status & SU_TCP_CONN_READ_USER_PAUSE) 
        && !(conn->read_status & SU_TCP_CONN_READ_PAUSE)) {
        su_tcp_read_start(conn);
    }
    conn->read_status &= ~SU_TCP_CONN_READ_USER_PAUSE;
    SU_RET_SELF;
}

SU_METHOD(tcp_conn, ref) {
    SU_SET_SELF;
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);
    uv_ref((uv_handle_t *)&conn->handle);
    SU_INFO("refed");
}

SU_METHOD(tcp_conn, unref) {
    SU_SET_SELF;
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);
    uv_unref((uv_handle_t *)&conn->handle);
    SU_INFO("unrefed");
}

SU_METHOD(tcp_conn, close) {
    SU_SET_SELF;
    su_tcp_conn_t *conn = SU_OBJ_FROM_SELF(su_tcp_conn_t);
    SU_OBJ_CLOSE(conn, su_tcp_conn_close_cb);
}
/* }}} */

/* {{{ su_tcp_conn_methods */
zend_function_entry su_tcp_conn_methods[] = {
    SU_ME(tcp_conn, __construct, NULL,  ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(tcp_conn, __destruct, NULL,  ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    //SU_ME(tcp_conn, set_keepalive, NULL, ZEND_ACC_PUBLIC)
    //SU_ME(tcp_conn, set_nodelay, NULL, ZEND_ACC_PUBLIC)
    //SU_ME(tcp_conn, set_timeout, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_conn, connect, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_conn, on, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_conn, write, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_conn, end, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_conn, pause, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_conn, resume, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_conn, close, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_conn, ref, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_conn, unref, NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(tcp_conn) {
    SU_CE_INIT(tcp_conn, "su\\tcp\\conn");
    SU_CE_ALIAS(tcp_conn, "su\\tcp\\connection");
    return SUCCESS;
}
/* }}} */
