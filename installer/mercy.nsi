; MERCY - Windows installer (NSIS 3, Modern UI 2)
; makensis -DVERSION=2.1 -DSTAGE=<folder with Mercy.vst3, Mercy.clap, MERCY_User_Manual.pdf, README.txt, licenses\> -DOUTFILE=<exe> mercy.nsi
Target amd64-unicode   ; a 64-bit installer (MERCY is 64-bit only)
ManifestDPIAware true
SetCompressor /SOLID lzma

!ifndef VERSION
  !define VERSION "2.1"
!endif
!define APP "Mercy"
!define PUBLISHER "slapworx"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\slapworx Mercy"

Name "MERCY ${VERSION}"
Caption "MERCY ${VERSION} setup"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\slapworx\Mercy"
RequestExecutionLevel admin
BrandingText "slapworx · plug-ins + samples. that slap."
!ifndef NUMVER
  !define NUMVER "2.1.0.0"
!endif
VIProductVersion "${NUMVER}"
VIAddVersionKey "ProductName" "MERCY"
VIAddVersionKey "CompanyName" "slapworx"
VIAddVersionKey "FileDescription" "MERCY drum processor installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "slapworx"

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!define MUI_ICON "mercy.ico"
!define MUI_UNICON "mercy.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "side.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "side.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_NODESC

!define MUI_WELCOMEPAGE_TITLE "MERCY ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "A drum processor inspired by three eras of Kanye West.$\r$\n$\r$\nThis installs MERCY as a VST3 and a CLAP plugin.$\r$\n$\r$\nPlease close your DAW before you continue, so the plugin files can be replaced."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TITLE "MERCY is installed"
!define MUI_FINISHPAGE_TEXT "Open your DAW and scan for new plugins.$\r$\nFL Studio: Options > Manage plugins > Find installed plugins.$\r$\n$\r$\nMERCY shows up under slapworx."
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Open the user manual"
!define MUI_FINISHPAGE_RUN_FUNCTION OpenManual
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "MERCY needs 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Function OpenManual
  ExecShell "open" "$INSTDIR\MERCY_User_Manual.pdf"
FunctionEnd

Section "-core"
  SetOutPath "$INSTDIR"
  File "${STAGE}/MERCY_User_Manual.pdf"
  File "${STAGE}/README.txt"
  SetOutPath "$INSTDIR\licenses"
  File "${STAGE}/licenses/*.*"
  WriteUninstaller "$INSTDIR\Uninstall Mercy.exe"
  CreateDirectory "$SMPROGRAMS\slapworx"
  CreateShortcut "$SMPROGRAMS\slapworx\MERCY user manual.lnk" "$INSTDIR\MERCY_User_Manual.pdf"
  CreateShortcut "$SMPROGRAMS\slapworx\Uninstall MERCY.lnk" "$INSTDIR\Uninstall Mercy.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "MERCY (slapworx)"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\Uninstall Mercy.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall Mercy.exe"'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "VST3 plugin" SecVST3
  ; a VST3 bundle folder: replace the whole thing so no old files linger
  RMDir /r "$COMMONFILES64\VST3\Mercy.vst3"
  SetOutPath "$COMMONFILES64\VST3"
  File /r "${STAGE}/Mercy.vst3"
  WriteRegDWORD HKLM "${UNINST_KEY}" "VST3" 1
SectionEnd

Section "CLAP plugin" SecCLAP
  SetOutPath "$COMMONFILES64\CLAP"
  ClearErrors
  File "${STAGE}/Mercy.clap"
  ${If} ${Errors}
    MessageBox MB_ICONEXCLAMATION "Mercy.clap couldn't be replaced. It's probably in use: close your DAW and run the installer again."
  ${EndIf}
  WriteRegDWORD HKLM "${UNINST_KEY}" "CLAP" 1
SectionEnd

Section "Uninstall"
  SetRegView 64
  RMDir /r "$COMMONFILES64\VST3\Mercy.vst3"
  Delete "$COMMONFILES64\CLAP\Mercy.clap"
  Delete "$SMPROGRAMS\slapworx\MERCY user manual.lnk"
  Delete "$SMPROGRAMS\slapworx\Uninstall MERCY.lnk"
  RMDir "$SMPROGRAMS\slapworx"
  Delete "$INSTDIR\MERCY_User_Manual.pdf"
  Delete "$INSTDIR\README.txt"
  RMDir /r "$INSTDIR\licenses"
  Delete "$INSTDIR\Uninstall Mercy.exe"
  RMDir "$INSTDIR"
  RMDir "$PROGRAMFILES64\slapworx"
  DeleteRegKey HKLM "${UNINST_KEY}"
  ; your presets (%APPDATA%\Mercy) and bakes (Documents\Mercy Bakes) are left alone
SectionEnd
