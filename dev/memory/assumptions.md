# Assumptions — LFGungame

## Open

| Date | Assumption | Impact If Wrong | Owner | Verification Needed | Status |
|---|---|---|---|---|---|
| 2026-05-27 | El daño del `Knife` se puede subir a one-hit fiable a cualquier zona | El cuchillo no mataría de un golpe → rompe E2/E3 | Claude/Codex | Prueba in-game tras tuning de daño melee | open |
| 2026-05-27 | Gate de permiso del comando = SteamID ∈ `Admin[]` del config (no el sistema de permisos LBmaster) | Admins no podrían arrancar, o cualquiera podría | Usuario/Codex | Confirmado en contrato; validar implementación | open |
| 2026-05-27 | `cmd` desnudo pero el contenido de `args` tras `!gungame ...` | Parsing de subcomando (p.ej. stop) incorrecto | Codex (fase 1) | verify in-game el split de args | open |
| 2026-05-29 | El whitelist de `WipeWeaponsOnly` (S4) asume que los GunPresets NO generan optic/suppressor sueltos como root o cargo independiente FUERA del árbol del arma | Un attachment huérfano del preset se preservaría → restos al cambiar de tier (rompe D2 parcial) | Codex/Claude | verify in-game con los presets reales del usuario; si aparece → cambiar a inventory-diff (camino B del plan P1-5) | open |
| 2026-05-29 | S5: `lastDamageWasKnife` es un flag por-entry seteado en cada `EEHitBy` y reseteado solo en respawn (P2-1) | Cuchillazo no-letal → víctima sobrevive → muere por otra causa SIN nuevo `EEHitBy` (sangrado de otra herida, caída) → flag stale → −1 indebido a la víctima | Codex/Claude | verify in-game (combate rápido lo hace improbable); mitigación: timestamp de recencia del cuchillazo, o derivar la causa letal del `damageType`/`dmgZone` en EEKilled en vez de memorizar | **P2-2 RESUELTO S5.5** (reset consume-once `RoundManager.c:662`); **P2-1 sigue OPEN, ahora INSTRUMENTADO** (`gap_ms=` en `KNIFE_DEATH_RECORDED`). Finding 2026-06-02 (R2): `EEKilled(Object killer)` NO trae `damageType` (vive en `EEHitBy`); y un window de recencia NO discrimina C1a (muerte por sangrado del cuchillo → −1 legítimo) de C1b (muerte tardía por otra causa → −1 indebido) porque ambas son diferidas sin `EEHitBy` terminal → P2-1 NO resoluble offline; fix real espera datos del ciclo 2 |
| 2026-05-29 | S5 asume que el `EEHitBy` del golpe letal llega ANTES que `EEKilled` (para que `lastDamageWasKnife` esté actualizado al clasificar la muerte) | Si el orden no se cumple, la clasificación cuchillo/no-cuchillo de la muerte sería incorrecta | Codex/Claude | verify in-game | open |

| 2026-05-29 | S5 asume que `EEKilled` llega despues del `EEHitBy` letal correspondiente, por lo que `lastDamageWasKnife` del entry clasifica la fuente letal | Una muerte ambiental despues de un golpe de cuchillo no letal podria aplicar un -1 incorrecto hasta el respawn | Codex/Claude | verify in-game en primer ciclo integrado: kill de cuchillo, kill de bala tras dano de cuchillo previo, muerte ambiental tras dano de cuchillo | open |
| 2026-06-10 | `OnClientRespawnEvent` param2 tras una muerte NORMAL = cadáver válido (no null) | Si es cadáver: GG-01 además equipa un arma de tier looteable en la arena (F4); si es null: los early-returns lo evitan y "solo" quedan rotos F1/F2/I4 | Claude | discriminador in-game: tras kill real, ¿hay arma suelta junto a un spawn de la arena? | open |
| 2026-06-10 | `ClientReadyEventTypeID` NO dispara además para el personaje nuevo post-respawn (New y Ready = rutas alternas) | GG-01 sobrevive a ambas hipótesis (OnPlayerReady no equipa); solo cambia la magnitud de GG-03 (fresh-connect invisible) | Claude | observar en RPT si un fresh-connect mid-VOTING recibe prompt (GGDBG lo loguea) | open |
| 2026-06-10 | `source` de `EEHitBy` en melee = el ITEM cuchillo (no el player) — `RoundManager.c:687-695` depende de ello | Si fuera el player, `isKnife` jamás sería true → TODO el sistema cuchillo (E2/E3/E4) inerte | Claude | paso 3.2 del test plan: cuchillazo real → ¿muerte one-hit + −1 a la víctima? | open |
| 2026-06-14 | El fix del respawn (diferir el equip fuera de `StartingEquipSetup`) recupera el birth-at-spawn GG-01v5 sin reintroducir el desync de apariencia | Si no: respawn sigue en vanilla-spawn+teleport, o vuelve el desync naked/arma-invisible que GG-01v5 arregló | Claude | test in-game: el respawneado nace en el punto GG, vestido/armado, sin lagazo; el observador lo ve igual | open |
| 2026-06-14 | `CallLater` con 3 params (PlayerBase/EntityAI/int) compila en este engine | Si no: compile-fail del módulo World → partir en helpers de 2 params | Claude/Codex | RPT de arranque del rebuild | open |
| 2026-06-14 | `"BandageDressing"` es la classname de venda correcta para el slot 3 (y aceptable duplicar la del preset) | Si no: `CreateInInventory` devuelve null → no hay venda en slot 3 | Usuario/Claude | in-game: ¿venda en tecla 3? ¿el usuario quiere la del preset en vez de una aparte? | open |
| 2026-06-14 | El `STR_` del join para el amigo = mod DESACTUALIZADO en su cliente (no el stringtable) | Si es el stringtable: hay que reordenar a canónico; los clientes inglés actualizados seguirían rotos | Usuario | que el amigo re-descargue limpio; ¿un cliente inglés ACTUALIZADO ve bien el join? | open |

## Resolved

| Date | Assumption | Resolution | Evidence | Closed By |
|---|---|---|---|---|
| 2026-05-27 | Firmas de comando y presets LBmaster | Verificadas contra source montado | ver [`verified-apis.md`](verified-apis.md) | Claude |
| 2026-05-27 | Posición del contador de tier (brief decía derecha; grill dijo izquierda) | **Abajo a la izquierda** | grill ronda 2/3 | Usuario |
| 2026-05-27 | Efecto del cuchillo (solo degrada vs doble) | **Doble: +1 asesino / −1 víctima** | grill ronda 3 (11a) | Usuario |
| 2026-05-27 | Dispatcher LBGroups usa prefijo `!` y `cmd` llega sin prefijo | **CONFIRMADO** (cliente envía con `!`; `OnChatCommand` compara `cmd` desnudo) | `missionGameplay.c:378-379`, `LBChatHandler.c:49-54` | research D |
| 2026-05-27 | `SpawnPreset`/`SpawnPresets` añaden vs limpian | **CONFIRMADO: solo AÑADEN** (cero borrado); el wipe del preset previo lo hace nuestro código | `LB_PresetBase.c:138-204` | research D |
| 2026-05-27 | `!gungamestop` se registra igual que `!gungame` | **CONFIRMADO** (cada comando = rama de string en los 3 overrides) | `LBChatHandler.c:49-54` | research D |
| 2026-05-27 | Cuchillos vanilla son `Weapon` | **FALSO: son `ToolBase`** → discriminar cuchillo por clase del config (`IsKindOf`), no por `IsMeleeWeapon` | `huntingknife.c:1`, research A | research A |
| 2026-05-27 | ¿Server carga Dabs + CF? | **CONFIRMADO por usuario**: usar Dabs MVC + CF como deps duras | confirmación usuario 2026-05-27 | Usuario |
| 2026-05-31 | Valores de las 5 constantes `RULE_*` (KILL/DEATH/KNIFE_DEATH/ASSIST_RECORDED, RESPAWN_EQUIPPED) reconstruidos por convención sin original (BUG-002) | **Sin dependencia del string exacto**: `LogRule()` usa `ruleId` solo como clave de rate-limit + etiqueta `Print()` al RPT; nunca se parsea/compara; sin consumidor externo (grep 0 en `LFGungame_dev`, sin scripts). Inocuo. La identidad literal vs original sigue sin confirmar (sin git/backup) pero es inmaterial | revisión 2026-05-31: `LFGungame_RoundManager.c:1681/519/1692-1707`, grep `_dev` | Claude |

(Open ⇡ vuelve al inicio del archivo)

## Carryovers de R21 ligera S0 (2026-05-27)

| Date | Item | Destino | Owner |
|---|---|---|---|
| 2026-05-27 | P2-1: borrar `_scaffold.c` residuales en `scripts\4_World\LFGungame\` y `scripts\5_Mission\LFGungame\` | Housekeeping al arrancar S1 | Codex (S1) |
| 2026-05-27 | P2-2: añadir fixture `invalid_maxplayers_out_of_range.json` en `_dev\tools\fixtures\configs\` | Housekeeping al arrancar S1 | Codex (S1) |
| 2026-05-27 | P2-3: `Validate()` no exige que `Knife` herede de `ToolBase` — classname existente pero no-tool pasa | Trabajo natural de discriminación del cuchillo en S5 | Codex (S5) |
| 2026-05-28 | R21 ligera S2+S2.5 — P2-1/P2-4: race fino reconnect↔voting (`secondsLeft=0` → `SetCountdown(0)` cierra dialog antes de pintar) | Defensa belt-and-suspenders ≤4 líneas cada lado: server omite BroadcastOpenDialog si `secondsLeft <= 1`; cliente `SetCountdown` clamp `>= 1` o ignora si `<= 0`. Housekeeping al arrancar S3 (mismos archivos tocados) | Codex (S3) — ✅ RESUELTO 2026-05-29 (server `BroadcastOpenDialog` skip `<=1` RoundManager.c:563; cliente `SetCountdown` `<=0`→Close + clamp `[1,60]` JoinMenu.c:114) |
| 2026-05-28 | R21 ligera S2+S2.5 — P2-3: claves stringtable huérfanas tras S2.5 (`STR_LFGUNGAME_JOIN_QUESTION/YES/NO`) | Delete + grep que confirme cero refs. Housekeeping al arrancar S3 | Codex (S3) — ✅ RESUELTO 2026-05-29 (3 claves borradas; 0 refs verificadas en `.c`/`.layout`) |
| 2026-05-27 | B4 — notificación "partida llena" diferida (no se envía chat al jugador bumpeado a público) | Layer issue: `LBChatHandler.SendSimpleChatMessage` vive en `LBmaster_Groups/scripts/5_Mission/...:65`, llamable solo desde 5_Mission; `LFGungame_RoundManager.OnPlayerVote` está en 4_World → compile-fail si se llama directo. Asignación del rol PUBLIC sí ocurre (B4 parcial). Codex deja TODO + log rate-limited en el call-site. **Patrón sugerido para resolverlo + I3 (anuncio de ganador, mismo problema)**: ScriptInvoker en el manager (4_World) `Event_OnNotifyPlayer(identity, stringKey)`; listener helper en 5_Mission que se suscribe y llama `LBChatHandler.SendSimpleChatMessage`. Aterriza en S6/S7 cuando exista más infra 5_Mission | Codex (S6/S7 — bridge ScriptInvoker) |
