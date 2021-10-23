set engineIncludePath=%1
set engineLibPath=%2
set extraDefines=%3

pushd .
IF EXIST D:\_tools\_sdks\emsdk-portable (
cd /d D:\_tools\_sdks\emsdk-portable
) ELSE (
cd /d D:\tools\sdks\emsdk-portable
)
call emsdk_env.bat
call emsdk.bat activate latest
popd
em++ -Wall -Wno-sign-compare ^
	-I%engineIncludePath%/glIncludes -I%engineIncludePath%/libIncludes -isystem%engineLibPath% -Wno-unused-variable -I%engineIncludePath%\..\src ^
	-D HTML5_BUILD -D OPENGL_BUILD %extraDefines% -s USE_GLFW=3 -s FULL_ES3=1 -s ASYNCIFY=1 -s TOTAL_MEMORY=268435456 -s ASSERTIONS=1 ^
	src/main.cpp -o bin/webgl/engine.js
