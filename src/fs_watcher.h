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

#ifndef PHP_SU_FS_WATCHER_H
#define PHP_SU_FS_WATCHER_H

#define SU_FS_WATCHER_STOPED 1
#define SU_FS_WATCHER_STARTED 2

enum su_fs_watcher_events {
    EV_FW_ERROR,
    EV_FW_CHANGE,
    EV_FW_CLOSE,
};

extern zend_class_entry *su_fs_watcher_ce;

SU_MINIT_FUNCTION(fs_watcher);

typedef struct su_fs_watcher_s {
    uv_fs_event_t handle;
    int events;
    int status;
    zend_string *path;
    su_cb_t* cbs[3];
    zend_object std;
} su_fs_watcher_t;

#endif