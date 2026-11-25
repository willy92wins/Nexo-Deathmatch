# Nexo-Deathmatch

Deathmatch por arenas para el servidor DayZ Nexo, basado en LFGungame.

Estado: punto de partida. `LFGungame/` es el código de LFGungame importado sin cambios; la versión 1.0 (NexoDM) se construye encima por fases, cada una en su PR.

## Dependencias
- [Community Framework](https://steamcommunity.com/sharedfiles/filedetails/?id=1559212036) (`JM_CF_Scripts`)
- LBmaster Core y LBmaster Groups (de pago, no incluidos): los loadouts se leen de los presets de LBmaster en `$profile\LBmaster\Config\Common\ItemPresets.json`.

## Estructura
- `LFGungame/`: mod (config.cpp, scripts 3_Game/4_World/5_Mission, layouts, stringtable).
- `dev/`: notas de desarrollo heredadas de LFGungame (CLAUDE.md, HANDOFF, product-spec, bug-ledger, APIs verificadas), herramientas de prueba y fixtures de config.
  `dev/examples/config.json` es un ejemplo del formato antiguo con el SteamID de admin sustituido por `CAMBIAR_STEAMID64`.

Los PBO y las firmas no se versionan (`.gitignore`).
