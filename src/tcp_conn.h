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

#ifndef PHP_SU_TCP_CONN_H
#define PHP_SU_TCP_CONN_H

SU_MINIT_FUNCTION(tcp_conn);

extern zend_class_entry *su_tcp_conn_ce;

enum su_tcp_conn_events {
    EV_TCP_CONN_ERROR,
    EV_TCP_CONN_CONNECT,
    EV_TCP_CONN_DATA,
    EV_TCP_CONN_CLOSE,
    EV_TCP_CONN_END,
    EV_TCP_CONN_LOOKUP,
    EV_TCP_CONN_TIMEOUT,
    EV_TCP_CONN_DRAIN,
    EV_TCP_CONN_HIGHWATERMARK,
};

#define SU_TCP_CONN_READING 1
#define SU_TCP_CONN_READ_PAUSE 2
#define SU_TCP_CONN_READ_USER_PAUSE 4
#define SU_TCP_CONN_PENDING_CLOSE 4

#include "src/rbuf/rbuf.h"

typedef struct su_tcp_conn_s {
    uv_tcp_t handle;
    uv_connect_t *req;
    uv_timer_t *timer;

    su_rbuf_t read_bufs;
    zend_long read_buf_size;
    zend_long read_buf_num;
    zend_long read_status;

    HashTable wrs;

    zend_long req_id;
    zend_long keepalive;
    zend_long nodelay;
    zend_long in_free;
    zend_long state;
    zend_long status;
    zend_long error_code;

    zval *server;
    uv_close_cb close_cb;
    /** events **/
    su_cb_t *cbs[9];
    zend_object std;
} su_tcp_conn_t;

void su_tcp_conn_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void su_tcp_conn_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void su_tcp_conn_close_cb(uv_handle_t *handle);

#endif