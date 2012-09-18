AC_PREREQ([2.60])
AC_INIT([backup-tool], [0.1], [Shrewd LLC, manvscode@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC

AM_PROG_CC_STDC
AC_HEADER_STDC

CFLAGS="-Wall -O2 -D_NO_FILE_STDIO_STREAM `pkg-config --cflags glib-2.0` `pkg-config --cflags libxml-2.0` ${CFLAGS}"
LDFLAGS=" -lcurl -lcrypto -lssl `pkg-config --libs glib-2.0` `pkg-config --libs libxml-2.0` ${LDFLAGS}"

AC_PROG_RANLIB

dnl AM_SILENT_RULES([yes])

AC_CHECK_HEADERS([glib.h])
AC_CHECK_HEADERS([getopt.h])

dnl We need glib for configuration file 
AC_CHECK_LIB([glib-2.0], [g_key_file_new], [glib_error=no], [glib_error=yes])
AC_CHECK_LIB([crypto], [BIO_new], [libcrypto_error=no], [libcrypto_error=yes])

if test "$libcrypto_error" = "yes"; then
	echo ""
	AC_MSG_WARN([********* Your system doesn't have libcrypto install. You need it to compile.])
	echo " CFLAGS = ${CFLAGS}"
	echo "LDFLAGS = ${LDFLAGS}"
	echo ""
	exit
fi

if test "$glib_error" = "yes"; then
	echo ""
	AC_MSG_WARN([********* Your system doesn't have Gnome library (glib) library. You need it to compile.])
	echo " CFLAGS = ${CFLAGS}"
	echo "LDFLAGS = ${LDFLAGS}"
	echo ""
	exit
fi

dnl We need glib for configuration file 
AC_CHECK_LIB([xml2], [xmlInitParser], [libxml2_error=no], [libxml2_error=yes])

if test "$libxml2_error" = "yes"; then
	echo ""
	AC_MSG_WARN([********* Your system doesn't have libxml2. You need it to compile.])
	echo " CFLAGS = ${CFLAGS}"
	echo "LDFLAGS = ${LDFLAGS}"
	echo ""
	exit
fi

dnl We need libcurl
AC_CHECK_LIB([curl], [curl_easy_init], [curl_error=no], [curl_error=yes])
AC_CHECK_HEADERS([curl/curl.h])

if test "$curl_error" = "yes"; then
	echo ""
	AC_MSG_WARN([********* Your system doesn't have cURL library. You need it to compile.])
	echo " CFLAGS = ${CFLAGS}"
	echo "LDFLAGS = ${LDFLAGS}"
	echo ""
	exit
fi

dnl We need libssl (openssl)
AC_CHECK_LIB([ssl], [HMAC_Init], [openssl_error=no], [openssl=yes])

if test "$openssl_error" = "yes"; then
	echo ""
	AC_MSG_WARN([********* Your system doesn't have OpenSSL library. You need it to compile.])
	echo " CFLAGS = ${CFLAGS}"
	echo "LDFLAGS = ${LDFLAGS}"
	echo ""
	exit
fi

AC_CONFIG_HEADERS([config.h])

AC_PROG_INSTALL

AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT
