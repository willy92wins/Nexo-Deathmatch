# F5a — datos del HUD (RPC + almacén cliente)

Hecho en código. No compilado y no probado in-game (esta sesión no usa shell ni el juego).

## 1. Vitales

Cada segundo, en servidor, solo participante vivo de ronda ACTIVE. Porcentajes 0-100 redondeados (medio hacia arriba) y se manda `HUD_VITALS` solo si alguno cambió, `guaranteed = false`, a su identidad.

- Tick: `NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:30` (`OnScheduledTick` → `NexoDM_TickVitals` en `:286`).
- Gate: `GetGame().IsServer()` y `IsDedicatedServer()` (el mod no muta estado fuera del dedicado). El cuerpo está en `#ifdef SERVER`, así que el `TickScheduler` del cliente no lo ejecuta.
- Fórmulas: salud `GetHealth/GetMaxHealth("", "Health")`, sangre `GetHealth("", "Blood") / 5000`, agua `GetStatWater().Get() / PlayerConstants.SL_WATER_MAX`, comida `GetStatEnergy().Get() / PlayerConstants.SL_ENERGY_MAX`.

## 2. Estado de ronda

`HUD_ROUND`: segundos que quedan (suelo de `m_RoundEndsAtMs - GetTime()`), `KillsToWin`, activa = 1 o 0.

- El reloj se arma al arrancar el timer de ronda: `NexoDM_RoundManager.c:477`.
- Envío al empezar, a cada participante vivo: `SendHudJoinState` en `:491`.
- Entrada tardía `:1227`, reconexión `:1281` y `:1300`, respawn `:1022` (quien estaba muerto en el `BeginRound` no recibió el primer paquete).
- Al acabar, con la ronda ya `ENDED` y el rol de participante todavía puesto: `:253`.
- El cliente descuenta en local: `NexoDM_HudData.GetRoundSecondsLeft` en `NexoDM_HudData.c:159`.

## 3. Mis datos

`HUD_SELF`: puntos, bajas, muertes, puesto (1 = primero). El puesto sale del mismo orden que el scoreboard (`CollectRankedParticipants` en `NexoDM_RoundManager.c:2878`, `IsBetterByTiebreak`).

- Al entrar (los mismos sitios que el punto 2): `SendHudSelf` en `NexoDM_RoundManager.c:2508`.
- Al cambiar: muerte, asesino acreditado y asistentes, dentro de `PublishKillHud` (`:2617`), llamado desde `OnPlayerKilled` (`:711`) antes de limpiar cuchillo y asistencias.

## 4. Top 5

`HUD_TOP5`: `int` filas (0-5) y, por fila, nombre, puntos, bajas, muertes. Orden = `CollectRankedParticipants` (`NexoDM_RoundManager.c:2878`, mismo selection-sort que `OnScoreboardRequest`).

- Si el top 5 cambió y pasó ≥ 1 s desde el último broadcast: `BroadcastHudTop5` (`:2757`).
- Si cambió dentro de ese segundo: `RequestHudTop5` (`:2711`) deja un `CallLater` a `FlushHudTop5` (`:2747`). `EndRound` vacía ese pendiente en el acto (`:251`) para no mandar un top vacío cuando `ReturnToIdle` ya borró las fichas.
- Quien entra recibe además una copia directa (`SendHudTop5To`, `:2784`) aunque el ranking global no haya cambiado: si no, el cliente reconectado se queda sin tabla.

## 5. Killfeed

`HUD_KILL` en cada baja puntuable de ronda activa (`PublishKillHud`, `:2640`): nombre del asesino acreditado (vacío si no hay), nombre de la víctima, classname del arma (cuchillo de config si el último golpe fue cuchillo; si no, `GetType()` del objeto que no es jugador; si no, lo que lleva en las manos; si no, vacío).

## 6. Almacén cliente

`NexoDM/scripts/4_World/NexoDM/NexoDM_HudData.c`. Singleton `Get()` solo en cliente (`:64`). Getters de vitales, ronda, cuenta propia, top e historial de bajas. Los handlers de `NexoDM_PlayerBase.c` (`:388`, `:411`, `:434`, `:450`, `:491`) escriben ahí. No dibuja nada.

Al pasar la ronda de inactiva a activa se vacía el killfeed (`SetRound`, `NexoDM_HudData.c:112`) para no mezclar la ronda anterior. Un `HUD_ROUND` activo repetido no lo vacía.

## 7. Robustez

- Servidor: si llega un RPC de HUD, `OnRPC` vuelve sin leer (`NexoDM_PlayerBase.c:67`).
- Cliente: un `ctx.Read` que falla, un porcentaje fuera de 0..100, segundos o `KillsToWin` negativos, o un flag de ronda distinto de 0/1, tiran el payload entero. El top 5 con filas fuera de 0..5 vuelve antes de leer filas; un `Read` a medias no llama a `SetTop`.
- Fixtures descritos (no hay stream binario de `ScriptRPC` en el repo): `dev/tools/fixtures/rpc/README.md`.

## Tabla RPC

| RPC | id | emisor | receptor | tipos en orden | a quién | cuándo |
|---|---|---|---|---|---|---|
| HUD_VITALS | 74621707 | servidor | cliente (`OnRPC`) | `Param4<int,int,int,int>` salud, sangre, agua, comida | solo el dueño | cada 1 s si está vivo en ronda y algún entero cambió; `guaranteed` false |
| HUD_ROUND | 74621708 | servidor | cliente | `Param3<int,int,int>` segundos, `KillsToWin`, activa 0/1 | cada participante conectado | `BeginRound`, entrada tardía, reconexión, respawn, `EndRound` |
| HUD_SELF | 74621709 | servidor | cliente | `Param4<int,int,int,int>` puntos, bajas, muertes, puesto | solo el dueño | al entrar (mismos sitios que la ronda) y al cambiar su ficha (baja, asistencia, muerte) |
| HUD_TOP5 | 74621710 | servidor | cliente | `int` filas, luego por fila `string`, `int`, `int`, `int` (nombre, puntos, bajas, muertes) | participantes; copia directa al que entra | cuando cambia el top 5, como mucho un broadcast por segundo; también al entrar |
| HUD_KILL | 74621711 | servidor | cliente | `string`, `string`, `string` (asesino, víctima, classname) | participantes conectados | cada baja puntuable con la ronda ACTIVE |

`guaranteed` es false solo en vitales (el segundo siguiente repite el valor). El resto va guaranteed true, igual que `SCOREBOARD_DATA` y `KILLER_CAM`.

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce (no hay shell en esta sesión). No hay PBO nuevo ni arranque de DayZ.
- Que `Param4` / `Param3<int,int,int>` sobrevivan el `Read` igual que `KILLER_CAM` (el patrón es el mismo que ya usa el mod, sin prueba de red).
- El redondeo de los porcentajes contra valores reales de salud/sangre/stats.
- El desfase del cronómetro local frente al `Timer` de la ronda.
- Los tres payloads rotos del README: el comportamiento sale del `return` antes de `SetTop`, no de un test ejecutado.
