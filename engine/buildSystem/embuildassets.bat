pushd .
IF EXIST D:\_tools\_sdks\emsdk-portable (
cd /d D:\_tools\_sdks\emsdk-portable
) ELSE (
cd /d D:\tools\sdks\emsdk-portable
)
call emsdk.bat activate latest
call emsdk_env.bat
popd

em++ -Wall -O1 -Wno-sign-compare -s ASYNCIFY=1 -s TOTAL_MEMORY=268435456 ^
	bin/assetLib.cpp -o bin/assetLibWebgl.o
