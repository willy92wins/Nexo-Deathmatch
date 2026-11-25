# F3 — Informe (ciclo de ronda, rotación y cámara del asesino)

Sin shell ni ejecución (por encargo): verificación por lectura. Líneas = estado final tras la edición.

## 1. Arranque (`nexodm`/`nexodmstop`, autostart, ack de chat)
- Nombres en constantes: `NexoDM/scripts/3_Game/NexoDM/NexoDM_Constants.c:89-90` (`CHAT_CMD_START="nexodm"`, `CHAT_CMD_STOP="nexodmstop"`); el handler compara contra ellas en `NexoDM/scripts/5_Mission/NexoDM/NexoDM_ChatHandler.c:46,61,91-96` (también `NexoDM_IsCommand`). Sin literales `gungame`/`gungamestop` como comando (quedan solo menciones históricas en comentarios/`config.cpp`, fuera de alcance).
- Ack solo si ACTIVE: `NexoDM_ChatHandler.c:50-64` — tras `StartGame()` comprueba `manager.GetState() == NexoDM_State.ACTIVE`; si no, envía `MSG_ROUND_START_REJECTED` (`NexoDM_Constants.c:86`: config no válido, mira el log). Resto del arranque intacto.
- Rearme tras rechazo: `StartGame` ya no pisa `m_Config` si `Load()` falla (carga en local y solo asigna si hay objeto) y los tres rechazos (load/validate/preset) llaman a `ScheduleAutostartIfEnabled("retry after rejected start")`: `NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:133-159`.
- Rearme sin config cargada: `ScheduleAutostartIfEnabled` arma con el delay por defecto cuando `EnsureConfig()` falla, y `OnAutostartTimer` vuelve a comprobar `GetAutostart()` antes de arrancar (no puede iniciar una ronda con Autostart desactivado): `NexoDM_RoundManager.c:1833-1865` (aprox., bloque autostart) y `OnAutostartTimer` en el mismo fichero. El rearme al terminar la ronda ya existía en `ReturnToIdle` → `ScheduleAutostartIfEnabled("round ended")`; se mantiene y ahora también queda cubierto el camino de rechazo.

## 2. Respawn infinito
- Servidor: en cada baja de participante con ronda ACTIVE se envía `KILLER_CAM` (ver punto 3) con `RespawnDelay`; el cliente, al terminar la cámara (o al instante si es 0), dispara el respawn con el flujo vanilla del menú. El personaje nuevo nace en el spawn por el camino GG-01v5/v6 ya existente (`OnClientNewEvent`/`ResolveParticipantBirthSpawn`/`EquipParticipantAtBirth` en `NexoDM_MissionServer.c` + `OnPlayerRespawn`), que usa `PickRandomSpawn()`: grupo al azar del mapa ACTIVO y punto al azar ±`SpawnRadius`, sin repetir el punto anterior (`m_SpawnRoundRobinIdx`). Sin cambios en ese camino.
- Nota: en la baja que cierra la ronda por `KillsToWin` no se envía cámara/respawn (la ronda ha terminado); ver punto 4.

## 3. Cámara del asesino (cliente)
- RPC nuevo `KILLER_CAM = 74621706`: `NexoDM/scripts/3_Game/NexoDM/NexoDM_RPC.c:6`.
- Emisor (servidor): `SendKillerCam` en `NexoDM_RoundManager.c:2205` — escribe `Param3<int,int,int>` = (lo, hi, RespawnDelay) y `rpc.Send(victim, KILLER_CAM, true, victimIdentity)` (`:2232`). Se llama desde `OnPlayerKilled` (`:625`) solo si la víctima es participante y la ronda está ACTIVE (guardas previas de `OnPlayerKilled`). El asesino enviado es el acreditado por la puntuación (incluye los fallbacks cuchillo/Option A ya existentes); sin asesino acreditado o suicidio → `0,0` (en `SendKillerCam`: solo se lee el NetworkID si `killerPlayer && killerPlayer != victim`). El jugador raíz de un arma en manos ya lo resuelve `ResolveObjectPlayer` vía `GetHierarchyRootPlayer` en el camino de `OnPlayerKilled`.
- Receptor (cliente): `NexoDM_HandleKillerCamRPC` en `NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:168` — lee el mismo `Param3<int,int,int>` en el mismo orden; payload incompleto o delay negativo se descarta entero (fail-closed, sin cámara ni respawn). El servidor no acepta ningún RPC nuevo del cliente: `KILLER_CAM` solo se maneja en la rama de cliente (`#ifndef SERVER`).
- Cámara: clase nueva `NexoDM_KillerCam` (`NexoDM/scripts/4_World/NexoDM/NexoDM_KillerCam.c:1`). `Start` (`:37` aprox.): delay 0 → respawn inmediato sin cámara; si no, crea `staticcamera` local, `SetFOV`/`LookAt`/`SetActive(true)`, y limpia el fundido con `ShowDeadScreen(false, 0)`. `OnUpdate` por frame desde `NexoDM_MissionGameplay.OnUpdate`: re-resuelve al asesino con `GetObjectByNetworkId` en cada frame; cámara en 3ª persona detrás/encima (`GetDirection` del objetivo, −4,5 m, +2 m, mirando a +1,6 m). Asesino `0,0`, no resuelto o desaparecido → mismo encuadre sobre el cadáver (`m_CorpsePos`). Al acabar: `SetActive(false)` + `ObjectDelete` y respawn. Si la creación de la cámara falla, borra el objeto y respawnea (nunca deja al cliente sin cámara activa). Sin `SelectPlayer`/`SelectSpectator` en ningún sitio.
- Respawn: `RespawnNow` reproduce `GameRespawn` de `ingamemenu.c:345-363`: `GetMenuDefaultCharacterData(false).SetRandomCharacterForced(false)`, `RespawnPlayer()`, `SimulateDeath(true)`, `ShowDeadScreen(true, 0)` en cola GUI, `DestroyAllMenus()`, `SetPlayerRespawning(true)`, `Continue()`. `OnMissionFinish` aborta la cámara sin respawnear (`Abort`).

## 4. Fin de ronda (RoundDuration / KillsToWin)
- `RoundDuration`: sin cambios (timer ya existente en `BeginRound` → `OnRoundTimeout`).
- `KillsToWin`: en `OnPlayerKilled`, tras sumar la baja, si `killerEntry.kills >= m_Config.GetKillsToWin()` se marca `roundEndsByKills` y al final del método se llama a `EndRound("kills_to_win", winningKillerId)`: `NexoDM_RoundManager.c:616-618` y `:651-656`. Gana quien lo alcanza primero porque la ronda se cierra en esa baja.

## 5. Puntos (100/50, ranking, payload y columna)
- Constantes `POINTS_PER_KILL=100` / `POINTS_PER_ASSIST=50`: `NexoDM_Constants.c:93-94`.
- Campo `points` + `lastPointsChangeMs` en `NexoDM_PlayerEntry`: `NexoDM_RoundManager.c:8-9` (init `:26-27`). Suman en la baja (`:609`) y en cada asistencia dentro de `AwardAssists` (`:841`) vía `AddPoints` (`:2191`), que guarda el instante (`GetTime()`) del último cambio.
- Ranking: `IsBetterByTiebreak` ahora es puntos desc → menos muertes → cambio de puntos más temprano (`0` = nunca puntuó, pierde): `NexoDM_RoundManager.c` (tras `AddPoints`/`SendKillerCam`, aprox. `:2240`). Lo usan el ganador por timeout y el orden del scoreboard (mismo comparador, sin cambios de llamada).
- Payload `SCOREBOARD_DATA`: emisor añade `points` como quinto campo por fila (`NexoDM_RoundManager.c:1245` aprox.) y receptor lo lee en el mismo orden y lo guarda en `NexoDM_ScoreRow.points` (`NexoDM_PlayerBase.c:140-155`, `NexoDM_Scoreboard.c:11`).
- Columna: `PText` por fila y cabecera `ColP "PTS"`; K/A/D desplazadas a 0,70/0,78/0,86 y PTS a 0,93 en `NexoDM/gui/layouts/nexodm_scoreboard.layout` y `nexodm_scoreboard_row.layout`. `SetData` escribe `PText` en `NexoDM_Scoreboard.c` (`pName`/`pStr`).

## 6. Rotación de mapa (`PickNextMapIndex`)
- Método puro `int PickNextMapIndex(int count, int current, int previous)`: `NexoDM_RoundManager.c:1275`. Solo depende de sus argumentos y `Math.RandomInt`: count≥3 → al azar distinto de current y previous (bucle con guarda de 64); count=2 → el otro; count≤1 → 0; current fuera de rango (primera ronda) → al azar entre todos.
- Estado: `m_ActiveMapIndex`/`m_PreviousMapIndex` (init 0/−1). En `EndRound` (solo si la ronda llegó a ACTIVE) se elige el siguiente: `NexoDM_RoundManager.c:216-224`. `PickRandomSpawn` usa el mapa activo vía `GetActiveMap()` (`:1288` aprox.), así que `BeginRound` (teletransporte + equipo, sin cambios) ya cae en el mapa activo.

## 7. Limpieza entre rondas y RV-1
- Inventario: verificado por lectura que todos los caminos vacían antes de equipar — `BeginRound` (`WipeInventory` → `EquipPlayer`), nacimiento GG-01 (`EquipParticipantAtBirth` → `WipeInventory`), fallback de `OnPlayerRespawn`, entrada tardía `OnPlayerReady` y reconexión `OnPlayerReconnect` (ambos `WipeInventory` → `EquipPlayer`). Sin cambios necesarios.
- Suelo del mapa anterior: `CleanupPreviousMapGroundWeapons()` en `NexoDM_RoundManager.c:663`, llamada al inicio de `BeginRound` (`:370`). Zona = caja de los puntos del mapa anterior (`m_PreviousMapIndex`) + 50 m, consultada como centro + radio (semidiagonal + 50) con la API vanilla de abajo; borra solo armas/munición/cargadores sin jugador raíz (`ShouldDeleteWeaponLoadoutItem`, el cuchillo se conserva). No se inventó ninguna API.

## 8. Dos P2 (freeze tardío, reconexión ENDED→NONE)
- Freeze tardío: flag `m_FreezeActive` (puesto en `BeginRound`, limpiado en `EndFreeze`/`EndRound`) y helper `ApplyFreezeToPlayer` (`NexoDM_RoundManager.c:1963`: `DisableSimulation(true)` + `SetAllowDamage(false)`). Se aplica en `OnPlayerReady` (`:1087`), `OnPlayerRespawn` (`:913`) y `OnPlayerReconnect` (`:1128`, `:1146`). `EndFreeze` → `SetFreezeForEntries(false)` ya desfrizaba a todos los participantes registrados, incluidos los tardíos.
- Reconexión en ventana ENDED: en `OnPlayerReconnect`, si el estado es `ENDED` (o `IDLE`) con entrada aún presente, se fuerza `entry.role = NONE` + `ApplyRoleToPlayer(NONE)` y se sale: `NexoDM_RoundManager.c:1113-1120` aprox.

## APIs vanilla usadas (ruta:línea verificada)
- `PlayerBase.EEKilled(Object)` (contexto, sin tocar firma): `P:\scripts\4_world\entities\manbase\playerbase.c:1214`.
- El cliente no conoce al asesino: `P:\scripts\3_game\gameplay.c:460-461`.
- `Object.GetNetworkID(out int, out int)`: `P:\scripts\3_game\entities\object.c:819`.
- `CGame.GetObjectByNetworkId(int, int)`: `P:\scripts\3_game\global\game.c:356`.
- `CGame.CreateObject(string, vector, bool)`: `P:\scripts\3_game\global\game.c:691`; patrón `staticcamera` + `SetActive(true)`: `P:\scripts\5_mission\dayzintroscene.c:62,73`.
- `Camera.SetActive(bool)`: `P:\scripts\3_game\entities\camera.c:45`; `SetFOV(float)`: `:67`; `LookAt(vector)`: `:80`.
- `Object.SetPosition(vector)`: `P:\scripts\3_game\entities\object.c:294`; `GetPosition()`: `:287`; `GetDirection()`: `:314`; `SetOrientation(vector)`: `:311` (no usada al final; se usa `LookAt`).
- `CGame.ObjectDelete(Object)`: `P:\scripts\3_game\global\game.c:705`.
- `DayZPlayerImplement.ShowDeadScreen(bool, float)`: `P:\scripts\4_world\entities\dayzplayerimplement.c:802` (fundido a negro `:820`, rama de salida `:825`); `SimulateDeath(bool)`: `:844` (`LockControls` `:883` aprox.).
- Flujo de respawn del menú `GameRespawn`: `P:\scripts\5_mission\gui\ingamemenu.c:345-363`; `CGame.RespawnPlayer()`: `P:\scripts\3_game\global\game.c:1107`; `DayZGame.GetMenuDefaultCharacterData(bool)`: `game.c:1593`; `MenuDefaultCharacterData.SetRandomCharacterForced(bool)`: `P:\scripts\3_game\gameplay.c:1115`; `MissionGameplay.DestroyAllMenus()`: `P:\scripts\5_mission\mission\missiongameplay.c:1158`; `SetPlayerRespawning(bool)`: `:1697`; `Continue()`: `:1334`.
- `CGame.GetObjectsAtPosition3D(vector, float, out array<Object>, out array<CargoBase>)`: `P:\scripts\3_game\global\game.c:930` (ya usada por `CleanupDeathSite`).
- `Math.RandomInt(int, int)` (max exclusivo): `P:\scripts\1_core\proto\enmath.c:38`; `Math.Sqrt(float)`: `:305`.

## LO QUE NO PUDE VERIFICAR
- Todo lo que exige ejecutar: validador offline de Enforce, build del PBO y comportamiento in-game (gate del usuario). En particular: que `staticcamera` local tome el control al activarse sobre un jugador muerto y lo devuelva al respawnear, que `ShowDeadScreen(false, 0)` gane al fundido de la muerte en todos los casos, y que el respawn automático no choque con el botón vanilla de respawn si el jugador pulsa antes.
- Si `GetObjectsAtPosition3D` con radios grandes (mapas extensos + 50 m) devuelve también objetos dentro de edificios/contenedores proxy: solo se borran `ItemBase` sin jugador raíz que sean arma/cargador/munición, pero no se pudo probar el coste ni el contenido exacto de `proxyCargos` (se ignora, como en `CleanupDeathSite`).
- Empate total en el ranking (mismos puntos, muertes e instante): el comparador devuelve `false` (gana el primero encontrado); el encargo no define un cuarto criterio.
- `PickNextMapIndex` no se probó con un harness aparte (sin shell); su pureza se sostiene por lectura: solo lee sus argumentos y llama a `Math.RandomInt`.
- La baja que cierra la ronda no envía `KILLER_CAM` (decisión documentada en el punto 2/4): la víctima usa la pantalla de muerte vanilla en esa última baja.
