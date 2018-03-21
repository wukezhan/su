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

#ifndef PHP_SU_UDP_H
#define PHP_SU_UDP_H

SU_MINIT_FUNCTION(udp);

extern zend_class_entry *su_udp_ce;

enum su_udp_events {
    EV_UDP_ERROR,
    EV_UDP_CONNECT,
    EV_UDP_RECV,
    EV_UDP_CLOSE,
    EV_UDP_END,
    EV_UDP_LOOKUP,
    EV_UDP_TIMEOUT,
    EV_UDP_DRAIN,
    EV_UDP_HIGHWATERMARK,
};

#define SU_UDP_RECVING 1
#define SU_UDP_RECV_PAUSE 2
#define SU_UDP_RECV_USER_PAUSE 4

#include "src/rbuf/rbuf.h"

typedef struct su_udp_s {
    uv_udp_t handle;
    uv_close_cb close_cb;
    union {
        struct sockaddr_in a4;
        struct sockaddr_in6 a6;
    } addr;
    int addr_type;

    su_rbuf_t recv_bufs;
    zend_long recv_buf_size;
    zend_long recv_buf_num;
    zend_long recv_status;

    int error_code;

    zend_long req_id;
    zend_long keepalive;
    zend_long nodelay;
    zend_long in_free;
    zend_long state;
    zend_long status;
    //events
    su_cb_t *cbs[9];
    zend_object std;
} su_udp_t;

typedef struct su_udp_send_s{
    uv_udp_send_t send;
    uv_buf_t buf;
    union {
        struct sockaddr_in a4;
        struct sockaddr_in6 a6;
    } addr;
    int addr_type;
} su_udp_send_t;

typedef struct su_udp_msg_s{
    uv_buf_t buf;
    struct sockaddr addr;
} su_udp_msg_t;

void su_udp_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void su_udp_close_cb(uv_handle_t *handle);

#endif