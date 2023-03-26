@ECHO OFF

REM The Visual Studio project automatically runs this script after building

if not exist ..\..\System\xopengl mkdir ..\..\System\xopengl
copy /Y ..\Shaders\*.* ..\..\System\xopengl\
copy /Y ..\XOpenGLDrv.int ..\..\System\
