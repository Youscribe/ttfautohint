#! /bin/sh
#
# strip-comments.sh <file1> <file2> ... > result
#
# This simple script strips off all HTML comments except in the first file.

cat $1

shift

while [ $# -gt 0 ]; do
  cat $1 \
  | sed -e '/<!--.*-->/d' \
        -e '/<!--/,/-->/d'
  shift
done

# eof
