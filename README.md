# Altair Language v1.7

#este repositorio no es el oficial, solo el código fuente

> "Memory has a place."

Altair es un lenguaje de nivel medio compilado a C, con control explícito de storage (`ram`, `disk`, `cache`, `temp`), sistema de tipos sencillo, y construcciones expresivas como `orbit`, `prefer`, `choose`, `snapshot`, y `migrate`.

---

## Novedades en v1.7

| # | Fix | Detalle |
|---|-----|---------|
| 1 | `release` statement | Nueva palabra clave para liberar variables explícitamente |
| 2 | `exit` fuera de loop | Ahora emite `exit(0)` en lugar de `break` (error de compilación) |
| 3 | `#line` directives | Los mensajes de error de gcc apuntan al `.at` original, no al C generado |
| 4 | Windows `HOME` | Usa `USERPROFILE` en Windows (antes `HOME` estaba vacío) |
| 5 | Windows `mkdirp` | Usa `_mkdir` en lugar de `mkdir(path, 0755)` (no compila en Windows) |
| 6 | `disk_save` storage | `cache/` se guarda en `cache/`, no en `disk/` (confusión de paths) |
| 7 | `fmemopen` → temp file | `snapshot restore` usa archivo temporal (Windows no tiene `fmemopen`) |
| 8 | `hostname`/`username` | Usa `GetComputerNameA` / `USERNAME` en Windows |
| 9 | Versión `1.6` | Todos los archivos actualizados |
| 10| Dead code `root=n` | Eliminado código inalcanzable en `parser.c` |
| 11| `sema.c` warnings | Variables no declaradas emiten advertencia (no error fatal) |
| 12| `altair_var_release` | Nueva función en runtime para liberación explícita |

---

## Instalación en Windows

1. Ejecuta `Altair-Setup-1.6.exe`
2. Sigue el asistente (instala en `C:\Program Files\Altair`)
3. Haz doble clic en **Altair Terminal** del Escritorio
4. Escribe: `altairc.exe hola.at -o hola.exe`

## Compilación manual en Windows

```bat
build_windows.bat
```

## Compilación en Linux/macOS

```bash
make
./altairc examples/hello.at -o hello
./hello
```

## Gráficos con raylib

Altair soporta `link graphics raylib` para programas con ventana, dibujo de formas, texto e input. El compilador busca raylib primero en `libs/raylib/<so>/` (vendorizada dentro del proyecto) y, si no la encuentra ahí, en el sistema.

```
libs/raylib/windows/include/raylib.h
libs/raylib/windows/lib/libraylib.a
libs/raylib/macos/include/raylib.h
libs/raylib/macos/lib/libraylib.a
libs/raylib/linux/include/raylib.h
libs/raylib/linux/lib/libraylib.a
```

Instrucciones completas de instalación por plataforma en `libs/raylib/README.md`.

```bash
./altairc test/paint.at -o paint
./paint
```

---



```
altairc <archivo.at> [opciones]

  -o <salida>     Nombre del binario de salida
  --emit-c        Muestra el C generado en stdout
  --emit-ast      Muestra el árbol AST
  --no-sema       Omite el análisis semántico
  -v, --version   Muestra la versión
  -h, --help      Muestra esta ayuda
```

---

## Ejemplo rápido

```altair
altair.doc;
    name = "Mi Programa"
    version = "1.0"
    author = "Tu Nombre"
create altair.doc

text saludo = "Hola, Mundo!" ram
log saludo

numeric contador = 0 ram
repeat 3;
    contador += 1
    log "Vuelta: " + contador
break

release contador
```

---

## Storage disponible

| Tipo | Descripción |
|------|-------------|
| `ram` | En memoria (mlock, rápido, volátil) |
| `disk` | Persistente en disco (`~/.altair/<app>/disk/`) |
| `cache` | Persistente con TTL (`~/.altair/<app>/cache/`) |
| `temp` | Volátil, zeroed al liberar |
| `auto` | Decide automáticamente |
| `orbit` | Cambia de storage según estado con `migrate` |
| `prefer` | Fallback entre storages |

---

## Palabra clave `release` (v1.7)

Libera la memoria de una variable explícitamente y la elimina del registro:

```altair
numeric x = 100 ram
/ ... usar x ...
release x
/ x ya no existe - accederla causaría warning en sema y error en runtime
```

---

## Estructura del proyecto

```
altair-1.6/
├── src/
│   ├── main.c            compilador CLI
│   ├── lexer.h/c         tokenizer
│   ├── ast.h/c           árbol sintáctico
│   ├── parser.c/h        parser recursivo descendente
│   ├── sema.c/h          análisis semántico
│   ├── codegen.c/h       generación de C
│   └── altair-terminal.c lanzador Windows
├── runtime/
│   ├── altair_rt.h       header del runtime
│   └── altair_rt.c       implementación del runtime
├── libs/
│   └── raylib/
│       ├── README.md     instrucciones de instalación por SO
│       ├── windows/      include/ + lib/ (vacío, a rellenar)
│       ├── macos/        include/ + lib/ (vacío, a rellenar)
│       └── linux/        include/ + lib/ (vacío, a rellenar)
├── examples/
│   ├── hello.at          programa básico
│   └── game.at           RPG de ejemplo
├── test/
│   └── paint.at           ejemplo con raylib
├── Makefile              build Linux/macOS
├── build_windows.bat     build Windows
├── instalador.nsi        script NSIS
└── README.md             este archivo
```

---

© 2026 Altair Project - Licencia MIT
