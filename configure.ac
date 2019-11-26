AC_INIT([gromox], [0])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux])
AM_INIT_AUTOMAKE([1.10 foreign subdir-objects])
LT_INIT([disable-static])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX

my_CXXFLAGS="-Wall -Wmissing-declarations"
AC_MSG_CHECKING([available C++ standard])
cxxmode=""
for i in "c++17" "c++14"; do
	CXXFLAGS="$CXXFLAGS_system -std=$i"
	AC_COMPILE_IFELSE([AC_LANG_SOURCE([])], [cxxmode="$i"])
	AS_IF([test -n "$cxxmode"], [break])
done
CXXFLAGS="$CXXFLAGS_system"
AS_IF([test -n "$cxxmode"], [
	my_CXXFLAGS="$my_CXXFLAGS -std=$cxxmode"
	AC_MSG_RESULT([$cxxmode])
])

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([HX], [libHX >= 1.10])
PKG_CHECK_MODULES([archive], [libarchive])
PKG_CHECK_MODULES([crypto], [libcrypto])
PKG_CHECK_MODULES([gumbo], [gumbo])
PKG_CHECK_MODULES([sqlite], [sqlite3])
PKG_CHECK_MODULES([ssl], [libssl])
PKG_CHECK_MODULES([xls], [libxls])
PKG_CHECK_MODULES([zlib], [zlib])
AC_CHECK_LIB([spf2], [SPF_debugx2], [spf_LIBS=-lspf2], [AC_MSG_ERROR([libspf2 not found])])
AC_SUBST([spf_LIBS])
AC_DEFUN([MYSQL_WITH_MYSQL_CONFIG],[
	AC_PATH_PROGS([MYSQL_CONFIG], [mysql_config mariadb_config])
	AC_ARG_WITH([mysql-config], AS_HELP_STRING([--with-mysql-config=PATH], [path to the mysql-config script of mysql]),
		[MYSQL_CONFIG="$withval"])
	AS_IF([test -z "$MYSQL_CONFIG"], [AC_MSG_ERROR([mysql config program not found])])
	mysql_CFLAGS=$($MYSQL_CONFIG --include)
	mysql_LIBS=$($MYSQL_CONFIG --libs)
	mysql_VERSION=$($MYSQL_CONFIG --version) # see if it's there

	if test -z "$mysql_VERSION"; then
		AC_MSG_ERROR([Cannot find a mysql installation. Please use --with-mysql-config=PATH or install libmysqlclient-dev.])
	fi

	AC_SUBST([mysql_CFLAGS])
	AC_SUBST([mysql_LIBS])
	AC_SUBST([mysql_VERSION])
	AC_MSG_CHECKING([for MySQL includes])
	AC_MSG_RESULT([$mysql_CFLAGS])
	AC_MSG_CHECKING([for MySQL libs])
	AC_MSG_RESULT([$mysql_LIBS])
])
MYSQL_WITH_MYSQL_CONFIG
AC_ARG_WITH([php], [AS_HELP_STRING([--without-php], [Disable building PHP components])])
AS_IF([test "$with_php" != "no"], [
	# Checks for PHP through php-config and tests the truth of
	# php-config results
	PHP_WITH_PHP_CONFIG
	PHP_CHECK_INCLUDES
	echo "PHP version: $PHP_VERSION"
	AS_CASE([$PHP_VERSION],
		[5.*], [AC_MSG_ERROR([php5 is not supported])],
		[7.*], [with_php=7]
	)
])

AC_SUBST([my_CXXFLAGS])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT