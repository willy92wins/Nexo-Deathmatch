# Fixtures RPC — HUD_TOP5 roto

Contrato del payload (servidor → cliente), en este orden:

1. `int` filas, válido solo en 0..5
2. por fila: `string` nombre, `int` puntos, `int` bajas, `int` muertes

El handler es `NexoDM_HandleHudTop5RPC` en `NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c`. Si un `ctx.Read` falla o las filas no están en 0..5, no llama a `NexoDM_HudData.SetTop`. El top anterior queda igual. No hay excepción.

Dato previo de referencia (lo que el cliente ya tenía antes del payload roto): 1 fila, nombre `Ana`, puntos 100, bajas 1, muertes 0.

## 1. Truncado tras la fila 2

`filas = 3`, y en el stream solo hay dos filas completas (nombre, puntos, bajas, muertes) y luego se acaba.

- Fila 1: `Ana`, 100, 1, 0
- Fila 2: `Bo`, 50, 0, 1
- Fila 3: ausente

Resultado: descartado. El `Read` de la tercera fila falla. El top sigue siendo `Ana / 100 / 1 / 0`.

## 2. Filas = 9

`filas = 9`. No se lee ninguna fila.

Resultado: descartado. 9 no está en 0..5. El top sigue siendo `Ana / 100 / 1 / 0`.

## 3. Nombre ausente

`filas = 1`. El stream no trae el `string` del nombre: el primer campo de la fila son los puntos (`int` 100) o el stream se corta antes del nombre.

Resultado: descartado. El `Read` del nombre falla y no se aplican los enteros de esa fila. El top sigue siendo `Ana / 100 / 1 / 0`.
