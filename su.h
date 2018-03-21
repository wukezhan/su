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
/** {{{ wrapper **/
#ifndef SU_COMMON_H
#define SU_COMMON_H

/** {{{ common wrapper **/
#define SU_MINIT_FUNCTION(module)    ZEND_MINIT_FUNCTION(su_##module)
#define SU_RINIT_FUNCTION(module)    ZEND_RINIT_FUNCTION(su_##module)
#define SU_MODULE_STARTUP(module)    ZEND_MODULE_STARTUP_N(su_##module)(INIT_FUNC_ARGS_PASSTHRU)
#define SU_MSHUTDOWN_FUNCTION(module)    ZEND_MSHUTDOWN_FUNCTION(su_##module)
#define SU_MODULE_SHUTDOWN(module)    ZEND_MODULE_SHUTDOWN_N(su_##module)(INIT_FUNC_ARGS_PASSTHRU)

#define SU_METHOD(classname, name) PHP_METHOD(su_##classname, name)
#define su_NULL NULL
#define SU_ME(classname, name, arg_info, flags) ZEND_ME(su_##classname, name, su_##arg_info, flags)

#define SU_BEGIN_ARG_INFO_EX(name, _unused, return_reference, required_num_args) ZEND_BEGIN_ARG_INFO_EX(su_##name, _unused, return_reference, required_num_args)
#define SU_ARG_INFO ZEND_ARG_INFO
#define SU_END_ARG_INFO ZEND_END_ARG_INFO

#define su_exit zend_bailout()
#define SU_SET_SELF zval *self = getThis()
#define SU_RET_SELF RETURN_ZVAL(self, 1, 0)
#define SU_STATIC_CLASS() (EG(current_execute_data)->called_scope->name)
#define SU_ZVAL_IS_NULL(pz) (ZVAL_IS_NULL(pz) || ZVAL_IS_NULL(Z_REFVAL_P(pz)))

extern PHPAPI void php_var_dump(zval *struc, int level);
extern PHPAPI void php_debug_zval_dump(zval *struc, int level);

#define pvp php_var_dump
#define pzp php_debug_zval_dump

#define su_cmalloc malloc
#define su_cfree free

#define su_malloc emalloc
#define su_free efree

void su_co_vm_stack_init(void);

#define SU_OUT(fmt_str, ...) php_printf("\n@su: %s#%d:\n"fmt_str"\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define SU_DEBUG 
#define SU_INFO 
#define SU_TRACE 
#if 1
#define SU_IO(fmt_str, ...) do {                                                                             \
    php_printf("\n[su#%d]: %s#%d:\n{ "fmt_str" }\n", (SU_G(process)?SU_G(process)->id:-1), __FILE__, __LINE__, ##__VA_ARGS__);    \
} while(0)
#else
#define SU_IO
#endif

#define SU_UVE(msg, r) SU_IO("[%s] [%d|%s]: %s", msg, r, uv_err_name(r), uv_strerror(r))
#define TODO 0

#define SU_IS_MASTER() (SU_G(process)->id == 0)
#define SU_IS_UPGRADER() 
#define SU_IS_DAEMON() 

#define MAKE_STR_2(a, b) a#b
#define su_cmp(cc, zs) (ZSTR_LEN(zs) == sizeof(cc)-1 && memcmp(cc, ZSTR_VAL(zs), sizeof(cc)-1) == 0)

#define SU_SS_SIMPLEX 1
#define SU_SS_MULTIPLEX 2

#define SU_MAIN_STACK 256*1024
#define SU_COCO_STACK 16*1024

/* msg with handle */
#define SU_MSG_FD           1<<0
/* handle is conn */
#define SU_MSG_CONN         1<<1
/* handle is serv */
#define SU_MSG_SERV         1<<2
/* handle is tcp */
#define SU_MSG_TCP          1<<3
/* handle is udp */
#define SU_MSG_UDP          1<<4
/* handle is pipe */
#define SU_MSG_PIPE         1<<5
/* server mode is cr */
#define SU_MSG_CR           1<<6
/* server mode is rr */
#define SU_MSG_RR           1<<7

#define CONTAINER_OF(ptr, type, field) ((type *) ((char *) (ptr) - offsetof(type, field)))

#define SU_OBJ_INIT(pz, classname) do { \
        zend_class_entry *ce = NULL;    \
        if ((ce = (zend_class_entry *)zend_hash_str_find_ptr(EG(class_table), classname, sizeof(classname)-1)) != NULL) {\
            object_init_ex((pz), ce);   \
        }                               \
    }while(0)

#define SU_ARR_GET_KEY(arr, key) zend_hash_str_find(Z_ARRVAL_P(arr), ZEND_STRL(key))
#define SU_ARR_GET_IDX(arr, idx) zend_hash_index_find(Z_ARRVAL_P(arr), idx)
#define SU_ARR_CNT(arr) (arr !=NULL && Z_TYPE_P(arr) == IS_ARRAY?zend_hash_num_elements(Z_ARRVAL_P(arr)):-1)

static inline su_vmc_t *su_vmc_stash() {
    su_vmc_t *ov = (su_vmc_t *)su_cmalloc(sizeof(su_vmc_t));
    ov->stack = EG(vm_stack);
    ov->top = EG(vm_stack_top);
    ov->end = EG(vm_stack_end);
    ov->scope = EG(scope);
    ov->ex = EG(current_execute_data);

    EG(vm_stack) = NULL;
    EG(vm_stack_top) = NULL;
    EG(vm_stack_end) = NULL;
    EG(scope) = NULL;
    EG(current_execute_data) = NULL;

    return ov;
}

static inline int su_vmc_unstash(su_vmc_t *cv) {
    EG(vm_stack) = cv->stack;
    EG(vm_stack_top) = cv->top;
    EG(vm_stack_end) = cv->end;
    EG(scope) = cv->scope;
    EG(current_execute_data) = cv->ex;

    su_cfree(cv);
    SU_INFO("unstash %p: %p, %p, %p\n\n", EG(vm_stack), EG(vm_stack_top), EG(vm_stack_end), EG(current_execute_data));
}

static inline int su_vmc_stash2(su_vmc_t *ov) {
    ov->stack = EG(vm_stack);
    ov->top = EG(vm_stack_top);
    ov->end = EG(vm_stack_end);
    ov->scope = EG(scope);
    ov->ex = EG(current_execute_data);

    EG(vm_stack) = NULL;
    EG(vm_stack_top) = NULL;
    EG(vm_stack_end) = NULL;
    EG(scope) = NULL;
    EG(current_execute_data) = NULL;

    return 0;
}

static inline int su_vmc_unstash2(su_vmc_t *cv) {
    EG(vm_stack) = cv->stack;
    EG(vm_stack_top) = cv->top;
    EG(vm_stack_end) = cv->end;
    EG(scope) = cv->scope;
    EG(current_execute_data) = cv->ex;
    return 0;
}

static inline zend_class_entry *su_get_ce(char *classname, int len){
    zend_class_entry *ce = NULL;
    if ((ce = (zend_class_entry *)zend_hash_str_find_ptr(EG(class_table), classname, len)) == NULL) {
        return NULL;
    }
    return ce;
}

static int su_call_function(const char* fn, int fn_len, zval *callable, zval *ret, int p_cnt, zval p_arr[], zval *p_ptr) {
    su_vmc_t *vmc = su_vmc_stash();
    su_co_vm_stack_init();
    zval *_ret = NULL;
    if (!ret) {
        zval __ret;
        _ret = &__ret;
    }
    zval func;
    if (!callable) {
        if (!fn) {
            php_error(E_ERROR, "call error func");
        }
        ZVAL_STRINGL(&func, fn, fn_len);
    }

    zend_fcall_info fci;

    fci.size = sizeof(fci);
    fci.object = NULL;
    ZVAL_COPY_VALUE(&fci.function_name, callable?callable: &func);
    fci.retval = ret ? ret : _ret;
    fci.no_separation = (zend_bool) 1;
#if 1
    fci.function_table = EG(function_table);
    fci.symbol_table = NULL;
#endif
    if (p_ptr) {
        zend_fcall_info_args(&fci, p_ptr);
    } else {
        fci.param_count = p_cnt;
        fci.params = p_arr;
    }

    int status = zend_call_function(&fci, NULL);
    if (p_ptr) {
        zend_fcall_info_args_clear(&fci, 1);
    }
    if (!callable) {
        zval_ptr_dtor(&func);
    }
    if (!ret) {
        zval_ptr_dtor(_ret);
    }
    zend_vm_stack_destroy();
    su_vmc_unstash(vmc);
    return status;
}

#define SU_CALL_FN_ARR(fn, ret, p_cnt, p_arr) su_call_function(fn, sizeof(fn)-1, NULL, ret, p_cnt, p_arr, NULL)
#define SU_CALL_FN_PTR(fn, ret, p_ptr) su_call_function(fn, sizeof(fn)-1, NULL, ret, 0, NULL, p_ptr)
#define SU_CALL_CB_ARR(cb, ret, p_cnt, p_arr) su_call_function(NULL, 0, cb, ret, p_cnt, p_arr, NULL)
#define SU_CALL_CB_PTR(cb, ret, p_ptr) su_call_function(NULL, 0, cb, ret, 0, NULL, p_ptr)

#define SU_METHOD_MAX_PARAM_SIZE 8

static inline zval* su_call_method(zval *object, zend_class_entry *obj_ce, zend_function **fn_proxy, const char *function_name, size_t function_name_len, zval *retval_ptr, int param_count, zval params[])
{
    int result;
    zend_fcall_info fci;
    zval retval;
    HashTable *function_table;
    if (param_count > SU_METHOD_MAX_PARAM_SIZE) {
        php_error(E_ERROR, "too many params");
    }
    su_vmc_t *vmc = su_vmc_stash();
    su_co_vm_stack_init();

    zval args[SU_METHOD_MAX_PARAM_SIZE];
    int i = 0;
    for (; i<param_count; i++) {
        ZVAL_COPY_VALUE(&args[i], &params[i]);
    }

    fci.size = sizeof(fci);
    /*fci.function_table = NULL; will be read form zend_class_entry of object if needed */
    fci.object = (object && Z_TYPE_P(object) == IS_OBJECT) ? Z_OBJ_P(object) : NULL;
    ZVAL_STRINGL(&fci.function_name, function_name, function_name_len);
    fci.retval = retval_ptr ? retval_ptr : &retval;
    fci.param_count = param_count;
    fci.params = params;
    fci.no_separation = 1;
    fci.symbol_table = NULL;

    if (!fn_proxy && !obj_ce) {
        /* no interest in caching and no information already present that is
         * needed later inside zend_call_function. */
        fci.function_table = !object ? EG(function_table) : NULL;
        result = zend_call_function(&fci, NULL);
        zval_ptr_dtor(&fci.function_name);
    } else {
        zend_fcall_info_cache fcic;

        fcic.initialized = 1;
        if (!obj_ce) {
            obj_ce = object ? Z_OBJCE_P(object) : NULL;
        }
        if (obj_ce) {
            function_table = &obj_ce->function_table;
        } else {
            function_table = EG(function_table);
        }
        if (!fn_proxy || !*fn_proxy) {
            if ((fcic.function_handler = zend_hash_find_ptr(function_table, Z_STR(fci.function_name))) == NULL) {
                /* error at c-level */
                zend_error_noreturn(E_CORE_ERROR, "Couldn't find implementation for method %s%s%s", obj_ce ? ZSTR_VAL(obj_ce->name) : "", obj_ce ? "::" : "", function_name);
            }
            if (fn_proxy) {
                *fn_proxy = fcic.function_handler;
            }
        } else {
            fcic.function_handler = *fn_proxy;
        }
        fcic.calling_scope = obj_ce;
        if (object) {
            fcic.called_scope = Z_OBJCE_P(object);
        } else {
            zend_class_entry *called_scope = zend_get_called_scope(EG(current_execute_data));

            if (obj_ce &&
                (!called_scope ||
                 !instanceof_function(called_scope, obj_ce))) {
                fcic.called_scope = obj_ce;
            } else {
                fcic.called_scope = called_scope;
            }
        }
        fcic.object = object ? Z_OBJ_P(object) : NULL;
        result = zend_call_function(&fci, &fcic);
        zval_ptr_dtor(&fci.function_name);
    }
    if (result == FAILURE) {
        /* error at c-level */
        if (!obj_ce) {
            obj_ce = object ? Z_OBJCE_P(object) : NULL;
        }
        if (!EG(exception)) {
            zend_error_noreturn(E_CORE_ERROR, "Couldn't execute method %s%s%s", obj_ce ? ZSTR_VAL(obj_ce->name) : "", obj_ce ? "::" : "", function_name);
        }
    }
    /* copy arguments back, they might be changed by references */
    for (i=0; i<param_count; i++) {
        if(Z_ISREF(args[i]) && !Z_ISREF(params[i])){
            ZVAL_COPY_VALUE(&params[i], &args[i]);
        }
    }
    zend_vm_stack_destroy();
    su_vmc_unstash(vmc);
    if (!retval_ptr) {
        zval_ptr_dtor(&retval);
        return NULL;
    }
    return retval_ptr;
}

#define SU_CALL_OM_ARR(obj_ce, obj, mn, ret, p_cnt, p_arr) su_call_method(obj, obj_ce, NULL, mn, sizeof(mn)-1, ret, p_cnt, p_arr)
#define SU_CALL_SM_ARR(obj_ce, mn, ret, p_cnt, p_arr) su_call_method(NULL, obj_ce, NULL, mn, sizeof(mn)-1, ret, p_cnt, p_arr)

#define SU_OUT_VMC(s) do { \
    php_printf("%s \nstack vs %p ~ top %p ~ end %p, cex %p\n\n",        \
        s,                                                              \
        EG(vm_stack),                                                   \
        EG(vm_stack_top),                                               \
        EG(vm_stack_end),                                               \
        EG(current_execute_data)                                        \
    );                                                                  \
} while(0);

static int str2int(char *str, int len){
    int ret = 0;
    if (!len) {
        while(*str != '\0'){
            ret = ret*10 + (*str - '0');
            str++;
        }
    } else {
        while (len--) {
            ret = ret*10 + (*str - '0');
            str++;
        }
    }
    return ret;
}

static char* su_itoa(int num,char *str,int radix)
{
    char index[]="0123456789ABCDEF";
    unsigned unum;
    int i=0,j,k;
    if (radix == 10 && num < 0) {
        unum = (unsigned) -num;
        str[i++] = '-';
    } else {
        unum = (unsigned) num;
    }
    do{
        str[i++] = index[unum%(unsigned)radix];
        unum /= radix;
    } while(unum);
    str[i]='\0';
    if(str[0]=='-') {
        k=1;
    } else {
        k=0;
    }
    char temp;
    for(j=k; j<=(i-1)/2; j++)
    {
        temp = str[j];
        str[j] = str[i-1+k-j];
        str[i-1+k-j] = temp;
    }
    return str;
}

static int su_json_decode(zend_string *json, zval *ret) {
    zval str;
    ZVAL_STR(&str, json);
    zval one;
    ZVAL_LONG(&one, 1);
    zval params[] = {str, one};
    SU_CALL_FN_ARR("json_decode", ret, 2, params);
    zval_ptr_dtor(&str);

    if (Z_TYPE_P(ret) == IS_NULL) {
        return FAILURE;
    }
    return SUCCESS;
}


#define su_ecalloc(type) ecalloc(1, sizeof(type) + zend_object_properties_size(ce))

#define OBJ_TYPE_NAME(obj_type) su_##obj_type##_t
#define OBJ_CE_NAME(obj_type) su_##obj_type##_ce
#define OBJ_ME_NAME(obj_type) su_##obj_type##_methods
#define OBJ_HANDLERS_NAME(obj_type) su_##obj_type##_handlers

#define SU_OBJ_INIT_STD(obj, ce, object_handlers) do {  \
    zend_object_std_init(&obj->std, ce);                \
	object_properties_init(&obj->std, ce);              \
	obj->std.handlers = &object_handlers;               \
} while(0)

#define SU_OBJ_FREE_STD(obj) zend_object_std_dtor(&obj->std)

#define SU_OBJ_HANDLERS_INIT(obj_type, object_handlers) do {                                \
    memcpy(&object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers)); \
    object_handlers.offset = XtOffsetOf(OBJ_TYPE_NAME(obj_type), std);                      \
    object_handlers.clone_obj = NULL;                                                       \
    object_handlers.free_obj = su_##obj_type##_free;                                        \
} while(0)

#define SU_CLASS_D(classname) zend_class_entry *su_##classname##_ce; static zend_object_handlers su_##classname##_handlers

#define SU_CE_INIT(obj_type, classname) do {                                    \
    zend_class_entry ce;                                                        \
    INIT_CLASS_ENTRY(ce, classname, OBJ_ME_NAME(obj_type));                     \
    ce.create_object = su_##obj_type##_create;                                  \
    OBJ_CE_NAME(obj_type) = zend_register_internal_class_ex(&ce, NULL);         \
    SU_OBJ_HANDLERS_INIT(obj_type, OBJ_HANDLERS_NAME(obj_type)); \
} while(0)

#define SU_CE_LITE_INIT(obj_type, classname) do {                               \
    zend_class_entry ce;                                                        \
    INIT_CLASS_ENTRY(ce, classname, OBJ_ME_NAME(obj_type));                     \
    OBJ_CE_NAME(obj_type) = zend_register_internal_class_ex(&ce, NULL);         \
} while(0)

#define SU_CE_ALIAS(obj_type, alias) do {                                       \
    zend_register_class_alias_ex(ZEND_STRL(alias), su_##obj_type##_ce);         \
} while(0)

#define SU_OBJ_FROM_STD(obj, type) ((type *) ((void *)obj - XtOffsetOf(type, std)))
#define SU_OBJ_FROM_SELF(type) SU_OBJ_FROM_STD(Z_OBJ_P(self), type)

#define SU_OBJ_WORKING          100
#define SU_OBJ_PENDING_CLOSE    200
#define SU_OBJ_CLOSING          300
#define SU_OBJ_CLOSED           400
#define SU_OBJ_FREEED           500

#define SU_OBJ_CLOSE(obj, cb) do {                                                  \
    if (obj->status < SU_OBJ_PENDING_CLOSE && (obj->read_status || !QUEUE_EMPTY(&obj->handle.write_queue))) {   \
        obj->status = SU_OBJ_PENDING_CLOSE;                                         \
        obj->close_cb = cb;                                                         \
    } else if (obj->status < SU_OBJ_CLOSING && !obj->read_status && QUEUE_EMPTY(&obj->handle.write_queue)) {    \
        uv_close((uv_handle_t *)obj, cb);                                           \
        obj->status = SU_OBJ_CLOSING;                                               \
    }                                                                               \
} while(0)

#define SU_OBJ_CHECK_CLOSE(obj) do {                                                \
    if (obj->status == SU_OBJ_PENDING_CLOSE && !obj->read_status && QUEUE_EMPTY(&obj->handle.write_queue)) { \
        uv_close((uv_handle_t *)obj, obj->close_cb);                                \
        obj->status = SU_OBJ_CLOSING;                                               \
    }                                                                               \
} while(0)

/** su cb **/
#define SU_TRY_FREE_CB(cb) do {                             \
    if ((cb) != NULL && (cb)->fci.size > 0) {               \
        SU_INFO("FLEN %d\n", (cb)->fci.size);               \
        if (Z_REFCOUNTED_P(&(cb)->fci.function_name)) {     \
            SU_INFO("REF %d\n", Z_REFCOUNT_P(&(cb)->fci.function_name));                         \
            zval_ptr_dtor(&(cb)->fci.function_name);        \
        }                                                   \
        (cb)->fci.size = 0;                                 \
    }                                                       \
}while(0)

#define SU_TRY_FREE_CBS(cbs) do {                           \
    int size = sizeof(cbs)/sizeof(void*);                   \
    while(size) {                                           \
        size --;                                            \
        SU_INFO("size %d\n", size);                         \
        SU_TRY_FREE_CB(cbs[size]);                          \
        if (cbs[size]) {                                    \
            su_free(cbs[size]);                             \
        }                                                   \
    }                                                       \
}while(0)

#define SU_TRY_REF_CB(cb) do {                              \
    Z_TRY_ADDREF_P(&(cb)->fci.function_name);               \
}while(0)

static void su_set_cb(su_cb_t* cbs[], int event, su_cb_t* tcb) {
    su_cb_t* cb = NULL;
    if (cbs[event] == NULL) {
        cb = su_malloc(sizeof(su_cb_t));
        cb->fci.size = 0;
        cbs[event] = cb;
    } else {
        cb = cbs[event];
    }
    SU_TRY_FREE_CB(cb);
    memcpy(cb, tcb, sizeof(su_cb_t));
    SU_TRY_REF_CB(cb);
}

static int su_call_cb(su_cb_t* cb, zval* retval, uint32_t param_count, zval *params) {
    if (cb == NULL) {
        return -2;
    }
    if (cb->fci.size == 0) {
        return FAILURE;
    }
    cb->fci.params = params;
    cb->fci.retval = retval;
    cb->fci.param_count = param_count;
    su_vmc_t *vmc = NULL;
    if (1 || cb->type) {
        su_co_vm_stack_init();
    }
    int status = zend_call_function(&cb->fci, &cb->fcic);
    if (1 || cb->type) {
        zend_vm_stack_destroy();
    }
    return status;
}

static void su_call_co_cb(void *arg) {
    su_cb_t* cb = (su_cb_t*) arg;
    zval retval;
    su_call_cb(cb, &retval, 0, NULL);
    zval_ptr_dtor(&retval);
    SU_TRY_FREE_CB(cb);
    su_free(cb);
}
/** }}} su cb **/

static void su_obj_handle_error(su_cb_t *cb, int error_code) {
    zval _tmp;
    zval _error_code;
    ZVAL_LONG(&_error_code, error_code);
    zval _error_msg;
    ZVAL_STRING(&_error_msg, uv_strerror(error_code));
    zval params[2] = {_error_code, _error_msg};
    su_vmc_t vmc;
    su_vmc_stash2(&vmc);
    su_call_cb(cb, &_tmp, 2, params);
    su_vmc_unstash2(&vmc);
    zval_ptr_dtor(&_tmp);
    zval_ptr_dtor(&_error_code);
    zval_ptr_dtor(&_error_msg);
}

#define SU_OBJ_TRY_ERR(obj, code) do {                      \
    if (code) {                                             \
        obj->error_code = code;                             \
    }                                                       \
    if ((obj)->cbs[0] && obj->error_code != 0) {            \
        su_obj_handle_error((obj)->cbs[0], obj->error_code);\
        obj->error_code = 0;                                \
    }                                                       \
} while(0)

#define STACK 256*1024

/** }}} common wrapper **/

#endif
/** }}} wrapper **/
