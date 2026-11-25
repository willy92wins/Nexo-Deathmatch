# F5b — HUD visual (cliente)

Hecho en código. No compilado y no probado in-game (esta sesión no usa shell ni el juego). Solo se lee `NexoDM_HudData` (F5a); sin cambios de servidor ni de RPC. La cuenta atrás existente se queda.

## 1. Salud, sangre, agua y comida (% bajo el icono vanilla)

- El HUD vanilla no se toca. Grupo propio `VitalsGroup` en [nexodm_hud.layout](../NexoDM/gui/layouts/nexodm_hud.layout) que replica la geometría de `NotifiersPanel` (`P:\gui\layouts\day_z_hud.layout:754-766`: 260×81, `halign right_ref`, `valign bottom_ref`, exact 1) para caer sobre los iconos sin entrar en el panel que recorta (`clipchildren 1`).
- Desplazamientos desde la derecha copiados de vanilla: Salud 0 (`Health`, `day_z_hud.layout:768`), Sangre 43 (`Blood`, `:938-941`), Comida 139 (`Hungry`, `:1387-1390`), Agua 182 (`Thirsty`, `:1553-1556`). Cada `%` (`HealthPct`/`BloodPct`/`FoodPct`/`WaterPct`, 35×14 en y=59, justo bajo el icono de 35×35 centrado) lo pinta `UpdateVitals`/`SetPct` en [NexoDM_HUD.c](../NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c) desde `NexoDM_HudData.GetHealthPercent/GetBloodPercent/GetWaterPercent/GetFoodPercent`.
- Color del % por umbrales (mock): blanco 220,220,220, aviso 220,220,0 (<50), crítico 220,0,0 (<25).
- Tendencia: F5a no envía tendencia, así que la flecha (`HealthTrendUp/Down` y equivalentes, `set:dayz_gui image:iconArrowUp1/iconArrowDown1` como `HealthArrowUp1` en `day_z_hud.layout:849-868`) se deriva en cliente comparando cada muestra con la anterior (`SetTrend`); queda debajo del % (y=74). Las flechas vanilla dentro de `NotifiersPanel` siguen donde vanilla las pone: no se duplican ni se mueven (ver «no reproducido»).

## 2. Arma actual abajo a la izquierda

- `WeaponGroup` (0.05, 0.80) en [nexodm_hud.layout](../NexoDM/gui/layouts/nexodm_hud.layout), encima de postura/aguante vanilla. `UpdateWeapon` en [NexoDM_HUD.c](../NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c): `player.GetEntityInHands()` casteado a `Weapon_Base`; si no es arma, grupo oculto.
- Render real: `WeaponPreview` (`ItemPreviewWidgetClass`) con `SetItem(weapon)` solo al cambiar de objeto en manos (`P:\scripts\3_game\gameplay.c:286-300`). Nombre: `weapon.GetDisplayName()` (`P:\scripts\3_game\entities\object.c:476`).
- Munición como en el mock (actual / máximo + barra): cargador interno con `GetTotalCartridgeCount/GetTotalMaxCartridgeCount` (`P:\scripts\4_world\entities\core\inherited\weapon.c:127-132`); con cargador separable, `GetMagazine(muzzle)` (`weapon.c:240`) + `Magazine.GetAmmoCount()` (`P:\scripts\4_world\entities\itembase\magazine\magazine.c:68`) + 1 si hay bala en recámara (`IsChamberEmpty`, `weapon.c:74`), máximo desde `CfgMagazines <clase> count`. Sin cargador, solo recámara. Etiqueta localizada `#STR_NEXODM_HUD_MAGAZINE`.

## 3. Tiempo restante y bajas (arriba, sin mira)

- `ClockGroup` centrado arriba (de `hud-fable.html`, sin la mira `.cross`, que no se incluye): `ClockTime` en mm:ss desde `NexoDM_HudData.GetRoundSecondsLeft()`, barra `ClockTrack/ClockFillMe` (mis bajas / `KillsToWin`, rojo de selección) y marca `ClockMarkLead` (bajas del líder = fila 0 del top), texto `mis / objetivo bajas` con `#STR_NEXODM_HUD_KILLS_LABEL`. Estética vanilla (panel negro 55 %, ver punto 6), no la paleta Fable.
- Extra del mock Nexo: `SelfGroup` arriba a la izquierda con puesto (`GetRank`), puntos y bajas/muertes de `HUD_SELF`. Sin asistencias: F5a no las envía en `HUD_SELF` (ver «no reproducido»).

## 4. Killfeed

- `KillGroup` arriba a la derecha (y=0.05, sobre el Top 5) y pequeño (4 líneas, fuente 24): más arriba y más pequeño que en los mocks. Líneas `asesino  arma  víctima` desde `GetKillKiller/GetKillWeapon/GetKillVictim`, la más reciente arriba.
- Cada línea caduca a los 6 s de llegar (`m_KillTimes` en el HUD; `NexoDM_HudData` no guarda tiempos ni borra líneas, así que la caducidad es solo visual, por línea). Al cambiar de ronda, F5a ya vacía el feed (`SetRound`) y el HUD reinicia su contador al detectar que el total baja.

## 5. Top 5 con tecla mantenida

- Input nuevo `UANexoDMTop5` con `kT` por defecto en [inputs.xml](../NexoDM/inputs.xml) (reasignable en Controles). Anotado en el propio XML: LBmaster Groups usa kT para su ping táctico; conflicto conocido, no se cambia.
- Poll por frame en 5_Mission: `NexoDM_UpdateTop5Hold` en [NexoDM_MissionGameplay.c](../NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionGameplay.c) usa `UAInput.LocalHold()` (`P:\scripts\3_game\inputapi\uainput.c:62`) y pasa el estado a `NexoDM_HUD.SetTop5Visible`; el HUD (4_World) solo muestra `TopGroup` mientras dura la pulsación. El scoreboard con L no se toca.
- Filas fijas `TopRow0..4` (puesto, nombre, bajas, muertes, puntos de `HUD_TOP5`). Tu fila: fondo negro y texto rojo si tu nombre de identidad coincide con el de la fila (respaldo: tu `GetRank()` cuando no hay nombre local).

## 6. Estilo de cajas (menú de escape vanilla)

| Uso | Valor | Origen vanilla |
|---|---|---|
| Fondo de caja | negro alfa 0.549 (`color 0 0 0 0.549`) | `P:\gui\layouts\day_z_ingamemenu.layout:102` y `:317` (`bottom`/`top`, `style DayZDefaultPanel`) |
| Resaltado (fondo) | `ARGB(255,0,0,0)` negro opaco | `P:\scripts\5_mission\gui\ingamemenu.c:370` (`ColorHighlight`) |
| Resaltado (texto) | `ARGB(255,255,0,0)` rojo | `P:\scripts\5_mission\gui\ingamemenu.c:371` |
| Texto normal | blanco `1 1 1 1`; secundario alfa 0.7843 | `ingamemenu.c:380`; alfa 0.7843 en `day_z_ingamemenu.layout:64` |
| Fuente | `gui/fonts/sdf_MetronLight24` | `day_z_ingamemenu.layout:38` y `:55` (la 72 de `:154` es para botones grandes) |
| Barra de objetivo | fondo `0 0 0 0.5`, relleno `0.7843 0 0 1` | alfa de barra del mock (inventario); rojo 0.7843 en `day_z_ingamemenu.layout:292` (`separator_red`) |
| Iconos/flechas vitales | `set:dayz_gui image:iconArrowUp1/iconArrowDown1`, tinte 0.8627 | `day_z_hud.layout:788` (tinte) y `:863` (`iconArrowUp1`) |

Sin bordes, sin degradados ni esquinas redondeadas: paneles planos `ImageWidgetClass` con color, como los `PanelWidgetClass … style DayZDefaultPanel` vanilla (el tinte lo da el `color`, no una textura).

## 7. Visibilidad

- Gate único `CanShowF5b` en [NexoDM_HUD.c](../NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c), evaluado en cada refresco (0,25 s): cliente no dedicado, `NexoDM_HudData.IsRoundActive()`, jugador local con `m_GG_Role == NexoDM_Role.PARTICIPANT`, sin menú abierto (`UIManager.GetMenu()`, `P:\scripts\3_game\tools\uimanager.c:59` — cubre pausa e inventario, y también el scoreboard) y sin cámara del asesino activa (`NexoDM_KillerCam.IsActive()`). Si el gate cae, `HideF5bGroups` oculta los seis grupos; la cuenta atrás conserva su comportamiento (oculta el HUD vanilla durante el freeze, como antes).
- Los mocks no dicen nada de la cámara del asesino: se oculta igual porque el jugador local está muerto viendo a otro; al terminar, el gate vuelve solo.

## Qué del mock no se pudo reproducir y por qué

- **Flecha de tendencia vanilla duplicada bajo el %:** vanilla ya dibuja sus flechas dentro de `NotifiersPanel` (arriba/abajo del icono) y F5a no manda tendencia (-3..+3). Se dibuja una flecha propia bajo el % derivada del Δ% entre muestras; la vanilla no se mueve ni se oculta (no se toca el HUD vanilla, punto 1 del encargo).
- **Asistencias en el bloque de puntuación (mock Nexo):** `HUD_SELF` de F5a trae puntos/bajas/muertes/puesto, sin asistencias. El bloque muestra puesto, puntos y bajas/muertes.
- **Reserva de munición (mock: 24 / 120):** no hay conteo de munición suelta en F5a ni se inventa escaneando el inventario; se muestra balas disponibles / capacidad máxima del cargador (o total interno), que es lo que el arma expone en cliente.
- **Nombre de zona junto al reloj («Puerto de Cherno · a 30 bajas»):** F5a no envía nombre de zona; el texto bajo el reloj es solo `mis bajas / objetivo`.
- **Avisos +100/+50 bajo el reloj y «Tú» en el killfeed:** F5a no manda puntos por evento ni identidad del jugador local en el feed; el killfeed va en texto plano `asesino arma víctima`.
- **Silueta SVG del arma:** sustituida por el `ItemPreviewWidget` real, como pide el encargo (punto 2).

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce y carga del `.layout` (sin shell ni juego en esta sesión). En particular: que la clase de layout del preview sea `ItemPreviewWidgetClass`, que `Widget.SetPos` (`P:\scripts\1_core\proto\enwidgets.c:140`) posicione bien la marca del líder en unidades relativas, y que todos los `FindAnyWidget` resuelvan (los nombres se han cruzado a mano entre `.c` y `.layout`).
- Alineación en píxeles del `%` bajo cada icono vanilla en resoluciones/safezones distintas de 1080p (la geometría copia la de `NotifiersPanel`, sin prueba visual).
- Que `LocalHold()` de `UANexoDMTop5` conviva con el ping táctico de LBmaster en kT (conflicto anotado, no probado).
- El refresco a 0,25 s frente a vitales a 1 s: la tendencia por Δ% puede no pintar flecha si el % no cambia entre muestras.
- Queda un fichero temporal `NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c.new` (nota de borrado dentro; sin shell no se pudo eliminar). Hay que borrarlo a mano antes de empaquetar el PBO.

## Corrección 1

Corrección de la revisión F5 ronda 1, solo en [nexodm_hud.layout](../NexoDM/gui/layouts/nexodm_hud.layout), [NexoDM_HUD.c](../NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c) y [NexoDM_MissionGameplay.c](../NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionGameplay.c). Sin tocar datos ni RPC (la parte de datos la corrige otro implementador en paralelo). No compilado ni probado in-game (sin shell en esta sesión).

- **#1 Flechas vanilla:** las ocho flechas (`Health/Blood/Food/WaterTrendUp/Down`) usan `image0 "set:dayz_gui image:iconArrowUp1"`; subida con `"flip v" 1` y bajada con `"flip v" 0`, como `HealthArrowUp1`/`HealthArrowDown1` en `P:\gui\layouts\day_z_hud.layout:849-891` (`"flip v" 1` en `:867`, `"flip v" 0` en `:888`). Desaparece el inexistente `iconArrowDown1`.
- **#2 Alineación con los iconos vanilla:** nuevo `VitalsFrame` `size 0.9 0.9` centrado que replica el marco de `HudFrameWidget` (`P:\gui\layouts\day_z_hud.layout:1-8`); dentro, `VitalsGroup` copia `NotifiersPanel` (`P:\gui\layouts\day_z_hud.layout:754-766`: 260×81, `halign right_ref`, `valign bottom_ref`, exact 1, `fixaspect fixwidth`). Con `valign bottom_ref` el eje Y crece hacia arriba desde el borde inferior, así que el `%` pasa de `y=59` a `y=9` (35×14, justo bajo el icono de 35×35) y la flecha de `y=74` a `y=1` (22×7, bajo el `%`), centrada con el desplazamiento de 6 px de las flechas vanilla (22 px en 35 px). Los desplazamientos X (0/43/139/182) no cambian.
- **#3 Flecha persistente:** la tendencia se guarda por vital (`m_TrendHealth/Blood/Water/Food`) y solo cambia cuando llega una muestra distinta (`NextTrend` conserva la dirección si el valor no cambia); la muestra anterior (`m_Prev*`) solo se actualiza ante un cambio real. La flecha ya no se apaga en el siguiente tick de 0,25 s mientras el vital siga cambiando.
- **#5 Freeze inicial:** `CanShowF5b()` devuelve `false` con `m_CountdownActive`, igual que el HUD vanilla se oculta durante la cuenta atrás; `ClockGroup` ya no pisa `RoundTag` (`nexodm_hud.layout:10-21`) durante los 10 s.
- **#9 Marca del líder:** `UpdateSelfAndClock()` conserva la Y del layout (capturada con `GetPos` en `CacheWidgets`, 0,48 sobre `ClockTrack` en `nexodm_hud.layout:194-196`) al llamar `SetPos(x, m_ClockMarkY, true)`; ya no cae sobre los dígitos de `ClockTime`.
- **#10 Arma sin solapes:** `WeaponGroup` sube de `position 0.05 0.80` a `0.05 0.60` (borde inferior en 0,75), por encima de `StancePanel`/`PlayerPanel` vanilla (`P:\gui\layouts\day_z_hud.layout:3614-3622` y `:3818-3828`, en el borde inferior del marco 0,9).
- **#11 Preview liberado:** `m_WeaponPreview.SetItem(null)` al guardar el arma o perder el jugador en `UpdateWeapon()`, al ocultar el HUD en `HideF5bGroups()` y en `Destroy()`, que además anula los widgets y arrays F5b.
- **#12 Munición:** conteo por todos los cañones (`GetMuzzleCount()`, `P:\scripts\4_world\entities\core\inherited\weapon.c:13-16`) como en `P:\scripts\5_mission\gui\itemactionswidget.c:505-548`; una recámara solo cuenta si no está vacía ni disparada ni encasquillada (`IsChamberEmpty`/`IsChamberFiredOut`/`IsChamberJammed`, `P:\scripts\4_world\entities\core\inherited\weapon.c:74-84`). Cargador interno con `GetTotalCartridgeCount/GetTotalMaxCartridgeCount` (`weapon.c:124-132`), separable contado una sola vez más la recámara útil por cañón; revólveres y dobles cañones suman cada recámara.
- **#13 Top 5 sin retardo:** `NexoDM_UpdateTop5Hold()` usa `LocalValue() > 0` con `LocalPress()`/`LocalRelease()` (`P:\scripts\3_game\inputapi\uainput.c:58-62`; patrón vanilla en `P:\scripts\5_mission\mission\missiongameplay.c:359` y `:482`) en lugar de `LocalHold()`, y `SetTop5Visible()` muestra u oculta `TopGroup` y refresca las filas en el mismo frame, sin esperar al tick de 0,25 s.
- **#15 Muerte e inconsciencia:** `CanShowF5b()` también exige `player.IsAlive()` y `!player.IsUnconscious()` (vanilla oculta `NotifiersPanel` al caer inconsciente: `P:\scripts\5_mission\gui\ingamehud.c:362-365` y `P:\scripts\5_mission\gui\ingamehudvisibility.c:43`), sin cámara del asesino en espera (`NexoDM_KillerCam.IsSuppressingDeathScreen()`) y sin respawn en curso (`Mission.IsPlayerRespawning()`, `P:\scripts\3_game\gameplay.c:827` y `P:\scripts\5_mission\mission\missiongameplay.c:1702-1705`).
- **P2 #17:** `WeaponMagLabel` pasa a `position 0.52 0.64`, `size 0.45 0.16` (0,64–0,80), sin cruzar `WeaponMeterTrack/Fill` (0,82–0,90).
- **P2 #18:** `SelfRank` ya no trae el placeholder `"#2"` del layout (texto vacío hasta el primer `HUD_SELF`), y `ClockFillMe`/`ClockMarkLead` se ocultan con `Show(false)` cuando `KillsToWin == 0`.
- **P2 #20:** `HideF5bGroups()` reinicia `m_Prev*` a -1, las tendencias a 0, `m_WeaponShown` a null (con `SetItem(null)`) y `m_Top5Held` a false; sin flecha falsa al respawnear.
- **P2 #21:** tras `SetItem(weapon)` se llama `m_WeaponPreview.SetView(weapon.GetViewIndex())` como en `P:\scripts\5_mission\gui\inspectmenunew.c:70-71` y `P:\scripts\3_game\gui\inventorygrid.c:672-674`, y el layout añade `"force flip enable" 1` al preview (patrón en `P:\gui\layouts\inventory\inventorygriditem.layout:48`).
- **P2 #22:** el Top 5 guarda `ref UAIDWrapper` (`GetPersistentWrapper()`, `P:\scripts\3_game\inputapi\uainput.c:99`) y resuelve el `UAInput` por frame con `InputP()` (`uainput.c:29`), patrón de `P:\scripts\4_world\classes\useractionscomponent\actioninput.c:79`; un cambio de preset en Opciones ya no deja un handle obsoleto.

Sin cambios en `NexoDM/inputs.xml` ni `NexoDM/stringtable.csv`: ningún punto de esta corrección pedía bindings ni textos nuevos (`UANexoDMTop5` sigue en `kT`).
