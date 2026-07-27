# Guía completa del lenguaje Altair

> Basada en el funcionamiento interno real del compilador (`altairc`, escrito en C:
> lexer → parser → sema → codegen → C → gcc → binario nativo) y del runtime
> (`altair_rt.c`). No incluye las funcionalidades de `async`, `safe block` ni
> `data ... save data`, que todavía no están implementadas.

## Índice

1. [Visión general](#1-visión-general)
2. [Estructura de un programa](#2-estructura-de-un-programa)
3. [Variables y almacenamiento](#3-variables-y-almacenamiento)
4. [Tipos de datos](#4-tipos-de-datos)
5. [Operadores y expresiones](#5-operadores-y-expresiones)
6. [Control de flujo](#6-control-de-flujo)
7. [Funciones](#7-funciones)
8. [Clases y objetos](#8-clases-y-objetos)
9. [Listas](#9-listas)
10. [Manejo de errores](#10-manejo-de-errores)
11. [Snapshots](#11-snapshots)
12. [Choose (aleatoriedad ponderada)](#12-choose-aleatoriedad-ponderada)
13. [Introspección](#13-introspección)
14. [Variables token](#14-variables-token)
15. [Orbit y prefer](#15-orbit-y-prefer)
16. [Servidor HTTP](#16-servidor-http)
17. [Rutas y handlers](#17-rutas-y-handlers)
18. [Middleware](#18-middleware)
19. [Límite de tasa (rate limiting)](#19-límite-de-tasa-rate-limiting)
20. [Health checks](#20-health-checks)
21. [Métricas](#21-métricas)
22. [Apagado ordenado (graceful shutdown)](#22-apagado-ordenado-graceful-shutdown)
23. [Sesiones](#23-sesiones)
24. [Configuración y variables de entorno](#24-configuración-y-variables-de-entorno)
25. [Pool de base de datos](#25-pool-de-base-de-datos)
26. [Planificador de tareas (job scheduler)](#26-planificador-de-tareas-job-scheduler)
27. [Gráficos (raylib)](#27-gráficos-raylib)
28. [Consultas de sistema integradas](#28-consultas-de-sistema-integradas)
29. [Referencia de la CLI `altairc`](#29-referencia-de-la-cli-altairc)
30. [Códigos de error](#30-códigos-de-error)
31. [`char`, `file`, `point#Tipo` y operadores a nivel de bit](#31-char-file-pointtipo-y-operadores-a-nivel-de-bit)
32. [E/S de ficheros y ejecución de procesos](#32-es-de-ficheros-y-ejecución-de-procesos)
33. [Punteros crudos](#33-punteros-crudos)
34. [Argumentos de línea de comandos](#34-argumentos-de-línea-de-comandos)
35. [Variables persistentes con archivo propio](#35-variables-persistentes-con-archivo-propio)
36. [El compilador auto-hospedado (Altair-Core)](#36-el-compilador-auto-hospedado-altair-core)

---

## 1. Visión general

Altair es un lenguaje **compilado estáticamente** y orientado a expresiones que
se traduce a C mediante `altairc`. Cada programa termina siendo un único
binario nativo, sin dependencias de runtime externas.

**Pipeline del compilador:**
```
código .at → lexer → parser → sema → codegen → código .c → gcc → binario
```

**Propiedades clave:**
- Cada variable tiene un *nivel de almacenamiento* declarado (`ram`/`disk`/`cache`/`temp`).
- Las variables pueden migrar entre niveles de almacenamiento en tiempo de
  ejecución mediante `orbit`.
- Texto, listas, objetos y tokens son valores de primera clase.
- Toda la memoria la gestiona el runtime (no hay que hacer `malloc`/`free`
  manual, salvo con los punteros crudos nuevos, ver §33).
- Los servidores HTTP se declaran con `listen`; las rutas con `route`.

---

## 2. Estructura de un programa

Todo programa Altair tiene una cabecera opcional y un cuerpo. La cabecera da
metadatos del programa; el cuerpo contiene todas las sentencias.

```altair
altair.doc;
    name = MyApp
    version = 1.0.0
    author = "Jane Smith"
create altair.doc

/ Esto es un comentario (línea que empieza por /)

define text greeting = "Hello, world!"
log greeting
```

**Campos de cabecera:** `name`, `version`, `author`.

El parser lee la cabecera desde `altair.doc;` hasta `create altair.doc`. Todo
lo demás es el cuerpo del programa.

**Comentarios:** un `/` al inicio de una línea (o tras espacios en blanco)
seguido de un espacio o una letra empieza un comentario de una línea.

---

## 3. Variables y almacenamiento

### Declaración

Hay dos formas equivalentes, ambas soportadas por el parser:

```altair
define <tipo> <nombre> [almacenamiento] [cualificadores] [= expresión]
<tipo> <nombre> [= expresión] [almacenamiento] [cualificadores]
```

La palabra `define` es **opcional**; el compilador reconoce igualmente una
declaración que empieza directamente por el tipo.

**Ejemplos:**
```altair
define numeric count = 0
numeric count2 = 0

define text name = "world"
define bool ready = true
define list items
define object user
```

### Niveles de almacenamiento

| Palabra clave | Significado |
|---|---|
| `ram`   | Memoria del proceso (por defecto). Rápido; se pierde al salir. |
| `disk`  | Almacenamiento persistente en fichero. Sobrevive a reinicios. |
| `cache` | Persistente con TTL opcional. Se auto-expira. |
| `temp`  | Se pone a cero al liberarse; para datos sensibles. |
| `auto`  | El runtime elige el mejor nivel. |

```altair
define text username disk
define text session_token temp
define text api_response cache expire=30m
```

### Cualificadores

| Cualificador | Ejemplo | Efecto |
|---|---|---|
| `const` | `define numeric pi const = 3.14159` | No se puede reasignar |
| `expire=<dur>` | `expire=5m` | Se auto-expira tras un tiempo |
| `weight=<n>` | `weight=5` | Pista de prioridad (0–100) |

**Duraciones:** `30s` (segundos), `5m` (minutos), `2h` (horas).

### Asignación

```altair
name = "Alice"
count = count + 1
count += 10
count -= 3
count *= 2
count /= 4
count %= 7
```

### Liberar (`release`)

Libera explícitamente una variable y su memoria:

```altair
define text buffer ram = "large data"
/ ... usar buffer ...
release buffer
```

`release` es seguro de llamar sobre variables que ya no existen (no hace nada).

---

## 4. Tipos de datos

### `numeric`

Un número de coma flotante de 64 bits (`double` en C internamente).

```altair
define numeric x = 42
define numeric pi = 3.14159
```

### `text`

Una cadena UTF-8.

```altair
define text greeting = "Hello!"
define text multi = "Línea 1\nLínea 2"
```

La concatenación usa `+`:
```altair
define text full = "Hola, " + name + "!"
```

### `bool`

```altair
define bool active = true
define bool done = false
```

### `list`

Colección ordenada de tamaño dinámico.

```altair
define list scores = [10, 20, 30]
scores.append(40)
scores.remove(0)      / elimina el índice 0
scores.clear()        / vacía la lista
define numeric n = scores.length()
define numeric v = scores[1]
scores[0] = 99
```

`.length` funciona con o sin paréntesis.

### `object`

Ver [§8 Clases y objetos](#8-clases-y-objetos).

### `token`

Un valor de un solo uso: se consume la primera vez que se lee, y una segunda
lectura lanza `ALT0004`.

```altair
define token invite_code = "XXXX-YYYY"
define text used = invite_code    / consume el token
define text again = invite_code   / ERROR: ALT0004, token ya consumido
```

### `char` — texto de un carácter *(añadido para self-hosting, §31)*

### `file` — manejador de fichero *(añadido para self-hosting, §31–32)*

### `point#Tipo` — puntero crudo *(añadido para self-hosting, §31, §33)*

---

## 5. Operadores y expresiones

### Aritméticos

| Op | Descripción | Ejemplo |
|---|---|---|
| `+` | Suma / concatena texto | `1 + 2`, `"a" + "b"` |
| `-` | Resta | `10 - 3` |
| `*` | Multiplica | `4 * 5` |
| `/` | Divide | `10 / 3` |
| `%` | Módulo | `10 % 3` |
| `-x` | Negación | `-count` |

### Comparación

`==`, `!=`, `<`, `>`, `<=`, `>=` — funcionan sobre `numeric` y `text`
(lexicográfica para texto).

### Lógicos

`&&` (y), `||` (o), `!` (no). *(En la fuente del propio compilador solo
existen estas formas simbólicas — no hay palabras "and"/"or"/"not"; esas
palabras sí las usa, por su cuenta, el lenguaje "Altair-Core" del compilador
auto-hospedado, ver §36).*

```altair
if x > 0 && x < 100;
    log "en rango"
break
```

### A nivel de bit *(añadido, §31.5)*

`& | ^ ~ << >>` — solo válidos sobre `numeric` (internamente se convierten a
entero de 64 bits, se opera, y se vuelve a `double`).

### Precedencia (de mayor a menor)

1. Unarios: `!`, `-`, `~`
2. `*`, `/`, `%`
3. `+`, `-`
4. `<<`, `>>`
5. `&`
6. `^`
7. `|`
8. `<`, `>`, `<=`, `>=`
9. `==`, `!=`
10. `&&`
11. `||`

---

## 6. Control de flujo

Casi todos los bloques en Altair terminan con la palabra `break` (no usan
llaves `{}`).

### if / elif / else

```altair
if score > 90;
    log "Excelente"
break
elif score > 70;
    log "Bien"
break
else;
    log "Necesita mejorar"
break
```

### while

```altair
define numeric n = 1
while n <= 10;
    log n
    n += 1
break
```

### repeat

```altair
repeat 5 times;
    log "hola"
break

/ o con una expresión:
repeat count;
    log "tick"
break
```

### forever

```altair
forever;
    log "corriendo..."
    wait 1s
break
```

### foreach

```altair
define list fruits = ["manzana", "plátano", "cereza"]
foreach fruit in fruits;
    log fruit
break
```

### exit

Sale del bucle actual, o termina el programa si no hay bucle:
```altair
while true;
    if done == true;
        exit        / sale del while
    break
break

exit        / termina el programa (fuera de un bucle)
```

### wait

```altair
wait 2s     / espera 2 segundos
wait 500ms  / (usar 0.5s para fracciones de segundo)
```

---

## 7. Funciones

```altair
fun greet name text;
    define text msg = "Hola, " + name + "!"
    log msg
    return msg
break
```

**Con tipo de retorno:**
```altair
fun add -> numeric numeric x, numeric y;
    return x + y
break
```

**Llamada:**
```altair
greet("Alice")
define numeric result = add(10, 20)
```

Las funciones pueden leer y escribir variables globales, y sus parámetros se
liberan correctamente al terminar (sin fugas de memoria).

**Funciones recursivas:**
```altair
fun factorial -> numeric numeric n;
    if n <= 1;
        return 1
    break
    return n * factorial(n - 1)
break
```

---

## 8. Clases y objetos

```altair
class Point;
    define numeric x = 0
    define numeric y = 0

    fun distance;
        define numeric dx = x * x
        define numeric dy = y * y
        return dx + dy
    break
break

create object Point as p
p.x = 3
p.y = 4
log p.distance()
log p.x
```

**Llamadas a métodos:**
```altair
define text result = myObj.methodName(arg1, arg2)
```

Los objetos usan arrays dinámicos de campos/métodos internamente (muy poco
overhead por instancia), así que crear muchos objetos es viable.

---

## 9. Listas

```altair
define list items = [1, 2, 3]

/ Añadir
items.append(4)
items.append("hola")

/ Acceso por índice (base 0)
define numeric first = items[0]

/ Modificar por índice
items[0] = 99

/ Eliminar por índice
items.remove(2)

/ Vaciar
items.clear()

/ Longitud (con o sin paréntesis)
define numeric n = items.length
define numeric m = items.length()

/ Iterar
foreach item in items;
    log item
break

/ Concatenar dos listas
define list all = listA + listB
```

---

## 10. Manejo de errores

```altair
try;
    / código que puede fallar
    define numeric result = 10 / 0
break
catch as err;
    log err.code
    log err.message
    log err.line
break
```

El runtime lanza errores en: división por cero, índice fuera de rango, acceso
a objeto nulo, doble consumo de token, reasignación de `const`.

**Anidar try/catch:**
```altair
try;
    try;
        / interno
    break
    catch as inner_err;
        log inner_err.message
    break
break
catch as outer_err;
    log outer_err.message
break
```

---

## 11. Snapshots

Los snapshots guardan todas las variables registradas en disco de forma
atómica (con checksum CRC32).

```altair
/ Guardar el estado actual
snapshot create "checkpoint1"

/ Restaurar un estado anterior
snapshot restore "checkpoint1"

/ Borrar un snapshot
snapshot delete "checkpoint1"
```

Se guardan en `~/.altair/<nombre_programa>/snap/`.

---

## 12. Choose (aleatoriedad ponderada)

```altair
choose outcome;
    "win"  70
    "draw" 20
    "lose" 10
break

log outcome
```

Los pesos son proporcionales — no hace falta que sumen 100.

---

## 13. Introspección

Consultas de metadatos del runtime y del compilador, mediante la sintaxis
`namespace@clave`:

```altair
/ Consultas de sistema
log system@time        / timestamp Unix
log system@random      / float 0.0–1.0 aleatorio
log system@pid         / PID del proceso
log system@hostname    / nombre de la máquina
log system@username    / usuario actual
log system@os          / "linux", "macos" o "windows"
log system@memory      / RAM usada (bytes)
log system@diskfree    / espacio libre en disco (bytes)

/ Introspección de variables
define disk numeric counter
log system@storage(counter)  / "disk"
log system@weight(counter)   / 0
log system@type(counter)     / "numeric"
log system@size(counter)     / tamaño serializado

/ Info del compilador
log compiler@version
log compiler@name        / "altairc"
log compiler@build       / fecha de compilación
log compiler@architecture  / "x86_64" o "arm64"

/ Metadatos del programa (de la cabecera altair.doc)
log program@name
log program@version
log program@author
```

---

## 14. Variables token

Un `token` solo se puede consumir una vez. Leerlo una segunda vez lanza
`ALT0004`.

```altair
define token api_key temp = "secret-api-key-abc123"

/ La primera lectura consume el token
define text key = api_key
log "Clave usada: " + key

/ La segunda lectura lanza error
define text key2 = api_key   / ERROR: Token already consumed
```

Útil para contraseñas de un solo uso, códigos de invitación, tokens CSRF, etc.

---

## 15. Orbit y prefer

### `orbit` — migración entre varios niveles

Declara una variable con estados de almacenamiento nombrados entre los que
puede migrar:

```altair
define numeric data orbit 1 "hot" ram, 2 "warm" disk, 3 "cold" cache expire=1h = 0

/ Mover a otro estado
migrate data as "warm"
migrate data as 3
```

### `prefer` — almacenamiento por orden de preferencia

Prueba niveles de almacenamiento en un orden de preferencia:

```altair
define text session prefer ram, cache expire=30m, disk = ""
```

El runtime usa el primer nivel disponible.

---

## 16. Servidor HTTP

Arranca un servidor HTTP en un puerto. El bloque `listen` contiene
declaraciones de rutas, middleware, health, métricas y apagado.

```altair
listen 8080;
    route "GET" "/hello";
        respond.text("Hello, World!")
    break
break
```

El servidor corre hasta que se termina. Las señales (`SIGTERM`, `Ctrl+C`)
disparan un apagado ordenado.

**Cómo funciona:** el servidor HTTP integrado de Altair usa sockets TCP
POSIX (sin librerías externas). Es de un solo hilo pero usa un modelo de
conexión por petición.

---

## 17. Rutas y handlers

```altair
route "METHOD" "/path";
    / cuerpo del handler
break
```

**Métodos soportados:** `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `*` (cualquiera)

### Parámetros de ruta

Sintaxis `:param` en las rutas:

```altair
route "GET" "/users/:id";
    define text user_id = param("id")
    respond.json("found user " + user_id)
break
```

### Acceso a la petición

Dentro de un handler:
```altair
define text body_data = body()                / cuerpo crudo de la petición
define text auth = header("Authorization")     / cabecera de la petición
define text id = param("id")                   / parámetro de ruta
```

### Respuesta

```altair
respond.text("respuesta en texto plano")
respond.json("cadena o valor json")
respond.status(201)
respond.status(404)
respond.status(500)
```

**Ejemplo:**
```altair
route "POST" "/users";
    define text payload = body()
    if payload == "";
        respond.status(400)
        respond.json("missing body")
    break
    respond.status(201)
    respond.json("user created")
break
```

---

## 18. Middleware

El middleware se ejecuta antes de cada handler de ruta. Si el middleware
llama a `stop`, se detiene la cadena de la petición.

```altair
middleware auth;
    define text token = header("Authorization")
    if token == "";
        respond.status(401)
        respond.json("unauthorized")
        stop
    break
break
```

Varios `middleware` se encadenan en orden:
```altair
listen 8080;
    middleware logger;
        log "petición recibida"
    break
    middleware auth;
        define text tok = header("X-API-Key")
        if tok != "secret";
            respond.status(401)
            respond.json("bad key")
            stop
        break
    break
    route "GET" "/data";
        respond.json("protected data")
    break
break
```

---

## 19. Límite de tasa (rate limiting)

Se puede añadir a cualquier ruta:

```altair
route "POST" "/login" rate_limit 10 per_minute;
    respond.json("ok")
break
```

Al superar el límite, el runtime devuelve automáticamente HTTP 429 con
`{"error":"rate limit exceeded"}`.

**Sintaxis:**
```altair
route "METHOD" "/path" rate_limit N per_minute;
```
o
```altair
rate_limit N per_second
```

---

## 20. Health checks

La declaración `health` registra una ruta `GET` que responde con estado JSON:

```altair
health "/health";
    check "database" -> true
    check "cache"    -> true
break
```

**Formato de respuesta:**
```json
{"status":"ok","checks":{"database":"ok","cache":"ok"}}
```

Si algún `check` devuelve `false`, el estado HTTP es `503 Service
Unavailable`. El lado derecho de `->` es una expresión (numérica o
booleana); cualquier valor distinto de cero se considera saludable.

---

## 21. Métricas

Registra un endpoint de métricas compatible con Prometheus:

```altair
metrics "/metrics";
```

Expone un `GET /metrics` en formato Prometheus de texto plano.

El endpoint produce algo como:
```
# TYPE events_total counter
events_total 42
```

---

## 22. Apagado ordenado (graceful shutdown)

Registra lógica de limpieza que se ejecuta al recibir `SIGTERM` o al llamar a
`altair_server_stop()`:

```altair
on_shutdown;
    log "Servidor apagándose, guardando datos..."
    snapshot create "shutdown_checkpoint"
break
```

**Ejemplo con ciclo de vida completo:**
```altair
listen 8080;
    route "GET" "/";
        respond.text("running")
    break
    on_shutdown;
        log "adiós!"
    break
break
```

---

## 23. Sesiones

Declara variables de sesión con TTL opcional:

```altair
session user_token expires 30m;
```

Las sesiones se guardan en memoria del proceso en un almacén clave-valor
indexado por un ID de sesión. Usa `header("X-Session-ID")` para recuperar el
ID de sesión del cliente.

**Dentro de handlers de ruta:**

```altair
route "GET" "/profile";
    define text sid = header("X-Session-ID")
    define text username = session_get(sid, "username")
    if username == "";
        respond.status(401)
        respond.json("not logged in")
    break
    respond.json("Hello " + username)
break

route "POST" "/login";
    define text user = body()
    session_set("my-session-id", "username", user, 1800)
    respond.json("logged in")
break
```

---

## 24. Configuración y variables de entorno

Usa el bloque `config` para declarar configuración respaldada por variables
de entorno:

```altair
config;
    env("DATABASE_URL") default "postgres://localhost/mydb"
    env("PORT") default "8080"
    env("API_SECRET") required
break
```

- `default "valor"` — valor de respaldo si la variable de entorno no está definida
- `required` — lanza `ALT0015` al arrancar si la variable falta

**Acceder a los valores:**

La declaración `env(...)` crea variables en el ámbito, nombradas según la
clave:
```altair
config;
    env("PORT") default "8080"
break

define numeric port = PORT
listen port;
    route "GET" "/";
        respond.text("ok")
    break
break
```

---

## 25. Pool de base de datos

Declara un pool de conexiones para una URL de base de datos:

```altair
db_pool db = connect("postgres://user:pass@localhost/mydb") max 20
```

Esto crea una variable `db` que guarda la cadena de conexión. Hay que
conectar tu propia librería cliente de base de datos en el C generado, o usar
el stub para prototipar. `max` fija el tamaño del pool (10 por defecto).

> Altair da la sintaxis de declaración y el stub de pooling; el soporte
> completo de consultas SQL requiere enlazar tu propia librería cliente
> (PostgreSQL/MySQL/SQLite) en el C generado.

---

## 26. Planificador de tareas (job scheduler)

Registra tareas periódicas en segundo plano con `job`:

```altair
job cleanup every 5m;
    log "ejecutando tarea de limpieza"
break
```

**Sintaxis:**
```altair
job <nombre> every <duración>;
    / cuerpo de la tarea
break
```

`schedule` es un alias de `job`:
```altair
schedule heartbeat every 30s;
    log "heartbeat"
break
```

**Cómo funciona:** las tareas se comprueban en cada petición
(`altair_jobs_tick()` se llama por conexión). En escenarios de poco tráfico,
usa `wait` dentro de un `forever` para temporización precisa en su lugar.

---

## 27. Gráficos (raylib)

Altair puede producir programas gráficos con ventana sobre raylib. Requiere
tener raylib disponible en tiempo de compilación (empaquetado en
`libs/raylib/<os>/` o instalado en el sistema).

### Activar el modo gráfico

```altair
link graphics raylib
```

Debe aparecer una vez, antes de cualquier sentencia de ventana/loop/draw.
Indica al compilador que enlace `-lraylib` (más las librerías de ventaneo de
la plataforma) en el binario final.

### Ventana

```altair
window
    title = "Mi Ventana"
    width = 800
    height = 600
    fps = 60
create window
```

### Bucle principal

```altair
loop
    clear skyblue
    / sentencias en cada frame
break
```

`clear <color>` limpia el frame. `break` cierra la definición del bloque
`loop` (no sale del programa — el `while` generado corre hasta que se cierra
la ventana).

### draw

```altair
draw <tipo>
    x = 100
    y = 100
    ...
create draw
```

| tipo | props requeridas | notas |
|---|---|---|
| `text` | `content`/`text`, `x`, `y`, `size`, `color` | |
| `rect` / `rectangle` | `x`, `y`, `width`/`w`, `height`/`h`, `color` | |
| `circle` | `x`, `y`, `radius`/`r`, `color` | |
| `line` | `x`, `y`, `x2`, `y2`, `color` | |
| `line_thick` / `thick_line` | añade `thick`/`thickness` | |
| `triangle` | `x`,`y`,`x2`,`y2`,`x3`,`y3`,`color` | |
| `pixel` | `x`, `y`, `color` | |
| `image` / `texture` | `image`/`src` (variable de textura), `x`, `y`, `color` | |

Los colores aceptan nombres de raylib (`red`, `gold`, `skyblue`, `white`,
...) o una declaración de bloque `color`.

### Ejemplo completo

```altair
link graphics raylib

window
    title = "Paint Test"
    width = 800
    height = 600
    fps = 60
create window

loop
    clear skyblue
    draw text
        content = "Altair Graphics"
        x = 10
        y = 10
        size = 24
        color = white
    create draw
    draw rect
        x = 100
        y = 100
        width = 200
        height = 120
        color = red
    create draw
    draw circle
        x = 500
        y = 200
        radius = 60
        color = gold
    create draw
break
```

Compilar y ejecutar:
```bash
altairc test/paint.at -o paint
./paint
```

---

## 28. Consultas de sistema integradas

| Consulta | Devuelve |
|---|---|
| `system@time` | Timestamp Unix (numeric) |
| `system@random` | Aleatorio 0.0–1.0 (numeric) |
| `system@pid` | PID del proceso (numeric) |
| `system@hostname` | Nombre de la máquina (text) |
| `system@username` | Usuario del SO (text) |
| `system@os` | "linux" / "macos" / "windows" (text) |
| `system@memory` | RAM usada en bytes (numeric) |
| `system@diskfree` | Bytes libres en disco (numeric) |
| `compiler@version` | Versión del compilador |
| `compiler@name` | "altairc" |
| `compiler@build` | Fecha de compilación |
| `compiler@architecture` | "x86_64" / "arm64" |
| `program@name` | De la cabecera altair.doc |
| `program@version` | De la cabecera altair.doc |
| `program@author` | De la cabecera altair.doc |

---

## 29. Referencia de la CLI `altairc`

```
altairc <source.at> [opciones]   Compila un programa Altair
altairc guide                    Escribe ALTAIR_GUIDE.md en el directorio actual
altairc guide --stdout           Imprime la guía por stdout
altairc --version                Imprime la versión del compilador
altairc --help                   Muestra la ayuda
```

**Opciones:**

| Flag | Descripción |
|---|---|
| `-o <file>` | Nombre del binario de salida (por defecto: `a.out`) |
| `--emit-c` | Imprime el código C generado por stdout |
| `--emit-ast` | Imprime un resumen de nodos del AST |
| `--no-sema` | Salta el análisis semántico (compila más rápido) |
| `-v` | Número de versión |

**Ejemplos:**

```bash
# Compilar un programa
altairc hello.at -o hello

# Compilar un servidor
altairc server.at -o myserver

# Depurar: ver qué C se genera
altairc server.at --emit-c | head -200

# Generar la guía del lenguaje
altairc guide
```

---

## 30. Códigos de error

| Código | Descripción |
|---|---|
| `ALT0001` | Variable desconocida o acceso a objeto nulo |
| `ALT0002` | Tipos incompatibles en aritmética o comparación |
| `ALT0003` | Error de parseo / sintaxis |
| `ALT0004` | Token ya consumido |
| `ALT0005` | Error de snapshot (crear/restaurar/borrar) |
| `ALT0006` | Bloque `prefer` sin entradas de almacenamiento |
| `ALT0007` | Asignación a una variable `const` |
| `ALT0008` | `weight` debe ser un entero no negativo |
| `ALT0009` | `orbit` tiene números de estado duplicados |
| `ALT0010` | División o módulo por cero |
| `ALT0011` | Clave o namespace de introspección desconocido |
| `ALT0012` | Estado de `orbit` no encontrado / variable sin orbit |
| `ALT0013` | Índice de lista (o de texto) fuera de rango |
| `ALT0014` | Campo de objeto no encontrado |
| `ALT0015` | Variable de entorno requerida no definida |
| `ALT0016` | Variable usada pero nunca declarada (error semántico) |

---

## 31. `char`, `file`, `point#Tipo` y operadores a nivel de bit

> Añadidos para dar soporte a **self-hosting** (que un compilador de Altair
> pueda algún día escribirse en el propio Altair). Tocan las cuatro etapas
> del compilador: `lexer.c/h` (tokens nuevos), `parser.c` (gramática nueva),
> `ast.h` (`VTYPE_FILE`, `VTYPE_POINTER`), `codegen.c` (nuevos casos de
> emisión) y `altair_rt.c/h` (nuevas etiquetas `ALT_FILE`/`ALT_POINTER` y
> nuevas funciones C integradas).

### 31.1 `char` — texto de un carácter

`char` es un **alias de tipo**, no un tipo de runtime nuevo. El parser mapea
el token `char` directamente a `VTYPE_TEXT` (`tok_to_vtype()` en
`parser.c`), así que una variable `char` es, a nivel de C/runtime, un valor
`ALT_TEXT` normal que contiene un solo carácter. Por eso toda operación de
texto (`+`, comparaciones, `length()`) ya funciona sobre `char` sin necesitar
codegen nuevo.

```altair
text code = "hola"
char c = code[0]
log c
```

### 31.2 Indexar texto: `text[índice]`

Antes de este cambio, el indexado `[i]` (`ND_INDEX_ACCESS`) solo funcionaba
sobre `ALT_LIST`. La función de runtime `altair_list_get()` ahora también
maneja `ALT_TEXT`: comprueba límites contra `strlen()` y devuelve una nueva
cadena de un carácter. Acceder fuera de rango lanza `ALT0013`, el mismo
código que usan las listas.

```altair
text s = "abc"
log s[0]      / "a"
log length(s) / 3   (length() ahora también acepta text, no solo listas)
```

### 31.3 `file` — un tipo manejador de fichero

`file` es un tipo de valor genuinamente nuevo: `VTYPE_FILE` en el AST mapea
a `ALT_FILE` en runtime. `AltairVal` recibió un campo `void *ptr` (union)
compartido entre `ALT_FILE` (guarda un `FILE*` de C) y `ALT_POINTER` (ver
más abajo). Copiar un valor `file` (`altair_val_copy`) es una copia
**superficial**: el `FILE*` subyacente se comparte, igual que al asignar un
puntero en C.

```altair
file f = open("data.txt")
text contents = read(f)
close(f)
```

### 31.4 `point#Tipo` — punteros crudos

Forma de declaración nueva, parseada como su propia rama en `parser.c`,
antes del camino genérico de declaración de tipos:

```altair
point#Tipo nombre [= expresión]
```

`point#Tipo` declara una variable de `VTYPE_POINTER` (`ALT_POINTER` en
runtime). El `Tipo` tras la `@` se guarda solo a efectos de documentación /
un futuro chequeo de tipos — el valor subyacente es un `void*` opaco (típicamente
de `ptr_alloc`), así que Altair todavía no obliga a que el puntero apunte
siempre a valores de `Tipo`. Es el bloque de construcción que describía la
propuesta original para construir ASTs, listas enlazadas y grafos.

```altair
point#node current = ptr_alloc(64)
if ptr_is_null(current);
    log "sin memoria"
break
ptr_free(current)
```

### 31.5 Operadores a nivel de bit

Tokens nuevos `& | ^ ~ << >>`, distinguidos correctamente de `&&`/`||` (el
lexer comprueba primero las formas de dos caracteres). Se insertaron nuevos
niveles de precedencia en la gramática de expresiones, del más flojo al más
fuerte, igual que en C:

```
comparación  ( == != < > <= >= )
      |
   bitor    ( | )
      |
   bitxor   ( ^ )
      |
   bitand   ( & )
      |
   shift    ( << >> )
      |
   suma/resta ( + - )
```

Cada operador compila a un pequeño helper de runtime (`altair_band`,
`altair_bor`, `altair_bxor`, `altair_bnot`, `altair_shl`, `altair_shr` en
`altair_rt.c`) que exige que ambos operandos sean `numeric` — los números de
Altair son `double` internamente, así que las operaciones a nivel de bit
convierten a `long long` antes de aplicar el operador de C y vuelven a
`double` al devolver. Pasar un operando que no sea numérico lanza `ALT0002`.

```altair
numeric flags = 6 & 3   / 2
numeric merged = 6 | 3  / 7
numeric x = 1 << 4      / 16
numeric inv = ~0        / -1
```

---

## 32. E/S de ficheros y ejecución de procesos

Son llamadas a función normales (`ND_FUNC_CALL`) — no hizo falta gramática
nueva, porque el compilador ya tenía un mecanismo genérico: cualquier
llamada `nombre(args)` que no sea un método conocido de lista/texto compila
directo a una llamada en C `_fn_nombre(args)`. Cada builtin de abajo es
simplemente una función de C llamada `_fn_<nombre>` implementada en
`altair_rt.c`.

| Función | Firma | Descripción |
|---|---|---|
| `open(path)` | `file` | Abre para lectura (modo `"r"`). |
| `open_write(path)` | `file` | Abre para escritura, truncando (modo `"w"`). |
| `open_append(path)` | `file` | Abre para añadir (modo `"a"`). |
| `read(f)` | `text` | Lee todo el contenido restante de `f`. |
| `read_line(f)` | `text` | Lee una línea (sin el `\n`/`\r` final). |
| `write(f, texto)` | `bool` | Escribe `texto` en `f`. |
| `close(f)` | `bool` | Cierra `f`. Se puede llamar dos veces sin problema. |
| `create_file(path)` | `bool` | Crea un fichero vacío si no existe. |
| `delete_file(path)` | `bool` | Borra un fichero. |
| `mkdir(path)` | `bool` | Crea un directorio (éxito si ya existe). |
| `file_exists(path)` | `bool` | Comprueba existencia vía `stat()`. |
| `list_dir(path)` | `list` | Lista los nombres de un directorio (excluye `.`/`..`). |
| `exec(cmd)` | `numeric` | Ejecuta `cmd` con `system()`; devuelve el código de salida. |
| `exec_capture(cmd)` | `text` | Ejecuta `cmd` con `popen()`; devuelve el stdout capturado. |

```altair
create_file("build/out.txt")
file f = open_write("build/out.txt")
write(f, "gcc invoked here")
close(f)

numeric rc = exec("gcc build/main.c -o programa.exe")
if rc == 0;
    log "build ok"
break
```

**Resolución de rutas:** las rutas se pasan tal cual a la librería estándar de
C (`fopen`, `stat`, `opendir`...) relativas al directorio de trabajo del
proceso — no hay ninguna resolución automática hacia una carpeta de
proyecto para `file`/`open`/etc. (Esa carpeta de "carpeta propia por
proyecto" descrita para las *variables persistentes* sí se implementó, ver
§35, pero es un mecanismo aparte, no parte de `file`/`open`.)

---

## 33. Punteros crudos

Los punteros se declaran con `point#Tipo` y se manipulan mediante dos
conjuntos de builtins: las funciones `ptr_*` (se llaman como cualquier otra
función) y el operador `p#` (el parser lo reescribe en las mismas llamadas
de runtime `p_*`, pero con semántica de mutación — ver §8).

### 33.1 Ciclo de vida — funciones `ptr_*`

| Función | Devuelve | Descripción |
|---|---|---|
| `ptr_alloc(n)` | `point` | Reserva `n` bytes a cero (`calloc`) y devuelve un puntero al bloque. El tamaño se guarda en una cabecera prepuesta al bloque. |
| `ptr_free(p)` | `bool` | Libera el bloque (incluida la cabecera) y pone a `NULL` el puntero interno de `p`. |
| `ptr_is_null(p)` | `bool` | `true` si `p` no es un puntero o su puntero interno es `NULL`. |

### 33.2 Acceso a nivel de byte — operador `p#`

`p#` es un prefijo de operador especial que el parser reconoce. `p#op(args)`
se reescribe en la llamada de runtime `p_op` correspondiente, y el primer
argumento siempre se pasa **por referencia** (así las mutaciones, como poner
el puntero a cero tras `p#free`, son visibles en la variable del llamador).

| Sintaxis | Devuelve | Descripción |
|---|---|---|
| `p#free(p)` | `bool` | Libera el bloque y pone `p` a `NULL` in situ (igual que `ptr_free` pero por la vía de referencia). |
| `p#null(p)` | `bool` | `true` si `p` es nulo o no es un puntero. |
| `p#bytes(p)` | `numeric` | Devuelve el número de bytes asignados a `p`. |
| `p#write(p, i, val)` | `bool` | Escribe `val` (numérico) en la ranura `i` del bloque (unidades de 8 bytes). Devuelve `false` si fuera de rango. |
| `p#read(p, i)` | `numeric` | Lee la ranura `i`. Devuelve `0` si fuera de rango. |

> **Tamaño de ranura:** `p#read`/`p#write` indexan en unidades de 8 bytes
> (`sizeof(double)`). Asigna `n * 8` bytes para almacenar `n` valores numéricos.

### 33.3 Ejemplo

```altair
/ reservar espacio para 4 valores numéricos (32 bytes)
point#numeric buf = ptr_alloc(32)
if ptr_is_null(buf);
    log "sin memoria"
break

p#write(buf, 0, 3.14)
p#write(buf, 1, 2.72)
log p#read(buf, 0)   / 3.14
log p#bytes(buf)     / 32

p#free(buf)
/ buf es ahora nulo — ptr_is_null(buf) devuelve true
```

---

## 34. Argumentos de línea de comandos

Antes de este cambio, un binario Altair compilado no tenía forma de leer su
propio `argv`. Como un compilador auto-hospedado necesita aceptar una ruta
de fichero fuente por línea de comandos, el `main(int argc, char **argv)`
generado ahora llama a `altair_set_args(argc, argv)` antes de ejecutar el
cuerpo del programa.

| Función | Firma | Descripción |
|---|---|---|
| `argc()` | `numeric` | Número de argumentos de línea de comandos (incluye argv[0]). |
| `arg(i)` | `text` | El argumento `i`-ésimo (base 0; `arg(0)` es la ruta del binario). |

```altair
numeric n = argc()
if n < 2;
    log "uso: programa <archivo>"
else;
    log "archivo: " + arg(1)
break
```

---

## 35. Variables persistentes con archivo propio

Sintaxis: tras la declaración normal de una variable, un identificador
punteado a modo de "nombre de fichero" (por ejemplo `player.coins`):

```altair
numeric coins = 120 player.coins
text playerName = "Victor" player.name
```

**Cómo funciona internamente:**

- **Parser:** tras parsear los cualificadores de almacenamiento en la
  declaración genérica de variable, si sigue un identificador de la forma
  `algo.algo`, se guarda como `n->persist_file` en el nodo AST (usa una
  instantánea/restauración del lexer para poder "mirar hacia delante" sin
  romper el resto del parseo si no hay patrón `ident.ident`).
- **Codegen — al declarar:** si `persist_file` está presente, se emite una
  llamada a `altair_persist_load("player.coins")`. Si el fichero ya existe,
  su valor **sobreescribe** el de la expresión inicial (esto permite que el
  valor sobreviva entre ejecuciones). Si no existe, se usa el valor inicial
  y se escribe inmediatamente al fichero. La variable además se registra
  en un listado global para volcarse también al apagar el programa.
- **Codegen — en cada asignación:** cualquier asignación posterior a esa
  variable (`coins = coins + 10`) dispara automáticamente un
  `altair_persist_save(...)` con el nuevo valor.
- **Formato en disco:** cada fichero de `variables/` tiene dos líneas: una
  etiqueta de tipo (`N` numeric, `T` text, `B` bool) y el valor serializado.
  Esto permite auto-describir el tipo al recargar sin que el compilador
  necesite conocerlo de antemano.
- **Ubicación:** todos estos ficheros se guardan automáticamente en la
  carpeta `variables/` (relativa al directorio de trabajo del binario
  compilado). La carpeta se crea sola si no existe.

```altair
altair.doc;
    name = "PersistTest"
    version = "1.0"
    author = "a"
create altair.doc

numeric coins = 120 player.coins
text playerName = "Victor" player.name

log "coins=" + coins
coins = coins + 10
log "coins tras sumar=" + coins
```

Primera ejecución → imprime `coins=120`, luego `coins tras sumar=130`, y dentro
de `variables/` quedan `player.coins` (`N` / `130`) y `player.name`
(`T` / `Victor`). Segunda ejecución → el `120` inicial es ignorado porque ya
existe `variables/player.coins`; arranca directamente con `coins=130`.

**Limitación conocida:** de momento solo `numeric`, `text` y `bool` se
serializan; declarar una variable `list` con nombre punteado no falla, pero
su contenido no se persiste entre ejecuciones.

---

## 36. El compilador auto-hospedado (Altair-Core)

`selfhost/altair.at` es un compilador escrito **enteramente en Altair** (usa
`file`, indexado de texto, `char`, y el control de flujo / funciones que
Altair ya tenía) que compila un subconjunto deliberadamente más pequeño del
lenguaje, llamado **Altair-Core**, directamente a C.

```
                 (bootstrap, una sola vez)
 altair.at  ──────────────────────────▶  altairc-selfhost
 (Altair real)      altairc (en C)        (binario nativo)

                 (repetible, sin necesitar inventar un compilador C)
 programa.atc ─────────────────────────▶ programa.gen.c ──▶ gcc ──▶ binario
              altairc-selfhost
```

Compilarlo:
```bash
./altairc selfhost/altair.at -o selfhost/altairc-selfhost
```

Usarlo:
```bash
./altairc-selfhost fib.atc fib.gen.c
gcc fib.gen.c -o fib_bin -lm   # -lm hace falta: numeric % compila a fmod()
./fib_bin
```

### 36.1 Gramática de Altair-Core

Altair-Core conserva la forma `if/elif/else...break`, `while...break` y
`fun nombre -> tipo ... break` de Altair, pero no tiene análisis semántico
propio — los errores de tipos aparecen como **errores de gcc** sobre el C
generado, no como errores de Altair. Los `;` tras las cabeceras son
opcionales: el lexer de Altair-Core los trata igual que un espacio en
blanco.

```
programa  := stmt*
stmt      := vardecl | assign | log | return | if | while | fun-call
vardecl   := ("numeric"|"text"|"bool") IDENT "=" expr
assign    := IDENT "=" expr
log       := "log" expr
if        := "if" expr stmt* ("elif" expr stmt*)* ("else" stmt*)? "break"
while     := "while" expr stmt* "break"
fun       := "fun" IDENT ("->" tipo)? (tipo IDENT ("," tipo IDENT)*)? stmt* "break"
return    := "return" expr?
```

Y expresiones con precedencia clásica (`or` → `and` → `not` → comparación →
suma/resta → mult/div/mod → unario → primario), donde `primario` incluye
número, cadena, `true`/`false`, identificador, llamada `nombre(args)` y
`(expr)`.

### 36.2 Mapeo de tipos (Altair-Core → C)

| Altair-Core | C generado |
|---|---|
| `numeric` | `double` |
| `text` | `char*` |
| `bool` | `int` |
| `%` | `fmod(a, b)` (el `%` nativo de C no aplica a `double`) |
| `+` | `+` nativo — **solo aritmético**, no concatena texto |

### 36.3 Runtime integrado (se emite una vez por fichero de salida)

| Función | Propósito |
|---|---|
| `concat(a, b)` | Concatenación de texto (no hay `+` para texto). |
| `streq(a, b)` | Igualdad de texto (`==`/`!=` sobre `char*` compararía punteros, no contenido). |
| `numstr(n)` | `numeric → text`, para usar con `log`. |

```altair
fun fib -> numeric numeric n;
    if n < 2;
        return n
    break
    return fib(n - 1) + fib(n - 2)
break

numeric i = 0
while i < 10;
    log numstr(fib(i))
    i = i + 1
break

log concat("hola, ", "mundo")
```

### 36.4 Qué significa y qué no significa "self-hosting" aquí

`altairc-selfhost` compila programas Altair-Core, pero **todavía no puede
compilar su propio código fuente** (`altair.at`), porque `altair.at` usa
funcionalidades que Altair-Core no soporta: `list`, `file`/`open`/`read`/
`close`, indexado de texto, y declaraciones con más de un parámetro
tipado. Cerrar ese círculo — reescribir `altair.at` usando *solo*
Altair-Core, tras ampliar Altair-Core con listas y E/S de ficheros — es el
siguiente paso natural y no está hecho todavía.

Lo que **sí** está probado de punta a punta:
- `altairc` (en C) compila con éxito `altair.at` (Altair real) →
  `altairc-selfhost`.
- `altairc-selfhost` compila con éxito programas Altair-Core (recursión,
  `if/elif/else`, `while`, concatenación de texto, `%`, comparación de
  texto) → C.
- El C generado compila limpio con `gcc` y produce el resultado correcto.

---

*Guía generada a partir de la implementación real del compilador y el
runtime de Altair. No cubre `async`, `safe block` ni `data ... save data`:
no están implementados todavía.*
