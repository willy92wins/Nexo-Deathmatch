# F0: descubrimiento para NexoDM v1.0 (2026-11-25)

Fase de solo lectura: no cambia código del mod. Las citas `ruta:línea` las he comprobado hoy, salvo las marcadas
«(subagente)», que salen de una lectura delegada y no he releído línea a línea.

## 1. APIs

### 1.1 Mundo cargado

| API | Firma | Dónde | Nota |
|---|---|---|---|
| `CGame.GetWorldName` | `proto void GetWorldName(out string world_name);` y `string GetWorldName()` | `P:\scripts\3_Game\global\game.c:932-937` | Devuelve el nombre del mundo (`ChernarusPlus`, `enoch`, `deerisle`…). Con esto el servidor filtra `Maps[]` por `World` y descarta los que no casan con el mapa cargado. |

### 1.2 LBmaster (no se reverifica: ya usado y probado en LFGungame)

| Uso | Dónde lo usa LFGungame | Definición en LBmaster_Core 4.2.0 |
|---|---|---|
| `LB_PresetLoader.Get.GetPreset(name)` | `LFGungame/scripts/4_World/LFGungame/LFGungame_RoundManager.c:1147`, `:2146`, `:2177` | `LBmaster_Core\scripts\4_World\LBmaster_Core\Config\LB_PresetLoader.c:146` |
| `LB_PresetBase.SpawnPreset(player, player)` | `LFGungame_RoundManager.c:1169`, `:2150`, `:2180` | `LBmaster_Core\...\Struct\LB_PresetBase.c:138` |
| Fichero de presets | (lo carga LBmaster) | `$profile:LBmaster/Config/Common/ItemPresets.json` (`LB_PresetLoader.c:5`, raíz `$profile:LBmaster/` en `Config\Loader\LBConfigLoaderBase.c:71`) |
| «Arma al azar dentro del preset» | — | `individualChance = false` (0): se suman las `chance` de los hermanos con 0 y sale **uno** por sorteo ponderado (`LB_PresetBase.c:4`, sorteo en `:172-195`). Con `allowDuplicates=0` no repite en varias tiradas (`:9`). |

Consecuencia para la v1.0: cada entrada de `Loadouts` es un preset de LBmaster cuyos hijos son las armas candidatas con
`individualChance: 0` y su `chance` (peso). El mod no sortea nada: llama a `SpawnPreset` y LBmaster elige.
LBmaster es de pago y su licencia prohíbe modificarlo o redistribuirlo (`LBmaster_Core\License.txt:3`): se declara como dependencia y no entra en el repo.

### 1.3 Cámara del asesino en 3ª persona (subagente, scripts vanilla en `P:\scripts\`)

| API | Firma | Dónde | Nota |
|---|---|---|---|
| `PlayerBase.EEKilled` | `override void EEKilled(Object killer)` | `4_world\entities\manbase\playerbase.c:1214` | Servidor. Aquí se resuelve el asesino (si `killer` es un arma, su raíz/propietario) y se le manda su NetworkID al muerto. |
| Evento de muerte en cliente | `typedef Param2<DayZPlayer, Object> PlayerDeathEventParams;` | `3_game\gameplay.c:460-461` | Comentario literal: «Not necessarily actually the killer, Client doesn't have this info». El asesino hay que mandarlo por RPC. |
| `CGame.GetObjectByNetworkId` | `proto native Object GetObjectByNetworkId(int lo, int hi);` | `3_game\global\game.c:356` | El cliente resuelve al asesino. |
| Cámara local | `Camera.Cast(g_Game.CreateObject("staticcamera", pos, true))` + `SetActive(true)` | `5_mission\dayzintroscene.c:62,73`; `StaticCamera` en `P:\bin\config.cpp:2073-2078` | El tercer argumento `true` crea el objeto solo en local. |
| `Camera` | `SetActive(bool)` :45, `LookAt(vector)` :80, `SetFOV` :67, `GetCurrentCamera` :7 | `3_game\entities\camera.c` | `SetPosition` y `SetOrientation` vienen de Entity. |
| Pantalla de muerte | `ShowDeadScreen(bool, float)`, `SimulateDeath(bool)` | `4_world\entities\dayzplayerimplement.c:802`, `:844` | Hace un fundido a negro (`:820`) y llama a `LockControls(true)` (`:849`); hay que neutralizarlo mientras se espectea. |
| Respawn desde el cliente | `proto native void RespawnPlayer();` | `3_game\global\game.c:1107` | Hay un precedente de respawn automático a los N s con `CallLaterByName(g_Game,"RespawnPlayer",t)`, pero solo bajo `ROBOCLIENT` (`playerbase.c:4208`). Fuera de bots, sin verificar. |
| Flujo vanilla en servidor | `OnClientRespawnEvent` :611, `OnClientNewEvent` :549, `OnClientReadyEvent` :599 | `5_mission\mission\missionserver.c` | LFGungame ya engancha `OnClientNewEvent` para nacer en el spawn (GG-01v5/v6). |
| `CGame.SelectSpectator` / `SelectPlayer` | `game.c:379`, `:340` | | Alternativa B (patrón del BattleRoyale); ver abajo. |

**Recomendación: diseño A.** El cuerpo muerto sigue siendo el jugador de la identidad. En el cliente se crea una cámara
local que orbita al asesino (resuelto con `GetObjectByNetworkId` a partir de un RPC del servidor) durante
`RespawnDelay` segundos, y al acabar se borra la cámara y se respawnea por el flujo vanilla. No se toca la posesión, así
que no hay riesgo de «Player with same UID» (el bug que el BattleRoyale arregló en su PR #56 al quitar el cuerpo con
`SelectPlayer(identity, null)`). Si `RespawnDelay = 0`, no hay cámara y se respawnea al instante.

Diseño B (`SelectPlayer(identity,null)` + `SelectSpectator`) queda descartado para la v1.0. El BattleRoyale tuvo que
montar un cuerpo «ancla» inmune y gestionar la cola de logout para que no se quedara colgado el UID. En una arena de
~200 m no hace falta.

Riesgos de A que verifica la F3 in-game (ninguno se puede verificar offline):
- El asesino tiene que seguir en la burbuja de red del cadáver. En una arena de ~200 m debería estarlo; si
  `GetObjectByNetworkId` devuelve null, la cámara se queda en la última posición.
- Hay que quitar el fundido a negro y el `DeathDarkening` de vanilla.
- No se sabe si `RespawnPlayer()` llamado por script en un cliente normal (no bot) respawnea sin pasar por el menú.
- No se sabe si `LockControls` deja llegar el ratón para orbitar.
- `EEKilled(killer)` puede entregar el arma en vez del jugador.

Referencia de diseño, sin copiar código (DSPL-SA): `<repos>\BattleRoyale\...\BattleRoyaleSpectators.c` y
`BattleRoyaleSpectatorCamera.c`.

## 2. Mapas: zonas de ChernarusPlus vanilla

Fuentes, por punto:
- **Centro de cada zona:** `P:\DZ\worlds\chernarusplus\world\config.cpp`, `CfgWorlds` → `Names`, el `position[]` de la clase que se indica.
- **Puntos (x, y, z):** instancias de carretera, acera o grava del terreno vanilla.
  - Salen de `P:\DZ\worlds\chernarusplus\world\chernarusplus.wrp`, leídas con `wrp_object_parser.py --roads-only`: 83.317 instancias, sin puentes.
  - `y` es el origen del objeto de carretera, es decir, la altura de la calzada. Están a nivel de suelo y fuera del agua.
- **Fuera de edificios:** cada punto queda a ≥ 9 m del centro de cualquier edificio de `mpmissions\dayzOffline.chernarusplus\mapgrouppos.xml` (misión vanilla del DayZServer de Steam).
- **Reparto de cada zona:**
  - todos los puntos están entre 20 y 110 m del centro y a ≥ 10 m entre ellos;
  - se agrupan en grupos separados elegidos por el punto más lejano;
  - un grupo con menos de 4 puntos se descarta.
- **Script:** `dev/tools/cp_spawns.py`.

| Mapa | Centro (clase de config.cpp) | Por qué |
|---|---|---|
| Kamyshovo | `Settlement_Kamyshovo` 12170, 3458 | Pueblo de costa pequeño y compacto. El mar cierra el sur, así que no hace falta muro. |
| Vybor | `Settlement_Vybor` 3910, 9020 | Pueblo grande de interior en una meseta (y ≈ 310): mucha cobertura urbana en ~200 m. |
| Pavlovo_Militar | `Local_MB_Pavlovo` 2016, 3408 | Base militar: espacio abierto con cobertura dura. Cambia el ritmo frente a los pueblos. |
| Grishino | `Settlement_Grishino` 5980, 10257 | Pueblo de interior mediano, con calles en cruz y casas bajas. |

Avisos:
- [ASSUMED] La `y` es la del origen de la pieza de carretera. Hay que confirmarla in-game con `surface_query` o
  `GetGame().SurfaceY` en la F2. En todo caso, el BattleRoyale midió que `CreatePlayer` deja siempre el cuerpo de pie en
  el suelo (su HANDOFF:25, subagente), así que un error pequeño de `y` no debería enterrar ni tirar a nadie.
- Los puntos están en calles y caminos, no dentro de casas. Algunos de Pavlovo y de Vybor están en la periferia, a más de
  40 m de cualquier edificio: conviene revisar en el juego si quedan demasiado abiertos.
- Sin verificar: que no haya vallas ni restos estáticos encima de un punto. Las vallas no están en `mapgrouppos.xml`.

Bloque `Maps` con el esquema v1.0. Los grupos de 3 puntos se han quitado; `SpawnRadius` va en metros:

```json
"Maps": [
  {
    "Name": "Kamyshovo",
    "WorldName": "ChernarusPlus",
    "SpawnRadius": 3,
    "SpawnGroups": [
      { "Name": "Este",  "Points": ["12252.5,5.7,3513.9", "12239.5,5.4,3539.2", "12227.7,5.7,3510.7", "12247.7,5.4,3529.5", "12210.9,5.5,3545.2"] },
      { "Name": "Centro","Points": ["12198.4,5.5,3544.4", "12153.4,6.1,3501.2", "12178.2,6.1,3504.4", "12127.8,6.0,3548.5", "12156.1,5.8,3545.5"] }
    ]
  },
  {
    "Name": "Vybor",
    "WorldName": "ChernarusPlus",
    "SpawnRadius": 3,
    "SpawnGroups": [
      { "Name": "Sureste", "Points": ["3980.4,321.2,8938.5", "3968.8,320.8,8935.0", "3946.4,317.3,8943.9", "3935.6,315.8,8947.4", "3919.2,313.5,8960.0"] },
      { "Name": "Noroeste","Points": ["3829.8,313.4,9084.7", "3826.7,313.0,9067.5", "3854.1,311.9,9027.2", "3844.1,312.7,9049.2", "3819.2,312.9,9060.7"] },
      { "Name": "Suroeste","Points": ["3907.7,312.2,8965.3", "3855.0,311.1,8980.3", "3850.3,311.0,8932.0", "3852.7,311.1,8969.0", "3871.9,310.8,8974.5"] }
    ]
  },
  {
    "Name": "Pavlovo_Militar",
    "WorldName": "ChernarusPlus",
    "SpawnRadius": 3,
    "SpawnGroups": [
      { "Name": "Sur",  "Points": ["2004.8,107.4,3320.4", "1994.2,110.0,3333.1", "2014.9,104.7,3312.1", "1978.5,113.6,3352.5", "2030.7,99.8,3302.9"] },
      { "Name": "Este", "Points": ["2102.5,103.3,3446.8", "2109.8,99.6,3428.9", "2112.8,96.6,3416.2", "2092.5,107.5,3469.6", "2112.8,92.8,3390.7"] }
    ]
  },
  {
    "Name": "Grishino",
    "WorldName": "ChernarusPlus",
    "SpawnRadius": 3,
    "SpawnGroups": [
      { "Name": "Noreste", "Points": ["6068.6,271.9,10314.3", "6048.4,270.2,10318.6", "6036.3,269.2,10321.4", "6021.5,268.0,10326.4"] },
      { "Name": "Suroeste","Points": ["5907.7,270.6,10239.9", "5915.6,274.2,10209.9", "5926.5,276.2,10202.8", "5896.9,271.7,10224.5", "5905.6,272.6,10217.5"] },
      { "Name": "Norte",   "Points": ["5923.4,269.1,10259.4", "5982.7,266.9,10345.0", "5934.9,268.4,10274.2", "6000.4,267.0,10334.7", "5974.7,267.0,10335.3"] }
    ]
  }
]
```

## 3. ¿Se puede probar in-game el respawn GG-01v5/v6 en esta máquina? (subagente)

**No, de forma automática.** Hay tres bloqueos; con cualquiera de ellos basta para descartarla:

1. **DayZDiag no compila LBmaster_Core.** Falla con «Can't compile "Game" script module» por un down-cast inseguro en
   `lbwidgetutils.c(53)`. Pasó las dos veces que se intentó: los crash logs `LFGungame_dev\_server\profiles\crash_2026-06-03_01-56-15.log`
   y `crash_2026-06-11_20-10-12.log`. No se puede parchear porque la licencia lo prohíbe. La versión retail lo tolera, pero
   según la skill `dayz-test-ingame` es solo manual.
2. **LFGungame no está entre los proyectos aprobados del lanzador gestionado** (`dayz_test_run`), así que `dayz-test.ps1` aborta sin lease.
3. **La parte de servidor de LBmaster va con licencia por IP** y la IP local no está autorizada (`test-plan-ingame-ciclo1.md:7`).

La ruta que ya funcionó el 11 y el 13 de junio es el servidor dedicado real «PruebasLF», con Guillermo jugando:
1. Reempaquetar con `-packonly` y firmar. El PBO del 14-jun está desfasado: dos `.c` se editaron después.
2. Desplegar y reiniciar.
3. Guillermo hace `!gungame` (o `Autostart="1"`), pulsa SÍ y espera unos 70 s (votación más congelación).
4. Muere y pulsa Respawn. Se comprueba que aparece en `RespawnCoordinates` con el preset y la línea `RESPAWN_EQUIPPED` en el log.

Basta un cliente; el MCP no está en ese servidor. Tiempo estimado: unos 45-75 min, con Guillermo jugando unos 10-15.
Antes hace falta un `config.json` con `RespawnCoordinates` del mapa real (el de ejemplo, 18939,…, cae fuera de
Chernarus) y presets `GunPreset*`/`Preset1` en el `ItemPresets.json` de ese servidor.

**Consecuencia para la F1:** el criterio «el servidor arranca con el mod sin errores de script» no se puede cumplir en
esta máquina sin intervención, por el bloqueo 1. Para la F1 la evidencia es el build (exit 0) y el validador offline; el
arranque queda para el servidor real.
