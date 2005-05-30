#!/bin/sh

# Run this when building from subversion.  Don't commit
# any of the files it generates.

#
# Require libtool, automake, and autoconf
#
(libtoolize --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have libtool installed to compile GayM.";
	echo;
	exit;
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have automake installed to compile GayM.";
	echo;
	exit;
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have autoconf installed to compile GayM.";
	echo;
	exit;
}

echo;
echo "Generating configuration files for GayM, please wait...."

echo;
echo "Running libtoolize, please ignore non-fatal messages...."
echo;
libtoolize --force || exit;

#
# Account for odd location of macros
#
for dir in "/usr/local/share/aclocal" \
	"/opt/gnome-1.4/share/aclocal" \
	"/sw/share/aclocal"
do
	if test -d $dir ; then
		ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $dir"
	fi
done

echo;
echo "Running aclocal...."
echo;
aclocal $ACLOCAL_FLAGS || exit;

echo;
echo "Running autoheader...."
echo;
autoheader --force || exit;

echo;
echo "Running automake...."
echo;
automake --force-missing --add-missing;

echo;
echo "Running autoconf...."
echo;
autoconf --force || exit;

#
# Apparently, running automake a second time like this fixes 
# distcheck for automake 1.5 and doesn't break it for 1.4.
#
echo;
echo "Running automake again...."
echo;
automake || exit;

echo;
echo "Finished generating configuration files for GayM."
echo "Now you may run './configure'."
echo;

