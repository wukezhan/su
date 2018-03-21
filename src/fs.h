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

#ifndef PHP_SU_FS_H
#define PHP_SU_FS_H

#define SU_FS_STOPED  1<<0
#define SU_FS_READING 1<<1
#define SU_FS_WRITING 1<<2

enum su_fs_events {
    EV_FS_ERROR,
    EV_FS_READ,
    EV_FS_WRITE,
    EV_FS_CLOSE,
};

SU_MINIT_FUNCTION(fs);

extern zend_class_entry *su_fs_ce;

typedef struct su_fs_write_buf_s {
    uv_buf_t buf;
    zend_string *zstr;
} su_fs_write_buf_t;

typedef struct su_fs_s {
    uv_fs_t open;
    uv_fs_t read;
    uv_fs_t write;
    int open_flags;
    int status;
    uv_buf_t read_buf;
    zend_long read_offset;
    zend_long write_offset;
    su_fs_write_buf_t write_buf;
    su_cb_t* cbs[8];
    zval *self;
    zend_object std;
} su_fs_t;

#endif