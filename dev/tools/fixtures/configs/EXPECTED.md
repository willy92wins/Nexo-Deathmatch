# Fixtures de config F2

`ok_config.json` pasa `NexoDM_Config.Validate()` en 3_Game cuando el mundo cargado es `ChernarusPlus` o `enoch`. El otro mapa solo avisa `W_CFG_MAP_OTHER_WORLD`. La existencia del preset en LBmaster no se puede ver en el JSON: la rechaza `StartGame` en 4_World.

En un mundo que no es el del mapa, `Validate` también escribe el aviso `W_CFG_MAP_OTHER_WORLD` más el nombre del mapa. Eso no es un fallo de regla. `LogRule` deduplica por id en 30 s.

| Fixture | Regla | Clave en el log | Qué rompe |
|---|---|---|---|
| `ok_config.json` | — | — | Nada en 3_Game. |
| `invalid_E_CFG_VERSION.json` | `E_CFG_VERSION` | `version` | `version` es 2. |
| `invalid_E_CFG_ROUND.json` | `E_CFG_ROUND` | `RoundDuration` | `RoundDuration` es 0. `KillsToWin` sigue en 30. |
| `invalid_E_CFG_RESPAWN_DELAY.json` | `E_CFG_RESPAWN_DELAY` | `RespawnDelay` | `RespawnDelay` es 31 (fuera de 0..30). |
| `invalid_E_CFG_MAPS.json` | `E_CFG_MAPS` | `Maps` | Los dos `WorldName` son `sakhal`. Falla si el mundo cargado no es `sakhal`. En `sakhal` este fixture pasaría la regla de mapas. |
| `invalid_E_CFG_SPAWN.json` | `E_CFG_SPAWN` | `Points` | El primer punto de cada mapa es `no-es-xyz`. Así falla tanto en `ChernarusPlus` como en `enoch`. |
| `invalid_E_CFG_PRESET.json` | `E_CFG_PRESET` | `ClothingPreset` | `ClothingPreset` está vacío. `Loadouts` sigue con nombres. La mitad 4_World (preset inexistente en LBmaster) no tiene fixture. |

Línea de log esperada, prefijo incluido: `[NexoDM][Config] <REGLA> <CLAVE>`.
