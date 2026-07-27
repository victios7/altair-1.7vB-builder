# Altair Self-Hosting (etapa 1)

Este directorio contiene `altair.at`: un compilador, **escrito en Altair**,
para un subconjunto del lenguaje llamado **Altair-Core**. `altair.at` se
compila con el `altairc` normal (en C) y produce un binario
(`altairc-selfhost`) que a su vez compila programas `.atc` (Altair-Core) a C.

## Compilar el compilador auto-hospedado

    cd ..
    ./altairc selfhost/altair.at -o selfhost/altairc-selfhost

## Compilar un programa Altair-Core con él

    cd selfhost
    ./altairc-selfhost fib.atc fib.gen.c
    gcc fib.gen.c -o fib_bin -lm
    ./fib_bin

## Qué es "Altair-Core"

Es un subconjunto tipado y simplificado de Altair pensado para ser fácil
de compilar desde Altair mismo:

- Tipos: `numeric` (double), `text` (char*), `bool` (int)
- `fun nombre -> tipo tipo1 param1, tipo2 param2;` ... `break`   (declarar función)
- `if cond; ... elif cond; ... else; ... break`
- `while cond; ... break`
- `return expr`
- operadores: `+ - * / %`  `== != < > <= >=`  `and or not`
- literales numéricos, texto (`"..."`, con `\n \t \" \\`), `true`/`false`
- `log expr` (expr debe ser `text`; usa `numstr(n)` para volcar un numeric)
- builtins: `concat(a,b)`, `streq(a,b)`, `numstr(n)`, `argc()`, `arg(i)`

**Restricciones conocidas** (para mantenerlo abarcable):
- `==`/`!=`/`<`/`>` son solo para `numeric` (para texto usa `streq`)
- no hay listas, objetos, clases, ni las features de red/GUI de Altair completo
- sin comprobación de tipos: los errores de tipo se ven recién al compilar
  el C generado con gcc

Esto demuestra el flujo real: **Altair compilando Altair compilando C**.
Cerrar el círculo completo (que `altairc-selfhost` compile su propio
código fuente, `altair.at`) requeriría ampliar Altair-Core con listas,
indexado de texto y E/S de ficheros — no está hecho todavía, pero el
diseño de `altair.at` ya sigue esa dirección para cuando se aborde.
