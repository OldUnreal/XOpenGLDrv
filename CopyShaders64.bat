@ECHO OFF

REM The Visual Studio project automatically runs this script after building

if not exist ..\..\System64\xopengl mkdir ..\..\System64\xopengl
copy /Y ..\Shaders\*.* ..\..\System64\xopengl\
copy /Y ..\XOpenGLDrv.int ..\..\System64\
