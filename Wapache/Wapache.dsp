# Microsoft Developer Studio Project File - Name="Wapache" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Wapache - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Wapache.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Wapache.mak" CFG="Wapache - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Wapache - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Wapache - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Wapache - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Urlmon.lib wininet.lib comsupp.lib ..\httpd\srclib\apr\Debug\libapr.lib ..\httpd\srclib\apr-iconv\Debug\libapriconv.lib ..\httpd\srclib\apr-util\Debug\libaprutil.lib ..\httpd\Debug\libhttpd.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "Wapache - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Urlmon.lib wininet.lib comsupp.lib ..\httpd\srclib\apr\Debug\libapr.lib ..\httpd\srclib\apr-iconv\Debug\libapriconv.lib ..\httpd\srclib\apr-util\Debug\libaprutil.lib ..\httpd\Debug\libhttpd.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Wapache - Win32 Release"
# Name "Wapache - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\clsid.c

!IF  "$(CFG)" == "Wapache - Win32 Release"

# SUBTRACT CPP /YX

!ELSEIF  "$(CFG)" == "Wapache - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\constants.cpp
# End Source File
# Begin Source File

SOURCE=.\expand_env.c
# End Source File
# Begin Source File

SOURCE=.\main.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp

!IF  "$(CFG)" == "Wapache - Win32 Release"

!ELSEIF  "$(CFG)" == "Wapache - Win32 Debug"

# ADD CPP /GX
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\util.cpp
# End Source File
# Begin Source File

SOURCE=.\wa_client.cpp
# End Source File
# Begin Source File

SOURCE=.\wa_config.c
# End Source File
# Begin Source File

SOURCE=.\wa_core.cpp
# End Source File
# Begin Source File

SOURCE=.\WapacheApplication.cpp
# End Source File
# Begin Source File

SOURCE=.\WapacheDispatcher.cpp
# End Source File
# Begin Source File

SOURCE=.\WapacheExternal.cpp
# End Source File
# Begin Source File

SOURCE=.\WapacheLock.cpp
# End Source File
# Begin Source File

SOURCE=.\WapacheProtocol.cpp
# End Source File
# Begin Source File

SOURCE=.\WapacheTrayIcon.cpp
# End Source File
# Begin Source File

SOURCE=.\WapacheWindow.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\constants.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\util.h
# End Source File
# Begin Source File

SOURCE=.\wa_config.h
# End Source File
# Begin Source File

SOURCE=.\wapache.h
# End Source File
# Begin Source File

SOURCE=.\WapacheApplication.h
# End Source File
# Begin Source File

SOURCE=.\WapacheDispatcher.h
# End Source File
# Begin Source File

SOURCE=.\WapacheExternal.h
# End Source File
# Begin Source File

SOURCE=.\WapacheLock.h
# End Source File
# Begin Source File

SOURCE=.\WapacheProtocol.h
# End Source File
# Begin Source File

SOURCE=.\WapacheTrayIcon.h
# End Source File
# Begin Source File

SOURCE=.\WapacheWindow.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\Wapache.rc
# End Source File
# End Group
# End Target
# End Project
