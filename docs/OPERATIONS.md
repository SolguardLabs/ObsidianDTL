# Operational Runbook

Este runbook describe como revisar ObsidianDTL como servicio local de relaying.
El objetivo es que un auditor pueda compilar, ejecutar escenarios, comparar
reportes y aislar cambios contables sin depender de infraestructura externa.

## Preparacion

Verificar Node:

```bash
node --version
```

Verificar compilador:

```bash
cc --version
```

En Windows, si `cc` no existe, el builder intenta localizar Visual Studio Build
Tools y cargar `vcvars64.bat` automaticamente.

Instalar dependencias de desarrollo:

```bash
npm ci
```

El proyecto no necesita dependencias runtime. `npm ci` instala herramientas de
desarrollo cuando se quiere replicar el workflow de CI.

## Build

```bash
npm run build
```

Salida esperada:

- `dist/obsidian-dtl` en Linux/macOS;
- `dist/obsidian-dtl.exe` en Windows.

El script escribe objetos intermedios en `dist/`. Esta carpeta esta ignorada por
git y puede eliminarse sin afectar fuentes.

## Smoke Tests Manuales

Ejecutar snapshot:

```bash
dist/obsidian-dtl snapshot
```

Comprobar:

- `packets.submitted` es `0`;
- `receipts.total` es `0`;
- `surface.domains` es `3`;
- `conservation_ok` es `true`.

Ejecutar liquidacion directa:

```bash
dist/obsidian-dtl direct
```

Comprobar:

- `packets.settled` es `1`;
- `routes.edge-direct.deliveries` es `1`;
- `balances.accounts.merchant` aumenta;
- `state_digest` tiene formato hexadecimal.

Ejecutar expiracion:

```bash
dist/obsidian-dtl expiry
```

Comprobar:

- `packets.settled` es `0`;
- `packets.rejected` es `1`;
- `last_rejection` describe expiracion;
- el balance del beneficiario no cambia.

## Suite Automatizada

```bash
npm test
```

La suite usa `--test-concurrency=1` para evitar que varios procesos intenten
recompilar o relinkar el mismo binario a la vez en Windows.

Validacion completa:

```bash
npm run ci
```

Incluye:

- check de formato local;
- build del binario C;
- tests de integracion.

## Lectura Del Reporte

Campos principales:

- `surface`: entidades configuradas.
- `packets`: conteo agregado de packets.
- `receipts`: conteo agregado de receipts.
- `balances.domains`: vault por dominio.
- `balances.accounts`: balance por cuenta.
- `domain_stats`: contadores economicos y nonces por dominio.
- `routes`: entregas y fallos por ruta.
- `planner`: health del route book y quotes de referencia.
- `packet_log`: packets registrados.
- `receipt_log`: receipts emitidos.
- `conservation_ok`: conservacion global de suministro.
- `state_digest`: digest final reproducible.

## Comparacion De Reportes

Para guardar un reporte:

```bash
dist/obsidian-dtl direct > direct.json
```

Para comparar digest:

```bash
node -e "console.log(require('./direct.json').state_digest)"
```

Si un cambio legitimo altera balances, receipts o rutas, el digest tambien debe
cambiar. Si solo cambia texto de documentacion, el digest no cambia.

## Escenarios Externos

Ejecutar un ejemplo:

```bash
dist/obsidian-dtl script examples/direct.obs
```

Crear un escenario negativo:

```text
SCENARIO ruta-inactiva
ROUTE edge-paused edge edge core relay-edge 20000 20 1 0
PACKET pkt-paused edge core alice merchant 41 100000000 50000 10 release
TRY_DELIVER pkt-paused edge-paused 0
```

El reporte debe indicar un rechazo de politica y `conservation_ok = true`.

## Checklist De Auditoria

- Compilar desde limpio.
- Ejecutar `npm run ci`.
- Revisar que no haya servicios externos.
- Confirmar que `vulnerability.md` esta en `.gitignore`.
- Revisar rutas registradas en el fixture.
- Revisar orden de validaciones en `transfer.c`.
- Revisar ventanas de nonce en `nonce.c`.
- Revisar seleccion de rutas y quotes en `routing.c`.
- Revisar campos incluidos en `obs_packet_digest`.
- Revisar campos incluidos en `obs_receipt_digest`.
- Crear al menos un script manual con `EMPTY`.
- Crear al menos un script negativo con `TRY_DELIVER`.
- Verificar balances antes y despues de cada entrega.
- Verificar que los rechazos no cambian balances.
- Verificar que receipts tardios quedan marcados.
- Verificar que el digest cambia ante transiciones contables.

## Limpieza

Eliminar artefactos:

```bash
rm -rf dist
```

En PowerShell:

```powershell
Remove-Item -Recurse -Force dist
```

No eliminar `src/`, `tests/`, `docs/` ni `examples/`.
