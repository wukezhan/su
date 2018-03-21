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

#ifndef PHP_SU_TIMER_H
#define PHP_SU_TIMER_H

SU_MINIT_FUNCTION(timer);

#define SU_TIMER_AFTER 1
#define SU_TIMER_EVERY 2

extern zend_class_entry *su_timer_ce;

#define EV_TIMER_TIMEOUT 1

typedef struct su_timer_s {
    uv_timer_t handle;
    int type;
    int status;
    int timeout;
    su_cb_t* cbs[2];
    zend_object std;
} su_timer_t;

#endif
