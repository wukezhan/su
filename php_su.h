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

#ifndef PHP_SU_H
#define PHP_SU_H

extern zend_module_entry su_module_entry;
#define phpext_su_ptr &su_module_entry

#define PHP_SU_VERSION "0.1.0"

#ifdef PHP_WIN32
#    define PHP_SU_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#    define PHP_SU_API __attribute__ ((visibility("default")))
#else
#    define PHP_SU_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#include "deps/libuv/include/uv.h"
#include "src/coco/coco.h"

#include "php.h"
#include "php_main.h"
#include "php_ini.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_smart_str.h"

typedef void *QUEUE[2];

typedef struct su_write_req_s {
    uv_write_t req;
    uv_buf_t buf;
    int send_handle;
} su_write_req_t;

typedef struct su_cb_s {
    zend_ulong type;
    zend_fcall_info fci;
    zend_fcall_info_cache fcic;
} su_cb_t;

typedef struct su_signal_s {
    uv_signal_t handle;
    zend_long signum;
    zend_long event_type;
} su_signal_t;

typedef struct su_ipc_msg_s {
    smart_str body;
    int len;
    int pos;
    int cnt;
    int type;
} su_ipc_msg_t;

typedef struct su_ipc_s {
    uv_pipe_t pipe;
    su_ipc_msg_t msg;
    int working;
    int servers;
} su_ipc_t;

typedef struct su_process_s {
    uv_process_t process;
    uv_process_options_t options;
    zend_ulong id;
    zend_ulong flags;
    zend_long status;
    su_ipc_t ipc;
    su_signal_t sigs[8];
    zval title;
    zval *self;
    zval *env;
    zval *stdio;
    uv_read_cb ipc_read_cb;
    uv_write_cb ipc_write_cb;
    uv_exit_cb exit_cb;
    HashTable scbs;
    su_cb_t *cbs[10];
    zend_object std;
} su_process_t;

typedef struct su_vmc_s {
    zend_vm_stack stack;
    zval *top;
    zval *end;
    zend_execute_data *ex;
    zend_class_entry *scope;
} su_vmc_t;

ZEND_BEGIN_MODULE_GLOBALS(su)
    zend_long version;
    zend_long fs_read_buf_size;
    coco_t* coco_running;
    coco_t* coco_io;
    int coco_ready;
    su_process_t *process;
    HashTable workers;
    HashTable servers;
    HashTable indexed_servers;
    zend_long state;
    zend_long backlog;
    char *name;
    uv_loop_t *loop;
    su_vmc_t *vmc;
    void (*execute_ex)(zend_execute_data *execute_data);
ZEND_END_MODULE_GLOBALS(su)

ZEND_EXTERN_MODULE_GLOBALS(su)
#define SU_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(su, v)

#if defined(ZTS) && defined(COMPILE_DL_SU)
ZEND_TSRMLS_CACHE_EXTERN();
#endif

#include "su.h"
#include "src/queue.h"

#endif    /* PHP_SU_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
