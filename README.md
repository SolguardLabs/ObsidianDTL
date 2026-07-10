# ObsidianDTL

![banner](./assets/banner.png)

ObsidianDTL es un relayer DTL escrito en C para transportar mensajes
economicos entre dominios internos. El runner local modela rutas primarias y de
fallback, receipts de confirmacion, ventanas de nonce, proteccion contra
duplicados y reportes JSON deterministas para integracion con herramientas de
auditoria.

El proyecto esta pensado como un servicio autocontenido. No requiere nodos
externos, bases de datos ni claves reales: los fixtures cargan dominios,
cuentas, rutas y packets reproducibles dentro del proceso.

## Componentes

- `src/obsidian.h`: tipos publicos, limites de capacidad y API interna.
- `src/ledger.c`: registro de dominios, cuentas, rutas, eventos y digest de
  estado.
- `src/nonce.c`: ventanas de nonce por dominio con rango aceptado y slots
  consumidos.
- `src/transfer.c`: validacion de packets, seleccion de ruta, receipts y
  movimiento contable.
- `src/scenario.c`: fixtures y escenarios CLI de referencia.
- `src/script.c`: parser de escenarios externos en formato lineal.
- `src/report.c`: serializacion JSON para tests y consumidores externos.
- `tests/node`: tests de integracion que ejecutan el binario compilado.
- `scripts/build.mjs`: compilacion portable con GCC, Clang o MSVC.
- `docs/`: runbook operativo, referencia de scripting y notas de receipts.
- `examples/`: scripts `.obs` pequenos para pruebas manuales.

## Modelo Operativo

Un packet economico declara:

- dominio logico de origen;
- dominio destino;
- nonce;
- importe bruto;
- fee maximo aceptado;
- beneficiario;
- tipo de instruccion;
- expiracion temporal.

Una ruta define el transporte operativo autorizado para mover packets entre un
origen logico y un destino. Las rutas tienen relayer asignado, fee fijo, demora
maxima de confirmacion, prioridad y estado activo.

El runner ejecuta cada entrega en cinco fases:

1. carga del packet y de la ruta seleccionada;
2. validacion de compatibilidad de dominios, fees y expiracion;
3. verificacion de ventana de nonce;
4. aplicacion contable sobre el dominio de origen y cuentas receptoras;
5. emision de receipt y digest de estado.

## Escenarios CLI

El binario acepta un escenario opcional:

```bash
dist/obsidian-dtl snapshot
dist/obsidian-dtl direct
dist/obsidian-dtl expiry
dist/obsidian-dtl duplicate
dist/obsidian-dtl late
dist/obsidian-dtl fallback
dist/obsidian-dtl routing
```

Si no se pasa argumento, se ejecuta `direct`.

Cada escenario devuelve JSON con:

- superficie registrada de dominios, cuentas y rutas;
- contadores de packets y receipts;
- balances por dominio y cuenta;
- estadisticas de nonces por dominio;
- entregas y fallos por ruta;
- packet log y receipt log;
- conservacion de suministro;
- digest final de estado.

## Scripts De Escenario

Tambien se puede ejecutar un archivo lineal:

```bash
dist/obsidian-dtl script ./scenario.obs
```

Comandos soportados:

```text
SCENARIO <name>
RESET
EMPTY
DOMAIN <id> <label> <vault>
ACCOUNT <id> <label> <balance>
ROUTE <id> <logical_origin> <transport_origin> <target> <relayer> <fee> <max_delay> <priority> <active>
PACKET <id> <logical_origin> <target> <sender> <beneficiary> <nonce> <amount> <max_fee> <ttl> <kind>
DELIVER <packet_id> <route_id> <confirmation_delay>
TRY_DELIVER <packet_id> <route_id> <confirmation_delay>
QUOTE <logical_origin> <target> <amount> <max_fee>
ADVANCE <delta>
CLOCK <value>
```

`RESET` carga el fixture por defecto. `EMPTY` limpia el ledger para crear un
fixture manual. `TRY_DELIVER` registra el rechazo en el reporte sin hacer fallar
el script. `QUOTE` selecciona una ruta valida sin mover balances.

Ejemplo:

```text
SCENARIO mesa-retail
PACKET pkt-101 edge core alice merchant 101 100000000 50000 12 release
DELIVER pkt-101 edge-direct 1
```

## Requisitos

- Node.js `20` o superior.
- Un compilador C:
  - Linux/macOS: `cc`, `gcc` o `clang`.
  - Windows: Visual Studio Build Tools con componente C++ o un GCC/Clang en
    `PATH`.

## Comandos

Compilar:

```bash
npm run build
```

Tests:

```bash
npm test
```

Validacion completa:

```bash
npm run ci
```

Scripts Bash equivalentes:

```bash
bash scripts/tests.sh
bash scripts/ci.sh
```

## CI

El workflow de GitHub Actions ejecuta:

1. checkout del repositorio;
2. instalacion de Node.js;
3. `npm ci`;
4. validacion de formato local;
5. compilacion del binario C;
6. tests de integracion con `node:test`.

Dependabot revisa dependencias npm y GitHub Actions semanalmente.

## Estado Del Proyecto

ObsidianDTL es un laboratorio tecnico autocontenido. El estado de protocolo se
genera localmente desde fixtures deterministas y los reportes JSON son estables
para comparaciones en revisiones, CI y herramientas externas.
