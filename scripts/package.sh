#! /bin/sh

tag=$(git describe|sed 's/\.//g')
cp src/lv.exe .
cp src/lv.exe.manifest .
7z a "${tag}-win32.zip" GPL.txt hello.sample hello.sample.gif index.html lv.exe lv.exe.manifest lv.hlp README README.md relnote.html
