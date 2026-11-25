# F8B — HUD final (visual)

Hecho en código. Sustituye el HUD visual de F5/F6 siguiendo `dev/mocks/hud-nexo-mezcla.html` (v4). Solo se lee [NexoDM_HudData.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HudData.c): sin cambios de servidor, de RPC ni de lógica de puntos (R25). Sin shell ni git en esta sesión: no compilado Enforce ni probado in-game (ver «LO QUE NO PUDE VERIFICAR»).

Ficheros tocados: [nexodm_hud.layout](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/gui/layouts/nexodm_hud.layout), [NexoDM_HUD.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c), [NexoDM_IngameHud.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/5_Mission/NexoDM/NexoDM_IngameHud.c) (nuevo) y [stringtable.csv](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/stringtable.csv). [NexoDM_MissionGameplay.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionGameplay.c) no cambia: la T (`UANexoDMTop5`) y el `OnUpdate` por frame ya existían y son la entrada que usa el HUD nuevo.

## 1. Arriba a la izquierda: panel KDA (sin puntos)

- Nuevo `KdaGroup` en [nexodm_hud.layout:88](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/gui/layouts/nexodm_hud.layout:88): tres bloques (etiqueta + número) separados por dos divisores `KdaDiv0`/`KdaDiv1`. Fondo `0 0 0 0.35`, el del bloque `.score` del mock (`hud-nexo-mezcla.html:76`).
- Lo rellena `UpdateKdaAndClock` en [NexoDM_HUD.c:585](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:585) desde `GetKills()` / `GetDeaths()` / `GetAssists()` de `NexoDM_HudData` (este último en [NexoDM_HudData.c:329](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HudData.c:329)). Ya no se leen `GetPoints()` ni `GetRank()` para pintar.
- Borrados `SelfGroup`, `SelfRank`, `SelfPoints` y `SelfKD` del layout y del código. Etiquetas nuevas en [stringtable.csv](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/stringtable.csv): `STR_NEXODM_HUD_KILLS` (Bajas), `STR_NEXODM_HUD_DEATHS` (Muertes) y `STR_NEXODM_HUD_ASSISTS` (Asistencias), resueltas por script con `Widget.TranslateString` en `CacheWidgets` ([NexoDM_HUD.c:376](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:376)), porque el `#STR` cocido en el `.layout` no resuelve en el workspace del HUD.

## 2. Arriba a la derecha: killfeed con icono de arma

- `KillGroup` en [nexodm_hud.layout:282](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/gui/layouts/nexodm_hud.layout:282) con 4 filas `KillRow0..3`. Cada fila: `KillKillerN` (derecha), `KillPreviewN` (`ItemPreviewWidgetClass`), `KillFallbackN` (`ImageWidget`), `KillWeaponTextN` y `KillVictimN`. Ya no se pinta el classname como texto en el feed principal.
- Lo pinta `UpdateKillfeed` en [NexoDM_HUD.c:793](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:793): asesino y víctima de `GetKillKiller` / `GetKillVictim` y classname de `GetKillWeapon` ([NexoDM_HudData.c:568](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HudData.c:568)). Caducidad de 6 s por línea, como en F5.
- Icono según `dev/F8-weapon-icon.md`: `GetKillIcon` en [NexoDM_HUD.c:730](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:730) mantiene un `map<string, EntityAI>` con lista LRU (`m_KillIconOrder`, tope 16, dentro del rango 10-20 del documento) y lista de fallos (`m_KillIconFailed`) para no reintentar spawns imposibles cada tick. Solo crea si `IsKindOf(cls, "Weapon_Base")` y existe en `CfgWeapons`; crea con `CreateObjectEx(cls, "0 0 0", ECE_LOCAL | ECE_CREATEPHYSICS | ECE_TRACE)`, luego `dBodyDestroy`, `DisableSimulation(true)` y `SetAllowDamage(false)`. El preview hace `SetItem` + `SetModelOrientation("0 0 0")` + `SetView(GetViewIndex())`, solo cuando cambia la entidad mostrada.
- Respaldo: si es `Weapon_Base` pero no se pudo crear la entidad, silueta por categoría (`pistol` si `Pistol_Base`, si no `shoulderright`) en `set:dayz_inventory`; si ni siquiera es arma (p. ej. el cuchillo), el `displayName` buscado en `CfgWeapons`, `CfgVehicles` y `CfgMagazines` (`GetKillWeaponName`, [NexoDM_HUD.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c) tras `GetKillIcon`), con el classname como último recurso.
- Limpieza: `ClearKillPreviews` pone `SetItem(null)` en los 4 previews al ocultar el HUD; la expulsión LRU (`EvictKillIcons`, [NexoDM_HUD.c:699](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:699)) desvincula el preview antes de borrar la entidad expulsada, y `ClearKillIconCache` ([NexoDM_HUD.c:675](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:675)) borra todas las entidades con `ObjectDelete` en `Destroy()`.

## 3. Abajo a la izquierda: postura y aguante vanilla (sin panel propio)

- Borrados `WeaponGroup`, `WeaponPreview`, `WeaponName`, `WeaponAmmo`, `WeaponMagLabel` y `WeaponMeterTrack`/`Fill` del layout, y `UpdateWeapon` con toda su lógica de munición del código. No queda ningún `ItemPreviewWidget` del arma en manos.
- La postura y el aguante son los vanilla, que ya viven en esa esquina dentro de `LeftHUDPanel`: `StancePanel` en `P:\gui\layouts\day_z_hud.layout:3614` y `StaminaBar` en `P:\gui\layouts\day_z_hud.layout:3832` (verificados). No se han movido ni recreado: al quitar el panel propio, abajo a la izquierda solo queda eso.
- El arma se sigue viendo en la quickbar vanilla de abajo al centro (`QuickbarGrid`, `P:\gui\layouts\day_z_hud.layout:1888`; `GenerateQuickBarBackgroundTiles(10)` en `P:\scripts\5_mission\gui\inventory\InventoryQuickbar.c:37`), que no se toca.

## 4. Abajo a la derecha: solo sangre y salud vanilla (sin números)

- Borrados `VitalsFrame` y `VitalsGroup` enteros del layout (los 4 `%` y las 8 flechas propias) y `UpdateVitals`, `SetPct`, `NextTrend`, `ApplyTrend` y todos los miembros de vitales del código. Ya no se pinta ningún número sobre los iconos.
- Agua, comida y temperatura vanilla se ocultan en el nuevo [NexoDM_IngameHud.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/5_Mission/NexoDM/NexoDM_IngameHud.c): `modded class IngameHud` sobrescribe `Init` (vanilla en `P:\scripts\5_mission\gui\ingamehud.c:143`), llama a `super.Init` y oculta los paneles `Thirsty` (`P:\gui\layouts\day_z_hud.layout:1553`), `Hungry` (`:1387`) y `Temperature` (`:1144`). Vanilla solo vuelve a mostrar iconos y flechas dentro de cada panel (`DisplayNotifier`, `ingamehud.c:447`), nunca el panel, así que el panel oculto esconde su icono y sus flechas. `Health` (`day_z_hud.layout:768`) y `Blood` (`:938`) quedan intactos, con su tinte vanilla por estado.
- El fichero nuevo no necesita registro: `missionScriptModule` carga el directorio entero (`NexoDM/config.cpp:57-61`).

## 5. Al matar ya no aparecen puntos

- No hay ningún widget de «+100» ni «+50», ni texto de puntos en el HUD de ronda: fuera `SelfPoints` (punto 1) y fuera la columna de puntos del Top 5 (punto 6). En F5 esos avisos bajo el reloj ya no existían como widget; aquí se cierra la puerta quitando todo display de puntos en partida.
- La lógica no se toca: `POINTS_PER_KILL` / `POINTS_PER_ASSIST` y el sumado en servidor quedan como estaban (solo los lee el leaderboard). Los puntos solo se ven donde el encargo los pide: el top 10 global del punto 7.

## 6. Top 5 centrado con T mantenida

- `TopGroup` pasa a estar centrado en [nexodm_hud.layout:597](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/gui/layouts/nexodm_hud.layout:597) (560 px de ancho a 1080p, como el `.board` del mock), con cabecera `TopTitle` + `TopKillsHead` / `TopDeathsHead` / `TopAssistsHead` y 5 filas con columnas Bajas, Muertes y Asistencias (nombre entero en su columna). La columna de puntos se sustituye por `TopAssists0..4`.
- Lo rellena `UpdateTop5` en [NexoDM_HUD.c:928](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:928) con `GetTopKills` / `GetTopDeaths` / `GetTopAssists` ([NexoDM_HudData.c:388](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HudData.c:388)). Tu fila: fondo negro opaco y texto rojo; solo puede salir resaltada una fila presente en el Top 5 (coincidencia por nombre de identidad, con el respaldo del puesto de `HUD_SELF` si no hay nombre local), porque solo se recorren las 5 filas recibidas.
- La visibilidad sigue siendo solo mientras se mantiene `UANexoDMTop5`: `NexoDM_UpdateTop5Hold` en [NexoDM_MissionGameplay.c:150](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionGameplay.c:150) → `SetTop5Visible`. Sin cambios en la tecla ni en el poll.

## 7. Fin de ronda: clasificación (10 s) y después top 10 global (10 s)

- Nueva `RoundGroup` centrada en [nexodm_hud.layout:1124](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/gui/layouts/nexodm_hud.layout:1124), mismo aspecto que el panel de T: panel negro al 55 %, mismas columnas (Bajas, Muertes, Asistencias) y tu fila en negro con texto rojo. Cabecera con `RoundTitle` («Fin de partida · gana X», o «Fin de partida» si no hay ganador: `STR_NEXODM_HUD_END_TITLE` / `STR_NEXODM_HUD_END_NO_WINNER`) y `RoundSub` con mapa y objetivo («Mapa · 30 bajas», con `STR_NEXODM_HUD_KILLS_LABEL`).
- Lo rellena `FillRoundPanel` en [NexoDM_HUD.c:1102](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:1102) desde `HUD_ROUND_RESULT`: `GetRoundResultWinner` / `GetRoundResultMap` / `GetRoundResultKillsToWin` / `GetRoundResultName` / `Kills` / `Deaths` / `Assists` / `IsReceiver` ([NexoDM_HudData.c:452](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HudData.c:452) y siguientes).
- El orden lo lleva `UpdateEndPanels` en [NexoDM_HUD.c:1035](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:1035), llamado por frame desde `OnUpdate`: al subir `GetRoundResultSerial` se abre la ventana de 10 s de la clasificación; el serial global nuevo solo deja el panel en espera (`m_GlobalPending`) y su ventana de 10 s (`GLOBAL_TOP_SHOW_SEC`, `NexoDM_Constants.c:36`) empieza cuando la clasificación se apaga, así que los dos paneles nunca se solapan y cada uno dura 10 s enteros. Como en F6, ambos se ocultan con la cuenta atrás o con un menú abierto, y una ronda nueva activa cancela la ventana.
- El top 10 global es la pantalla de F6 reestilada, sin tocar su lógica de datos: `GlobalTopGroup` centrado en [nexodm_hud.layout:3569](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/gui/layouts/nexodm_hud.layout:3569), mismo panel y resaltado, con sus puntos (`GlobalPoints0..9`, cabecera `STR_NEXODM_HUD_POINTS`) y sus bajas. Lo rellena `FillGlobalPanel` en [NexoDM_HUD.c:1207](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:1207) desde `GetGlobalName` / `GetGlobalPoints` / `GetGlobalKills` / `GetGlobalIsReceiver`, igual que antes.

## 8. Estilo: menú de escape vanilla y colores del mock

- Cajas planas sin bordes, degradados ni esquinas redondeadas: `ImageWidgetClass` con `color 0 0 0 0.549` en `TopPanel`, `RoundPanel` y `GlobalPanel`, y fila propia con fondo `ARGB(255, 0, 0, 0)` y texto `ARGB(255, 255, 0, 0)` puesto por código en `UpdateTop5`, `FillRoundPanel` y `FillGlobalPanel`.
- El reloj de arriba al centro (`ClockGroup`) se queda como estaba en F5: no está en los puntos 1-8 y el encargo manda no tocar lo que no está en la lista. Tampoco se toca la cuenta atrás de inicio de ronda.

## De dónde sale cada color y cada fuente (ruta:línea vanilla)

| Uso en el HUD | Valor | Origen vanilla verificado |
|---|---|---|
| Fondo de panel (Top 5, ronda, global) | negro alfa 0.549 | `P:\gui\layouts\day_z_ingamemenu.layout:102` (`bottom`, `style DayZDefaultPanel`; también `:317`) |
| Fondo del panel KDA | negro alfa 0.35 | Del mock, `dev/mocks/hud-nexo-mezcla.html:76` (`.score`); no es un valor vanilla |
| Resaltado, fondo de tu fila | `ARGB(255, 0, 0, 0)` | `P:\scripts\5_mission\gui\ingamemenu.c:370` (`ColorHighlight`) |
| Resaltado, texto de tu fila | `ARGB(255, 255, 0, 0)` | `P:\scripts\5_mission\gui\ingamemenu.c:371` |
| Texto principal | blanco `1 1 1 1` | `P:\scripts\5_mission\gui\ingamemenu.c:380` (`ColorNormal`); el blanco de iconos de estado es 220,220,220 en `P:\scripts\5_mission\gui\ingamehud.c:276` |
| Texto secundario / puestos | blanco alfa 0.7843 (`1 1 1 0.7843`; en código `ARGB(200, 255, 255, 255)`) | `P:\gui\layouts\day_z_hud.layout:1912` (`Zeroing`, `color 1 1 1 0.7843`) |
| Divisores del KDA | `1 1 1 0.2941` | `P:\gui\layouts\day_z_hud.layout:1133` (`NotifierDivider`) |
| Fondo de la barra del reloj | negro alfa 0.5 | `P:\gui\layouts\day_z_hud.layout:3849` (`StaminaBackground`, `0 0 0 0.502`) |
| Relleno de la barra del reloj | rojo `0.7843 0 0 1` (= 200,0,0) | `P:\gui\layouts\day_z_ingamemenu.layout:292` (`separator_red`) |
| Aviso / crítico de vitales | 220,220,0 / 220,0,0 | `P:\scripts\5_mission\gui\ingamehud.c:499` y `:503`; ya no los usa código propio: los pinta vanilla en sus iconos |
| Fuente de todo el HUD | `gui/fonts/sdf_MetronLight24` | `P:\gui\layouts\day_z_ingamemenu.layout:38` y `:55` (la misma que usa el menú de escape) |

El mock cita además `colormanager.c:6` y `:10` para el negro al 55 % y el rojo de selección 200,0,0: no he localizado ese fichero en el árbol `P:\scripts` de esta máquina (probadas las rutas `3_game/gui`, `3_game/tools`, `3_game/global`, `3_game/systems` y `5_mission/gui`), así que esos dos valores se han tomado de las citas equivalentes verificadas de la tabla (menú de escape y `separator_red`), no del `colormanager.c` sin verificar.

## Cuántas filas de la clasificación caben a 1080p

26 filas. Con las alturas del mock: título 52 px + cabecera 36 px + fila 34 px. El marco útil vanilla es el 90 % de 1080 px = 972 px. 52 + 36 + 26 × 34 = 972 px exactos: la fila 27 ya no cabe. Por eso `RoundGroup` trae `RoundRow0..25` (`ROUND_ROWS = 26` en [NexoDM_HUD.c:80](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c:80)) y `FillRoundPanel` recorta `GetRoundResultCount()` a ese máximo sin inventar scroll ni paginado. Si la ronda tiene más de 26 participantes, se ven los 26 primeros y el resto no sale en esta pantalla (siguen en el scoreboard con L).

## Qué del mock no se pudo reproducir y por qué

- **Siluetas SVG del killfeed y de la quickbar del mock:** son dibujos del HTML. En el juego el icono del arma es el render 3D real con `ItemPreviewWidget` (punto 2), y en la quickbar el render lo hace vanilla. No hay SVG que copiar.
- **Icono de mira junto a las bajas del panel KDA:** el mock lo dibuja como SVG decorativo al lado del número de bajas. No he usado ninguna imagen `set:dayz_gui` sin verificar su nombre en el imageset, así que el bloque de bajas va sin ese adorno; el resto del panel es igual.
- **Resaltado «Tú» en negrita en el killfeed:** el mock marca tu nombre en blanco fuerte. El feed no distingue tu fila con otro peso de fuente porque el HUD usa una sola `sdf_MetronLight24` para todos los textos, como vanilla; sí se conserva la fila más reciente en blanco y las viejas atenuadas (alfa 0.7843 en el layout).
- **Las casillas aproximadas de la quickbar del mock:** el propio mock avisa de que son aproximadas. La quickbar real es la vanilla y no se toca (punto 3).
- **Anchos de texto exactos del mock:** el mock usa Fira Sans como aproximación a Metron y avisa de que los anchos no son exactos. En el juego manda la `sdf_MetronLight24` real, así que las columnas pueden variar unos píxeles respecto al HTML.

## LO QUE NO PUDE VERIFICAR

- No compilé Enforce ni arranqué cliente o servidor: esta sesión no tiene shell. En particular quedan sin probar: que `modded class IngameHud` compile y oculte los paneles sin que `IngameHudVisibility` los vuelva a mostrar, que `dBodyDestroy` y `DayZGame.IsKindOf(string, string)` resuelvan en 4_World como en la consola de scripts (`P:\scripts\5_mission\gui\scriptconsoleitemstab.c:692` y `:700`, solo DEVELOPER), y que `ConfigGetText` con ruta `displayName` devuelva el nombre traducido.
- No comprobé en un cliente real que `HUD_ROUND_RESULT` y `HUD_GLOBAL_TOP10` lleguen en ese orden ni que la secuencia de paneles (10 s + 10 s) se vea sin solapes; la lógica de espera está en `UpdateEndPanels`, pero el orden de red es el que describe `dev/F8A-REPORT.md`, no observado aquí.
- No medí el coste de render de hasta 4 `ItemPreviewWidget` del killfeed ni el tirón de la primera carga de cada modelo de arma (límite ya anotado en `dev/F8-weapon-icon.md`). La caché está topada a 16 entidades locales.
- No verifiqué que los nombres de imagen `pistol` y `shoulderright` existan tal cual en `dayz_inventory.imageset`: son los que cita la investigación de F8 como siluetas por categoría. Si un nombre no existiera, esa fila mostraría el hueco vacío en vez de la silueta (el nombre en texto solo sale para no-armas).
- No localicé `colormanager.c` en `P:\scripts` (ver la tabla de colores): los valores usados están corroborados en el menú de escape vanilla, pero la cita literal del mock queda sin verificar.
- La alineación en píxeles del panel KDA, el Top 5 centrado y la clasificación de 26 filas está calculada en unidades relativas desde las medidas del mock a 1920×1080; sin cliente real no hay prueba visual en otras resoluciones ni con safezone distinta.
- Los fondos de panel son `ImageWidgetClass` con solo `color`, el mismo patrón que ya usaban F5/F6 en este layout; su renderizado efectivo en esta versión del motor no está probado in-game en esta sesión.
