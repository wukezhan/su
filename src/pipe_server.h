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

#ifndef PHP_SU_PIPE_SERVER_H
#define PHP_SU_PIPE_SERVER_H

#define SU_PIPE_SERVER_STOPED          1
#define SU_PIPE_SERVER_READING         2
#define SU_PIPE_SERVER_READ_PAUSE      4
#define SU_PIPE_SERVER_READ_USER_PAUSE 8

enum su_pipe_server_events {
    EV_PIPE_SERVER_ERROR,
    EV_PIPE_SERVER_CLOSE,
    EV_PIPE_SERVER_CONN,
    EV_PIPE_SERVER_PAUSE,
    EV_PIPE_SERVER_RESUME,
};

SU_MINIT_FUNCTION(pipe_server);

extern zend_class_entry *su_pipe_server_ce;

typedef struct su_pipe_server_s {
    uv_pipe_t handle;
    int status;
    int read_status;
    int error_code;
    char *bind;
    uv_close_cb close_cb;
    su_cb_t* cbs[5];
    zend_object std;
} su_pipe_server_t;

void su_pipe_server_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void su_pipe_server_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void su_pipe_server_close_cb(uv_handle_t *handle);

#endif