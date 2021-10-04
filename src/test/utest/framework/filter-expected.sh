#!/bin/sh

# This little script generates the 'expected' output for mapdb-related tests.
# The following modifications are done:
#  - CRLF line endings are converted to LF
#  - Lines beginning with '[DONTCHECK]' are removed and thus not checked.
#  - Lines beginning with 'MDB:' are removed and thus not checked.
#  - The characters '*', '+', '[', ']' are quoted.
#  - Trailing spaces are removed.
#  - At the begin of each line, '.*' is inserted.
# Only content between 'KUT TAP TEST START' and 'KUT TAP TEST FINISHED' is
# considered.

sed -n \
    -e '/^KUT TAP TEST START/,/KUT TAP TEST FINISHED/!d' \
    -e '/\[DONTCHECK\]/d' \
    -e '/^MDB:.*/d' \
    -e 's/\*/\\*/g' \
    -e 's/\+/\\+/g' \
    -e 's/\[/\\[/g' \
    -e 's/\]/\\]/g' \
    -e 's/\r$//g' \
    -e 's/ *$//g' \
    -e 's/^/.*/' \
    -e 'p' $@
