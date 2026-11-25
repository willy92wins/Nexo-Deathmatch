# NexoDM v1.0: prueba única en PruebasLF

Checklist de lo que solo se puede comprobar en el juego. La prueba se hace una vez, con la v1.0 completa y el paquete
firmado de la última fase (`C:\Users\guill\Repos\nexo-deploy\<fase>\@NexoDM`, clave `Return0`).

Preparación:
- Servidor con `@CF`, `@LBmaster_Core`, `@LBmaster_Groups` y `@NexoDM`; `keys\Return0.bikey` en la carpeta `keys` del servidor.
- `ItemPresets.json` de LBmaster con un preset de ropa y al menos dos loadouts. En cada loadout, un padre con
  `individualChance: 0` y 3 armas hijas con su `chance`.
- `$profile:NexoDM\config.json` con esos nombres en `ClothingPreset` y `Loadouts`, y con los mapas de Chernarus del config por defecto.
- Recoger al final: `script_*.log` y `.RPT` del servidor, y el `script_*.log` de cada cliente.

## F1 · Limpieza

| # | Paso | Resultado esperado | Qué recoger |
|---|---|---|---|
| 1.1 | Arrancar el servidor con el mod | 0 errores de script; el mod carga | `script_*.log`, `.RPT` |
| 1.2 | Conectar un cliente | Entra sin kick de firma | log del cliente |

## F2 · Config de mapas y loadouts

| # | Paso | Resultado esperado | Qué recoger |
|---|---|---|---|
| 2.1 | Arrancar sin `config.json` en el perfil | Se escribe el config por defecto con los 4 mapas; la ronda no arranca (los `CAMBIAR_` no existen) | el fichero generado y la línea `E_CFG_PRESET` del log |
| 2.2 | Copiar cada fixture de `dev/tools/fixtures/configs/` como `config.json` y lanzar la ronda | `ok_config.json` arranca; cada `invalid_*.json` se rechaza con su id de regla (ver `EXPECTED.md`) | una línea de log por fixture |
| 2.3 | Config con un mapa de otro mundo y otro de Chernarus | Aviso `W_CFG_MAP_OTHER_WORLD` y la ronda arranca con el de Chernarus | log |

## F3 · Ciclo de ronda, rotación y cámara del asesino

Hacen falta 2 clientes (A y B). La config tiene `RespawnDelay: 5`, `KillsToWin: 5`, `RoundDuration: 300` y los 4 mapas.

| # | Paso | Resultado esperado | Qué recoger |
|---|---|---|---|
| 3.1 | El admin escribe `!nexodm` con un config válido | Responde «ronda iniciada»; los dos aparecen en un punto del mapa activo, congelados 10 s | log `round entered active`, captura |
| 3.2 | `!nexodm` con un config inválido | El chat dice que el config no es válido; el log da la regla | log |
| 3.3 | A mata a B 10 veces (B no pulsa nada) | Cada vez B ve a A en 3ª persona 5 s y reaparece solo, en un punto del mapa activo distinto del anterior y dentro de ±`SpawnRadius` | captura de la cámara, coordenadas de cada respawn en el log |
| 3.4 | B muere por caída o suicidio | La cámara se queda sobre su cuerpo 5 s y reaparece | captura |
| 3.5 | A se desconecta mientras B le está viendo | Sin crash; la cámara de B vuelve a su cuerpo y reaparece | log del cliente B |
| 3.6 | Repetir 3.3 con `RespawnDelay: 0` | B reaparece al instante, sin cámara | log |
| 3.7 | B pulsa Respawn a mano durante la cámara | Un solo respawn, sin doble personaje ni menú colgado | log |
| 3.8 | A llega a 5 bajas | La ronda acaba en esa baja; gana A | log `kills_to_win` |
| 3.9 | Ronda sin llegar a 5 bajas | La ronda acaba a los 300 s; el ganador sale por puntos, luego menos muertes, luego quien llegó antes | log |
| 3.10 | 4 bajas + 2 asistencias de A | A tiene 500 puntos en el scoreboard (L) | captura del scoreboard |
| 3.11 | Encadenar 5 rondas con `Autostart: true` | Se rearma tras cada ronda; el mapa nunca repite ni el actual ni el anterior | log de rotación por ronda |
| 3.12 | Al empezar cada ronda | Nadie conserva armas de la ronda anterior; no quedan armas en el suelo del mapa anterior | captura del inventario y del suelo |
| 3.13 | B entra durante los 10 s de congelación | Queda congelado e inmune hasta que acaba la congelación | captura |