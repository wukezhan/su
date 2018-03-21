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
#include "ext/standard/info.h"
#include "php_su.h"

#include "src/co.h"
#include "src/rbuf/rbuf.h"
#include "src/chan.h"
#include "src/fs.h"
#include "src/fs_watcher.h"
#include "src/pipe.h"
#include "src/pipe_server.h"
#include "src/process.h"
#include "src/timer.h"
#include "src/tcp_conn.h"
#include "src/tcp_server.h"
#include "src/udp.h"

//#include "src/test.h"

ZEND_DECLARE_MODULE_GLOBALS(su)

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("su.backlog", "128", PHP_INI_ALL, OnUpdateLong, backlog, zend_su_globals, su_globals)
    STD_PHP_INI_ENTRY("su.fs_read_buf_size", "8192", PHP_INI_ALL, OnUpdateLong, fs_read_buf_size, zend_su_globals, su_globals)
    STD_PHP_INI_ENTRY("su.name", "su", PHP_INI_ALL, OnUpdateString, name, zend_su_globals, su_globals)
PHP_INI_END()
/* }}} */

/* {{{ php_su_init_globals
 */
static void php_su_init_globals(zend_su_globals *su_globals)
{
    /* do globals init after ini */
    /*php_printf("su init globals %d\n", su_globals->backlog);
    //su_globals->backlog = 0;
    //su_globals->name = NULL;*/
}

ZEND_API void su_main_ex(void *ex) {
    zend_execute_ex((zend_execute_data *)ex);
    SU_G(vmc) = su_vmc_stash();
}

ZEND_API void su_execute_ex(zend_execute_data *ex) {
    SU_TRACE("su_execute_ex start");
    zend_execute_ex = SU_G(execute_ex);
    if (SU_G(coco_io) == NULL) {
        //php_error(E_WARNING, "co create");
        coco_create(su_co_main, NULL, SU_MAIN_STACK);
    } else {
        //
    }
    //php_error(E_WARNING, "ex create");
    coco_create(su_main_ex, (void *)ex, SU_MAIN_STACK);
    su_co_scheduler();
    if (SU_G(vmc)) {
        su_vmc_unstash(SU_G(vmc));
        SU_G(vmc) = NULL;
    }
    SU_TRACE("su_execute_ex end");
}

/* }}} */

PHP_GINIT_FUNCTION(su){
#if defined(COMPILE_DL_SU) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    /* do globals init before ini register, it may be overwritten by ini */
}

PHP_GSHUTDOWN_FUNCTION(su){
    /* free all globals */
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(su)
{
    //ZEND_INIT_MODULE_GLOBALS(su, php_su_init_globals, NULL);
    REGISTER_INI_ENTRIES();
    /* init after REGISTER_INI_ENTRIES because we may need ini  */
    ZEND_INIT_MODULE_GLOBALS(su, php_su_init_globals, NULL);
    SU_MODULE_STARTUP(co);
    SU_MODULE_STARTUP(chan);
    SU_MODULE_STARTUP(fs);
    SU_MODULE_STARTUP(fs_watcher);
    SU_MODULE_STARTUP(pipe);
    SU_MODULE_STARTUP(pipe_server);
    SU_MODULE_STARTUP(process);
    SU_MODULE_STARTUP(timer);
    SU_MODULE_STARTUP(tcp_server);
    SU_MODULE_STARTUP(tcp_conn);
    SU_MODULE_STARTUP(udp);

    SU_G(loop) = uv_default_loop();

    //SU_MODULE_STARTUP(test);
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(su)
{
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(su)
{
#if defined(COMPILE_DL_SU) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    SU_G(process) = NULL;
    zend_hash_init(&SU_G(workers), 8, NULL, NULL, 1);
    zend_hash_init(&SU_G(servers), 2, NULL, NULL, 1);
    zend_hash_init(&SU_G(indexed_servers), 2, NULL, NULL, 1);

    SU_G(execute_ex) = zend_execute_ex;
    zend_execute_ex = su_execute_ex;
    su_process_init();
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(su)
{
    if (zend_execute_ex == su_execute_ex) {
        zend_execute_ex = SU_G(execute_ex);
    }
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(su)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "su support", "enabled");
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}
/* }}} */

/* {{{ su_functions[]
 *
 * Every user visible function must have an entry in su_functions[].
 */
const zend_function_entry su_functions[] = {
    PHP_FE_END    /* Must be the last line in su_functions[] */
};
/* }}} */

/* {{{ su_module_entry
 */
zend_module_entry su_module_entry = {
    STANDARD_MODULE_HEADER,
    "su",
    su_functions,
    PHP_MINIT(su),
    PHP_MSHUTDOWN(su),
    PHP_RINIT(su),        /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(su),    /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(su),
    PHP_SU_VERSION,
    PHP_MODULE_GLOBALS(su),
    PHP_GINIT(su),
    PHP_GSHUTDOWN(su),
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

/* check if this necessary */

#ifdef COMPILE_DL_SU
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE();
#endif
ZEND_GET_MODULE(su)
#endif

#define SU_CO_VM_LARGE_STACK_PAGE_SLOTS (16 * 1024) /* should be a power of 2 */
#define SU_CO_VM_SMALL_STACK_PAGE_SLOTS (256) /* 256*16 = 1024*4 = 4096 */

#define SU_CO_VM_STACK_PAGE_SLOTS(small) ((small) ? SU_CO_VM_SMALL_STACK_PAGE_SLOTS : SU_CO_VM_SMALL_STACK_PAGE_SLOTS)

#define SU_CO_VM_STACK_PAGE_SIZE(small)  (SU_CO_VM_STACK_PAGE_SLOTS(small) * sizeof(zval))

#define SU_CO_VM_STACK_FREE_PAGE_SIZE(small) \
    ((SU_CO_VM_STACK_PAGE_SLOTS(small) - ZEND_VM_STACK_HEADER_SLOTS) * sizeof(zval))

#define SU_CO_VM_STACK_PAGE_ALIGNED_SIZE(small, size) \
    (((size) + ZEND_VM_STACK_HEADER_SLOTS * sizeof(zval) \
      + (SU_CO_VM_STACK_PAGE_SIZE(small) - 1)) & ~(SU_CO_VM_STACK_PAGE_SIZE(gen) - 1))

static zend_always_inline zend_vm_stack su_co_vm_stack_new_page(size_t size, zend_vm_stack prev) {
    zend_vm_stack page = (zend_vm_stack)emalloc(size);

    page->top = ZEND_VM_STACK_ELEMETS(page);
    page->end = (zval*)((char*)page + size);
    page->prev = prev;
    return page;
}

void su_co_vm_stack_init(void)
{
    EG(vm_stack) = su_co_vm_stack_new_page(SU_CO_VM_STACK_PAGE_SIZE(1), NULL);
    EG(vm_stack)->top ++;
    EG(vm_stack_top) = EG(vm_stack)->top;
    EG(vm_stack_end) = EG(vm_stack)->end;
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
