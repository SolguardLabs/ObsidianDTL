# Receipts And Nonce Windows

Esta nota resume como leer los controles de receipts y ventanas de nonce en el
runner local. No reemplaza la revision del codigo, pero sirve como mapa para
auditores que empiezan por los reportes JSON.

## Packet Digest

`obs_packet_digest` cubre los campos economicos del packet:

- identificador;
- origen logico;
- destino;
- sender;
- beneficiary;
- nonce;
- amount;
- max fee;
- timestamps;
- kind.

El digest es determinista y se imprime en `packet_log[].digest`.

## Receipt Digest

`obs_receipt_digest` cubre:

- identificador del receipt;
- packet;
- ruta;
- origen logico;
- transporte;
- destino;
- relayer;
- amount;
- fee;
- nonce del packet;
- digest del packet;
- timestamp de confirmacion.

El digest se imprime en `receipt_log[].digest`.

## Confirmaciones Tardias

Una confirmacion se marca como tardia cuando:

- `confirmed_at` queda despues de `packet.expires_at`; o
- `confirmation_delay` supera `route.max_delay`.

El receipt puede seguir aceptado si los campos canonicos coinciden. La senal
`late` sirve para monitoreo operativo y conciliacion posterior.

## Ventanas De Nonce

Cada dominio mantiene:

- `floor`: minimo nonce esperado;
- `span`: anchura maxima aceptada;
- `values`: slots consumidos;
- `accepted`: consumos aceptados;
- `rejected`: consumos rechazados.

El reporte no imprime todos los slots para mantener el JSON compacto. En su
lugar expone:

- `nonce_floor`;
- `nonces_used`;
- `nonce_digest`.

## Rechazos

Un rechazo queda visible en cuatro zonas:

- `packets.rejected`;
- `last_rejection`;
- `routes.<id>.failures`;
- `packet_log[].rejected`.

Los rechazos esperados en scripts deben usar `TRY_DELIVER` para que el runner
complete el reporte y permita inspeccionar balances.

## Lectura Rapida

Para una entrega correcta se espera:

- un packet submitted;
- un receipt accepted;
- una delivery en la ruta;
- un incremento del beneficiario;
- un incremento del relayer;
- una reduccion del vault de origen;
- `conservation_ok = true`.

