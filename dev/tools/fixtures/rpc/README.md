# Fixtures RPC — HUD_TOP5 roto

Contrato del payload (servidor → cliente), en este orden:

1. `int` filas, válido solo en 0..5
2. por fila: `string` nombre, `int` puntos, `int` bajas, `int` muertes, `int` asistencias

Un entero negativo en puntos, bajas, muertes o asistencias descarta el paquete entero (mismo efecto que un `Read` fallido: no se llama a `SetTop`).

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

# Fixtures RPC — HUD_ROUND_RESULT roto

Contrato (servidor → cliente), en este orden:

1. `string` nombre del ganador (puede ir vacío)
2. `string` nombre del mapa (puede ir vacío)
3. `int` `KillsToWin` (>= 0)
4. `int` filas, válido solo en 0..64
5. por fila: `string` nombre, `int` bajas, `int` muertes, `int` asistencias, `int` es el receptor (0 o 1; `< 0` descarta)

El handler es `NexoDM_HandleHudRoundResultRPC` en `NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c`. Si un `ctx.Read` falla, las filas no están en 0..64, un nombre de fila está vacío o un número es negativo, no llama a `NexoDM_HudData.SetRoundResult`. El resultado anterior y su serial quedan igual.

Dato previo de referencia: serial ya recibido, ganador `Ana`, mapa `Elektro`, `KillsToWin` 20, 1 fila `Ana / 3 / 1 / 0 / receptor`.

## 1. Truncado tras la cabecera

Cabecera completa (`Ana`, `Elektro`, 20, `filas = 2`) y solo una fila completa. La segunda fila no está en el stream.

Resultado: descartado. El `Read` de la segunda fila falla. Sigue `Ana / Elektro / 20 / Ana 3-1-0`.

## 2. Filas = 65

`filas = 65` tras una cabecera válida. No se lee ninguna fila.

Resultado: descartado. 65 no está en 0..64. El resultado anterior sigue.

## 3. Filas = -1

`filas = -1`. No se lee ninguna fila.

Resultado: descartado. El resultado anterior sigue.

## 4. Nombre de fila vacío

`filas = 1`. El `string` del nombre es `""`. El resto de la fila puede ir bien formado.

Resultado: descartado. No se llama a `SetRoundResult`. El resultado anterior sigue.

## 5. Bajas negativas

`filas = 1`, nombre `Bo`, bajas `-1`, muertes 0, asistencias 0, receptor 0.

Resultado: descartado. Un número negativo tira el paquete entero. El resultado anterior sigue.

## 6. KillsToWin negativo

Cabecera con `KillsToWin = -1` antes del conteo de filas.

Resultado: descartado. No se leen filas. El resultado anterior sigue.

## 7. Stream cortado en el nombre del mapa

Solo llega el `string` del ganador. Falta el mapa.

Resultado: descartado. El `Read` del mapa falla. El resultado anterior sigue.
