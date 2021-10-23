set vcroot=C:\Program Files (x86)\Microsoft Visual Studio\2017
set sdkroot=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A

call "%vcroot%\Community\Common7\Tools\VsDevCmd.bat"
cd bin\box2d_win32
msbuild.exe Box2D.sln
