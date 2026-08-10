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


SetCompressor lzma
SetCompressorDictSize 64

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
    
    ; Archivos principales
    File "altairc.exe"
    File "altair-terminal.exe"
    File "ALTAIR_LOGO.ico"
    
    ; Runtime
    SetOutPath "$INSTDIR\runtime"
    File /r "runtime\*.*"
    
    ; Ejemplos (opcional, pero útil)
    SetOutPath "$INSTDIR\examples"
    File /r "examples\*.*"
    
    ; ======================================================
    ; MINGW64 - SOLO LO ESENCIAL (sin doc, share, etc.)
    ; ======================================================
    SetOutPath "$INSTDIR\mingw64"
    
    ; 1. Binarios ejecutables (gcc, g++, ld, etc.)
    File /r "mingw64\bin\*.exe"
    File /r "mingw64\bin\*.dll"
    
    ; 2. Librerías estáticas y objetos (todo lib/*, sin C++)
    File /r /x "libstdc++*" /x "*c++*" "mingw64\lib\*.*"
    
    ; 3. Headers (solo C, sin C++)
    File /r /x "c++" "mingw64\include\*.*"
    
    ; 4. Herramientas internas de gcc (libexec, sin cc1plus/lto1)
    File /nonfatal /r /x "cc1plus.exe" /x "lto1.exe" "mingw64\libexec\*.*"
    
    ; 5. Directorio mingw64/x86_64-w64-mingw32 (si existe)
    File /nonfatal /r "mingw64\x86_64-w64-mingw32\*.*"
    
    ; 6. Directorio mingw64/mingw64 (si existe)
    File /nonfatal /r "mingw64\mingw64\*.*"
    
    ; EXPLÍCITAMENTE NO COPIAMOS: share/, doc/, man/, info/, etc.
    
    ; ======================================================
    ; CONFIGURAR PATH (evitar duplicados)
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
