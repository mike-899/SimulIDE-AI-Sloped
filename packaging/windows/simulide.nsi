; NSIS installer for SimulIDE-AI-Sloped
; makensis -DVERSION=0.4.13-SR5-CE -DSRCDIR=<dir with bin\ share\ LICENSE> -DOUTFILE=<exe> simulide.nsi
Unicode true
!include "MUI2.nsh"

!ifndef VERSION
  !define VERSION "dev"
!endif
!ifndef SRCDIR
  !error "Define SRCDIR (folder with bin\ and share\)"
!endif
!ifndef OUTFILE
  !define OUTFILE "SimulIDE-setup.exe"
!endif

!define APPNAME "SimulIDE AI Sloped"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\SimulIDE-AI-Sloped"

Name "${APPNAME} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\SimulIDE-AI-Sloped"
InstallDirRegKey HKLM "Software\SimulIDE-AI-Sloped" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\simulide.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SRCDIR}\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Spanish"

Section "SimulIDE" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${SRCDIR}\LICENSE"
  SetOutPath "$INSTDIR\bin"
  File /r "${SRCDIR}\bin\*.*"
  SetOutPath "$INSTDIR\share"
  File /r "${SRCDIR}\share\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  SetRegView 64
  WriteRegStr HKLM "Software\SimulIDE-AI-Sloped" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "SimulIDE-AI-Sloped"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\bin\simulide.exe"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1

  ; Shortcuts (working dir = bin so that ../share/simulide resolves)
  SetOutPath "$INSTDIR\bin"
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\SimulIDE.lnk" "$INSTDIR\bin\simulide.exe"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\SimulIDE.lnk" "$INSTDIR\bin\simulide.exe"
SectionEnd

Section "Uninstall"
  SetRegView 64
  Delete "$DESKTOP\SimulIDE.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\SimulIDE.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"
  RMDir /r "$INSTDIR\bin"
  RMDir /r "$INSTDIR\share"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "${UNINSTKEY}"
  DeleteRegKey HKLM "Software\SimulIDE-AI-Sloped"
SectionEnd
