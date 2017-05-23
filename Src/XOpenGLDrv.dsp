# Microsoft Developer Studio Project File - Name="XOpenGLDrvDrv" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=XOpenGLDrvDrv - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "XOpenGLDrvDrv.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "XOpenGLDrvDrv.mak" CFG="XOpenGLDrvDrv - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "XOpenGLDrvDrv - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "XOpenGLDrvDrv - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/UT420/XOpenGLDrvDrv/Src", HFMAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "XOpenGLDrvDrv - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\Lib"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XOpenGLDrvDRV_EXPORTS" /YX /FD /c
# ADD CPP /nologo /Zp4 /MD /W4 /WX /vd0 /GX /O2 /Ob2 /I "..\..\Core\Inc" /I "..\..\Engine\Inc" /D "NDEBUG" /D "_WINDOWS" /D "WIN32" /D "UNICODE" /D "_UNICODE" /D ThisPackage=XOpenGLDrvDrv /Yu"XOpenGLDrvDrv.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 ..\..\Core\Lib\Core.lib ..\..\Engine\Lib\Engine.lib user32.lib gdi32.lib /nologo /dll /incremental:yes /machine:I386 /out:"..\..\System\XOpenGLDrvDrv.dll"

!ELSEIF  "$(CFG)" == "XOpenGLDrvDrv - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Lib"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XOpenGLDrvDRV_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /Zp4 /MDd /W4 /WX /Gm /vd0 /GX /ZI /Od /I "..\..\Core\Inc" /I "..\..\Engine\Inc" /D "_DEBUG" /D "_WINDOWS" /D "WIN32" /D "UNICODE" /D "_UNICODE" /D ThisPackage=XOpenGLDrvDrv /Yu"XOpenGLDrvDrv.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ..\..\Core\Lib\Core.lib ..\..\Engine\Lib\Engine.lib user32.lib gdi32.lib /nologo /dll /debug /machine:I386 /out:"..\..\System\XOpenGLDrvDrv.dll" /pdbtype:sept
# SUBTRACT LINK32 /incremental:no

!ENDIF 

# Begin Target

# Name "XOpenGLDrvDrv - Win32 Release"
# Name "XOpenGLDrvDrv - Win32 Debug"
# Begin Group "Src"

# PROP Default_Filter "*.cpp;*.h"
# Begin Source File

SOURCE=.\XOpenGLDrv.cpp
# End Source File
# Begin Source File

SOURCE=.\XOpenGLDrvDrv.cpp
# ADD CPP /Yc"XOpenGLDrvDrv.h"
# End Source File
# Begin Source File

SOURCE=.\XOpenGLDrvDrv.h
# End Source File
# Begin Source File

SOURCE=..\..\System\XOpenGLDrvDrv.ini
# End Source File
# Begin Source File

SOURCE=..\..\System\XOpenGLDrvDrv.int
# End Source File
# Begin Source File

SOURCE=.\XOpenGLDrvFuncs.h
# End Source File
# End Group
# End Target
# End Project
