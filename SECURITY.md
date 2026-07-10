# Security Model

ObsidianDTL modela un relayer DTL interno donde los dominios economicos
mantienen vaults separados y las rutas de transporte estan registradas antes de
aceptar packets. La suite local no usa claves reales ni red externa; los
digests canonicos representan las decisiones de vinculacion que un servicio
productivo delegaria en firmas y receipts firmados.

## Supuestos

- Los dominios registrados son entidades internas autorizadas.
- Las cuentas del fixture representan operadores, relayers y beneficiarios ya
  conocidos por el sistema.
- Las rutas solo pueden crearse durante bootstrap o por automatizacion de
  configuracion aprobada.
- Los packets deben declarar origen logico, destino, nonce, importe, fee maximo
  y expiracion.
- Los receipts deben referenciar el packet, la ruta, el relayer y el digest del
  packet aceptado.

## Invariantes Esperadas

- El suministro total agregado se conserva despues de cada escenario.
- Un packet expirado no produce movimiento contable.
- Un nonce ya consumido dentro de su ventana operativa no puede contabilizar una
  segunda entrega por la misma superficie de recepcion.
- Un receipt solo es aceptado cuando coincide con el digest y los campos
  canonicos de la entrega.
- Las rutas inactivas, con destino incorrecto o fee superior al permitido deben
  rechazarse.
- Las confirmaciones tardias quedan reflejadas como senal operacional sin
  invalidar reportes historicos.

## Validaciones Automatizadas

La suite `node:test` cubre:

- bootstrap de dominios, cuentas y rutas;
- liquidacion primaria;
- ruta fallback autorizada;
- packets expirados;
- entrega duplicada sobre una ruta;
- confirmacion tardia;
- escenarios externos por script.

Los tests ejecutan el binario compilado y consumen su JSON publico. Esto evita
que la suite valide un modelo paralelo diferente al runner C.

## Gestion De Dependencias

El proyecto no tiene dependencias runtime de npm. Node se usa para scripts de
build, checks y tests. `prettier` queda como dependencia de desarrollo para
repositorios que quieran aplicar formato sobre Markdown, YAML o JavaScript.

Dependabot revisa:

- `npm`;
- `github-actions`.

## Alcance De Revision

Areas recomendadas para auditoria:

- vinculacion canonica entre packets, rutas y receipts;
- ventanas de nonce por dominio;
- orden de validacion y aplicacion contable;
- manejo de expiraciones y confirmaciones tardias;
- consistencia de reportes JSON y digest final;
- scripts externos con comandos `ROUTE`, `PACKET` y `DELIVER`.

## Reporte Interno

Los hallazgos deben incluir:

- escenario minimo reproducible;
- packet, ruta y receipt implicados;
- balances antes y despues;
- contador o digest afectado;
- severidad economica;
- propuesta de test de regresion.

No se deben usar claves, endpoints ni datos productivos para reproducir
escenarios. Todo el analisis esperado puede hacerse con el fixture local o con
scripts `.obs` creados especificamente para revision.

