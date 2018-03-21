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

#ifndef PHP_SU_PROCESS_H
#define PHP_SU_PROCESS_H

SU_MINIT_FUNCTION(process);

#define SU_PROCESS_ONETIME 1<<0
#define SU_PROCESS_FOREVER 1<<1

#define SU_PROCESS_IPC     1<<0
#define SU_PROCESS_DAEMON  1<<1
#define SU_PROCESS_WINDOW  1<<2
#define SU_PROCESS_UPGRADE 1<<3

enum process_ev_type {
    EV_PROCESS_EXIT,
    EV_PROCESS_SIGUSR1,
    EV_PROCESS_SIGUSR2,
    EV_PROCESS_SIGINT,
    EV_PROCESS_SIGTERM,
    EV_PROCESS_SIGWINCH,
    EV_PROCESS_ONLINE,
    EV_PROCESS_OFFLINE,
    EV_PROCESS_MESSAGE,
    EV_PROCESS_ERROR,
};

extern zend_class_entry *su_process_ce;

#define SU_PROC() SU_OBJ_FROM_STD(Z_OBJ_P(SU_G(process)), su_process_t)
#define SU_SET_PROC su_process_t *proc = SU_G(process)
#define SU_SET_PROCXXX su_process_t *proc = SU_OBJ_FROM_STD(Z_OBJ_P(SU_G(process)), su_process_t)

int su_proc_send_to_pipe(uv_stream_t *chan, zend_string *msg, uv_stream_t *handle, int handle_type);
#define su_proc_send_to_master(msg, handle) su_proc_send_to_pipe(NULL, msg, handle, 0)

zend_string *su_proc_msg_encode(char *body, int blen, int type);
int su_proc_msg_decode(char *msg, int mlen, su_ipc_msg_t *im);

int su_process_chan_ref();
int su_process_chan_unref();


#endif