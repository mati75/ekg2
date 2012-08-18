AC_DEFUN([AC_EKG2_FASTCONF], [
	AC_ARG_ENABLE([fast-configure],
		AS_HELP_STRING([--enable-fast-configure],
			[enable non-thorough (fast) configure tests [default=off]]))
])

AC_DEFUN([AC_EKG2_CHECK_LIB], [
dnl AC_EKG2_CHECK_LIB(name, func, headers, [if-yes], [if-no])
dnl Perform AC_CHECK_HEADERS & AC_CHECK_LIB for a particular library. Run
dnl 'if-yes' if both checks succeed (if not specified, defaults to AC_CHECK_LIB
dnl default commands); otherwise run 'if-no'.

	AC_REQUIRE([AC_EKG2_FASTCONF])
	found_any_header=no

	AC_CHECK_HEADERS([$3], [
		AS_IF([test "x$enable_fast_configure" != "xyes"], [
			AC_CHECK_LIB([$1], [$2], [$4], [$5])
		], [
			LIBS="-l$1 $LIBS"
			AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_LIB$1), [1], [define if you have $1])
			$4
		])

		found_any_header=yes
		break
	])

	AS_IF([test $found_any_header != yes], [
		$5
	])
	AS_UNSET([found_any_header])
])

AC_DEFUN([AC_EKG2_CHECK_FLAGEXPORTED_LIB], [
dnl AC_EKG2_CHECK_FLAGEXPORTED_LIB(variable-prefix, lib-name, func, header, [if-yes], [if-no])

	AC_REQUIRE([AC_EKG2_FASTCONF])
	pc_save_CPPFLAGS=$CPPFLAGS
	CPPFLAGS="$$1_CFLAGS $CPPFLAGS"

	AS_IF([test "x$enable_fast_configure" != "xyes"], [
		AC_CHECK_HEADER([$4], [
			pc_save_LIBS=$LIBS
			LIBS="$$1_LIBS $LIBS"

			AC_CHECK_FUNC([$3], [
				m4_ifval([$2], [
					AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_LIB$2), [1], [define if you have $1])
				])

				$5
			], [
				LIBS=$pc_save_LIBS
				CPPFLAGS=$pc_save_CPPFLAGS

				$6
			])
		], [
			CPPFLAGS=$pc_save_CPPFLAGS

			$6
		])
	], [
		LIBS="$$1_LIBS $LIBS"
		m4_ifval([$2], [
			AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_LIB$2), [1], [define if you have $1])
		])

		$5
	])
])

AC_DEFUN([AC_EKG2_CHECK_PKGCONFIG_LIB], [
dnl AC_EKG2_CHECK_PKGCONFIG_LIB(pkg-name, fallback-name, func, header, [if-yes], [if-no], [if-fallback-yes])
dnl Perform PKG_CHECK_MODULES for <pkg-name>, grabbing LIBS & CFLAGS there; then
dnl perform an AC_CHECK_HEADERS & AC_CHECK_FUNC to check if the obtained data is
dnl useful.
dnl
dnl If the pkg-config call fails to grab needed data, fallback to
dnl AC_EKG2_CHECK_LIB. If the fallback succeeds to find the library, run either
dnl <if-fallback-yes> if specified or <if-yes> otherwise.

	PKG_CHECK_MODULES(AS_TR_SH([$1]), [$1], [
		AC_EKG2_CHECK_FLAGEXPORTED_LIB(AS_TR_SH([$1]), [$2], [$3], [$4], [$5], [$6])
	], [
		AC_EKG2_CHECK_LIB([$2], [$3], [$4], m4_default([$7], [$5]), [$6])
	])
])
