# F8A — datos del HUD final

Solo datos (servidor → cliente y almacén). No se han tocado `NexoDM/gui/`, `NexoDM_HUD.c` ni `NexoDM_MissionGameplay.c`. Puntos, `IsBetterByTiebreak`, leaderboard y el flujo de ronda siguen igual. El top 5 compara también las asistencias en la caché de “no ha cambiado”, para que un cambio solo de asistencias salga en el siguiente envío.

## 1. HUD_SELF

Orden: `int` puntos, `int` bajas, `int` muertes, `int` asistencias, `int` puesto.

- Emisor: cinco `Write` en `NexoDM_RoundManager.c:2641-2645` (`SendHudSelf`).
- Receptor: `NexoDM_HandleHudSelfRPC` en `NexoDM_PlayerBase.c:626`. Negativo en cualquiera de los cinco sale sin `SetSelf`.
- Almacén: `SetSelf` / `GetAssists` en `NexoDM_HudData.c:299` y `:329`.

## 2. HUD_TOP5

Orden de fila: nombre, puntos, bajas, muertes, asistencias. El orden de filas sigue siendo `CollectRankedParticipants` (`IsBetterByTiebreak`).

- Emisor: `NexoDM_RoundManager.c:2892` (`SendHudTop5To`).
- Caché de asistencias: `m_HudTop5Assists` en `NexoDM_RoundManager.c:72`, comparación `:2918`, recuerdo `:2953`.
- Receptor: `NexoDM_HandleHudTop5RPC` en `NexoDM_PlayerBase.c` (tras las muertes lee asistencias y rechaza puntos, bajas, muertes o asistencias `< 0` antes de `SetTop`).
- Almacén: `NexoDM_HudTopRow.assists` y `GetTopAssists` en `NexoDM_HudData.c:388`.

## 3. HUD_KILL

Ya llevaba el classname y el cliente ya lo guardaba. No se ha cambiado quién lo recibe.

- Emisor: `HudWeaponClass` (cuchillo de config, si no `GetType()` del objeto que no es jugador, si no lo que lleva en las manos, si no vacío) y `rpc.Write(weaponClass)` en `NexoDM_RoundManager.c:2779`.
- Receptor: tres `Read` y `AddKill` en `NexoDM_PlayerBase.c:850`. Un `Read` que falla no llama a `AddKill`.
- Almacén: `NexoDM_HudKillLine.weaponClass` y `GetKillWeapon` en `NexoDM_HudData.c:568`.

## 4. HUD_ROUND_RESULT

`NexoDM_RPC.c:13` = `74621713`. Se envía en `EndRound` solo si la ronda estaba ACTIVE, después del leaderboard y antes de `BroadcastGlobalTop10` (`NexoDM_RoundManager.c:268-269`). Destino: cada participante conectado (el mismo bucle que el top 10: ficha `PARTICIPANT` y `FindPlayerBySteamId`).

El nombre del mapa se copia de `GetActiveMap().Name` antes de rotar el índice (`NexoDM_RoundManager.c:224-229`). `admin_stop` o un ganador sin nombre dejan el ganador vacío. Si `GetPlayerName` está vacío se usa `playerName` de la ficha. Filas: `CollectRoundFichas` (todos los participantes, conectados o no), mismo `IsBetterByTiebreak`, tope 64. Nombre de fila vacío en origen se sustituye por `playerName` y, si sigue vacío, por el SteamID, para no obligar al cliente a tirar la clasificación. `isReceiver` es 1 si el SteamID de la fila es el del receptor.

`ClearMatchStats` no borra este dato: el `HUD_ROUND` inactivo del mismo `EndRound` vacía vitales, ficha, top y killfeed, y el resultado tiene que seguir ahí. `Reset` sí lo borra. `ResetExceptGlobal` lo conserva junto al top 10 (`NexoDM_HudData.c:187-213`) para que borrar el cuerpo no se lleve la clasificación. `SetRoundResult` sube `m_RoundResultSerial` (`NexoDM_HudData.c:452`).

## 5. Fail-closed

| Payload | Qué descarta el paquete entero |
|---|---|
| HUD_SELF | `Read` que falla; puntos, bajas, muertes, asistencias o puesto `< 0` |
| HUD_TOP5 | `Read` que falla; filas fuera de 0..5; nombre de fila vacío; puntos, bajas, muertes o asistencias `< 0` |
| HUD_KILL | `Read` que falla (asesino, víctima o classname). El classname vacío es válido |
| HUD_ROUND_RESULT | `Read` que falla; `KillsToWin < 0`; filas fuera de 0..64; nombre de fila vacío; bajas, muertes, asistencias o `isReceiver < 0` |

En todos, el `return` ocurre antes de escribir el almacén. Casos rotos de `HUD_ROUND_RESULT`: `dev/tools/fixtures/rpc/README.md:36`.

## Tabla RPC tocados

| RPC | Id | Tipos en orden | A quién | Cuándo |
|---|---|---|---|---|
| HUD_SELF | 74621709 | `int` puntos, `int` bajas, `int` muertes, `int` asistencias, `int` puesto | solo el dueño | igual que antes: al entrar, al cambiar su ficha y al reenviar rangos tras una baja o una desconexión |
| HUD_TOP5 | 74621710 | `int` filas (0..5); por fila `string` nombre, `int` puntos, `int` bajas, `int` muertes, `int` asistencias, `bool` es el receptor | cada participante, con su propio flag; copia directa al que entra | igual que antes: cuando cambia el top 5 (ahora también si solo cambian asistencias), como mucho un broadcast por segundo; también al entrar y al cerrar la ronda (`FlushHudTop5`) |
| HUD_KILL | 74621711 | `string` asesino, `string` víctima, `string` classname (vacío si no hay) | participantes conectados | cada baja puntuable con la ronda ACTIVE. Sin cambio de destinatarios |
| HUD_ROUND_RESULT | 74621713 | `string` ganador (vacío si no hay), `string` mapa de la ronda que acaba, `int` `KillsToWin`, `int` filas (0..64); por fila `string` nombre, `int` bajas, `int` muertes, `int` asistencias, `int` es el receptor (1/0) | cada participante conectado | al acabar una ronda que estuvo ACTIVE, antes de `HUD_GLOBAL_TOP10` |

## LO QUE NO PUDE VERIFICAR

- No compilé Enforce ni arranqué el servidor. El encargo prohibía shell. No hay log de script ni cliente real.
- No ejecuté los fixtures: el README describe el contrato; no hay lector automático en este paso.
- No medí en un cliente que `HUD_ROUND_RESULT` llegue antes que `HUD_GLOBAL_TOP10`. En `EndRound` el `Send` del resultado está en la línea anterior al del top 10, al mismo identity.
- No comprobé `Param` de script: `HUD_SELF` pasó de un `Param4` a cinco enteros sueltos. El orden es el de los `Write`/`Read` de arriba; no hay prueba de red.
- El HUD visual sigue leyendo puntos en `NexoDM_HUD.c`. Esta fase no lo cambia: bajas, muertes y asistencias ya están en `NexoDM_HudData` para el siguiente implementador (`GetAssists`, `GetTopAssists`, `GetRoundResultSerial`).

## Corrección 1

Revisión `F8-REVIEW-R1.md`, solo #3 y #10.

- **#3.** Cada fila de `HUD_TOP5` lleva al final un `bool` (`true` si el SteamID de la fila es el del destinatario). El envío ya era uno por participante (`SendHudTop5To`, `NexoDM_RoundManager.c:2855`); el flag se calcula ahí con `identity.GetPlainId()` (`:2890`) y se escribe en `:2900`. El tope de un broadcast por segundo no se ha movido (`RequestHudTop5`). El cliente lo lee en `NexoDM_HandleHudTop5RPC` (`NexoDM_PlayerBase.c:695`) y lo guarda en `NexoDM_HudTopRow.isReceiver`. Getter: `GetTopIsReceiver` (`NexoDM_HudData.c:397`). Un `Read` que falla sigue sin llamar a `SetTop`.
- **#10.** `HudDisplayName` (`NexoDM_RoundManager.c:3267`): `GetPlayerName`, si vacío `playerName`, si vacío `"?"`. Lo usan `SendHudTop5To` y `SendRoundResultTo`. `HUD_GLOBAL_TOP10` hace lo mismo con el nombre de la tabla y, si falta, la ficha del SteamID (`NexoDM_RoundManager.c:3154`); si sigue vacío, `"?"` (`:3159` y `:3166`). Ya no se manda el SteamID ni un nombre vacío en esas filas.

No se ha compilado ni probado en juego. El HUD visual sigue comparando nombres; esta corrección no lo cambia.
