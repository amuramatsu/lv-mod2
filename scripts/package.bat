@echo off
setlocal

cd /d "%~dp0.."

if exist "C:\Program Files\7-Zip\7z.exe" (
	set "sz=C:\Program Files\7-Zip\7z.exe"
) else if exist "C:\Program Files (x86)\7-Zip\7z.exe" (
	set "sz=C:\Program Files (x86)\7-Zip\7z.exe"
)

for /f %%i in ('git describe --contains') do set tag=%%i
rem Remove dots
set tag=%tag:.=%

copy /y src\lv.exe .
copy /y src\lv.exe.manifest .
"%sz%" a "%tag%-win32.zip" GPL.txt hello.sample hello.sample.gif index.html lv.exe lv.exe.manifest lv.hlp README README.md relnote.html
