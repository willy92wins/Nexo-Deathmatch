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
