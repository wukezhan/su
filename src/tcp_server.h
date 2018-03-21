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

#ifndef PHP_SU_TCP_SERVER_H
#define PHP_SU_TCP_SERVER_H

SU_MINIT_FUNCTION(tcp_server);

extern zend_class_entry *su_tcp_server_ce;

enum su_tcp_server_events {
    EV_TCP_SERVER_ERROR,
    EV_TCP_SERVER_CONNECTION,
    EV_TCP_SERVER_CLOSE,
    EV_TCP_SERVER_LISTEN,
    EV_TCP_SERVER_PAUSE,
    EV_TCP_SERVER_RESUME,
};

#define TCP_SERVER_INIT     0
#define TCP_SERVER_CLOSE    1<<0
#define TCP_SERVER_RUN      1<<1
#define TCP_SERVER_PAUSE    1<<2

typedef struct su_tcp_server_s {
    uv_tcp_t handle; // 放在第一位可用于直接强制类型转换
    uv_connection_cb connection_cb;
    uv_close_cb read_cb;
    union {
        struct sockaddr_in a4;
        struct sockaddr_in6 a6;
    } addr;
    int addr_type;
    int mode;
    int req_id;
    HashTable workers;
    HashTable connections;
    int flags;
    int status;
    int id;
    zval *self;
    su_cb_t *cbs[6];
    zend_object std;
} su_tcp_server_t;

int su_master_tcp_server_try_start(zval *options, int type, uv_stream_t *chan);
void su_tcp_fetch_server(zval *options);
void su_tcp_connection_close(uv_handle_t *handle);

/* the on connection callback for round-robin server*/
void su_tcp_rr_server_conn_cb(uv_stream_t* server, int status);
void su_worker_rr_tcp_server_conn_co(void* arg);
int su_worker_rr_tcp_server_conn_cb(uv_stream_t *chan, zval *zserver);

/* the on connection callback for common-race server*/
void su_tcp_cr_server_conn_cb(uv_stream_t* server, int status);

/* the on connection callback for reuse-port server*/
void su_tcp_rp_server_conn_cb(uv_stream_t* server, int status);

#endif