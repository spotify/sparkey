AC_INIT([libsparkey], [0.1.0], [krka@spotify.com], [sparkey])
AM_INIT_AUTOMAKE([-Werror foreign])
LT_INIT
AC_PROG_CC_C99

AC_CHECK_PROGS([DOXYGEN],[doxygen])
if test -z "$DOXYGEN";
   then AC_MSG_WARN([Doxygen not found - continuing without Doxygen support])
fi

AM_CONDITIONAL([HAVE_DOXYGEN],
   [test -n "$DOXYGEN"])
   AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([Doxyfile])
])



AC_SEARCH_LIBS([snappy_compress],
  [snappy],,[AC_MSG_ERROR([Could not find snappy])
])

AM_CONDITIONAL([APPLE], [test x$build_vendor = xapple])
AM_COND_IF([APPLE], [
],[
   AC_SEARCH_LIBS([clock_gettime],
     [rt],,[AC_MSG_ERROR([Could not find rt])
   ])
])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
AC_OUTPUT
