set vcroot=C:\Program Files (x86)\Microsoft Visual Studio\2017
set sdkroot=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A

pushd .
call "%vcroot%\Community\Common7\Tools\VsDevCmd.bat"
popd
devenv /Run bin\engine.exe
