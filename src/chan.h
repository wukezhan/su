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

#ifndef PHP_SU_CHAN_H
#define PHP_SU_CHAN_H

#define SU_CHAN_INIT 0
#define SU_CHAN_SENDER_YIELD 1<<0
#define SU_CHAN_RECVER_YIELD 1<<1
#define SU_CHAN_SENDER_READY 1<<2
#define SU_CHAN_RECVER_READY 1<<3
#define SU_CHAN_CHAOS 4
#define SU_CHAN_STOP 5

SU_MINIT_FUNCTION(chan);

extern zend_class_entry *su_chan_ce;

typedef struct su_chan_s {
    zval* self;
    int status;
    int sender_status;
    int recver_status;
    su_cb_t* cbs[2];
    coco_t* coroutine;
    coco_t* coco_sender;
    coco_t* coco_recver;
    su_vmc_t* vmc;
    int cap;
    int size;
    int head;
    int tail;
    zval* data;
    zend_object std;
} su_chan_t;

#endif