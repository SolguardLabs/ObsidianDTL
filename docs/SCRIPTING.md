# Referencia de scripting

El modo `script` permite construir escenarios reproducibles sin recompilar el
runner. Cada linea contiene un comando y argumentos separados por espacios. Las
lineas vacias y las lineas que empiezan por `#` se ignoran.

```bash
dist/obsidian-dtl script examples/direct.obs
```

El reporte final siempre se emite en JSON por `stdout`. Los errores de parseo,
configuracion o entrega se emiten por `stderr` y devuelven codigo de salida no
cero, salvo cuando se usa `TRY_DELIVER`.

## Ciclo de vida

Por defecto, un script arranca con el fixture base:

- dominios `edge`, `core` y `treasury`;
- cuentas `alice`, `merchant`, `relay-edge`, `relay-core`, `ops` y `auditor`;
- rutas `edge-direct`, `edge-fallback` y `treasury-direct`.

Esto permite que un escenario minimo solo declare packets y entregas.

```text
SCENARIO venta-simple
PACKET pkt-1 edge core alice merchant 1 100000000 50000 10 release
DELIVER pkt-1 edge-direct 0
```

Para limpiar todo el fixture se usa `EMPTY`.

```text
EMPTY
SCENARIO mesa-manual
DOMAIN branch Branch 1000000
DOMAIN hub Hub 500000
ACCOUNT operator Operator 0
ACCOUNT receiver Receiver 0
ACCOUNT relay Relay 0
ROUTE branch-direct branch branch hub relay 25 10 1 1
PACKET pkt-branch branch hub operator receiver 1 1000 25 10 release
DELIVER pkt-branch branch-direct 1
```

Para volver al fixture base despues de cambios manuales se usa `RESET`.

## Comandos

### `SCENARIO`

Asigna un nombre al reporte.

```text
SCENARIO cierre-diario
```

El nombre no afecta la contabilidad. Solo se incluye en `scenario` y en el
digest final.

### `EMPTY`

Limpia el ledger y deja el reloj en `1`.

```text
EMPTY
```

Despues de `EMPTY` se deben registrar dominios, cuentas y rutas antes de crear
packets.

### `RESET`

Limpia el ledger y carga el fixture base.

```text
RESET
```

Es util para scripts largos donde se quieran separar secciones con estados
independientes.

### `DOMAIN`

Registra un dominio economico.

```text
DOMAIN <id> <label> <vault>
```

Ejemplo:

```text
DOMAIN branch Branch 1000000
```

`label` no admite espacios. `vault` debe ser un entero positivo o cero.

### `ACCOUNT`

Registra una cuenta.

```text
ACCOUNT <id> <label> <balance>
```

Ejemplo:

```text
ACCOUNT merchant Merchant 0
```

Las cuentas se usan como remitentes, beneficiarios y relayers. El runner valida
que existan antes de aceptar packets o rutas.

### `ROUTE`

Registra una ruta de entrega.

```text
ROUTE <id> <logical_origin> <transport_origin> <target> <relayer> <fee> <max_delay> <priority> <active>
```

Ejemplo:

```text
ROUTE branch-direct branch branch hub relay 25 10 1 1
```

Campos:

- `logical_origin`: dominio economico declarado por el packet.
- `transport_origin`: dominio operativo que transporta la entrega.
- `target`: dominio destino.
- `relayer`: cuenta que cobra el fee.
- `fee`: importe cobrado por entrega.
- `max_delay`: demora maxima esperada para receipts.
- `priority`: orden operativo usado por herramientas externas.
- `active`: `1` para aceptar entregas; `0` para rechazar la ruta.

### `PACKET`

Registra una instruccion economica.

```text
PACKET <id> <logical_origin> <target> <sender> <beneficiary> <nonce> <amount> <max_fee> <ttl> <kind>
```

Ejemplo:

```text
PACKET pkt-1 edge core alice merchant 9 100000000 50000 10 release
```

`kind` acepta:

- `release`;
- `withdraw`;
- `settle`.

Los tres tipos comparten el mismo modelo contable en el runner local. La
diferencia queda serializada en el digest para que herramientas externas puedan
distinguirlos.

### `QUOTE`

Selecciona la mejor ruta disponible para un origen, destino, importe y fee
maximo. No registra packets, no emite receipts y no mueve balances.

```text
QUOTE <logical_origin> <target> <amount> <max_fee>
```

Ejemplo:

```text
QUOTE edge core 100000000 50000
```

Si ninguna ruta satisface los limites, el script falla. Para escenarios donde se
quiera continuar despues de un fallo de seleccion, modelar la ruta como inactiva
y usar `TRY_DELIVER` sobre un packet explicito.

### `DELIVER`

Intenta entregar un packet por una ruta.

```text
DELIVER <packet_id> <route_id> <confirmation_delay>
```

Si la entrega se rechaza, el script falla y devuelve codigo no cero.

### `TRY_DELIVER`

Intenta entregar un packet por una ruta, pero no aborta el script si se rechaza.

```text
TRY_DELIVER <packet_id> <route_id> <confirmation_delay>
```

El rechazo queda en:

- `packets.rejected`;
- `routes.<id>.failures`;
- `last_rejection`;
- `packet_log[].rejected`.

### `ADVANCE`

Avanza el reloj relativo del ledger.

```text
ADVANCE <delta>
```

Ejemplo:

```text
ADVANCE 5
```

Se usa para simular expiraciones.

### `CLOCK`

Asigna un valor absoluto al reloj.

```text
CLOCK <value>
```

`CLOCK` es util para reproducir reportes con tiempos concretos.

## Reglas De Parseo

- Los argumentos se separan por espacios o tabuladores.
- No hay comillas ni escapes.
- Los labels deben escribirse sin espacios.
- Los enteros se interpretan en base 10.
- Los comentarios empiezan por `#` al inicio de la linea.
- El parser procesa el archivo de arriba hacia abajo.

## Patrones Recomendados

Usar nonces pequenos en ejemplos facilita leer `domain_stats`.

Mantener un solo packet por script ayuda a aislar cambios contables.

Usar `TRY_DELIVER` para rechazos esperados evita que los escenarios negativos
dependan del codigo de salida.

Nombrar packets con prefijos de dominio, por ejemplo `edge-001`, mejora la
trazabilidad en `packet_log`.
