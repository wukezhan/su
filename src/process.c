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
#include "php_main.h"
#include "php_ini.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_smart_str.h"

#include "php_su.h"

#include "src/co.h"
#include "src/pipe.h"
#include "src/tcp_server.h"
#include "src/process.h"

extern char** environ;

static zend_long process_id = 0;
SU_CLASS_D(process);

#define su_reset_ipc_msg(msg) do {  \
    smart_str_free(&msg->body);     \
    msg->pos = 0;                   \
    msg->len = 0;                   \
    msg->cnt = 0;                   \
} while (0)

int su_process_spawn(zval *zproc, int detached);
void su_master_read_cb(uv_stream_t *chan, ssize_t nread, const uv_buf_t* buf);
void su_master_worker_exit_cb(uv_process_t *req, int64_t exit_status, int term_signal);

static zend_object* su_process_create(zend_class_entry *ce) {
    su_process_t *proc = (su_process_t *)su_ecalloc(su_process_t);
    proc->id = -1;
    proc->flags = 0;
    proc->ipc_read_cb = su_master_read_cb;
    proc->exit_cb = su_master_worker_exit_cb;
    SU_OBJ_INIT_STD(proc, ce, su_process_handlers);
    proc->self = (zval *)su_malloc(sizeof(zval));
    proc->stdio = NULL;
    proc->env = NULL;
    ZVAL_UNDEF(&proc->title);

    return &proc->std;
}

void su_process_free(zend_object *std) {
    su_process_t *proc = SU_OBJ_FROM_STD(std, su_process_t);
    if (proc->self) {
        zval_ptr_dtor(proc->self);
        su_free(proc->self);
    }
    SU_OBJ_FREE_STD(proc);
    if (proc->options.stdio) {
        efree(proc->options.stdio);
    }
    if (proc->options.args) {
        efree(proc->options.args);
    }
    if (proc->options.env) {
        efree(proc->options.env);
    }
    if (proc->stdio) {
        zval_ptr_dtor(proc->stdio);
        efree(proc->stdio);
    }
    if (proc->env) {
        zval_ptr_dtor(proc->env);
        efree(proc->env);
    }
    if (Z_TYPE(proc->title) == IS_STRING) {
        zval_ptr_dtor(&proc->title);
    }
    /*clean signal*/
    /*clean signal cbs*/
    SU_TRY_FREE_CBS(proc->cbs);
    su_free(proc);
}

/* process etc */
typedef struct su_proc_msg_s {
    zend_string *body;
    int type;
} su_proc_msg_t;

#define SU_MSG(msg) SU_IO("\n\tbody: %s, \n\ttype: %d\n", ZSTR_VAL(msg->body), msg->type)

int su_process_chan_ref() {
    SU_SET_PROC;
    if (proc->id > 0) {
        uv_ref((uv_handle_t *)&proc->ipc.pipe);
    }
}

int su_process_chan_unref() {
    SU_SET_PROC;
    if (proc->id > 0 && proc->ipc.servers <= 0) {
        uv_unref((uv_handle_t *)&proc->ipc.pipe);
    }
}

int su_ipc_msg_unpack(char *msg, int msg_len, su_ipc_msg_t *im) {
    int quota = im->len - im->pos;
    if (quota > msg_len) {
        // 半包
        smart_str_appendl(&im->body, msg, msg_len);
        im->pos += msg_len;
    } else {
        // 全包
        // 粘包
        smart_str_appendl(&im->body, msg, quota);
        smart_str_0(&im->body);
        im->pos = im->len;
    }
    return msg_len - quota;
}

int su_proc_msg_decode(char *msg, int msg_len, su_ipc_msg_t *im) {
    if (im->pos < im->len) {
        return su_ipc_msg_unpack(msg, msg_len, im);
    }
    smart_str_free(&im->body);
    char *p = msg;
    do {
        if (*p == '$') {
            im->cnt ++;
        } else if (im->cnt > 0) {
            if (im->cnt == 1) {
                im->type = (int) (unsigned char) *p;
            } else if (im->cnt == 2) {
                im->len = im->len*10 + (*p - '0');
            } else {
                return su_ipc_msg_unpack(p, msg_len, im);
            }
        }
        p++;
    } while (msg_len--);
    return msg_len;
}

zend_string *su_proc_msg_encode(char *body, int blen, int type) {
    smart_str msg = {0};
    smart_str_appendc(&msg, '$');
    smart_str_appendc(&msg, (unsigned char)type);
    smart_str_appendc(&msg, '$');
    smart_str_append_long(&msg, blen);
    smart_str_appendc(&msg, '$');
    smart_str_appendl(&msg, body, blen);
    smart_str_0(&msg);

    zend_string *ret = zend_string_init(ZSTR_VAL(msg.s), ZSTR_LEN(msg.s), 0);
    smart_str_free(&msg);
    return ret;
}

void su_send_handle_close_cb(uv_handle_t* handle) {
    SU_TRACE("sent handle closed");
}

void su_process_pipe_write_cb(uv_write_t* req, int status) {
    su_write_req_t *wr = (su_write_req_t *)req;
    zend_string *msg = (zend_string *)req->data;
    if (msg) {
        zend_string_release(msg);
    }
    if (wr->send_handle & SU_MSG_FD && !(wr->send_handle & SU_MSG_SERV)) {
        /* do close for conn */
        uv_close((uv_handle_t *) req->send_handle, su_send_handle_close_cb);
    }
    free(wr);
}

int su_proc_send_to_pipe(uv_stream_t *chan, zend_string *msg, uv_stream_t *handle, int handle_type) {
    if (chan == NULL) {
        SU_SET_PROC;
        if (!(proc->flags & SU_PROCESS_IPC)) {
            return -1;
        }
        /* if no chan, then send to master */
        chan = (uv_stream_t *) &proc->ipc.pipe;
    }
    su_write_req_t *wr = (su_write_req_t *) malloc(sizeof(su_write_req_t));
    wr->req.data = (void *)msg;
    wr->buf = uv_buf_init(ZSTR_VAL(msg), ZSTR_LEN(msg));
    wr->send_handle = handle_type;
    int r = uv_write2(&wr->req, chan, (const uv_buf_t *)&wr->buf, 1, handle, su_process_pipe_write_cb);
    return r;
}

int su_worker_msg_dispatch(uv_stream_t *chan, su_ipc_msg_t *pm) {
    if (pm->type & SU_MSG_SERV) {
        su_process_chan_unref(); // 在此即可，无需再往外提了
        zval options;
        if (su_json_decode(pm->body.s, &options) == FAILURE) {
            return FAILURE;
        }
        zval *zkey = zend_hash_str_find(Z_ARRVAL(options), ZEND_STRL("__key"));
        zval *zserver = zend_hash_str_find(&SU_G(servers), Z_STRVAL_P(zkey), Z_STRLEN_P(zkey));
        if (pm->type & SU_MSG_FD) {
            if (pm->type & SU_MSG_CR) {
                zval *host = zend_hash_str_find(Z_ARRVAL(options), ZEND_STRL("host"));
                zval *port = zend_hash_str_find(Z_ARRVAL(options), ZEND_STRL("port"));
                if (pm->type & SU_MSG_TCP) {
                    su_tcp_server_t *server = SU_OBJ_FROM_STD(Z_OBJ_P(zserver), su_tcp_server_t);
                    uv_tcp_init(SU_G(loop), &server->handle);
                    int r = uv_accept(chan, (uv_stream_t*) &server->handle);
                    r = uv_listen((uv_stream_t*) &server->handle, 128, su_tcp_cr_server_conn_cb);
                    //SU_UVE("listen", r);
                } else if (pm->type & SU_MSG_UDP) {
                    //
                } else {
                    // unknown
                }
            }
        } else {
            if (pm->type & SU_MSG_RR) {
                SU_G(process)->ipc.servers ++;
                su_process_chan_ref();
                zval *zidx = zend_hash_str_find(Z_ARRVAL(options), ZEND_STRL("__idx"));
                // 此处无需区分到底是何类型，直接以 idx 加入 indexed_servers 即可
                Z_ADDREF_P(zserver);
                zend_hash_index_add(&SU_G(indexed_servers), Z_LVAL_P(zidx), zserver);
            } else {
                // unknown
            }
        }
    } else {
        if (pm->type & SU_MSG_RR && pm->type & SU_MSG_FD) {
            int idx = atoi(ZSTR_VAL(pm->body.s));
            zval *server = zend_hash_index_find(&SU_G(indexed_servers), idx);
            if (pm->type & SU_MSG_TCP) {
                su_worker_rr_tcp_server_conn_cb(chan, server);
            } else if (pm->type & SU_MSG_UDP) {
                //
            } else if (pm->type & SU_MSG_PIPE) {
                //
            }
        }
    }
}

void su_worker_pipe_read_cb(uv_stream_t *chan, ssize_t nread, const uv_buf_t* buf) {
    su_ipc_t *ipc = (su_ipc_t *)chan;
    if (nread < 0) {
        //something went wrong
        if (nread == UV_EOF) {
            uv_close((uv_handle_t *) chan, NULL);
        }
    } else if (nread == 0) {
        //no data
    } else {
        su_ipc_msg_t *msg = &ipc->msg;
        if (msg->pos == 0) {
            ipc->working ++;
        }
        int r = nread;
        do {
            r = su_proc_msg_decode(buf->base+(nread-r), r, msg);
            if (msg->len == msg->pos) {
                // dispatch
                su_worker_msg_dispatch(chan, msg);
                su_reset_ipc_msg(msg);
            }
        } while (r > 0);
        if (msg->pos == 0) {
            ipc->working --;
        }
    }
    free(buf->base);
}

void su_worker_pipe_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void su_worker_on_SIGINT(uv_signal_t* handle, int signum) {
    //SU_TRACE("INT");
}

#define su_signal_install(loop, s, sn) do {                         \
    uv_signal_init(loop, &(s).handle);                              \
    if (SU_G(process)->id == 0){                                    \
        uv_signal_start(&(s).handle, su_master_on_##sn, sn);        \
    } else {                                                        \
        uv_signal_start(&(s).handle, su_worker_on_##sn, sn);        \
    }                                                               \
    uv_unref((uv_handle_t *)&(s).handle);                           \
    (s).signum = sn;                                                \
} while(0)

int su_process_signal_install() {
    SU_SET_PROC;
    uv_loop_t *loop = SU_G(loop);

    return 0;
}

int su_process_init() {
    if (strcasecmp("cli", sapi_module.name) != 0) {
        return 0;
    }

    if (SU_G(process) == NULL) {
        // 初始化进程对象
        zval zproc;
        object_init_ex(&zproc, su_process_ce);
        su_process_t *process = SU_OBJ_FROM_STD(Z_OBJ_P(&zproc), su_process_t);
        zval retval;
        SU_CALL_OM_ARR(su_process_ce, &zproc, "__construct", &retval, 0, NULL);
        zval_ptr_dtor(&retval);
        SU_G(process) = process;

        char *cwid = getenv("SU_WORKER_ID");
        if (cwid) {
            uv_loop_t *loop = SU_G(loop);
            process->id = atoi(cwid);

            char *cflag = getenv("SU_PROCESS_FLAGS");
            if (cflag) {
                process->flags = atoi(cflag);

                if (process->flags & SU_PROCESS_IPC) {
                    int cfd = 3;
                    uv_pipe_t *pipe = &process->ipc.pipe;
                    uv_pipe_init(loop, pipe, 1);
                    int r = uv_pipe_open(pipe, cfd);
                    /* 对于子进程，此处应该unref，这样才方便退出 */
                    r = uv_read_start((uv_stream_t *)pipe, su_worker_pipe_alloc_cb, su_worker_pipe_read_cb);
                    uv_unref((uv_handle_t *)pipe);
                    #if 1
                    zend_string *msg = su_proc_msg_encode(ZEND_STRL("online"), 0);
                    int err = su_proc_send_to_master(msg, NULL);
                    if (err) {
                        php_error(E_WARNING, "online err: %s", uv_strerror(err));
                    }
                    #endif
                }
            } else {
                process->flags = 0;
            }
        } else {
            process->id = 0;
        }

        if (process->id >= 0) {
            //su_process_signal_install();
        }
    }

    return 0;
}

/* {{{ ARG_INFO */
#if 0
SU_BEGIN_ARG_INFO_EX(process_construct_arginfo, 0, 0, 0)
    SU_ARG_INFO(0, config)
SU_END_ARG_INFO()
#endif
/* }}} */

/* {{{ PHP METHODS */
SU_METHOD(process, running) {
    if (strcasecmp("cli", sapi_module.name) != 0) {
        php_error(E_ERROR, "su\\process only works in php cli");
        return;
    }
    RETURN_ZVAL(SU_G(process)->self, 1, 0);
}

SU_METHOD(process, __construct) {
    if (strcasecmp("cli", sapi_module.name) != 0) {
        php_error(E_ERROR, "su\\process only works in php cli");
        return;
    }
    SU_SET_SELF;
    su_process_t *proc = SU_OBJ_FROM_SELF(su_process_t);
    ZVAL_COPY(proc->self, self);
    proc->id = process_id++;

    zval opts;
    array_init(&opts);
    zend_update_property(su_process_ce, self, ZEND_STRL("_options"), &opts);
    zval_ptr_dtor(&opts);

    zend_hash_init(&proc->scbs, 8, NULL, NULL, 1);
    SU_RET_SELF;
}

SU_METHOD(process, set_options) {
    SU_SET_SELF;
    zval *opts;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z/", &opts) == FAILURE) {
        php_error(E_ERROR, "invalid %s::set_options($options) param", SU_STATIC_CLASS());
    }

    zend_update_property(su_process_ce, self, ZEND_STRL("_options"), opts);
}

void coco_sig(void *arg) {
    su_cb_t *cb = (su_cb_t *)arg;
    zval retval;
    su_call_cb(cb, &retval, 0, NULL);
    zval_ptr_dtor(&retval);
}

void su_proc_on_signal2(uv_signal_t* handle, int signum) {
    su_process_t *proc = (su_process_t *)handle->data;
    zval *sig_cbs = zend_hash_index_find(&proc->scbs, signum);
    zend_ulong h;
    void *vcb;
    ZEND_HASH_FOREACH_NUM_KEY_PTR(Z_ARRVAL_P(sig_cbs), h, vcb) {
        coco_create(coco_sig, vcb, SU_COCO_STACK);
    } ZEND_HASH_FOREACH_END();
}

void su_set_sig_cb2(su_process_t *proc, int sn, su_cb_t *tcb) {
    #if 0
    zval *sh = zend_hash_index_find(&proc->scbs, sn);
    if (sh == NULL) {
        zval _sh;
        array_init(&_sh);
        zend_hash_index_add(&proc->scbs, sn, &_sh);
        sh = zend_hash_index_find(&proc->scbs, sn);
    }
    su_cb_t *cb = (su_cb_t *)su_malloc(sizeof(su_cb_t));
    memcpy(cb, tcb, sizeof(su_cb_t));
    zend_hash_next_index_insert_ptr(Z_ARRVAL_P(sh), cb);
    su_signal_t *s = &proc->sigs[EV_PROCESS_SIGINT];
    if (!(s)->signum) {
        (s)->signum = sn;
        (s)->handle.data = proc;
        uv_signal_init(SU_G(loop), &(s)->handle);
        uv_signal_start(&(s)->handle, su_proc_on_signal, sn);
        uv_unref((uv_handle_t *)&(s)->handle);
    }
    #endif
}

void su_proc_on_signal(uv_signal_t* handle, int signum) {
    coco_create(coco_sig, handle->data, SU_COCO_STACK);
}

#define su_set_sig_cb(proc, sn, cb) do {\
    su_set_cb(proc->cbs, EV_PROCESS_##sn, cb);\
    su_signal_t *s = &proc->sigs[EV_PROCESS_##sn];\
    if (!(s)->event_type) {\
        (s)->event_type = EV_PROCESS_##sn;\
        (s)->handle.data = proc;\
        (s)->handle.data = proc->cbs[EV_PROCESS_##sn];\
        uv_signal_init(SU_G(loop), &(s)->handle);\
        uv_signal_start(&(s)->handle, su_proc_on_signal, sn);\
        uv_unref((uv_handle_t *)&(s)->handle);\
    }\
} while(0)

SU_METHOD(process, on) {
    SU_SET_SELF;
    zend_string *event;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sf", &event, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::after($time, $callback) param", SU_STATIC_CLASS());
    }

    su_process_t *proc = SU_OBJ_FROM_SELF(su_process_t);

    if(su_cmp("error", event)) {
        su_set_cb(proc->cbs, EV_PROCESS_ERROR, &cb);
    } else if(su_cmp("exit", event)) {
        su_set_cb(proc->cbs, EV_PROCESS_EXIT, &cb);
    } else if(su_cmp("message", event)) {
        su_set_cb(proc->cbs, EV_PROCESS_MESSAGE, &cb);
    } else if(su_cmp("online", event)) {
        su_set_cb(proc->cbs, EV_PROCESS_ONLINE, &cb);
    } else if(su_cmp("offline", event)) {
        su_set_cb(proc->cbs, EV_PROCESS_OFFLINE, &cb);
    } else if(su_cmp("SIGUSR1", event)) {
        su_set_sig_cb(proc, SIGUSR1, &cb);
    } else if(su_cmp("SIGUSR2", event)) {
        su_set_sig_cb(proc, SIGUSR2, &cb);
    } else if(su_cmp("SIGINT", event)) {
        su_set_sig_cb(proc, SIGINT, &cb);
    } else if(su_cmp("SIGTERM", event)) {
        su_set_sig_cb(proc, SIGTERM, &cb);
    } else if(su_cmp("SIGWINCH", event)) {
        su_set_sig_cb(proc, SIGWINCH, &cb);
    } else {
        // throw error
    }
    SU_RET_SELF;
}

SU_METHOD(process, is_master) {
    SU_SET_SELF;
    su_process_t *proc = SU_OBJ_FROM_SELF(su_process_t);
    RETURN_BOOL(proc->id <= 0);
}

SU_METHOD(process, is_worker) {
}

void su_init_read_cb(uv_stream_t *chan, ssize_t nread, const uv_buf_t* buf) {
    if (nread<0) {
        //something went wrong
        if (nread == UV_EOF) {
            uv_close((uv_handle_t *) chan, NULL);
        }
    } else if (nread == 0) {
        //no data
    } else {
        su_ipc_t *ipc = (su_ipc_t *)chan;
        su_ipc_msg_t *msg = &ipc->msg;
        if (msg->pos == 0) {
            ipc->working ++;
        }
        int r = nread;
        do {
            r = su_proc_msg_decode(buf->base+(nread-r), r, msg);
            if (msg->len == msg->pos) {
                // say bye-bye to daemon
                uv_unref((uv_handle_t *)chan);
                smart_str_free(&msg->body);
                msg->pos = 0;
                msg->len = 0;
                msg->cnt = 0;
            }
        } while (r > 0);
        if (msg->pos == 0) {
            ipc->working --;
        }
    }
    free(buf->base);
}

SU_METHOD(process, daemonize) {
    SU_SET_SELF;
    su_process_t *proc = SU_OBJ_FROM_SELF(su_process_t);
    if (!(proc->flags & SU_PROCESS_DAEMON)) {
        SU_TRACE("DAEMONIZE FLAGS %d", proc->flags);
        SU_TRACE("FLAGS %d", SU_PROCESS_IPC);
        SU_TRACE("FLAGS %d", SU_PROCESS_DAEMON);
        proc->ipc_read_cb = su_init_read_cb;
        su_process_spawn(self, 1);
        uv_unref((uv_handle_t*)proc);
        su_exit;
        // exit on daemon ready
    }
}

SU_METHOD(process, send) {
}

int su_process_refresh_title(su_process_t *proc) {
    smart_str state = {0};

    if (proc->id == 0) {
        smart_str_appendl(&state, "master", 6);
    } else if(proc->id > 0) {
        smart_str_appendl(&state, "worker#", 7);
        smart_str_append_long(&state, proc->id);
    }
    if (proc->status) {
        /* ignore for now */
    }
    smart_str_0(&state);

    smart_str title = {0};
    if (!Z_ISUNDEF_P(&proc->title)) {
        smart_str_appendl(&title, Z_STRVAL_P(&proc->title), Z_STRLEN_P(&proc->title));
        smart_str_appendc(&title, ':');
    }
    if (ZSTR_LEN(state.s)>0) {
        smart_str_appendc(&title, '(');
        smart_str_appendl(&title, ZSTR_VAL(state.s), ZSTR_LEN(state.s));
        smart_str_appendc(&title, ')');
    } else {
        return 0;
    }
    smart_str_appendc(&title, ' ');
    smart_str_appends(&title, sapi_module.executable_location);
    smart_str_appendc(&title, ' ');
    smart_str_appends(&title, SG(request_info).argv[0]);
    smart_str_0(&title);

    zval ts;
    ZVAL_STRINGL(&ts, ZSTR_VAL(title.s), ZSTR_LEN(title.s));
    SU_CALL_FN_ARR("cli_set_process_title", NULL, 1, &ts);
    zval_ptr_dtor(&ts);
    smart_str_free(&title);
    smart_str_free(&state);
    return 0;
}

SU_METHOD(process, set_title) {
    SU_SET_SELF;
    zend_string *title;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &title) == FAILURE) {
        php_error(E_ERROR, "invalid %s::set_title($title) param", SU_STATIC_CLASS());
    }

    su_process_t *proc = SU_OBJ_FROM_SELF(su_process_t);
    ZVAL_STRINGL(&proc->title, ZSTR_VAL(title), ZSTR_LEN(title));
    su_process_refresh_title(proc);
}

void su_master_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}


int su_master_msg_dispatch(uv_stream_t *chan, su_ipc_msg_t *pm) {
    if (!(pm->type & SU_MSG_FD)) {
        if (pm->type & SU_MSG_SERV) {
            zval options;
            ZVAL_UNDEF(&options);
            zval str;
            ZVAL_STRINGL(&str, ZSTR_VAL(pm->body.s), ZSTR_LEN(pm->body.s));
            zval one;
            ZVAL_LONG(&one, 1);
            zval params[] = {str, one};
            SU_CALL_FN_ARR("json_decode", &options, 2, params);
            //pzp(&options, 1);
            zval_ptr_dtor(&str);
            if (pm->type & SU_MSG_TCP) {
                su_master_tcp_server_try_start(&options, pm->type, chan);
            } else {
                //su_udp_master_fetch_server(options, type, client);
            }
            zval_ptr_dtor(&options);
        } else {
            SU_TRACE("recv plain msg: %s", ZSTR_VAL(pm->body.s));
        }
    } else {
        //
    }
}

void su_master_read_cb(uv_stream_t *chan, ssize_t nread, const uv_buf_t* buf) {
    if (nread<0) {
        if (nread == UV_EOF) {
            uv_close((uv_handle_t *) chan, NULL);
        }
    } else if (nread == 0) {
        //no data
    } else {
        su_ipc_t *ipc = (su_ipc_t *)chan;
        su_ipc_msg_t *msg = &ipc->msg;
        if (msg->pos == 0) {
            ipc->working ++;
        }
        int r = nread;
        do {
            r = su_proc_msg_decode(buf->base + (nread - r), r, msg);
            if (msg->len == msg->pos) {
                su_master_msg_dispatch(chan, msg);
                smart_str_free(&msg->body);
                msg->pos = 0;
                msg->len = 0;
                msg->cnt = 0;
            }
        } while (r > 0);
        if (msg->pos == 0) {
            ipc->working --;
        }
    }
    free(buf->base);
}

void su_master_worker_close_cb(uv_handle_t* handle) {
    su_process_t *worker = (su_process_t *)handle;
    efree(worker->options.args);
    efree(worker);
}

void su_master_worker_exit_cb(uv_process_t *req, int64_t exit_status, int term_signal) {
    /* check req->status (it's been set when receiving SIGNAL) */
    /**
     * clean the pipe
     */
    su_process_t *proc = (su_process_t *)req;
    uv_read_stop((uv_stream_t *)&proc->ipc.pipe);
    if (0 /* forever & not in exiting */ ) {
        /* respawn the process*/
    } else {
        /* close the process */
        uv_close((uv_handle_t*)req, su_master_worker_close_cb);
    }
}

#define STDIO_IGNORE    0
#define STDIO_NONE      0
#define STDIO_INHERIT   1
#define STDIO_PIPE      2

int su_process_set_stdio(uv_loop_t *loop, su_process_t *proc, zval *opts) {
    zval *stdio = SU_ARR_GET_KEY(opts, "stdio");
    uv_stdio_container_t *child_stdio = NULL;
    uv_pipe_t *pipe = NULL;
    proc->stdio = emalloc(sizeof(zval));
    array_init(proc->stdio);
    if (stdio == NULL || Z_TYPE_P(stdio) != IS_ARRAY) {
        int type = STDIO_INHERIT;
        if (stdio) {
            type = zval_get_long(stdio);
        }
        if (type == STDIO_INHERIT) {
            /* used for: worker,  */
            child_stdio = (uv_stdio_container_t *)emalloc(sizeof(uv_stdio_container_t)*4);
            child_stdio[0].flags = UV_INHERIT_FD;
            /* here fd need to be configurable */
            child_stdio[0].data.fd = 0;
            child_stdio[1].flags = UV_INHERIT_FD;
            child_stdio[1].data.fd = 1;
            child_stdio[2].flags = UV_INHERIT_FD;
            child_stdio[2].data.fd = 2;

            pipe = (uv_pipe_t *)&proc->ipc.pipe;
            uv_pipe_init(loop, pipe, 1);
            child_stdio[3].flags = UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE;
            child_stdio[3].data.stream = (uv_stream_t*) pipe;

            proc->flags |= SU_PROCESS_IPC;
        } else {
            //do nothing
        }
    } else {
        int num = zend_hash_num_elements(Z_ARRVAL_P(stdio));
        child_stdio = emalloc(sizeof(uv_stdio_container_t)*num);
        zend_long h;
        zend_string *key;
        zval *val;
        int idx = 0;
        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(stdio), h, key, val) {
            int is_int = 1;
            if (Z_TYPE_P(val) == IS_STRING) {
                if (memcmp(Z_STRVAL_P(val), ZEND_STRL("pipe")) == 0) {
                    is_int = 0;
                    zval zp;
                    object_init_ex(&zp, su_pipe_ce);
                    child_stdio[idx].flags = UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE;
                    su_pipe_t *pipe = su_pipe_init(&zp, 0);
                    child_stdio[idx].data.stream = (uv_stream_t*) &pipe->handle;
                    add_index_zval(proc->stdio, idx, &zp);
                    //php_var_dump(proc->stdio, 1);
                }
            } else if (Z_TYPE_P(val) == IS_OBJECT) {
                is_int = 0;
                child_stdio[idx].flags = UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE;
                child_stdio[idx].data.stream = (uv_stream_t*) SU_OBJ_FROM_STD(Z_OBJ_P(val), su_pipe_t);
                Z_TRY_ADDREF_P(val);
                add_index_zval(proc->stdio, idx, val);
            }
            if (is_int) {
                if (Z_TYPE_P(val) == IS_NULL) {
                    child_stdio[idx].flags = UV_IGNORE;
                } else {
                    zend_long fd = zval_get_long(val);
                    child_stdio[idx].flags = UV_INHERIT_FD;
                    child_stdio[idx].data.fd = fd;
                }
            }
            idx ++;
        } ZEND_HASH_FOREACH_END();
    }
    proc->options.stdio = child_stdio;
    proc->options.stdio_count = 4;
}

int su_process_set_env(su_process_t *proc, zval *opts) {
    if (proc->options.env) {
        return SUCCESS;
    }
    zval *zenv = SU_ARR_GET_KEY(opts, "env");
    zval *pzenv = proc->env;
    char **env = NULL;
    zend_long i = 0;
    if (pzenv == NULL) {
        pzenv = (zval *)emalloc(sizeof(zval));
        proc->env = pzenv;
        array_init(pzenv);
        int num = SU_ARR_CNT(zenv);
        if (num == -1) {
            /* inherit */
            char **_env;
            for (_env = environ; _env != NULL && *_env != NULL; _env++) {
                add_next_index_string(pzenv, *_env);
            }
        } else {
            zend_long h;
            zend_string *key;
            zval *val;
            ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(zenv), h, key, val) {
                /* make string */
                smart_str tmp = {0};
                if (key) {
                    smart_str_appendl(&tmp, ZSTR_VAL(key), ZSTR_LEN(key));
                } else {
                    smart_str_append_long(&tmp, h);
                }
                smart_str_appendc(&tmp, '=');
                if (Z_TYPE_P(val) == IS_STRING) {
                    smart_str_appendl(&tmp, Z_STRVAL_P(val), Z_STRLEN_P(val));
                } else {
                    zend_string *sval = zval_get_string(val);
                    smart_str_appendl(&tmp, ZSTR_VAL(sval), ZSTR_LEN(sval));
                    zend_string_release(sval);
                }
                smart_str_0(&tmp);
                add_next_index_stringl(pzenv, ZSTR_VAL(tmp.s), ZSTR_LEN(tmp.s));
                smart_str_free(&tmp);
            } ZEND_HASH_FOREACH_END();
        }
        char wid[20] = {0};
        char flags[20] = {0};
        sprintf(wid, "SU_WORKER_ID=%d", proc->id);
        sprintf(flags, "SU_PROCESS_FLAGS=%d", proc->flags);
        add_next_index_string(pzenv, wid);
        add_next_index_string(pzenv, flags);
    }
    int cnt = SU_ARR_CNT(pzenv);
    env = (char **)emalloc(sizeof(char*)*(cnt + 1));
    zend_long h;
    zend_string *key;
    zval *val;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(pzenv), val) {
        env[i] = Z_STRVAL_P(val);
        i++;
    } ZEND_HASH_FOREACH_END();
    env[cnt] = NULL;

    proc->options.env = env;
    return SUCCESS;
}

int su_process_set_file_args(zval *zproc) {
    su_process_t *proc = SU_OBJ_FROM_STD(Z_OBJ_P(zproc), su_process_t);
    zval *zfile = zend_read_property(su_process_ce, zproc, ZEND_STRL("_file"), 1, NULL);
    zval *zargs = zend_read_property(su_process_ce, zproc, ZEND_STRL("_args"), 1, NULL);
    zend_long argc = SU_ARR_CNT(zargs);
    char **args = (char **)emalloc(sizeof(char*)*(argc+2));
    args[0] = Z_STRVAL_P(zfile);
    if (argc > 0) {
        int i;
        zval *zarg;
        for (i = 0; i < argc; i++) {
            zarg = SU_ARR_GET_IDX(zargs, i);
            if (!(Z_TYPE_P(zarg) == IS_STRING)) {
                zend_string *str = zval_get_string(zarg);
                add_index_stringl(zargs, i, ZSTR_VAL(str), ZSTR_LEN(str));
                zend_string_release(str);
            }
            args[i+1] = Z_STRVAL_P(zarg);
        }
    }
    args[argc+1] = NULL;
    proc->options.file = args[0];
    proc->options.args = args;
}

int su_process_spawn(zval *zproc, int detached) {
    su_process_t *proc = SU_OBJ_FROM_STD(Z_OBJ_P(zproc), su_process_t);
    uv_loop_t *loop = SU_G(loop);

    zval *opts = zend_read_property(su_process_ce, zproc, ZEND_STRL("_options"), 1, NULL);
    su_process_set_stdio(loop, proc, opts);
    if (detached) {
        proc->options.flags |= UV_PROCESS_DETACHED;
        proc->flags |= SU_PROCESS_DAEMON;
    }

    if (!(SU_G(process)->flags & SU_PROCESS_DAEMON)) {
        proc->flags |= SU_PROCESS_WINDOW;
    }

    proc->options.exit_cb = proc->exit_cb;

    su_process_set_file_args(zproc);
    su_process_set_env(proc, opts);

    uv_spawn(loop, &proc->process, &proc->options);
    if (proc->stdio) {
        zend_long h;
        zend_string *key;
        zval *io;
        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(proc->stdio), h, key, io) {
            // 暂仅支持pipe
            su_pipe_t *pipe = SU_OBJ_FROM_STD(Z_OBJ_P(io), su_pipe_t);
            if (pipe->cbs[EV_PIPE_DATA]) {
                su_pipe_try_start(pipe);
            }
        } ZEND_HASH_FOREACH_END();
    }
    if (proc->flags & SU_PROCESS_IPC) {
        uv_read_start((uv_stream_t*)&proc->ipc.pipe, su_master_alloc_cb, su_master_read_cb);
    }
}

SU_METHOD(process, close) {
}

int su_process_init_file_args(zval *zproc, zend_string *file, zval *zargs) {
    su_process_t *proc = SU_OBJ_FROM_STD(Z_OBJ_P(zproc), su_process_t);
    if (!file) {
        zend_update_property_string(su_process_ce, zproc, ZEND_STRL("_file"), sapi_module.executable_location);
    } else {
        zend_update_property_stringl(su_process_ce, zproc, ZEND_STRL("_file"), ZSTR_VAL(file), ZSTR_LEN(file));
    }
    int zargc = SU_ARR_CNT(zargs);
    if ((zargc) < 0) {
        /* XXX this should be moved to spawn_workers, args not set */
        zval arr;
        array_init(&arr);
        int i;
        for (i = 0; i < SG(request_info).argc; i++) {
            add_next_index_string(&arr, SG(request_info).argv[i]);
        }
        zend_update_property(su_process_ce, zproc, ZEND_STRL("_args"), &arr);
        zval_ptr_dtor(&arr);
    } else {
        zend_update_property(su_process_ce, zproc, ZEND_STRL("_args"), zargs);
    }

    return SUCCESS;
}

SU_METHOD(process, run) {
    SU_SET_SELF;
    zend_string *file = NULL;
    zval *args = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|Sa", &file, &args) == FAILURE) {
        // throw exception
        return;
    }
    su_process_init_file_args(self, file, args);
    su_process_spawn(self, 0);
}

void su_tmp_read_cb(uv_stream_t *chan, ssize_t nread, const uv_buf_t* buf) {
    SU_TRACE("READ %d: %s", nread, buf->base);
}

SU_METHOD(process, stdin) {
    SU_SET_SELF;
    su_process_t *proc = SU_OBJ_FROM_SELF(su_process_t);
    if (proc->stdio) {
        zval *stdin = SU_ARR_GET_IDX(proc->stdio, 0);
        if (stdin) {
            su_pipe_t *p = SU_OBJ_FROM_STD(Z_OBJ_P(stdin), su_pipe_t);
            RETURN_ZVAL(stdin, 1, 0);
        }
    }
}

SU_METHOD(process, stdout) {
    SU_SET_SELF;
    su_process_t *proc = SU_OBJ_FROM_SELF(su_process_t);
    if (proc->stdio) {
        zval *stdout = SU_ARR_GET_IDX(proc->stdio, 1);
        if (stdout) {
            su_pipe_t *p = SU_OBJ_FROM_STD(Z_OBJ_P(stdout), su_pipe_t);
            RETURN_ZVAL(stdout, 1, 0);
        }
    }
}

SU_METHOD(process, stderr) {
    SU_SET_SELF;
    su_process_t *proc = SU_OBJ_FROM_SELF(su_process_t);
    if (proc->stdio) {
        zval *stderr = SU_ARR_GET_IDX(proc->stdio, 2);
        if (stderr) {
            su_pipe_t *p = SU_OBJ_FROM_STD(Z_OBJ_P(stderr), su_pipe_t);
            RETURN_ZVAL(stderr, 1, 0);
        }
    }
}

SU_METHOD(process, spawn) {
    //
}

SU_METHOD(process, spawn_workers) {
    zend_string *file;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &file) == FAILURE) {
        return;
    }
}

/* }}} */

/* {{{ uv_process_methods */
zend_function_entry su_process_methods[] = {
    SU_ME(process, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(process, running, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    SU_ME(process, spawn, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    SU_ME(process, spawn_workers, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    SU_ME(process, on, NULL, ZEND_ACC_PUBLIC)
    SU_ME(process, is_master, NULL, ZEND_ACC_PUBLIC)
    SU_ME(process, is_worker, NULL, ZEND_ACC_PUBLIC)
    SU_ME(process, daemonize, NULL, ZEND_ACC_PUBLIC)
    SU_ME(process, send, NULL, ZEND_ACC_PUBLIC)
    SU_ME(process, set_title, NULL, ZEND_ACC_PUBLIC)
    SU_ME(process, set_options, NULL, ZEND_ACC_PUBLIC)
    SU_ME(process, stdout, NULL, ZEND_ACC_PUBLIC)
    SU_ME(process, close, NULL, ZEND_ACC_PUBLIC)
    SU_ME(process, run, NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(process) {
    SU_CE_INIT(process, "su\\process");
    zend_declare_property_null(su_process_ce, ZEND_STRL("_file"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(su_process_ce, ZEND_STRL("_args"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(su_process_ce, ZEND_STRL("_options"), ZEND_ACC_PUBLIC);
}
/* }}} */
