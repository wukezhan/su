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

#include "src/fs_watcher.h"
#include "src/fs.h"

SU_CLASS_D(fs);

static zend_object* su_fs_create(zend_class_entry *ce) {
    su_fs_t *fs = (su_fs_t *)su_ecalloc(su_fs_t);
    SU_OBJ_INIT_STD(fs, ce, su_fs_handlers);
    fs->self = (zval *)su_malloc(sizeof(zval));
    fs->read_offset = -1;
    fs->write_offset = -1;

    return &fs->std;
}

void su_fs_close(su_fs_t* fs) {
    if (fs->status == SU_FS_STOPED) {
        return ;
    }
    fs->status = SU_FS_STOPED;
    if (fs->open.result > 0) {
        close(fs->open.result);
        fs->open.result = 0;
    }
    if (fs->read_buf.len) {
        fs->read_buf.len = 0;
        efree(fs->read_buf.base);
    }
    SU_TRY_FREE_CBS(fs->cbs);
    zval *self = (zval *)fs->self;
    if (self) {
        zval_ptr_dtor(self);
        su_free(self);
        fs->self = NULL;
    }
}

void su_fs_free(zend_object *std) {
    su_fs_t *fs = SU_OBJ_FROM_STD(std, su_fs_t);
    su_fs_close(fs);
    SU_OBJ_FREE_STD(fs);
    su_free(fs);
}

/* {{{ ARG_INFO */
#if 0
SU_BEGIN_ARG_INFO_EX(fs_construct_arginfo, 0, 0, 0)
    SU_ARG_INFO(0, config)
SU_END_ARG_INFO()
#endif
/* }}} */

/* {{{ PHP METHODS */
SU_METHOD(fs, __construct) {
    SU_SET_SELF;
    SU_RET_SELF;
}

SU_METHOD(fs, __destruct) {
    SU_SET_SELF;
    SU_RET_SELF;
}

#define su_fs_is_opened(fs) (fs->open.result > 0)
#define su_fs_sf_to_nf(sf, nf) do {                                                             \
    if (sf == NULL || su_cmp("r", sf)) nf = O_RDONLY;                                           \
    else if (su_cmp("rs", sf) || su_cmp("sr", sf)) nf = O_RDONLY | O_SYNC;                      \
    else if (su_cmp("r+", sf)) nf = O_RDWR;                                                     \
    else if (su_cmp("rs+", sf) || su_cmp("sr+", sf)) nf = O_RDWR | O_SYNC;                      \
    else if (su_cmp("w", sf)) nf = O_TRUNC | O_CREAT | O_WRONLY;                                \
    else if (su_cmp("wx", sf) || su_cmp("xw", sf)) nf = O_TRUNC | O_CREAT | O_WRONLY | O_EXCL;  \
    else if (su_cmp("w+", sf)) nf = O_TRUNC | O_CREAT | O_RDWR;                                 \
    else if (su_cmp("wx+", sf) || su_cmp("xw+", sf)) nf = O_TRUNC | O_CREAT | O_RDWR | O_EXCL;  \
    else if (su_cmp("a", sf)) nf = O_APPEND | O_CREAT | O_WRONLY;                               \
    else if (su_cmp("ax", sf) || su_cmp("xa", sf)) nf = O_APPEND | O_CREAT | O_WRONLY | O_EXCL; \
    else if (su_cmp("a+", sf)) nf = O_APPEND | O_CREAT | O_RDWR;                                \
    else if (su_cmp("ax+", sf) || su_cmp("xa+", sf)) O_APPEND | O_CREAT | O_RDWR | O_EXCL;      \
    else nf = -1;                                                                               \
} while(0)

SU_METHOD(fs, open) {
    SU_SET_SELF;
    zend_string *path;
    zend_string *flags = NULL;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|f", &path, &flags, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::open($path, $flags, $callback=NULL) param", SU_STATIC_CLASS());
    }

    su_fs_t *fs = SU_OBJ_FROM_SELF(su_fs_t);
    int nargs = ZEND_NUM_ARGS();
    if (su_fs_is_opened(fs)) {
        php_error(E_WARNING, "fs is opened");
        RETURN_FALSE;
    }

    int nflags = 0;
    su_fs_sf_to_nf(flags, nflags);
    if (nflags == -1) {
        php_error(E_WARNING, "invalid open flags");
        RETURN_FALSE;
    }
    fs->open_flags = nflags;

    int r = uv_fs_open(SU_G(loop), &fs->open, ZSTR_VAL(path), nflags, 0, NULL);
    if (fs->open.result) {
        uv_fs_req_cleanup(&fs->read);
    }
    RETURN_LONG(fs->open.result);
}

void su_fs_read_co(void *arg) {
    su_fs_t *fs = (su_fs_t *)arg;
    zval buf;
    zval err;
    ZVAL_LONG(&err, 0);
    if (fs->read.result > 0) {
        ZVAL_STRINGL(&buf, fs->read_buf.base, fs->read.result);
    } else if (fs->read.result == 0) {
        ZVAL_STRINGL(&buf, "", 0);
    } else {
        ZVAL_LONG(&err, fs->read.result);
    }
    fs->status &= ~SU_FS_READING;
    zval params[2] = { buf, err };
    zval retval;
    su_call_cb(fs->cbs[EV_FS_READ], &retval, 2, params);
    zval_ptr_dtor(&buf);
    zval_ptr_dtor(&retval);
}

void su_fs_read_cb(uv_fs_t *req) {
    uv_fs_req_cleanup(req);
    su_fs_t *fs = CONTAINER_OF(req, su_fs_t, read);
    coco_create(su_fs_read_co, fs, SU_COCO_STACK);
}

SU_METHOD(fs, read) {
    SU_SET_SELF;
    zend_string *path;
    zend_string *flags;
    zend_long size = 0;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|lf", &size, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::read($size, $callback=NULL) param", SU_STATIC_CLASS());
    }

    if (size == 0) {
        size = SU_G(fs_read_buf_size);
    }

    int nargs = ZEND_NUM_ARGS();
    su_fs_t *fs = SU_OBJ_FROM_SELF(su_fs_t);
    if (!su_fs_is_opened(fs)) {
        if (nargs < 2) {
            RETURN_FALSE;
        }
    }

    if (fs->status & SU_FS_READING) {
        php_error(E_WARNING, "can not read before previous request complete");
        RETURN_FALSE;
    }
    fs->status |= SU_FS_READING;

    if (!fs->read_buf.len) {
        fs->read_buf.len = size * sizeof(char);
        fs->read_buf.base = (char *)emalloc(fs->read_buf.len);
    } else if (fs->read_buf.len != size * sizeof(char)) {
        efree(fs->read_buf.base);
        fs->read_buf.len = size * sizeof(char);
        fs->read_buf.base = (char *)emalloc(fs->read_buf.len);
    }
    if (nargs == 2) {
        // 增加判断
        if (!fs->cbs[EV_FS_READ]) {
            if (fs->cbs[EV_FS_READ]) {
                //php_var_dump(&fs->cbs[EV_FS_READ]->fci.function_name, 1);
            }
        }
        su_set_cb(fs->cbs, EV_FS_READ, &cb);
        int r = uv_fs_read(SU_G(loop), &fs->read, fs->open.result, &fs->read_buf, 1, fs->read_offset, su_fs_read_cb);
    } else {
        int r = uv_fs_read(SU_G(loop), &fs->read, fs->open.result, &fs->read_buf, 1, fs->read_offset, NULL);
        RETVAL_STRINGL(fs->read_buf.base, fs->read.result);
        fs->status &= ~SU_FS_READING;
    }
    fs->read_offset = -1;
}

void su_fs_write_co(void *arg) {
    su_fs_t *fs = (su_fs_t *)arg;
    zval len;
    zval err;
    ZVAL_LONG(&err, 0);
    if (fs->write.result < 0) {
        ZVAL_LONG(&err, fs->write.result);
        ZVAL_LONG(&len, 0);
    } else {
        ZVAL_LONG(&len, fs->write.result);
        ZVAL_LONG(&err, 0);
    }
    zend_string_release(fs->write_buf.zstr);
    fs->write_buf.zstr = NULL;
    fs->status &= ~SU_FS_WRITING;
    zval params[2] = { len, err };
    zval retval;
    su_call_cb(fs->cbs[EV_FS_WRITE], &retval, 2, params);
    zval_ptr_dtor(&retval);
}

void su_fs_write_cb(uv_fs_t *req) {
    uv_fs_req_cleanup(req);
    su_fs_t *fs = CONTAINER_OF(req, su_fs_t, write);
    coco_create(su_fs_write_co, fs, SU_COCO_STACK);
}

SU_METHOD(fs, write) {
    SU_SET_SELF;
    zend_string *buf;
    su_cb_t cb;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|f", &buf, &cb.fci, &cb.fcic) == FAILURE) {
        php_error(E_ERROR, "invalid %s::read($size, $callback=NULL) param", SU_STATIC_CLASS());
    }

    int nargs = ZEND_NUM_ARGS();
    su_fs_t *fs = SU_OBJ_FROM_SELF(su_fs_t);
    if (!su_fs_is_opened(fs)) {
        if (nargs < 2) {
            RETURN_FALSE;
        }
    }

    if (!(fs->open_flags & (O_RDWR | O_WRONLY) )) {
        if (nargs < 2) {
            /* should be writable */
            RETURN_FALSE;
        }
    }

    if (fs->status & SU_FS_WRITING) {
        php_error(E_WARNING, "can not write before previous request complete");
        RETURN_FALSE;
    }
    fs->status |= SU_FS_WRITING;

    zend_string_addref(buf);
    fs->write_buf.zstr = buf;
    fs->write_buf.buf.base = ZSTR_VAL(buf);
    fs->write_buf.buf.len = ZSTR_LEN(buf);

    if (nargs == 2) {
        // 增加判断
        if (!fs->cbs[EV_FS_WRITE] || fs->cbs[EV_FS_WRITE]) {
            if (fs->cbs[EV_FS_WRITE]) {
                //php_var_dump(&fs->cbs[EV_FS_READ]->fci.function_name, 1);
            }
        }
        su_set_cb(fs->cbs, EV_FS_WRITE, &cb);
        int r = uv_fs_write(SU_G(loop), &fs->write, fs->open.result, &fs->write_buf.buf, 1, fs->write_offset, su_fs_write_cb);
    } else {
        int r = uv_fs_write(SU_G(loop), &fs->write, fs->open.result, &fs->write_buf.buf, 1, fs->write_offset, NULL);
        fs->status &= ~SU_FS_WRITING;
    }
    fs->write_offset = -1;
}

SU_METHOD(fs, set_offset) {
    SU_SET_SELF;
    zend_long offset = -1;
    zend_long type = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|l", &offset, &type) == FAILURE) {
        php_error(E_ERROR, "invalid %s::set_offset($offset, $type=0) param", SU_STATIC_CLASS());
    }
    su_fs_t *fs = SU_OBJ_FROM_SELF(su_fs_t);
    if (type == 1) {
        fs->write_offset = offset;
    } else {
        fs->read_offset = offset;
    }
    SU_RET_SELF;
}

SU_METHOD(fs, stat) {
}

SU_METHOD(fs, close) {
    SU_SET_SELF;
    su_fs_t *fs = SU_OBJ_FROM_SELF(su_fs_t);
    su_fs_close(fs);
}

/* }}} */

/* {{{ uv_fs_methods */
zend_function_entry su_fs_methods[] = {
    SU_ME(fs, __construct, NULL,    ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SU_ME(fs, __destruct, NULL,    ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    SU_ME(fs, open, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(fs, read, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(fs, write, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(fs, set_offset, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(fs, stat, NULL,    ZEND_ACC_PUBLIC)
    SU_ME(fs, close, NULL,    ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ SU_MINIT_FUNCTION */
SU_MINIT_FUNCTION(fs) {
    SU_CE_INIT(fs, "su\\fs");

    return SUCCESS;
}
/* }}} */
