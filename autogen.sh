#!/bin/sh
#
# $Id$

set -e

: ${AUTOCONF=autoconf}
: ${AUTOHEADER=autoheader}
: ${AUTOMAKE=automake}
: ${ACLOCAL=aclocal}
: ${GETTEXTIZE=gettextize}
: ${AUTOPOINT=autopoint}
: ${XGETTEXT=xgettext}
: ${LIBTOOLIZE=libtoolize}

if test "$*"; then
	ARGS="$*"
else
	test -f config.log && ARGS=`grep '^  \$ \./configure ' config.log | sed 's/^  \$ \.\/configure //' 2> /dev/null`
fi

echo "Running libtoolize..."
$LIBTOOLIZE --force --automake --copy || exit 1

echo "Running gettextize..."
# Ensure that gettext is reasonably new.
gettext_ver=`$GETTEXTIZE --version | \
	sed '2,$d;# remove all but the first line
		s/.* //;# take text after the last space
		s/-.*//;# strip "-pre" or "-rc" at the end
		s/\([^.][^.]*\)/0\1/g;# prepend 0 to every token
		s/0\([^.][^.]\)/\1/g;# strip leading 0 from long tokens
		s/$/.00.00/;# add .00.00 for short version strings
		s/\.//g;# remove dots
		s/\(......\).*/\1/;# leave only 6 leading digits
		'`

if test -z "$gettext_ver"; then
	echo "Cannot determine version of gettext" 2>&1
	exit 1
fi

if test "$gettext_ver" -lt 01038; then
	echo "Don't use gettext older than 0.10.38" 2>&1
	exit 1
fi

rm -rf intl
can_autopoint=0
if test "$gettext_ver" -ge 01100; then
	if test "$gettext_ver" -lt 01105; then
		echo "Upgrade gettext to at least 0.11.5 or downgrade to 0.10.40" 2>&1
		exit 1
	fi

	autopoint_ver=`$AUTOPOINT --version`
	if echo "${autopoint_ver}" | grep -q git; then
		if git --version >/dev/null 2>&1; then
			can_autopoint=1
		fi
	elif echo "${autopoint_ver}" | grep -q cvs; then
		if cvs -v >/dev/null 2>&1; then
			can_autopoint=1
		fi
	else # dir format
		can_autopoint=1
	fi
fi

if test $can_autopoint = 1; then
	$AUTOPOINT --force || exit 1
else
	$GETTEXTIZE --copy --force || exit 1
	if test -e po/ChangeLog~; then
		rm -f po/ChangeLog
		mv po/ChangeLog~ po/ChangeLog
	fi
fi

# Generate po/POTFILES.in
echo "Generating po/POTFILES.in"

# Ensure that gettext is reasonably new.
xgettext_ver=`$XGETTEXT --version | \
	sed '2,$d;# remove all but the first line
		s/.* //;# take text after the last space
		s/-.*//;# strip "-pre" or "-rc" at the end
		s/\([^.][^.]*\)/0\1/g;# prepend 0 to every token
		s/0\([^.][^.]\)/\1/g;# strip leading 0 from long tokens
		s/$/.00.00/;# add .00.00 for short version strings
		s/\.//g;# remove dots
		s/\(......\).*/\1/;# leave only 6 leading digits
		'`

if test -z "$xgettext_ver"; then
        echo "Cannot determine version of gettext" 2>&1
        exit 1
fi

if test "$xgettext_ver" -gt 01200; then
	XGETTEXT_OPTIONS="--from-code=iso-8859-2"
fi

$XGETTEXT --keyword=_ --keyword=N_ --output=- $XGETTEXT_OPTIONS `find . -name '*.[ch]'` | \
	sed -ne '/^#:/{s/#://; s/:[0-9]*/\
/g; s/ //g; p;}' | \
	grep -v '^$' | sort | uniq | grep -v 'regex.c' | grep -v '^contrib' >po/POTFILES.in


if test ! -r m4/gettext.m4; then
        if test -r /usr/share/aclocal/gettext.m4; then
                cp /usr/share/aclocal/gettext.m4 m4/gettext.m4
        else
		echo "gettext.m4 wasn't found - copy it manualy to m4/"
		exit 1
        fi
fi

echo "Running aclocal..."
$ACLOCAL -I m4 || exit 1 

echo "Running autoheader..."
$AUTOHEADER || exit 1 

echo "Running automake..."
$AUTOMAKE --foreign --add-missing || exit 1

echo "Running autoconf..."
$AUTOCONF || exit 1

test x$NOCONFIGURE = x && echo "Running ./configure $ARGS" && ./configure $ARGS
