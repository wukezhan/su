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

#include "php.h"
#include "php_ini.h"
#include "Zend/zend_interfaces.h"

#include "php_su.h"

#include "src/rbuf/rbuf.h"

su_rbuf_t *su_rbuf_new(su_rbuf_t *rbuf, int cap) {
    rbuf->cap = cap;
    rbuf->size = 0;
    rbuf->head = 0;
    rbuf->tail = -1;
    rbuf->bufs = (void **)su_malloc(sizeof(void *)*cap);
    return rbuf;
}

int su_rbuf_recap(su_rbuf_t *rbuf, int cap) {
    void **new_bufs = (void **)su_malloc(sizeof(void *)*cap);
    if (rbuf->size > cap) {
        return -1;
    } else if (rbuf->size > 0) {
        int head = rbuf->head;
        int size = rbuf->size;
        void **p = new_bufs;
        while(size --) {
            *p = *(rbuf->bufs+head);
            head = (head+1) % rbuf->cap;
            p ++;
        }
    }
    su_free(rbuf->bufs);
    rbuf->bufs = new_bufs;
    rbuf->head = 0;
    rbuf->tail = rbuf->size - 1;
    rbuf->cap = cap;

    return 0;
}

void su_rbuf_free(su_rbuf_t *rbuf) {
    su_free(rbuf->bufs);
}

int su_rbuf_is_full(su_rbuf_t *rbuf) {
    if (rbuf->size == rbuf->cap) {
        return 1;
    }
    return 0;
}

int su_rbuf_push(su_rbuf_t *rbuf, void **buf) {
    if (rbuf->size == rbuf->cap) {
        return FAILURE;
    }
    rbuf->size ++;
    rbuf->tail = (rbuf->tail+1) % rbuf->cap;
    *(rbuf->bufs+rbuf->tail) = *buf;

    return SUCCESS;
}

void* su_rbuf_read(su_rbuf_t *rbuf) {
    if (rbuf->size == 0) {
        return NULL;
    }
    int head = rbuf->head;
    rbuf->head = (head+1) % rbuf->cap;
    rbuf->size --;
    void* buf = *(rbuf->bufs+head);
    return buf;
}

