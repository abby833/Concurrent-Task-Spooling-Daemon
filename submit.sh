#!/bin/sh
# submit.sh: usage ./submit.sh <id> < commands.txt
id="$1"
cat > "spool/.$id.tmp"                       # write the whole file first...
mv "spool/.$id.tmp" "spool/inbox/$id.request"  # ...then publish it atomically