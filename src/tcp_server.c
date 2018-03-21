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
#include <stdlib.h>

#include "php.h"
#include "php_main.h"
#include "php_ini.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_smart_str.h"

#include "php_su.h"

#include "src/co.h"
#include "src/process.h"
#include "src/tcp_conn.h"
#include "src/tcp_server.h"

#define SU_CR 1
#define SU_RP 2
#define SU_RR 3

SU_CLASS_D(tcp_server);

static zend_object* su_tcp_server_create(zend_class_entry *ce) {
    su_tcp_server_t *server = (su_tcp_server_t *)su_ecalloc(su_tcp_server_t);
    SU_OBJ_INIT_STD(server, ce, su_tcp_server_handlers);
    /* here can be inited in __construct */
    //uv_tcp_init(SU_G(loop), &server->handle);
    server->handle.data = (zval *)su_malloc(sizeof(zval));

    return &server->std;
}

void su_tcp_server_close_cb(uv_handle_t *handle) {
    su_tcp_server_t *server = (su_tcp_server_t *) handle;
    zval *self = (zval *)server->handle.data;
    if (self) {
        /* 释放关联对象 */
        zval_ptr_dtor(self);
        su_free(self);
        server->handle.data = NULL;
    }
}

void su_tcp_server_pause_cb(uv_handle_t *handle) {
    su_tcp_server_t *server = (su_tcp_server_t *) handle;
    SU_TRACE("SERVER PAUSE %p", server);
    if (server->status & TCP_SERVER_PAUSE) {
        return;
    }
    server->status |= TCP_SERVER_PAUSE;
    /* pause the server, don't free the object */
}

void su_tcp_server_free(zend_object *std) {
    /* 真正与 su_tcp_server_t 相关的工作在此处做 */
    su_tcp_server_t *server = SU_OBJ_FROM_STD(std, su_tcp_server_t);
    if (!(server->status & TCP_SERVER_CLOSE)) {
        //server->status = TCP_SERVER_CLOSE;
        /** 异常错误导致走到此处的，先尝试关闭句柄 **/
        /** 此处应无法再走到 **/
        if (server->mode != SU_RR) {
            uv_close((uv_handle_t *)&server->handle, su_tcp_server_close_cb);
        }
    } else {
        /* 正常调用close关闭的，释放事件 */
        SU_TRY_FREE_CBS(server->cbs);
        SU_OBJ_FREE_STD(server);
        su_free(server);
    }
}

int su_tcp_init(su_tcp_server_t* sts)
{
    int status = uv_tcp_init_ex(SU_G(loop), &sts->handle, sts->addr_type);

#ifdef HAVE_REUSEPORT
    uv_os_fd_t fd = 0;
    status = uv_fileno((uv_handle_t*)(&sts->handle), &fd);
    if (status == 0) {
        int on = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
        if (sts->mode == SU_CR) {
            //当存在reuseport时，CR模式升级为RP模式
            SU_TRACE("CR -> RP");
            sts->mode = SU_RP;
        }
    }
#endif

    return 0;
}

int su_tcp_bind(su_tcp_server_t* sts, zval* options)
{
    sts->addr_type = AF_INET;
    zval* host = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("host"));
    zval* port = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("port"));
    int i = 0, status = 0;
    while (i < Z_STRLEN_P(host)) {
        if (Z_STRVAL_P(host)[i] == ':') {
            sts->addr_type = AF_INET6;
            break;
        }
        i++;
    }
    int iport = Z_TYPE_P(port) == IS_LONG ? Z_LVAL_P(port) : zval_get_long(port);
    uv_tcp_init(SU_G(loop), &sts->handle);
    if (sts->addr_type == AF_INET6) {
        uv_ip6_addr(Z_STRVAL_P(host), iport, &(sts->addr.a6));
        status = uv_tcp_bind(&sts->handle, (const struct sockaddr*)&sts->addr.a6, 0);
    } else {
        status = uv_ip4_addr(Z_STRVAL_P(host), iport, &(sts->addr.a4));
        status = uv_tcp_bind(&sts->handle, (const struct sockaddr*)&(sts->addr.a4), 0);
    }
    return status;
}

void su_simple_tcp_server_try_start(zval* server, zval* options)
{
    // do tcp bind
    // do simple tcp listen
    su_tcp_server_t* sts = SU_OBJ_FROM_STD(Z_OBJ_P(server), su_tcp_server_t);
    zval* host = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("host"));
    zval* port = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("port"));
    
    su_tcp_init(sts);
    int iport = Z_TYPE_P(port) == IS_LONG ? Z_LVAL_P(port) : zval_get_long(port);
    int status;
    if (sts->addr_type == AF_INET6) {
        uv_ip6_addr(Z_STRVAL_P(host), iport, &(sts->addr.a6));
        status = uv_tcp_bind(&sts->handle, (const struct sockaddr*)&sts->addr.a6, 0);
    } else {
        status = uv_ip4_addr(Z_STRVAL_P(host), iport, &(sts->addr.a4));
        status = uv_tcp_bind(&sts->handle, (const struct sockaddr*)&(sts->addr.a4), 0);
    }
    // listen
    sts->connection_cb = su_tcp_cr_server_conn_cb;
    uv_listen((uv_stream_t*)&sts->handle, 128, sts->connection_cb);
}

int su_master_tcp_server_try_start(zval* options, int type, uv_stream_t* chan)
{
    SU_TRACE("master fetch server");
    zval* skey = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("__key"));
    zval* server = zend_hash_str_find(&SU_G(servers), Z_STRVAL_P(skey), Z_STRLEN_P(skey));
    su_tcp_server_t *sts = NULL;
    if (!server) {
        zval _server;
        object_init_ex(&_server, su_tcp_server_ce);
        zend_hash_update(&SU_G(servers), Z_STR_P(skey), &_server);
        server = zend_hash_str_find(&SU_G(servers), Z_STRVAL_P(skey), Z_STRLEN_P(skey));
        sts = SU_OBJ_FROM_STD(Z_OBJ_P(server), su_tcp_server_t);
        zend_hash_init(&sts->workers, 2, NULL, NULL, 1);
        sts->id = zend_hash_num_elements(&SU_G(indexed_servers)) + 1;
        Z_TRY_ADDREF_P(server);
        zend_hash_next_index_insert(&SU_G(indexed_servers), server);
        su_tcp_bind(sts, options);
        if (type & SU_MSG_RR) {
            int r = uv_listen((uv_stream_t*)&sts->handle, 1024, su_tcp_rr_server_conn_cb);
        }
    } else {
        sts = SU_OBJ_FROM_STD(Z_OBJ_P(server), su_tcp_server_t);
    }
    su_process_t* worker = CONTAINER_OF(chan, su_process_t, ipc);
    zend_hash_next_index_insert_ptr(&sts->workers, worker);
    add_assoc_long(options, "__idx", sts->id);
    if (type & SU_MSG_RR) {
        zval json;
        SU_CALL_FN_ARR("json_encode", &json, 1, options);
        int _type = SU_MSG_SERV | SU_MSG_TCP | SU_MSG_RR;
        zend_string* msg = su_proc_msg_encode(Z_STRVAL(json), Z_STRLEN(json), _type);
        su_proc_send_to_pipe(chan, msg, NULL, 0);
        zval_ptr_dtor(&json);
    } else if (type & SU_MSG_CR) {
        zval json;
        SU_CALL_FN_ARR("json_encode", &json, 1, options);
        int _type = SU_MSG_FD | SU_MSG_TCP | SU_MSG_SERV | SU_MSG_CR;
        zend_string* msg = su_proc_msg_encode(Z_STRVAL(json), Z_STRLEN(json), _type);
        SU_TRACE("send type %d %s", _type, Z_STRVAL_P(&json));
        // do send
        su_proc_send_to_pipe(chan, msg, (uv_stream_t*)&sts->handle, _type);
        zval_ptr_dtor(&json);
    } else {
        //SU_TRACE("unexpected error");
    }
}

void su_tcp_server_try_start(zval* server)
{
    zval* options = zend_read_property(su_tcp_server_ce, server, ZEND_STRL("_options"), 1, NULL);
    zval* host = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("host"));
    zval* port = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("port"));
    smart_str skey = { 0 };
    smart_str_appends(&skey, "tcp:");
    smart_str_appendl(&skey, Z_STRVAL_P(host), Z_STRLEN_P(host));
    smart_str_appendc(&skey, ':');
    if (Z_TYPE_P(port) == IS_STRING) {
        smart_str_appendl(&skey, Z_STRVAL_P(port), Z_STRLEN_P(port));
    } else {
        smart_str_append_long(&skey, Z_LVAL_P(port));
    }
    smart_str_0(&skey);

    zend_hash_str_add(&SU_G(servers), ZSTR_VAL(skey.s), ZSTR_LEN(skey.s), server);
    if (SU_G(process)->id == 0 && SU_G(process)->status == 0) {
        su_simple_tcp_server_try_start(server, options);
    } else {
        su_tcp_server_t* sts = SU_OBJ_FROM_STD(Z_OBJ_P(server), su_tcp_server_t);
        if (sts->mode == SU_RP) {
            /*reuse port*/
            // bind
            su_tcp_init(sts);
            int iport = Z_TYPE_P(port) == IS_LONG ? Z_LVAL_P(port) : zval_get_long(port);
            int status;
            if (sts->addr_type == AF_INET6) {
                uv_ip6_addr(Z_STRVAL_P(host), iport, &(sts->addr.a6));
                status = uv_tcp_bind(&sts->handle, (const struct sockaddr*)&sts->addr.a6, 0);
            } else {
                status = uv_ip4_addr(Z_STRVAL_P(host), iport, &(sts->addr.a4));
                status = uv_tcp_bind(&sts->handle, (const struct sockaddr*)&(sts->addr.a4), 0);
            }
            // listen
            sts->connection_cb = su_tcp_cr_server_conn_cb;
            uv_listen((uv_stream_t*)&sts->handle, 128, sts->connection_cb);
            Z_TRY_ADDREF_P(server);
            zend_hash_add(&SU_G(servers), skey.s, server);
        } else {
            add_assoc_stringl(options, "__key", ZSTR_VAL(skey.s), ZSTR_LEN(skey.s));
            zval json;
            SU_CALL_FN_ARR("json_encode", &json, 1, options);
            int type = SU_MSG_SERV | SU_MSG_TCP;
            if (sts->mode == SU_CR) {
                type |= SU_MSG_CR;
            } else {
                type |= SU_MSG_RR;
            }
            SU_TRACE("fetch type %d", type);
            zend_string *msg = su_proc_msg_encode(Z_STRVAL_P(&json), Z_STRLEN_P(&json), type);
            su_proc_send_to_master(msg, NULL);
            su_process_chan_ref();
            zval_ptr_dtor(&json);
        }
    }
    smart_str_free(&skey);
}

/* the on connection callback for round-robin server*/
void su_tcp_rr_server_conn_cb(uv_stream_t* server, int status)
{
    su_tcp_server_t* sts = (su_tcp_server_t*)server;
    uv_tcp_t connection;
    uv_tcp_init(SU_G(loop), &connection);
    int r = uv_accept(server, (uv_stream_t*)&connection);
    if (r == 0) {
        zval* zw = zend_hash_get_current_data(&sts->workers);
        su_process_t* worker = (su_process_t*)Z_PTR_P(zw);
        int _type = SU_MSG_FD | SU_MSG_TCP | SU_MSG_RR;
        /* 此处对总 server 数限制为一个 unsigned char（256） */
        char idx[25];
        su_itoa(sts->id, idx, 10);
        zend_string* msg = su_proc_msg_encode(idx, 1, _type);
        // do send
        su_proc_send_to_pipe((uv_stream_t*)&worker->ipc.pipe, msg, (uv_stream_t*)&connection, _type);
        if (!zend_hash_has_more_elements(&sts->workers)) {
            zend_hash_internal_pointer_reset(&sts->workers);
        } else {
            zend_hash_move_forward(&sts->workers);
        }
    }
}


void su_worker_rr_tcp_server_conn_co(void* arg) {
    zval *zconn = (zval *)arg;
    su_tcp_conn_t *conn = SU_OBJ_FROM_STD(Z_OBJ_P(zconn), su_tcp_conn_t);
    zval *zserver = conn->server;
    su_tcp_server_t *server = SU_OBJ_FROM_STD(Z_OBJ_P(zserver), su_tcp_server_t);

    if (server->cbs[EV_TCP_SERVER_CONNECTION]) {
        SU_TRACE("su_worker_rr_tcp_server_conn_co %p", server->cbs[EV_TCP_SERVER_CONNECTION]);
        zval params[1] = {*zconn};
        zval retval;
        su_call_cb(server->cbs[EV_TCP_SERVER_CONNECTION], &retval, 1, params);
        zval_ptr_dtor(&retval);
    }
}

int su_worker_rr_tcp_server_conn_cb(uv_stream_t *chan, zval *zserver) {
    if (zserver == NULL) {
        return FAILURE;
    }
    SU_TRACE("TCP RR");

    zval _zconn;
    object_init_ex(&_zconn, su_tcp_conn_ce);
    SU_CALL_OM_ARR(su_tcp_conn_ce, &_zconn, "__construct", NULL, 0, NULL);
    su_tcp_conn_t *conn = SU_OBJ_FROM_STD(Z_OBJ_P(&_zconn), su_tcp_conn_t);
    conn->server = zserver;
    int r = uv_accept(chan, (uv_stream_t *)&conn->handle);
    if (r == 0) {
        conn->state = 1;
        SU_TRACE("rr conn in");
        conn->status = SU_OBJ_WORKING;
        su_tcp_server_t *server = SU_OBJ_FROM_STD(Z_OBJ_P(zserver), su_tcp_server_t);
        int idx = (int) conn->handle.io_watcher.fd;
        zend_hash_index_update(&server->connections, idx, &_zconn);
        zval *zconn = zend_hash_index_find(&server->connections, idx);
        coco_create(su_worker_rr_tcp_server_conn_co, (void *)zconn, SU_COCO_STACK);
    } else {
        uv_close((uv_handle_t *) conn, NULL);
        zval_ptr_dtor(&_zconn);
    }
}

/* deal the on connection for common-race server */
void su_tcp_cr_server_conn_co(void* arg)
{
    uv_stream_t* server = (uv_stream_t* )arg;
    zval connection;
    object_init_ex(&connection, su_tcp_conn_ce);
    SU_CALL_OM_ARR(su_tcp_conn_ce, &connection, "__construct", NULL, 0, NULL);
    su_tcp_conn_t* conn = SU_OBJ_FROM_STD(Z_OBJ_P(&connection), su_tcp_conn_t);

    /* if server can accept new conn */
    int r = uv_accept(server, (uv_stream_t*)&conn->handle);
    if (r == 0) {
        // 已连接
        SU_TRACE("cr conn in");
        conn->status = SU_OBJ_WORKING;
        zval params[1] = { connection };
        su_tcp_server_t* sts = (su_tcp_server_t*)server;
        if (sts->cbs[EV_TCP_SERVER_CONNECTION] == NULL) {
            uv_close((uv_handle_t*)conn, su_tcp_conn_close_cb);
        } else {
            zval retval;
            su_call_cb(sts->cbs[EV_TCP_SERVER_CONNECTION], &retval, 1, params);
            zval_ptr_dtor(&retval);
        }
    } else {
        uv_close((uv_handle_t*)conn, su_tcp_conn_close_cb);
    }
    zval_ptr_dtor(&connection);
}

void su_tcp_cr_server_conn_cb(uv_stream_t* server, int status)
{
    if (status == -1) {
        SU_DEBUG("thundering herd");
        return;
    }
    coco_create(su_tcp_cr_server_conn_co, (void *)server, SU_COCO_STACK);
}

/* the on connection callback for reuse-port server*/
void su_tcp_rp_server_conn_cb(uv_stream_t* server, int status)
{
    //
}

/* {{{ ARG_INFO */
#if 0
SU_BEGIN_ARG_INFO_EX(tcp_construct_arginfo, 0, 0, 0)
    SU_ARG_INFO(0, config)
SU_END_ARG_INFO()
#endif
/* }}} */

/* {{{ PHP METHODS */
SU_METHOD(tcp_server, __construct) {
    SU_SET_SELF;
#ifdef HAVE_REUSEPORT
    zend_ulong mode = SU_RP;
#else
    zend_ulong mode = SU_CR;
#endif
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &mode) == FAILURE) {
        return;
    }

    su_tcp_server_t* sts = SU_OBJ_FROM_SELF(su_tcp_server_t);
    sts->mode = mode;
    ZVAL_COPY(sts->handle.data, self);

    zval options;
    array_init(&options);
    zend_update_property(su_tcp_server_ce, self, ZEND_STRL("_options"), &options);
    zval_ptr_dtor(&options);

    if (mode == SU_RR) {
        zend_hash_init(&sts->connections, 16, NULL, NULL, 1);
    }

    SU_RET_SELF;
}

SU_METHOD(tcp_server, set_option)
{
    SU_SET_SELF;
    SU_RET_SELF;
}

SU_METHOD(tcp_server, listen)
{
    SU_SET_SELF;
    su_tcp_server_t* server = SU_OBJ_FROM_SELF(su_tcp_server_t);
    zval* host;
    zval* port;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz", &host, &port) == FAILURE) {
        php_error(E_ERROR, "error::listen param error");
    }
    int status = 0;
    zval* options = zend_read_property(su_tcp_server_ce, self, ZEND_STRL("_options"), 0, NULL);
    add_assoc_zval(options, "host", host);
    add_assoc_zval(options, "port", port);
    su_tcp_server_try_start(self);

    SU_RET_SELF;
}

int su_tcp_server_close(zval *self) {
    su_tcp_server_t *server = SU_OBJ_FROM_SELF(su_tcp_server_t);
    // NEED REVIEW
    if (server->status & TCP_SERVER_CLOSE) {
        return SUCCESS;
    }
    server->status |= TCP_SERVER_CLOSE;
    uv_close((uv_handle_t *) server, su_tcp_server_close_cb);
    return SUCCESS;
}

SU_METHOD(tcp_server, close)
{
    SU_SET_SELF;
    su_tcp_server_close(self);
}

SU_METHOD(tcp_server, pause)
{
    SU_SET_SELF;
    su_tcp_server_t *server = SU_OBJ_FROM_SELF(su_tcp_server_t);
    // NEED REVIEW
    if (server->status & TCP_SERVER_PAUSE) {
        return;
    }
    server->status |= TCP_SERVER_PAUSE;
    uv_close((uv_handle_t *) server, su_tcp_server_pause_cb);
}

SU_METHOD(tcp_server, resume)
{
    SU_SET_SELF;
    su_tcp_server_t *server = SU_OBJ_FROM_SELF(su_tcp_server_t);
    if (!(server->status & TCP_SERVER_PAUSE)) {
        return;
    }
    server->status &= ~TCP_SERVER_PAUSE;
    su_tcp_server_try_start(self);
}

SU_METHOD(tcp_server, on)
{
    SU_SET_SELF;
    su_tcp_server_t* server = SU_OBJ_FROM_SELF(su_tcp_server_t);
    zend_string* event;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sf", &event, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::on($event, $callback) param", SU_STATIC_CLASS());
    }
    if (su_cmp("connection", event)) {
        su_set_cb(server->cbs, EV_TCP_SERVER_CONNECTION, &cb);
    } else if (su_cmp("error", event)) {
        su_set_cb(server->cbs, EV_TCP_SERVER_ERROR, &cb);
    } else if (su_cmp("close", event)) {
        su_set_cb(server->cbs, EV_TCP_SERVER_CLOSE, &cb);
    } else if (su_cmp("listen", event)) {
        su_set_cb(server->cbs, EV_TCP_SERVER_LISTEN, &cb);
    } else if (su_cmp("pause", event)) {
        su_set_cb(server->cbs, EV_TCP_SERVER_PAUSE, &cb);
    } else if (su_cmp("resume", event)) {
        su_set_cb(server->cbs, EV_TCP_SERVER_RESUME, &cb);
    }
    SU_RET_SELF;
}

SU_METHOD(tcp_server, __destruct)
{
    SU_SET_SELF;
    su_tcp_server_close(self);
}

/* {{{ su_tcp_server_methods */
zend_function_entry su_tcp_server_methods[] = {
    SU_ME(tcp_server, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(tcp_server, __destruct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    SU_ME(tcp_server, set_option, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_server, listen, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_server, close, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_server, pause, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_server, resume, NULL, ZEND_ACC_PUBLIC)
    SU_ME(tcp_server, on, NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(tcp_server) {
    SU_CE_INIT(tcp_server, "su\\tcp\\server");
    return SUCCESS;
}
/* }}} */
