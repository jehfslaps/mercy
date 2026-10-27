#!/bin/bash
# Run a CI step and, if it fails, put the last 40 lines of its output into a GitHub annotation
# (annotations are readable through the API even where log downloads aren't).
#   tools/ci_run.sh "title" command args...
title=$1; shift
log=$(mktemp)
"$@" 2>&1 | tee "$log"
rc=${PIPESTATUS[0]}
if [ "$rc" -ne 0 ]; then
  msg=$(tail -n 40 "$log" | sed -e 's/%/%25/g' -e 's/\r//g' | awk '{printf "%s%%0A", $0}')
  echo "::error title=$title failed::$msg"
fi
exit "$rc"
