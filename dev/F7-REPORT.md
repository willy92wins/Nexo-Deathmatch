# F7 — KO corto (opción (a)): informe

## Qué se hizo y dónde

KO corto solo en servidor, en todo el servidor con el mod (haya ronda o no): al empezar la
inconsciencia se programa a 1000 ms una subida del shock al máximo; a partir de ahí despierta
vanilla con sus reglas (incluido su mínimo de ~2 s, no tocado).

| Cambio | Ruta:línea |
|---|---|
| Constante `KO_WAKE_DELAY_MS = 1000` (sin clave de config) | [NexoDM_Constants.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/3_Game/NexoDM/NexoDM_Constants.c:49) |
| Miembro `m_NexoDM_KoStartMs` + init a 0 | [NexoDM_PlayerBase.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:11), [:27](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:27) |
| `override OnUnconsciousStart`: `super` primero, solo `#ifdef SERVER` + `IsServer()`, guarda la hora, log `[NexoDM][KO] unconscious start t=… ms`, `Remove` previo + `CallLater(NexoDM_KoWake, KO_WAKE_DELAY_MS, false)` | [NexoDM_PlayerBase.c:122](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:122) |
| `NexoDM_KoWake`: sale sin hacer nada si no hay game, no es servidor, `!IsAlive()`, `IsSetForDeletion()` o `!IsUnconscious()`; si sigue inconsciente, `SetHealth("", "Shock", GetMaxHealth("", "Shock"))` y log `[NexoDM][KO] shock raised to max t=… ms start=… ms` con ambas horas | [NexoDM_PlayerBase.c:153](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:153), subida en [:182](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:182) |
| Cancelación del `CallLater` pendiente en el `EEDelete` ya sobrescrito por el mod (bloque `#ifdef SERVER` antes de `super.EEDelete(parent)`) | [NexoDM_PlayerBase.c:95](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:95) |

Comportamientos de borde: si el jugador muere inconsciente, el callback vence con `IsAlive()`
en falso y no hace nada (muere con normalidad; `EEKilled` no se ha tocado); si se borra antes
de 1 s, `EEDelete` cancela el pendiente; si ya despertó por vanilla antes del segundo, el
callback sale por `!IsUnconscious()` sin tocar el shock. La cámara del asesino
(`NexoDM_KillerCam.c`) no se ha modificado. Sin ternarios; nombres nuevos todos con prefijo
`NexoDM_`/`m_NexoDM_`; cada API externa lleva su cita `R2:` junto al uso.

Decisión de puerta de servidor: `#ifdef SERVER` más `IsServer()` en tiempo de ejecución, no
`NexoDM_IsDedicatedServer()`, porque el encargo pide «solo en servidor / todo el servidor con
el mod», sin limitar a dedicado.

## APIs vanilla verificadas (leídas en `P:\scripts\`, DayZ 1.30)

| API / hecho | Cita verificada |
|---|---|
| Activación por shock ≤ 25 | `4_world/classes/playermodifiers/modifiers/unconsciousness.c:16` (`UNCONSCIOUS_THRESHOLD`) |
| Desactivación con shock ≥ 50 y pulso `REGULAR` | `unconsciousness.c:46-53`; al desactivar, `SendPlayerUnconsciousness(player, false)` en `unconsciousness.c:31` |
| Umbrales 25 / 50 | `3_game/playerconstants.c:155-156` |
| Mínimo vanilla ~2 s (`m_UnconsciousTime > 2`, no tocado) | `4_world/entities/manbase/playerbase.c:3420` (bloque 3412-3434) |
| `PlayerBase.OnUnconsciousStart()` sobrescrito | `playerbase.c:3756` |
| `PlayerBase.EEDelete(EntityAI)` ya sobrescrito por el mod | `playerbase.c:1393` |
| Escritura vanilla de shock `SetHealth("", "Shock", …)` | `4_world/classes/playermodifiers/modifiers/diseases/heatstroke.c:383` |
| Máximo de shock con `GetMaxHealth("", "Shock")` | `4_world/classes/shockhandler.c:34-35` |
| `Object.SetHealth/GetMaxHealth/GetHealth(string,string,…)` | `3_game/entities/object.c:1015` / `:1008` / `:994` |
| `Object.IsAlive()` | `3_game/entities/object.c:517` |
| `EntityAI.IsSetForDeletion()` | `3_game/entities/entityai.c:812` |
| `Man.IsUnconscious()` (impl. `PlayerBase`) | `3_game/entities/man.c:63`; `playerbase.c:3925-3928` |
| `GetGame()` | `3_game/gameplay.c:636` |
| `CGame.IsServer()` | `3_game/global/game.c:1120` |
| `CGame.GetTime()` (ms de misión, usado como «hora» en el log) | `3_game/global/game.c:1535` |
| `CGame.GetCallQueue(int)` (categoría `GAMEPLAY` documentada en :1540-1542) | `3_game/global/game.c:1550` |
| `ScriptCallQueue.CallLater(func,int,bool,...)` / `Remove(func)` | `2_gamelib/tools.c:61` / `:65` |
| `Print(void)` | `1_core/proto/endebug.c:96` |

## LO QUE NO PUDE VERIFICAR

- **Compilación ni prueba in-game**: el encargo prohíbe shell/git, así que no hay RPT de
  arranque ni test en servidor. Primer compile-check pendiente: arranque del servidor con el
  mod y un KO real, buscando en el log las dos líneas `[NexoDM][KO]`.
- **Pulso**: vanilla solo despierta con pulso `REGULAR` (`unconsciousness.c:48`); subir el
  shock al máximo no fuerza la salida si el pulso es irregular — en ese caso manda vanilla.
- **`Remove` sin pendiente**: solo consta el prototipo (`tools.c:65`); que sea no-op seguro sin
  llamada encolada no está verificado en vanilla (el mod ya lo asume en otros sitios).
- **Precisión del `CallLater`**: el retardo nominal es 1000 ms; la hora real de despertar la
  fijan el tick de la cola `GAMEPLAY` y el mínimo vanilla de ~2 s, no medidos.
- Las citas `R2:` preexistentes del fichero (p. ej. `GetTime` en `game.c:1510`, `IsAlive` en
  `object.c:523`) difieren en pocas líneas de esta copia 1.30; no se han tocado (R25: nada
  más) y las citas nuevas usan las líneas verificadas aquí.

## Corrección 1

Solo los dos hallazgos de la ronda 1; nada más (R25/R20):

- **#1 (P1) — re-subida periódica con tope.** Nuevas constantes sin clave de config
  `KO_WAKE_REPEAT_MS = 500` y `KO_WAKE_MAX_MS = 10000` en
  [NexoDM_Constants.c:53](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/3_Game/NexoDM/NexoDM_Constants.c:53)
  y [NexoDM_Constants.c:54](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/3_Game/NexoDM/NexoDM_Constants.c:54).
  Tras la primera subida a 1000 ms, `NexoDM_KoWake` se reprograma a sí mismo cada 500 ms
  (`CallLater(NexoDM_KoWake, KO_WAKE_REPEAT_MS, false)`) en
  [NexoDM_PlayerBase.c:213](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:213),
  así que un segundo impacto que baje el shock entre 1 s y el despertar vanilla (>2 s) vuelve
  a quedar corregido en ≤500 ms y no devuelve un KO completo. Tope duro desde el inicio de
  la inconsciencia (`m_NexoDM_KoStartMs`): si `elapsed >= KO_WAKE_MAX_MS` el callback sale
  antes de subir el shock, y solo se reprograma si `elapsed + 500 < 10000`, en
  [NexoDM_PlayerBase.c:191](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:191)
  y [NexoDM_PlayerBase.c:213](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:213)
  (subidas nominales a 1000, 1500, …, 9500 ms; nunca bucle infinito). Cada tick vuelve a
  pasar todas las puertas antes de subir y de reprogramar, así que el bucle se para sin
  reprogramar cuando despierta (`!IsUnconscious()`), muere (`!IsAlive()`) o está marcado
  para borrado (`IsSetForDeletion()`); el borrado además sigue cancelando el pendiente en
  el `EEDelete` ya existente, sin cambios, en
  [NexoDM_PlayerBase.c:95](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:95).
  La subida sigue siendo `SetHealth("", "Shock", GetMaxHealth("", "Shock"))` en
  [NexoDM_PlayerBase.c:201](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:201),
  con el mismo log `[NexoDM][KO] shock raised to max …` por subida.
- **#2 (P2) — desconexión respetada.** Puerta `IsPlayerDisconnected()` (cita vanilla
  `P:\scripts\4_world\entities\manbase\playerbase.c:2638`, puesto a `true` por
  `PlayerBase.OnDisconnect()` en `playerbase.c:7665`, leída en esta copia) en los dos
  únicos puntos de actuación: al arrancar la inconsciencia, antes de guardar la hora,
  loguear o programar nada, en
  [NexoDM_PlayerBase.c:138](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:138),
  y en cada tick de `NexoDM_KoWake`, antes de subir el shock y sin reprogramar, en
  [NexoDM_PlayerBase.c:172](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:172).
  Si un cuerpo se desconecta mientras está KO, el siguiente tick (≤500 ms) sale por esta
  puerta sin tocar el shock: se respeta la muerte vanilla por combat-log y no se le
  resucita el shock a un cuerpo desconectado.

Sin cambios en ningún otro fichero ni comportamiento (R25); sin renombres ni limpieza de
lo preexistente (R20). Sigue sin poder verificarse compilación ni in-game en este encargo
(solo ficheros, sin shell ni git): pendiente el mismo compile-check de arranque buscando
las líneas `[NexoDM][KO]` repetidas cada ~500 ms hasta despertar o hasta el tope de 10 s.
