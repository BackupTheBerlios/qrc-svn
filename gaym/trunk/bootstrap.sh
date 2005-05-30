#!/bin/sh -e

# Run this when building from subversion.  Don't commit
# any of the files it generates.

libtoolize
aclocal
autoheader
automake --add-missing
autoconf

echo "Bootstrapping successful."
