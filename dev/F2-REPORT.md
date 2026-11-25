# F2-REPORT — config de mapas y loadouts

El config pasa a tipos JSON reales, con `Maps` / `Loadouts`. Un config inválido hace que `StartGame` vuelva a IDLE. No hay migración del formato viejo.

## Ficheros

- `NexoDM/scripts/3_Game/NexoDM/NexoDM_Constants.c` — claves, reglas `E_CFG_*` / `W_CFG_MAP_OTHER_WORLD` y defaults (`CAMBIAR_*`, 1200, 30, 5, 15). Fuera `GunPresets`, `RespawnCoordinates`, `RespawnRadius` y los parseos string de bool/int.
- `NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c` — reescrito. Clases JSON `NexoDM_MapConfig` y `NexoDM_SpawnGroupConfig`. Copias de runtime `NexoDM_ParsedMap` / `NexoDM_ParsedSpawnGroup` (vectores). El cache `s_ValidMaps` es static para que `JsonFileLoader` no lo escriba en el JSON.
- `NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c` — `StartGame` resuelve presets; `PickRandomSpawn` usa el primer mapa válido; `EquipPlayer` usa `GetLoadouts().Get(0)`.
- `NexoDM/scripts/5_Mission/NexoDM/NexoDM_ChatHandler.c` — solo las citas `R2:` de `Load` y `GetAdmins`, que apuntaban a líneas del fichero viejo.
- `dev/tools/fixtures/configs/ok_config.json` y `invalid_E_CFG_{VERSION,ROUND,RESPAWN_DELAY,MAPS,SPAWN,PRESET}.json`
- `dev/tools/fixtures/configs/EXPECTED.md`

`CreateDefaults` copia el bloque `Maps` de `dev/F0-discovery.md` (§2): Kamyshovo, Vybor, Pavlovo_Militar, Grishino. `NexoDM_Config.c:628` (`AddDefaultMaps`).

## Reglas

Log: `[NexoDM][Config] <id> <clave>` vía `LogRule` (`NexoDM_Config.c:722`). `StartGame` rechaza si `Validate()` es false (`NexoDM_RoundManager.c:134`) o si el preset de LBmaster no resuelve (`NexoDM_RoundManager.c:143`).

1. `E_CFG_VERSION` — `version != 1`. `NexoDM_Config.c:157`. Clave `version`.
2. `E_CFG_ROUND` — `RoundDuration <= 0` o `KillsToWin <= 0`. `NexoDM_Config.c:163`. La clave que falla va en el detalle (`RoundDuration`, `KillsToWin`, o las dos).
3. `E_CFG_RESPAWN_DELAY` — `RespawnDelay` fuera de 0..30. `NexoDM_Config.c:178`. Clave `RespawnDelay`.
4. `E_CFG_MAPS` — ningún mapa del mundo cargado queda válido. `NexoDM_Config.c:420` y `:454`. Un `WorldName` distinto (sin distinguir mayúsculas, `WorldMatches` en `:465`, `GetGame().GetWorldName()`) se salta con `W_CFG_MAP_OTHER_WORLD` y el nombre del mapa (`:441`). El resto sigue.
5. `E_CFG_SPAWN` — mapa de este mundo sin grupos (`:485`, clave `SpawnGroups`), grupo sin puntos (`:504`, clave `Points`) o punto que `TryParseCoord` no acepta (`:522`, clave `Points`). Ese mapa tira el config entero.
6. `E_CFG_PRESET` — en 3_Game, `ClothingPreset` vacío o `Loadouts` vacío / entrada vacía (`:190`, `:370`). En 4_World, `LoadoutPresetsExist` (`NexoDM_RoundManager.c:1209`): si `LB_PresetLoader.Get` es null o `GetPreset` no devuelve el preset, la ronda no arranca y el log lleva el nombre.

Siguen `E_CFG_KNIFE_INVALID` (`:393`) y `E_CFG_MAX_PLAYERS_INVALID` si `MaxPlayers < 2` (`:184`). `Admin` no tenía rechazo en `Validate`; sigue igual (solo aviso de placeholder `CAMBIAR_STEAMID64`).

Getters nuevos: `GetLoadouts` `:264`, `GetKillsToWin` `:287`, `GetRespawnDelay` `:292`, `GetValidMaps` `:321`. Los dos int solo se leen aquí; los usa F3.

Consumo: mapa activo = primero de `GetValidMaps` (`NexoDM_RoundManager.c:1116`, comentario `F3: rotación de mapa`). Grupo al azar, punto al azar, ±`SpawnRadius` en X/Z, Y del punto (`:1162`). `m_SpawnRoundRobinIdx` guarda el índice plano del punto anterior (se pone a -1 al nacer el manager y al empezar la ronda). Arma = `GetLoadouts().Get(0)` (`:1547`, comentario `F4: loadout al azar en cada spawn`).

## Grep

Patrón: `GunPresets|RespawnCoordinates|GetRespawnCoords|GetRespawnRadius|GetGunPresets` en `NexoDM/scripts`.

Salida literal de la herramienta:

```
No matches found
```

## LO QUE NO PUDE VERIFICAR

- No hay shell. No corrí el validador offline de Enforce ni un parseo JSON. Los 7 fixtures están escritos a mano; no hay comprobación mecánica de que parsean.
- `LB_PresetLoader.Get.GetPreset` no se ejecutó. El config por defecto (`CAMBIAR_*`) debe impedir el arranque, pero no lo vi en un servidor.
- `invalid_E_CFG_MAPS.json` falla solo si el mundo cargado no es `sakhal`. `invalid_E_CFG_SPAWN.json` rompe `Points` en los dos mapas para que falle en `ChernarusPlus` y en `enoch`.
- `SpawnRadius` <= 0 no tiene regla en el encargo: no se rechaza. Un radio negativo llegaría a `Math.RandomFloat` tal cual.
- `GetValidMaps()` está vacío hasta que corre `Validate()`. `StartGame` valida antes de spawnear.
- No pude borrar los fixtures viejos (no hay herramienta de borrado). Siguen en `dev/tools/fixtures/configs/`: `invalid_gunpresets_empty.json`, `invalid_knife_missing.json`, `invalid_maxplayers_out_of_range.json`, `invalid_preset_missing.json`, `invalid_roundduration_zero.json`. No entran en el set nuevo. Hay que borrarlos a mano.

## Corrección 1

El validador (`ES-VARIABLE-NAMED-AS-TYPE`) rechazaba el miembro `World` en `NexoDM_Config.c:17` y `:43` porque `World` es una clase vanilla. La clave JSON pasa a `WorldName`.

- Miembros `NexoDM_MapConfig.WorldName` y `NexoDM_ParsedMap.WorldName`.
- Usos: `mapCfg.WorldName`, `parsed.WorldName = mapCfg.WorldName`, `mapCfg.WorldName = world` en `AddMap`. El parámetro local `world` y `GetWorldName()` / `WorldMatches` no cambian.
- Los 7 fixtures: `"World"` → `"WorldName"`. `EXPECTED.md` nombra la clave `WorldName`.
- Esta sección y la regla 4 de arriba.

Grep `\bWorld\b` en `NexoDM/scripts`: sin coincidencias (no queda un identificador `World` propio; `GetWorldName` y `WorldMatches` siguen). En los 7 fixtures, `"World"` ya no aparece.
