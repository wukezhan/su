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
#include "src/udp.h"

SU_CLASS_D(udp);

void su_udp_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void su_udp_recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr* addr, unsigned flags);

#define su_udp_recv_start(uh) do {                                  \
    if (uh->recv_bufs.cap == 0) {                                   \
        su_rbuf_new(&uh->recv_bufs, uh->recv_buf_num);              \
    }                                                               \
    uv_udp_recv_start(&uh->handle, su_udp_alloc_cb, su_udp_recv_cb);\
}while(0)

#define SU_UDP_OBJ_CLOSE(obj, cb) do {                                                  \
    if (obj->status < SU_OBJ_PENDING_CLOSE && (obj->recv_status || obj->handle.send_queue_count)) {   \
        obj->status = SU_OBJ_PENDING_CLOSE;                                         \
        obj->close_cb = cb;                                                         \
    } else if (obj->status < SU_OBJ_CLOSING && !obj->recv_status && !obj->handle.send_queue_count) {    \
        uv_close((uv_handle_t *)obj, cb);                                           \
        obj->status = SU_OBJ_CLOSING;                                               \
    }                                                                               \
} while(0)

#define SU_UDP_OBJ_CHECK_CLOSE(obj) do {                                                \
    if (obj->status == SU_OBJ_PENDING_CLOSE && !obj->recv_status && !obj->handle.send_queue_count) { \
        uv_close((uv_handle_t *)obj, obj->close_cb);                                \
        obj->status = SU_OBJ_CLOSING;                                               \
    }                                                                               \
} while(0)

static zend_object* su_udp_create(zend_class_entry *ce) {
    su_udp_t *uh = (su_udp_t *)su_ecalloc(su_udp_t);
    SU_OBJ_INIT_STD(uh, ce, su_udp_handlers);
    uv_udp_init(SU_G(loop), &uh->handle);
    uh->handle.data = (zval *)su_malloc(sizeof(zval));

    return &uh->std;
}

void su_udp_try_free(su_udp_t *uh) {
    if (uh->status == SU_OBJ_FREEED) {
        return;
    }
    uh->status = SU_OBJ_FREEED;
    SU_TRY_FREE_CBS(uh->cbs);
    zval *self = (zval *)uh->handle.data;
    if (self) {
        /* 释放关联对象 */
        zval_ptr_dtor(self);
        su_free(self);
        uh->handle.data = NULL;
    }
}

//udp close callback
void su_udp_close_cb(uv_handle_t *handle) {
    su_udp_t *uh = (su_udp_t *) handle;
    uh->status = SU_OBJ_CLOSED;
    /* 释放事件 */
    su_udp_try_free(uh);
}

void su_udp_free(zend_object *std) {
    /* 真正与 su_udp_t 相关的工作在此处做 */
    su_udp_t *uh = SU_OBJ_FROM_STD(std, su_udp_t);
    SU_INFO("CONN %p %d", uh, uh->status);
    if (uh->recv_bufs.cap) {
        su_rbuf_free(&uh->recv_bufs);
    }
    su_udp_try_free(uh);
    SU_OBJ_FREE_STD(uh);
    su_free(uh);
}

void su_udp_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    su_udp_t *uh = (su_udp_t *) handle;
    buf->base = su_malloc(uh->recv_buf_size);
    buf->len = uh->recv_buf_size;
}

void su_udp_recv_co(void *arg) {
    su_udp_t *uh = (su_udp_t *) arg;
    su_udp_msg_t *msg;
    char ip[INET6_ADDRSTRLEN]; // INET6_ADDRSTRLEN need to be defined
    struct sockaddr_in *a4;
    struct sockaddr_in6 *a6;
    int port;
    while (msg = (su_udp_msg_t *)su_rbuf_read(&uh->recv_bufs)) {
        zval zbuf;
        ZVAL_STRINGL(&zbuf, msg->buf.base, msg->buf.len);
        zval zaddr;
        if (msg->addr.sa_family == AF_INET) {
            a4 = (struct sockaddr_in*)&msg->addr;
            uv_inet_ntop(AF_INET, &a4->sin_addr, ip, sizeof ip);
            port = ntohs(a4->sin_port);
        } else {
            a6 = (struct sockaddr_in6*)&msg->addr;
            uv_inet_ntop(AF_INET6, &a6->sin6_addr, ip, sizeof ip);
            port = ntohs(a6->sin6_port);
        }
        ZVAL_STRING(&zaddr, ip);
        zval zport;
        ZVAL_LONG(&zport, port);
        zval params[3] = { zbuf, zaddr, zport };
        zval retval;
        su_call_cb(uh->cbs[EV_UDP_RECV], &retval, 3, params);
        zval_ptr_dtor(&retval);
        zval_ptr_dtor(&zbuf);
        zval_ptr_dtor(&zaddr);
        zval_ptr_dtor(&zport);
        su_free(msg->buf.base);
        su_free(msg);
    }
    if (uh->recv_status & SU_UDP_RECV_PAUSE) {
        /* only restart if SU_UDP_RECV_PAUSE */
        if (!(uh->recv_status & SU_UDP_RECV_USER_PAUSE)) {
            su_udp_recv_start(uh);
        }
        uh->recv_status &= ~SU_UDP_RECV_PAUSE;
    }
    uh->recv_status &= ~SU_UDP_RECVING;
    SU_UDP_OBJ_CHECK_CLOSE(uh);
}

void su_udp_recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr* addr, unsigned flags) {
    su_udp_t *uh = (su_udp_t *) handle;
    if (buf->len == 0) {
        SU_OUT("nread %d", nread);
    }
    if (nread <= 0) {
        if (nread == UV_EOF) {
            uh->state = -1;
            SU_UDP_OBJ_CLOSE(uh , su_udp_close_cb);
        } else if(nread == 0) {
            //
        }else {
            SU_UDP_OBJ_CLOSE(uh , su_udp_close_cb);
        }
        su_free(buf->base);
    } else {
        su_udp_msg_t *msg = (su_udp_msg_t *)su_malloc(sizeof(su_udp_msg_t));
        msg->buf.base = buf->base;
        msg->buf.len = nread;
        memcpy(&msg->addr, addr, sizeof(struct sockaddr));
        su_rbuf_push(&uh->recv_bufs, (void *)&msg);
        if (su_rbuf_is_full(&uh->recv_bufs)) {
            uh->recv_status |= SU_UDP_RECV_PAUSE;
            if (!(uh->recv_status & SU_UDP_RECV_USER_PAUSE)) {
                uv_udp_recv_stop(handle);
            }
        }
        if (!(uh->recv_status & SU_UDP_RECVING)) {
            uh->recv_status |= SU_UDP_RECVING;
            coco_create(su_udp_recv_co, (void *)uh, SU_COCO_STACK);
        }
    }
}

SU_METHOD(udp, __construct) {
    SU_SET_SELF;
    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);
    ZVAL_COPY(uh->handle.data, self);
    /* here can be set by user for later */
    uh->recv_buf_num = 2;
    uh->recv_buf_size = 65536;
    SU_RET_SELF;
}

SU_METHOD(udp, __destruct) {
    SU_INFO("su_udp class dtor");
    SU_SET_SELF;
    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);
    // NEED REFACTOR
    if (uh->status != -1) {
        uh->status = -1;
        SU_INFO("su_udp class dtor");
        uv_close((uv_handle_t *) uh, su_udp_close_cb);
    }
    //SU_INFO("__destruct ref %d\n", Z_REFCOUNT_P(&(uh->cbs[EV_UDP_CONNECT])->fci.function_name));
}

SU_METHOD(udp, bind) {
    SU_SET_SELF;
    zend_long port = 0;
    zend_string *ip = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|S", &port, &ip) == FAILURE) {
        php_error(E_ERROR, "invalid %s::bind($port, $ip=\"127.0.0.1\") param", SU_STATIC_CLASS());
    }
    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);
    struct sockaddr_in addr;
    //int r = uv_ip4_addr(ip?ZSTR_VAL(ip):"127.0.0.1", port, &addr);
    int r = uv_ip4_addr("0.0.0.0", port, &addr);

    unsigned int flags = UV_UDP_REUSEADDR;
    r = uv_udp_bind(&uh->handle, (const struct sockaddr*) &addr, 0);
    if (0 == r) {
        uh->state = 1;
        uh->status = SU_OBJ_WORKING;
        // 此处需进一步改造
        su_udp_recv_start(uh);
    } else {
        SU_OBJ_TRY_ERR(uh, r);
    }
}

SU_METHOD(udp, on) {
    SU_SET_SELF;
    zend_string *event;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sf", &event, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::after($time, $callback) param", SU_STATIC_CLASS());
    }
    SU_INFO("udp::on(%s)\n", ZSTR_VAL(event));

    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);

    if (su_cmp("message", event)) {
        su_set_cb(uh->cbs, EV_UDP_RECV, &cb);
        if (uh->state == 1) {
            su_udp_recv_start(uh);
        }
    } else if(su_cmp("error", event)) {
        su_set_cb(uh->cbs, EV_UDP_ERROR, &cb);
    } else if(su_cmp("close", event)) {
        su_set_cb(uh->cbs, EV_UDP_CLOSE, &cb);
    } else if(su_cmp("connect", event)) {
        su_set_cb(uh->cbs, EV_UDP_CONNECT, &cb);
    } else if(su_cmp("end", event)) {
        su_set_cb(uh->cbs, EV_UDP_END, &cb);
    } else if(su_cmp("lookup", event)) {
        su_set_cb(uh->cbs, EV_UDP_LOOKUP, &cb);
    } else if(su_cmp("timeout", event)) {
        su_set_cb(uh->cbs, EV_UDP_TIMEOUT, &cb);
    } else {
        // throw error
    }
    SU_RET_SELF;
}

void su_udp_send_cb(uv_udp_send_t* req, int status) {
    su_udp_t *uh = (su_udp_t *)req->handle;
    su_udp_send_t *sus = (su_udp_send_t *)req;
    zend_string *msg = (zend_string *)req->data;
    if (msg) {
        zend_string_release(msg);
    }
    efree(sus);
    if (status == 0) {
        if (uh->status < SU_OBJ_WORKING) {
            uh->status = SU_OBJ_WORKING;
        }
        // comment this tmply for NULL callback
        //su_udp_recv_start(uh);
    }
    SU_UDP_OBJ_CHECK_CLOSE(uh);
}

SU_METHOD(udp, send) {
    SU_SET_SELF;
    //SU_INFO("WRITE CONN OBJ %p", self);
    zend_string *buf = NULL;
    zend_long port;
    zend_string *ip = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sl|S", &buf, &port, &ip) == FAILURE) {
        php_error(E_ERROR, "invalid params");
    }

    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);
    zend_string_addref(buf);
    su_udp_send_t *req = (su_udp_send_t *)su_malloc(sizeof(su_udp_send_t));
    req->send.data = (void *)buf;
    uv_buf_t ubuf = uv_buf_init(ZSTR_VAL(buf), ZSTR_LEN(buf));
    uv_ip4_addr(ip?ZSTR_VAL(ip):"127.0.0.1", port, &req->addr.a4);
    int r = uv_udp_send(&req->send, &uh->handle, &ubuf, 1, (const struct sockaddr*)&req->addr, su_udp_send_cb);

    SU_RET_SELF;
}

SU_METHOD(udp, set_membership) {
    SU_SET_SELF;
    zend_string *mc_addr = NULL;
    zend_string *if_addr = NULL;
    zend_long action = UV_JOIN_GROUP;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lS|S", &action, &mc_addr, &if_addr) == FAILURE) {
        php_error(E_ERROR, "invalid params");
        return;
    }
    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);

    int r = uv_udp_set_membership(
        &uh->handle,
        ZSTR_VAL(mc_addr),
        if_addr?ZSTR_VAL(if_addr): NULL,
        action
    );

    SU_UVE("ERROR", r);

    SU_RET_SELF;
}


SU_METHOD(udp, pause) {
    SU_SET_SELF;
    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);
    if (!(uh->recv_status & (SU_UDP_RECV_PAUSE|SU_UDP_RECV_USER_PAUSE))) {
        uv_udp_recv_stop(&uh->handle);
    }
    uh->recv_status |= SU_UDP_RECV_USER_PAUSE;
    SU_RET_SELF;
}

SU_METHOD(udp, resume) {
    SU_SET_SELF;
    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);
    if ((uh->recv_status & SU_UDP_RECV_USER_PAUSE) 
        && !(uh->recv_status & SU_UDP_RECV_PAUSE)) {
        su_udp_recv_start(uh);
    }
    uh->recv_status &= ~SU_UDP_RECV_USER_PAUSE;
    SU_RET_SELF;
}


SU_METHOD(udp, ref) {
    SU_SET_SELF;
    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);
    uv_ref((uv_handle_t *)&uh->handle);
    SU_INFO("refed");
}

SU_METHOD(udp, unref) {
    SU_SET_SELF;
    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);
    uv_unref((uv_handle_t *)&uh->handle);
    SU_INFO("unrefed");
}

SU_METHOD(udp, close) {
    SU_SET_SELF;
    su_udp_t *uh = SU_OBJ_FROM_SELF(su_udp_t);
    SU_UDP_OBJ_CLOSE(uh, su_udp_close_cb);
    //SU_INFO("close ref %d\n", Z_REFCOUNT_P(&(uh->cbs[EV_UDP_CONNECT])->fci.function_name));
}
/* }}} */

/* {{{ su_udp_methods */
zend_function_entry su_udp_methods[] = {
    SU_ME(udp, __construct, NULL,  ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(udp, __destruct, NULL,  ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    SU_ME(udp, bind, NULL, ZEND_ACC_PUBLIC)
    SU_ME(udp, on, NULL, ZEND_ACC_PUBLIC)
    SU_ME(udp, send, NULL, ZEND_ACC_PUBLIC)
    SU_ME(udp, set_membership, NULL, ZEND_ACC_PUBLIC)
    //SU_ME(udp, drop_membership, NULL, ZEND_ACC_PUBLIC)
    //SU_ME(udp, set_broadcast, NULL, ZEND_ACC_PUBLIC)
    SU_ME(udp, pause, NULL, ZEND_ACC_PUBLIC)
    SU_ME(udp, resume, NULL, ZEND_ACC_PUBLIC)
    SU_ME(udp, close, NULL, ZEND_ACC_PUBLIC)
    SU_ME(udp, ref, NULL, ZEND_ACC_PUBLIC)
    SU_ME(udp, unref, NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(udp) {
    SU_CE_INIT(udp, "su\\udp");
    REGISTER_LONG_CONSTANT("SU\\UDP\\JOIN_GROUP", UV_JOIN_GROUP, CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SU\\UDP\\DROP_GROUP", UV_LEAVE_GROUP, CONST_PERSISTENT);
    return SUCCESS;
}
/* }}} */
