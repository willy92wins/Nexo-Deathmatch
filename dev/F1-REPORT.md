# F1 — Informe de limpieza (NexoDM)

Hecho en ficheros. El validador offline de Enforce no se ha ejecutado (esta sesión no tiene shell).

## 1. Ficheros cambiados y qué se quitó

- `NexoDM/scripts/3_Game/NexoDM/NexoDM_Constants.c`
- `NexoDM/scripts/3_Game/NexoDM/NexoDM_RPC.c`
- `NexoDM/scripts/3_Game/NexoDM/NexoDM_MenuIDs.c`
- `NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c`
- `NexoDM/scripts/3_Game/NexoDM/NexoDM_Debug.c` (solo el comentario; la clase se queda, sin llamadas)
- `NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c`
- `NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c`
- `NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c`
- `NexoDM/scripts/4_World/NexoDM/NexoDM_Scoreboard.c`
- `NexoDM/scripts/4_World/NexoDM/NexoDM_JoinMenu.c` (sustituido por un comentario de una línea)
- `NexoDM/scripts/5_Mission/NexoDM/NexoDM_ChatHandler.c`
- `NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionGameplay.c`
- `NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionServer.c`
- `NexoDM/gui/layouts/nexodm_hud.layout`
- `NexoDM/gui/layouts/nexodm_scoreboard.layout`
- `NexoDM/gui/layouts/nexodm_scoreboard_row.layout`
- `NexoDM/gui/layouts/nexodm_join.layout` (vacío; la herramienta lo lee como fichero vacío)
- `NexoDM/stringtable.csv`
- `F1-REPORT.md`

### Punto 1 — Sistema de tiers

Quitado `tier` de `NexoDM_PlayerEntry`. Borrados `ChangeTier`, `ApplyTierLoadout`, `SetTierSync`, `GetMaxTierIndex`, `RecalculateLeaders`, `OnPlayerGoneRecalculate` y la victoria por índice máximo dentro de `OnPlayerKilled`. `IsBetterByTiebreak` queda kills > assists > deaths. NetSync: fuera `m_GG_Tier`, `m_GG_TierMax`, `m_GG_IsLeader` y el `OnVariablesSynchronized` que solo alimentaba el HUD. HUD: fuera `UpdateTier`, `ApplyTierPending` y los widgets de arma, pips y corona en `nexodm_hud.layout`. Scoreboard: fuera la columna (`ColWeapon`, `TierText`, `TierBar`, corona de fila) y los campos `tier`/`tierMax` del payload `SCOREBOARD_DATA` (ahora nombre, kills, assists, deaths). `EquipPlayer`, `ResolveParticipantBirthSpawn` y `EquipParticipantAtBirth` ya no llevan parámetro `tier`. El desempate de `OnRoundTimeout` se queda.

### Punto 2 — Cuchillo que bajaba de nivel

En `OnPlayerKilled` se eliminó el bloque que restaba nivel a la víctima y el `SetTierSync` asociado. Siguen `KnifeOneHit` (`SetHealth` a 0) y el crédito de la muerte (cuchillo con `lastKnifeKillerId`, y el fallback de atacante reciente).

### Punto 3 — `ggsim`

Fuera el comando en `NexoDM_ChatHandler.c` (`OnChatCommand` y `NexoDM_IsCommand`) y `DebugSim` en el RoundManager.

### Punto 4 — Rol de público

Fuera `NexoDM_Role.PUBLIC`, `PublicCoordinates`, `PublicRespawnRadius` (campos, defaults, `Validate`, getters y avisos de placeholder) y las constantes `DEFAULT_PUBLIC_*` / `KEY_PUBLIC_*`. Fuera `GetPublicIds`, `CountPublic`, `CleanupParticipantsToPublic`, `m_PendingPublicCleanup`, `EnsurePublicEntriesForConnectedPlayers` y la rama de público de `PickRandomSpawn` (ahora solo coords de participante). `EquipPlayer` ya no viste a un no participante con un camino distinto más allá de salir antes del arma.

Al terminar la ronda ya no hay teletransporte ni vaciado de inventario: solo se anuncia al ganador por tiempo (no en `admin_stop`) y se sincroniza el rol a `NONE` para que el scoreboard se cierre. No inventé un sustituto del traslado.

### Punto 5 — Voto / pantalla de unirse

Fuera el estado `VOTING`, `NexoDM_PromptState`, `OnPlayerVote`, `OnVotingClosed`, `CloseVotingEarly`, `StartVotingTimer`, `GetVotingSecondsLeft`, `PromptConnectedPlayers`, `HasPromptPending`, `BroadcastOpenDialog`, `BroadcastCloseDialog`, `CloseOrphanedDialogs`, el timer de voto y los RPC `VOTE`, `OPEN_DIALOG`, `CLOSE_DIALOG` (los que quedan conservan su id: `COUNTDOWN` 74621702, `SCOREBOARD_REQ` 74621704, `SCOREBOARD_DATA` 74621705). `COUNTDOWN` sigue en el HUD de arranque de ronda. Fuera `NexoDM_JoinMenu`, el id `JOIN`, la rama de `CreateScriptedMenu` y los textos `STR_NEXODM_JOIN_*`, `STR_NEXODM_YES`, `STR_NEXODM_NO`, `STR_NEXODM_ROLE_PUBLIC`, `STR_NEXODM_SCOREBOARD_TIER`. `nexodm_join.layout` quedó vacío.

`StartGame` (lo disparan `!gungame` y el autostart) inscribe a todos los conectados como participante (`EnrollConnectedParticipants`, admins incluidos) y llama a `BeginRound`. Quien entra con la ronda `ACTIVE` pasa a participante en `OnPlayerReady` / `OnPlayerReconnect`: si no tenía ficha, se le pone en un spawn de participante, se le vacía el inventario y se le equipa. La ronda acaba por `OnRoundTimeout` o por `gungamestop`.

## 2. Stubs `// F3`

Un solo comentario, no un método vacío: en `EquipPlayer`, justo antes de leer el arma, `// F3: Loadouts aleatorios` y `guns.Get(0)`. No hizo falta un stub de símbolo borrado: cada llamada se actualizó o se eliminó con su método.

## 3. Grep del gate

Herramienta de búsqueda del editor (no hay shell). Patrón insensible a mayúsculas.

`ggsim|tier|lfgungame` en `NexoDM/` (scripts, layouts, stringtable, config): **No matches found**.

`public` en `NexoDM/scripts`: **No matches found**. No queda ninguna API de DayZ que contenga esa subcadena; no hay excepciones que listar.

Nombres borrados (`ChangeTier`, `ApplyTierLoadout`, `SetTierSync`, `GetMaxTierIndex`, `RecalculateLeaders`, `UpdateTier`, `m_GG_Tier`, `DebugSim`, `OnPlayerVote`, `OPEN_DIALOG`, `JoinMenu`, `promptState`, `Role.PUBLIC`, `State.VOTING`, `GetPublic`, `CleanupParticipants`, `m_PendingPublic`, `m_GG_BirthTier`, …): **No matches found**, salvo `KnifeOneHit`, que el encargo manda conservar (config + `OnPlayerDamaged`).

## 4. LO QUE NO PUDE VERIFICAR

- Validador offline de Enforce (0 errores y tope de 8 avisos `ES-EMPTY-IFDEF-UNSUPPORTED-PATTERN`). No hay shell.
- Compilación del mod y una ronda in-game.
- Que un `config.json` ya escrito en `$profile:NexoDM` siga cargando: los campos de público ya no están en la clase; el loader suele ignorar claves de más, pero no lo he probado. Un config nuevo ya no las escribe.
- El fichero `nexodm_join.layout` lo veo vacío. No puedo afirmar 0 bytes en disco sin un `dir`.

## Notas (no inventadas como feature)

- Tras `EndRound` el jugador conserva el equipo de la ronda. Antes `CleanupParticipantsToPublic` lo movía y lo vaciaba. El rol sincronizado pasa a `NONE`.
- Quedan en el stringtable, sin la palabra tier y sin usos en script: `STR_NEXODM_GAME_FULL`, `STR_NEXODM_ROLE_PARTICIPANT`, `STR_NEXODM_RANK_FIRST`, `STR_NEXODM_HUD_WEAPON`. El encargo no pedía borrarlos.
- `NexoDM_Debug` se queda sin llamadas.
