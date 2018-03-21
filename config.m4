dnl $Id$
dnl config.m4 for extension su

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(su, for su support,
dnl Make sure that the comment is aligned:
dnl [  --with-su             Include su support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(su, whether to enable su support,
dnl Make sure that the comment is aligned:
[  --enable-su           Enable su support])

DEPS_DIR="$srcdir/deps"

AC_DEFUN([SU_CHECK_REUSEPORT],
[
	AC_MSG_CHECKING([for reuseport])

	AC_TRY_COMPILE([
		#include <sys/socket.h>
	], [
		int on = 1;
		setsockopt(0, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
	], [
		AC_DEFINE([HAVE_REUSEPORT], 1, [have SO_REUSEPORT?])
		AC_MSG_RESULT([yes])
	], [
		AC_MSG_RESULT([no])
	])
])

if test "$PHP_SU" != "no"; then
	SU_CHECK_REUSEPORT

	PHP_ADD_INCLUDE("$DEPS_DIR/libctx")
	PHP_ADD_INCLUDE("$DEPS_DIR/libuv/include")
	PHP_SUBST(SU_SHARED_LIBADD)
	shared_objects_su="$DEPS_DIR/build/lib/libctx.a $DEPS_DIR/build/lib/libuv.a $shared_objects_su"

	PHP_SUBST([CFLAGS])

	PHP_NEW_EXTENSION(su, 
		su.c \
		src/chan.c \
		src/co.c \
		src/coco/coco.c \
		src/fs.c \
		src/fs_watcher.c \
		src/pipe.c \
		src/pipe_server.c \
		src/process.c \
		src/rbuf/rbuf.c \
		src/tcp_conn.c \
		src/tcp_server.c \
		src/timer.c \
		src/udp.c \
	, $ext_shared)

	PHP_ADD_MAKEFILE_FRAGMENT([deps/Makefile])
fi

