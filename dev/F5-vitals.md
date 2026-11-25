# F5 · Valores de salud, sangre, agua y comida en el cliente (investigación, 2026-11-25)

Lectura de `P:\scripts` (DayZ 1.30). Las citas `ruta:línea` las sacó un subagente de lectura y no se han releído línea a
línea; quien implemente F5 debe verificarlas.

**Conclusión:** el cliente no tiene los 4 valores exactos. El HUD vanilla solo recibe un nivel de 0 a 4 y una tendencia
de -3 a +3 por `ERPCs.RPC_SYNC_DISPLAY_STATUS` (`4_world\classes\virtualhud\displaystatus.c:171-190`; empaquetado en
`displayelementtendency.c:3-10`). Hay que calcular en el servidor y enviar solo al dueño, como hace vanilla con
`TransferValues` (`4_world\classes\transfervalues.c:150`, `RPCSingleParam(..., true, player.GetIdentity())`).

| Valor | Servidor | Máximo | Nota |
|---|---|---|---|
| Salud | `GetHealth("","Health")` / `GetMaxHealth("","Health")` | 100 (`P:\DZ\characters\data\config.cpp:381-387`) | En el cliente, `GetHealth` no es fiable (`3_game\tools\component\componentenergymanager.c:1298`, comentario TODO). `TransferValues.GetHealth()` da salud/max con unos 5 s de retraso |
| Sangre | `GetHealth("","Blood")` / 5000 | 5000 (`config.cpp:417-420`) | `TransferValues.GetBlood()` NO sirve: es `InverseLerp(3000, max)` (`transfervalues.c:114-115`) |
| Agua | `GetStatWater().Get()` / `PlayerConstants.SL_WATER_MAX` | 5000 (`3_game\playerconstants.c:51`) | El stat no se sincroniza (flag `EMPTY`, `4_world\classes\playerstats\playerstatspco.c:341`) |
| Comida | `GetStatEnergy().Get()` / `PlayerConstants.SL_ENERGY_MAX` | 5000 (`playerconstants.c:45`) | Flag `EMPTY` (`playerstatspco.c:340`) |

**Envío recomendado:** RPC propio solo al dueño desde `PlayerBase.OnScheduledTick(float)` (`4_world\entities\manbase\playerbase.c:3007`).
Que corra solo en servidor (`g_Game.IsServer()`), porque `MissionGameplay.TickScheduler` también lo llama en el cliente
(`5_mission\mission\missiongameplay.c:225-230`). Cada 1 s y solo si cambió algún entero redondeado. Payload
`Param4<int,int,int,int>` en %, de 0 a 100. Alternativa: NetSync con `RegisterNetSyncVariableInt`
(`3_game\entities\entityai.c:2850`), que replica a toda la burbuja de red y no solo al dueño.

**Widgets del HUD vanilla** (`P:\gui\layouts\day_z_hud.layout`): `HudPanel` > `NotifiersPanel` (260×81, abajo a la
derecha, `clipchildren 1`, `:754-766`). Contiene los paneles de 35×81 `Health` (`:768`, icono `IconHealth`), `Blood`
(`:938`, `IconBlood`), `Hungry` (`:1387`, `IconHungry`) y `Thirsty` (`:1553`, `IconThirsty`), con las flechas
`<Panel>ArrowUp1..3` y `ArrowDown1..3`. Precedente de texto en un icono: `TemperatureValueTop`/`Bottom` (`:1179`, `:1193`).
En `5_mission\gui\ingamehud.c`: `m_Notifiers = FindAnyWidget("NotifiersPanel")` (`:169`) y mapeo de claves (`:245-288`).
No hay widgets llamados `Water`, `Food` ni `Energy`: son `Thirsty` y `Hungry`. Como `NotifiersPanel` recorta, el % se
coloca en un layout propio, alineado con `GetScreenPos`/`GetScreenSize` de cada icono, o hay que agrandar el panel.
