# F5b — HUD visual (cliente)

Hecho en código. No compilado y no probado in-game (esta sesión no usa shell ni el juego). Solo se lee `NexoDM_HudData` (F5a); sin cambios de servidor ni de RPC. La cuenta atrás existente se queda.

## 1. Salud, sangre, agua y comida (% bajo el icono vanilla)

- El HUD vanilla no se toca. Grupo propio `VitalsGroup` en [nexodm_hud.layout](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/gui/layouts/nexodm_hud.layout) que replica la geometría de `NotifiersPanel` (`P:\gui\layouts\day_z_hud.layout:754-766`: 260×81, `halign right_ref`, `valign bottom_ref`, exact 1) para caer sobre los iconos sin entrar en el panel que recorta (`clipchildren 1`).
- Desplazamientos desde la derecha copiados de vanilla: Salud 0 (`Health`, `day_z_hud.layout:768`), Sangre 43 (`Blood`, `:938-941`), Comida 139 (`Hungry`, `:1387-1390`), Agua 182 (`Thirsty`, `:1553-1556`). Cada `%` (`HealthPct`/`BloodPct`/`FoodPct`/`WaterPct`, 35×14 en y=59, justo bajo el icono de 35×35 centrado) lo pinta `UpdateVitals`/`SetPct` en [NexoDM_HUD.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c) desde `NexoDM_HudData.GetHealthPercent/GetBloodPercent/GetWaterPercent/GetFoodPercent`.
- Color del % por umbrales (mock): blanco 220,220,220, aviso 220,220,0 (<50), crítico 220,0,0 (<25).
- Tendencia: F5a no envía tendencia, así que la flecha (`HealthTrendUp/Down` y equivalentes, `set:dayz_gui image:iconArrowUp1/iconArrowDown1` como `HealthArrowUp1` en `day_z_hud.layout:849-868`) se deriva en cliente comparando cada muestra con la anterior (`SetTrend`); queda debajo del % (y=74). Las flechas vanilla dentro de `NotifiersPanel` siguen donde vanilla las pone: no se duplican ni se mueven (ver «no reproducido»).

## 2. Arma actual abajo a la izquierda

- `WeaponGroup` (0.05, 0.80) en [nexodm_hud.layout](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/gui/layouts/nexodm_hud.layout), encima de postura/aguante vanilla. `UpdateWeapon` en [NexoDM_HUD.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c): `player.GetEntityInHands()` casteado a `Weapon_Base`; si no es arma, grupo oculto.
- Render real: `WeaponPreview` (`ItemPreviewWidgetClass`) con `SetItem(weapon)` solo al cambiar de objeto en manos (`P:\scripts\3_game\gameplay.c:286-300`). Nombre: `weapon.GetDisplayName()` (`P:\scripts\3_game\entities\object.c:476`).
- Munición como en el mock (actual / máximo + barra): cargador interno con `GetTotalCartridgeCount/GetTotalMaxCartridgeCount` (`P:\scripts\4_world\entities\core\inherited\weapon.c:127-132`); con cargador separable, `GetMagazine(muzzle)` (`weapon.c:240`) + `Magazine.GetAmmoCount()` (`P:\scripts\4_world\entities\itembase\magazine\magazine.c:68`) + 1 si hay bala en recámara (`IsChamberEmpty`, `weapon.c:74`), máximo desde `CfgMagazines <clase> count`. Sin cargador, solo recámara. Etiqueta localizada `#STR_NEXODM_HUD_MAGAZINE`.

## 3. Tiempo restante y bajas (arriba, sin mira)

- `ClockGroup` centrado arriba (de `hud-fable.html`, sin la mira `.cross`, que no se incluye): `ClockTime` en mm:ss desde `NexoDM_HudData.GetRoundSecondsLeft()`, barra `ClockTrack/ClockFillMe` (mis bajas / `KillsToWin`, rojo de selección) y marca `ClockMarkLead` (bajas del líder = fila 0 del top), texto `mis / objetivo bajas` con `#STR_NEXODM_HUD_KILLS_LABEL`. Estética vanilla (panel negro 55 %, ver punto 6), no la paleta Fable.
- Extra del mock Nexo: `SelfGroup` arriba a la izquierda con puesto (`GetRank`), puntos y bajas/muertes de `HUD_SELF`. Sin asistencias: F5a no las envía en `HUD_SELF` (ver «no reproducido»).

## 4. Killfeed

- `KillGroup` arriba a la derecha (y=0.05, sobre el Top 5) y pequeño (4 líneas, fuente 24): más arriba y más pequeño que en los mocks. Líneas `asesino  arma  víctima` desde `GetKillKiller/GetKillWeapon/GetKillVictim`, la más reciente arriba.
- Cada línea caduca a los 6 s de llegar (`m_KillTimes` en el HUD; `NexoDM_HudData` no guarda tiempos ni borra líneas, así que la caducidad es solo visual, por línea). Al cambiar de ronda, F5a ya vacía el feed (`SetRound`) y el HUD reinicia su contador al detectar que el total baja.

## 5. Top 5 con tecla mantenida

- Input nuevo `UANexoDMTop5` con `kT` por defecto en [inputs.xml](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/inputs.xml) (reasignable en Controles). Anotado en el propio XML: LBmaster Groups usa kT para su ping táctico; conflicto conocido, no se cambia.
- Poll por frame en 5_Mission: `NexoDM_UpdateTop5Hold` en [NexoDM_MissionGameplay.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionGameplay.c) usa `UAInput.LocalHold()` (`P:\scripts\3_game\inputapi\uainput.c:62`) y pasa el estado a `NexoDM_HUD.SetTop5Visible`; el HUD (4_World) solo muestra `TopGroup` mientras dura la pulsación. El scoreboard con L no se toca.
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

- Gate único `CanShowF5b` en [NexoDM_HUD.c](/C:/Users/guill/Repos/Nexo-Deathmatch/NexoDM/scripts/4_World/NexoDM/NexoDM_HUD.c), evaluado en cada refresco (0,25 s): cliente no dedicado, `NexoDM_HudData.IsRoundActive()`, jugador local con `m_GG_Role == NexoDM_Role.PARTICIPANT`, sin menú abierto (`UIManager.GetMenu()`, `P:\scripts\3_game\tools\uimanager.c:59` — cubre pausa e inventario, y también el scoreboard) y sin cámara del asesino activa (`NexoDM_KillerCam.IsActive()`). Si el gate cae, `HideF5bGroups` oculta los seis grupos; la cuenta atrás conserva su comportamiento (oculta el HUD vanilla durante el freeze, como antes).
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
