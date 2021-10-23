@echo off
set clArgs=%*

set vcroot=C:\Program Files (x86)\Microsoft Visual Studio\2017
set sdkroot=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A

pushd .
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat"
popd

REM -Od ???
REM -Oi Do intrinsics
REM -GR- Turn off RTTI
REM -MT The right one
REM -Z7 Add debug info the old way
REM -Gm No build cache
REM -EHsc No C++ Exceptions

rem @echo on
cl %clArgs%
