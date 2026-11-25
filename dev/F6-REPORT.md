# F6 — leaderboard global

## 1. Config `MinPlayersToScore`

Clave int, default 20.

- Campo: `NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:61`
- Default en `CreateDefaults`: `NexoDM_Config.c:129` (`NexoDM_Constants.c:34`)
- `< 1` → `Validate` false y `E_CFG_MIN_PLAYERS`: `NexoDM_Config.c:198-201`
- No se compara con `MaxPlayers` (`NexoDM_Config.c:203`). `MaxPlayers` ya no limita la ronda.
- Lectura en ronda: `GetMinPlayersToScore` `NexoDM_Config.c:319`
- `ok_config.json:8` = 20. `invalid_E_CFG_MIN_PLAYERS.json:8` = 0. El resto de fixtures inválidos también llevan 20, para no sumar esta regla a la suya. Tabla: `dev/tools/fixtures/configs/EXPECTED.md:16-18`
- `MaxPlayers` no se ha tocado como límite de participantes.

Un `config.json` ya desplegado sin la clave carga `0` y `Validate` lo rechaza. Hay que añadir la clave.

## 2. Puntuación al acabar la ronda

Dentro de `EndRound`, solo si la ronda llegó a ACTIVE, después del top 5 final y antes de quitar roles y de que `ReturnToIdle` borre las fichas: `NexoDM_RoundManager.c:244-256`.

- Fichas: todo `role == PARTICIPANT`, conectado o no. Orden con `IsBetterByTiebreak`: `CollectRoundFichas` `NexoDM_RoundManager.c:3033`
- Si el número de fichas es menor que `MinPlayersToScore`, no se llama a `AddRound` ni a `SaveSafe`. Log: `[NexoDM] no puntúa (N < Min)` con los dos números: `NexoDM_RoundManager.c:2931-2938`
- Si puntúa: puesto 0 suma +10 … puesto 9 suma +1; del 10 en adelante el bonus es 0. A eso se le suman las kills de la ronda. Esas kills también suben el contador global de kills y `rounds` sube 1. El nombre sale de la ficha (`playerName`, guardado al ver la identidad) y, si el jugador sigue conectado, del nombre vivo. Si `SaveSafe` falla, el log dice `leaderboard no guardado` (`NexoDM_RoundManager.c:2979`).

El top 5 de la ronda sigue usando solo participantes online (`CollectRankedParticipants`). No se ha cambiado.

## 3. Persistencia

Clase `NexoDM_Leaderboard` en `NexoDM/scripts/3_Game/NexoDM/NexoDM_Leaderboard.c`. Carga en el primer `Get()` (`EnsureLoaded`, `:148`). Rutas en `NexoDM_Constants.c:18-21`.

Orden global: puntos descendente, luego kills descendente (`IsBetterGlobal`, `:228`). El JSON se guarda ya en ese orden.

### Formato JSON

Documento `NexoDM_LeaderboardFile.entries`. Cada entrada: `steamId`, `name`, `points`, `kills`, `rounds`.

```json
{
  "entries": [
    { "steamId": "76561198000000000", "name": "Ana", "points": 37, "kills": 12, "rounds": 3 }
  ]
}
```

Ejemplo de una ronda que puntúa, Ana 1.ª con 4 kills: +10 de puesto y +4 de kills → `points` 14, `kills` 4, `rounds` 1.

### APIs de fichero

Leídas en esta sesión en `P:\scripts\1_core\proto\ensystem.c` y `P:\scripts\3_game\tools\jsonfileloader.c`.

| API | Cita | Uso |
|---|---|---|
| `JsonFileLoader<T>.LoadFile` | `scripts/3_game/tools/jsonfileloader.c:7` | Carga del principal y relectura del `.tmp` (`NexoDM_Leaderboard.c:107`) |
| `JsonFileLoader<T>.SaveFile` | `scripts/3_game/tools/jsonfileloader.c:42` | Escribe `leaderboard.json.tmp` (`NexoDM_Leaderboard.c:97`) |
| `FileExist` | `scripts/1_core/proto/ensystem.c:397` | Principal, `.bak` y `.tmp` |
| `DeleteFile` | `scripts/1_core/proto/ensystem.c:528` | Destino parcial y `.bak` solo para renovarlo tras un `.tmp` releído |
| `CopyFile` | `scripts/1_core/proto/ensystem.c:531` | Principal → `.bak`, luego `.tmp` → principal (`NexoDM_Leaderboard.c:88`) |
| `MakeDirectory` | `scripts/1_core/proto/ensystem.c:525` | Crea `$profile:NexoDM` si falta, en la primera carga |

No hay `RenameFile` en ese proto. El `.tmp` se relee; solo entonces se copia el principal a `.bak` y se sustituye. Si algo falla, el principal anterior no se borra (salvo que el copy del `.tmp` falle después de haberlo quitado: el destino parcial se borra y el `.bak` sigue). Al cargar, si el principal falta o es ilegible, se usa un `.tmp` legible y, si no, el `.bak`. `QuarantineBroken` (`NexoDM_Leaderboard.c:269`) no escribe el `.bak`. Entradas con `steamId` vacío, duplicado o números negativos se descartan con aviso.

## 4. Dónde se ve

Panel nuevo `GlobalTopGroup` en `NexoDM/gui/layouts/nexodm_hud.layout:772`, a la izquierda (el top 5 sigue a la derecha). Diez filas `GlobalRow0`…`GlobalRow9`, cada una con `GlobalRowBg`, `GlobalRank`, `GlobalName`, `GlobalPoints`, `GlobalKills`. Mismo panel negro translúcido, fuente y colores que `TopGroup`. Título `#STR_NEXODM_HUD_GLOBAL_TOP` en `NexoDM/stringtable.csv` (español: «Top 10 global»).

El HUD busca esos nombres en `NexoDM_HUD.c:433-445` y los pinta en `UpdateGlobalTop` (`:971`). La fila con `isReceiver != 0` lleva fondo negro y texto rojo, igual que el top 5. El grupo se muestra cuando llega un serial nuevo y se oculta al cumplirse 10 s (`GLOBAL_TOP_SHOW_SEC`, `NexoDM_Constants.c:35`). También se oculta con la cuenta atrás, con un menú abierto (pausa o inventario) y, si la ronda siguiente ya está activa, se cancela la ventana (`NexoDM_HUD.c:988`). `EEDelete` del cuerpo local llama a `ResetExceptGlobal` (`NexoDM_PlayerBase.c:100`) y no apaga el panel. La tecla del top 5 no se ha modificado.

Si el nombre guardado está vacío, el RPC manda el SteamID para que el cliente no tire la fila por nombre vacío.

### RPC `HUD_GLOBAL_TOP10`

`NexoDM_RPC.c:12` = `74621712`. Servidor → cliente. Un paquete por participante conectado al cerrar la ronda, puntúe o no (`BroadcastGlobalTop10`, `NexoDM_RoundManager.c:2968`).

| Campo | Tipo | Notas |
|---|---|---|
| filas | `int` | 0..10 |
| por fila: nombre | `string` | vacío → el cliente descarta el paquete entero |
| por fila: puntos | `int` | `< 0` descarta el paquete entero |
| por fila: kills | `int` | `< 0` descarta el paquete entero |
| por fila: es el receptor | `int` | 1 o 0. `< 0` descarta el paquete entero |

Handler: `NexoDM_PlayerBase.c:566`. Un `Read` que falla (truncado) sale sin `SetGlobal`: se queda el dato anterior. `ClearMatchStats` no borra este dato. `Reset` (fin de misión y desconexión) sí. `EEDelete` no.

## LO QUE NO PUDE VERIFICAR

- No compilé el mod ni arranqué el servidor. El encargo prohibía shell. No hay prueba in-game del panel, del RPC ni del fichero en `$profile`.
- No ejecuté `Validate()` contra los fixtures. La regla está escrita; el resultado de log no está observado.
- No comprobé si `CopyFile` pisa un destino existente: el código borra `.bak` y el vivo antes de copiar. Si `DeleteFile` del vivo falla después de haber copiado el `.bak`, el vivo queda y el `.tmp` también; la tabla en memoria ya está sumada.
- No medí el orden de los dos RPC (`HUD_ROUND` y `HUD_GLOBAL_TOP10`) en un cliente real. Van seguidos en el mismo `EndRound`, cada uno con `Send` al mismo identity.

## Corrección 1

Revisión `F6-REVIEW-R1.md`, solo los puntos pedidos.

- **#1 y #2.** `SaveSafe` escribe el `.tmp`, lo relee y solo entonces copia el principal a `.bak` y lo sustituye (`NexoDM_Leaderboard.c:88`). Un fallo deja el principal anterior, borra el destino parcial y lo dice el log. `EnsureLoaded` (`:162`), si el principal falta o no se lee, toma un `.tmp` legible y si no el `.bak`, y no pisa el `.bak`. `QuarantineBroken` (`:269`) solo aparta el ilegible a `.broken`.
- **#9.** `ApplyRoundToLeaderboard` registra `leaderboard no guardado` si `SaveSafe` devuelve false (`NexoDM_RoundManager.c:2979`).
- **#3.** `NexoDM_PlayerEntry.playerName` se rellena en enrolado, ready, respawn y reconexión (`RememberPlayerName`, `NexoDM_RoundManager.c:3193`). Al puntuar se usa esa ficha si el jugador ya no está (`:2968`).
- **#4.** `UpdateGlobalTop` no dibuja el panel con cuenta atrás o menú, y una ronda nueva activa anula los 10 s (`NexoDM_HUD.c:988`).
- **#5.** `EEDelete` llama a `ResetExceptGlobal` (`NexoDM_HudData.c:147`), que conserva filas y serial.
- **#10.** Al aceptar un fichero se descartan `steamId` vacío, duplicado y `points`/`kills`/`rounds` negativos, con línea en el log (`AcceptEntries`).

No se ha compilado ni probado en juego. `CopyFile` con destino ya existente sigue sin estar documentado en `ensystem.c:531`; por eso el `.bak` se borra antes de copiar el principal, y un principal parcial se borra antes de dar por fallido el copy.
