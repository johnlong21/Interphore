pushd .
IF EXIST D:\_tools\_sdks\emsdk-portable (
cd /d D:\_tools\_sdks\emsdk-portable
) ELSE (
cd /d D:\tools\sdks\emsdk-portable
)
call emsdk.bat update
call emsdk.bat install latest
call emsdk.bat activate latest
call emsdk_env.bat
popd
