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

#ifndef PHP_SU_CO_H
#define PHP_SU_CO_H

#define SU_CO_STOPED 1

SU_MINIT_FUNCTION(co);

extern zend_class_entry *su_co_ce;

uv_loop_t *su_loop_get_default();
int su_co_scheduler_init();
int su_co_epoch_renew();
int su_co_ref();
int su_co_unref();
void su_co_main(void *v);

#endif