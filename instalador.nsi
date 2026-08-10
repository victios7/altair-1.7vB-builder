Unicode True
!include "MUI2.nsh"

!define APPNAME    "Altair"
!define APPVERSION "1.7.5vB"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Altair"

Name "Altair Language ${APPVERSION}"
OutFile "Altair-Setup-${APPVERSION}.exe"
InstallDir "$PROGRAMFILES64\Altair"
RequestExecutionLevel admin
ShowInstDetails show

; ======================================================
; COMPRESIÓN NORMAL (para archivos pequeños)
; ======================================================
SetCompressor /SOLID zlib

!define MUI_ABORTWARNING
!define MUI_ICON "ALTAIR_LOGO.ico"
!define MUI_UNICON "ALTAIR_LOGO.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Spanish"

Section "Altair Compiler"
    SetOutPath "$INSTDIR"
    
    ; Archivos principales (comprimidos)
    File "altairc.exe"
    File "altair-terminal.exe"
    File "ALTAIR_LOGO.ico"
    
    ; Runtime
    SetOutPath "$INSTDIR\runtime"
    File /r "runtime\*.*"
    
    ; Ejemplos
    SetOutPath "$INSTDIR\examples"
    File /r "examples\*.*"
    
    ; ======================================================
    ; MINGW64 - SIN COMPRESIÓN (INSTALACIÓN INSTANTÁNEA)
    ; ======================================================
    SetOutPath "$INSTDIR\mingw64"
    
    ; Desactivar compresión para mingw64
    SetCompress off
    File /r /x "share" /x "doc" /x "man" /x "info" "mingw64\*.*"
    SetCompress auto  ; Volver a compresión normal
    
    ; ======================================================
    ; CONFIGURAR PATH
    ; ======================================================
    ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    
    Push "$INSTDIR;$INSTDIR\mingw64\bin"
    Push $0
    Call PathContains
    Pop $1
    
    ${If} $1 == 0
        WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" \
            "Path" "$0;$INSTDIR;$INSTDIR\mingw64\bin"
        SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment"
    ${EndIf}
    
    ; Acceso directo al escritorio
    CreateShortcut "$DESKTOP\Altair Terminal.lnk" \
        "$INSTDIR\altair-terminal.exe" "" \
        "$INSTDIR\ALTAIR_LOGO.ico" 0
    
    ; Registrar desinstalador
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayName"     "Altair ${APPVERSION}"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion"  "${APPVERSION}"
    WriteRegStr HKLM "${UNINST_KEY}" "Publisher"       "Altair Language"
    WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon"     "$INSTDIR\ALTAIR_LOGO.ico"
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Function PathContains
    Exch $0
    Exch
    Exch $1
    Push $2
    Push $3
    
    StrCpy $2 0
    StrLen $3 $1
    
    ${Do}
        StrCpy $2 $0 $3 $2
        ${If} $2 == ""
            ${Break}
        ${EndIf}
        
        ${If} $2 == $1
            StrCpy $3 1
            ${Break}
        ${EndIf}
        
        IntOp $2 $2 + 1
    ${Loop}
    
    Pop $3
    Pop $2
    Pop $1
    Exch $0
FunctionEnd

Section "Uninstall"
    Delete "$DESKTOP\Altair Terminal.lnk"
    Delete "$INSTDIR\altairc.exe"
    Delete "$INSTDIR\altair-terminal.exe"
    Delete "$INSTDIR\ALTAIR_LOGO.ico"
    Delete "$INSTDIR\uninstall.exe"
    
    RMDir /r "$INSTDIR\runtime"
    RMDir /r "$INSTDIR\mingw64"
    RMDir /r "$INSTDIR\examples"
    RMDir "$INSTDIR"
    
    DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
