# Raylib vendorizado para Altair 1.6.5vC

El compilador `altairc` ya está preparado para buscar raylib en estas
carpetas ANTES de depender de que esté instalada en el sistema:

```
libs/raylib/windows/include/raylib.h
libs/raylib/windows/lib/libraylib.a   (o raylib.lib)
libs/raylib/macos/include/raylib.h
libs/raylib/macos/lib/libraylib.a
libs/raylib/linux/include/raylib.h
libs/raylib/linux/lib/libraylib.a
```

Solo tienes que descargar el release de raylib para cada plataforma
y copiar `raylib.h` a `include/` y la librería (`.a`/`.lib`) a `lib/`.
No hace falta tocar nada más del código: `main.c` ya añade
`-I` / `-L` apuntando a estas rutas cuando el programa `.at` usa
`link graphics raylib`, y luego intenta el sistema como respaldo.

## 1. Windows (MinGW-w64)

Opción A - MSYS2 (recomendado, ya trae `.a` compatible con MinGW):
```
pacman -S mingw-w64-x86_64-raylib
```
Luego copia desde `C:\msys64\mingw64\`:
```
include\raylib.h        -> libs/raylib/windows/include/raylib.h
lib\libraylib.a          -> libs/raylib/windows/lib/libraylib.a
```

Opción B - binarios oficiales:
https://github.com/raysan5/raylib/releases
Descarga el ZIP `raylib-X.X_win64_mingw-w64.zip`, y copia igual
`include/raylib.h` y `lib/libraylib.a`.

El enlazado final en Windows usa:
`-lraylib -lopengl32 -lgdi32 -lwinmm`
(estas 3 últimas son librerías del sistema, siempre presentes en
cualquier Windows con MinGW, no hace falta vendorizarlas).

## 2. macOS

Con Homebrew:
```
brew install raylib
```
Copia:
```
$(brew --prefix raylib)/include/raylib.h  -> libs/raylib/macos/include/raylib.h
$(brew --prefix raylib)/lib/libraylib.a   -> libs/raylib/macos/lib/libraylib.a
```

El enlazado final en macOS usa además los frameworks del sistema
(siempre presentes, no requieren instalación):
`-framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL`

## 3. Linux

Compilando raylib desde fuente (recomendado para tener un `.a` estático
portable, ya que los paquetes de distro varían mucho de nombre):
```
git clone https://github.com/raysan5/raylib.git
cd raylib/src
make PLATFORM=PLATFORM_DESKTOP
```
Copia:
```
raylib/src/raylib.h         -> libs/raylib/linux/include/raylib.h
raylib/src/libraylib.a      -> libs/raylib/linux/lib/libraylib.a
```

O si tu distro tiene el paquete de desarrollo (`libraylib-dev` en
Debian/Ubuntu recientes, `raylib` en Arch/AUR), puedes instalarlo
directamente en el sistema en vez de vendorizarlo — `altairc` también
encontrará esa instalación como respaldo si no hay nada en
`libs/raylib/linux/`.

## Verificar que quedó bien

Desde la raíz del proyecto (donde está `altairc` y la carpeta `libs/`):
```
./altairc test/paint.at -o /tmp/altair_paint
/tmp/altair_paint
```
Si compila y abre una ventana con el texto, un rectángulo rojo y un
círculo dorado, todo está correcto.

## Nota importante

Yo (Claude) no tengo acceso a internet en este entorno sandbox, así
que no pude descargar los binarios reales de raylib para meterlos ya
listos en el zip. Esta carpeta viene vacía a propósito (solo con este
README) — el paso de copiar `raylib.h` + la librería es algo que
tienes que hacer tú una vez por plataforma, siguiendo las instrucciones
de arriba.
