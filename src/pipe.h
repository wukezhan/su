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

#ifndef PHP_SU_PIPE_H
#define PHP_SU_PIPE_H

#define SU_PIPE_STOPED          1
#define SU_PIPE_READING         2
#define SU_PIPE_READ_PAUSE      4
#define SU_PIPE_READ_USER_PAUSE 8
#define SU_PIPE_PENDING_CLOSE   16

enum su_pipe_events {
    EV_PIPE_ERROR,
    EV_PIPE_CLOSE,
    EV_PIPE_CONNECT,
    EV_PIPE_DATA,
    EV_PIPE_END,
};

SU_MINIT_FUNCTION(pipe);

extern zend_class_entry *su_pipe_ce;

#include "src/rbuf/rbuf.h"

typedef struct su_pipe_s {
    uv_pipe_t handle;
    uv_connect_t *req;
    int status;
    int error_code;
    su_rbuf_t read_bufs;
    zend_long read_buf_size;
    zend_long read_buf_num;
    zend_long read_status;
    uv_close_cb close_cb;
    su_cb_t* cbs[5];
    zend_object std;
} su_pipe_t;

void su_pipe_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void su_pipe_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void su_pipe_close_cb(uv_handle_t *handle);
su_pipe_t *su_pipe_init(zval *zp, int ipc);

#define su_pipe_try_start(pipe) do {                                                    \
    if (pipe->read_bufs.cap == 0) {                                                     \
        su_rbuf_new(&pipe->read_bufs, pipe->read_buf_num);                              \
    }                                                                                   \
    if (pipe->handle.io_watcher.fd >= 0) {                                              \
        uv_read_start((uv_stream_t *)&pipe->handle, su_pipe_alloc_cb, su_pipe_read_cb); \
    }                                                                                   \
}while(0)

#endif