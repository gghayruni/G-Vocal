!include "MUI2.nsh"
; MakeNSIS normally switches to this script's directory. Move to project root.
!cd ".."
Unicode true
Name "G-Vocal 0.1.0"
OutFile "dist\G-Vocal-Setup-0.1.0.exe"
InstallDir "$COMMONFILES64\VST3"
RequestExecutionLevel admin
ShowInstDetails show
SetCompressor /SOLID lzma
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "Russian"
Section "Install G-Vocal VST3" SEC01
  SetOutPath "$INSTDIR\G-Vocal.vst3"
  File /r "build\GVocal_artefacts\Release\VST3\G-Vocal.vst3\*.*"
  SetOutPath "$PROGRAMFILES64\G-Vocal"
  WriteUninstaller "$PROGRAMFILES64\G-Vocal\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\G-Vocal" "DisplayName" "G-Vocal 0.1.0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\G-Vocal" "UninstallString" '"$PROGRAMFILES64\G-Vocal\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\G-Vocal" "Publisher" "Hayruni Creative Studio"
SectionEnd
Section "Uninstall"
  RMDir /r "$COMMONFILES64\VST3\G-Vocal.vst3"
  Delete "$PROGRAMFILES64\G-Vocal\Uninstall.exe"
  RMDir "$PROGRAMFILES64\G-Vocal"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\G-Vocal"
SectionEnd
