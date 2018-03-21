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

#ifndef PHP_SU_RBUF_H
#define PHP_SU_RBUF_H

typedef struct su_rbuf_s {
    int cap;
    int size;
    int head;
    int tail;
    void** bufs;
} su_rbuf_t;

su_rbuf_t *su_rbuf_new(su_rbuf_t *rbuf, int cap);

int su_rbuf_recap(su_rbuf_t *rbuf, int cap);

void su_rbuf_free(su_rbuf_t *rbuf);

int su_rbuf_is_full(su_rbuf_t *rbuf);

int su_rbuf_push(su_rbuf_t *rbuf, void **buf);

void* su_rbuf_read(su_rbuf_t *rbuf);

int rbuf_test();

#endif