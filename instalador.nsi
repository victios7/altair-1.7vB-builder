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
    ; MINGW64 - INCLUSIÓN REDUCIDA (solo lo necesario: bin y DLLs)
    ; Para evitar que el instalador sea enorme y que la extracción se
    ; quede bloqueada por antivirus o long-paths, NO incluimos todo
    ; el árbol mingw64 por defecto. Para incluirlo, definir la macro
    ; INCLUDE_MINGW al invocar makensis: makensis -DINCLUDE_MINGW instalador.nsi
    ; ======================================================

!ifdef INCLUDE_MINGW
    ; Informar en detalles que vamos a instalar mingw (aparece en la UI)
    DetailPrint "Instalando componente mingw64 (solo bin y DLLs)..."

    ; Establecemos el directorio base para mingw
    SetOutPath "$INSTDIR\mingw64"

    ; Desactivar compresión para evitar procesamiento extra
    SetCompress off

    ; Copiar ejecutables y utilidades desde bin
    SetOutPath "$INSTDIR\mingw64\bin"
    File /r "mingw64\bin\*.*"

    ; Copiar solamente DLLs desde lib (evita headers, doc, etc.)
    SetOutPath "$INSTDIR\mingw64\lib"
    File /r "mingw64\lib\*.dll"

    ; Volver a compresión automática para el resto
    SetCompress auto

    DetailPrint "mingw64 instalado (bin y DLLs)."
!else
    DetailPrint "NO se incluirá mingw64 en este instalador (INCLUDE_MINGW no definido)."
!endif

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

; ------------------------------------------------------
; Subrutina PathContains: comprueba si una ruta está en PATH
; Entrada: [top] = búsqueda (ruta a añadir)  [next] = PATH actual
; Salida: pop $R0 = 1 si ya contiene, 0 si no
; ------------------------------------------------------
Function PathContains
  Exch $R0 ; PATH actual
  Exch
  Exch $R1 ; ruta a buscar
  Push $R2
  Push $R3

  StrCpy $R2 "0"
  ; Convertimos a minúsculas para comparación más robusta
  StrLower $R0 $R0
  StrLower $R1 $R1

  ; Buscamos la ruta dentro de PATH
  ClearErrors
  StrStr $R0 $R1 $R3
  IfErrors 0 +2
    StrCpy $R2 "1"

  Pop $R3
  Pop $R2
  Exch
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
