; Installer script for win32 Gaim
; Generated NSIS script file (generated by makensitemplate.phtml 0.21)
; Herman on Sep 11 02 @ 21:52

; NOTE: this .NSI script is designed for NSIS v1.8+

Name "Gaim 0.60 alpha 3 (Win32)"
OutFile "gaim-0.60-alpha3.exe"
Icon .\pixmaps\gaim-install.ico

; Some default compiler settings (uncomment and change at will):
; SetCompress auto ; (can be off or force)
; SetDatablockOptimize on ; (can be off)
; CRCCheck on ; (can be off)
; AutoCloseWindow false ; (can be true for the window go away automatically at end)
; ShowInstDetails hide ; (can be show to have them shown, or nevershow to disable)
; SetDateSave off ; (can be on to have files restored to their orginal date)

InstallDir "$PROGRAMFILES\Gaim"
InstallDirRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" ""
DirShow show ; (make this hide to not let the user change it)
DirText "Select the directory to install Gaim in:"


Section "" ; (default section)
SetOutPath "$INSTDIR"
; add files / whatever that need to be installed here.
File /r .\win32-install-dir\*.*
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" "" "$INSTDIR"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gaim" "DisplayName" "Gaim (remove only)"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gaim" "UninstallString" '"$INSTDIR\uninst.exe"'
; write out uninstaller
WriteUninstaller "$INSTDIR\uninst.exe"
SectionEnd ; end of default section

Section "Gaim Start Menu Group"
  SetOutPath "$SMPROGRAMS\Gaim"
  CreateShortCut "$SMPROGRAMS\Gaim\Gaim.lnk" \
                 "$INSTDIR\gaim.exe"
  CreateShortCut "$SMPROGRAMS\Gaim\Unistall.lnk" \
                 "$INSTDIR\uninst.exe"
SectionEnd



; begin uninstall settings/section
UninstallText "This will uninstall Gaim from your system"

Section Uninstall
; add delete commands to delete whatever files/registry keys/etc you installed here.
RMDir /r "$SMPROGRAMS\Gaim"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Gaim"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Gaim"
RMDir /r "$INSTDIR"
SectionEnd ; end of uninstall section

; eof

