#!/bin/sh

if [ "$1" = "release" ]; then
	OPTIONS="-O2 -s"
else
	OPTIONS="-Wall"
fi

x86_64-w64-mingw32-gcc $OPTIONS -o DBExec.exe ../database.c db_exec.c -lodbc32