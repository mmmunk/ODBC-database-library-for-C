#!/bin/sh

if [ "$1" = "release" ]; then
	OPTIONS="-march=native -mtune=native -O2 -s"
else
	OPTIONS="-Wall"
fi

gcc $OPTIONS -o db_exec ../database.c db_exec.c -lodbc
