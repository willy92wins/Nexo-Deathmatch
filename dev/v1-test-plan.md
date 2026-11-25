# NexoDM v1.0: prueba única en PruebasLF

Checklist de lo que solo se puede comprobar en el juego. La prueba se hace una vez, con la v1.0 completa y el paquete
firmado de la última fase (`<repos>\nexo-deploy\<fase>\@NexoDM`, clave `Return0`).

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

## F4 · Loadouts en cada spawn

| # | Paso | Resultado esperado | Qué recoger |
|---|---|---|---|
| 4.1 | 20 respawns de B con 2 loadouts de 3 armas cada uno | Las 20 veces aparece con la ropa del preset y un arma en manos con bala en recámara; salen al menos 2 armas distintas | captura de 3 respawns, log `RESPAWN_EQUIPPED` |
| 4.2 | Un loadout que trae su propia venda y su propio cuchillo | Hotbar 1 arma, 2 cuchillo, 3 venda, sin repetidos en el inventario | captura del inventario |
| 4.3 | Matar a B y acercarse al cadáver a los 2 s | Ni armas, ni cargadores, ni munición, ni cajas de munición recogibles | captura |
| 4.4 | Esperar junto al cadáver | Se borra a los max(10, RespawnDelay + 5) s, nunca antes de que acabe la cámara de B | log con hora de muerte y de borrado |
| 4.5 | Escopeta (MP-133 o BK-43) en un loadout | La bala en recámara es letal, no `RubberSlug` | captura del arma |
| 4.6 | B se desconecta inconsciente y su cuerpo muere | Su cadáver tampoco deja armas | captura |

## F5 · HUD

Diseño aprobado por Guillermo el 25-nov a las 18:37 (`dev/mocks/hud-nexo-dayz.html`, SHA-256 `B0D40554…DF45`). En la prueba se compara la captura del juego con ese mock.

| # | Paso | Resultado esperado | Qué recoger |
|---|---|---|---|
| 5.1 | Entrar en una ronda | Bajo Salud, Sangre, Agua y Comida aparece su %, con la flecha de tendencia vanilla debajo; el HUD vanilla sigue igual | captura a 1080p y a 1440p |
| 5.2 | Recibir daño, sangrar, beber y comer | El % baja o sube como mucho 1 s después y coincide con el valor real (consola de admin o log) | capturas antes y después |
| 5.3 | Arma en manos | Abajo a la izquierda, el modelo 3D del arma con sus accesorios y su nombre; al guardarla se oculta; al cambiar de arma cambia | capturas |
| 5.4 | Durante la ronda | Arriba, el tiempo restante y las bajas (de `KillsToWin`); el tiempo coincide con el del servidor ±1 s | captura y log `HUD_ROUND` |
| 5.5 | Matar y morir | Cada baja sale en el killfeed (arriba, pequeño) y se va a los pocos segundos | captura |
| 5.6 | Mantener T y soltar | Mientras se mantiene, el top 5 con tu fila resaltada (negro con texto rojo); al soltar desaparece; no se ve en ningún otro momento | captura |
| 5.7 | Tras 4 bajas y 2 asistencias | 500 puntos en el HUD y en el log del servidor | log de servidor y de cliente |
| 5.8 | Cámara del asesino, menú de pausa, fin de ronda | El HUD nuevo se oculta donde dice el mock y no queda nada pintado fuera de la ronda | capturas |
| 5.9 | Con LBmaster Groups cargado, pulsar T | Sale el top 5. Que salte también el ping táctico de grupo es lo esperado: Guillermo decidió el 25-nov a las 21:02 dejar T aunque coincida, reasignable en Controles | captura |
| 5.10 | Cliente en inglés | Sin claves `STR_` sueltas | captura |

## F6 · Leaderboard global

| # | Paso | Resultado esperado | Qué recoger |
|---|---|---|---|
| 6.1 | `MinPlayersToScore: 20` con 2-3 jugadores; acabar una ronda | `leaderboard.json` con el mismo SHA-256 que antes; log `no puntúa (N < Min)` | hash antes y después, log |
| 6.2 | `MinPlayersToScore: 2` con 3 jugadores; acabar una ronda | 1.º = kills + 10, 2.º = kills + 9, 3.º = kills + 8 en `leaderboard.json`; `rounds` +1 cada uno | el JSON y el ranking de la ronda (L) |
| 6.3 | Reiniciar el servidor y jugar otra ronda que puntúe | Los puntos anteriores siguen y se suman | JSON antes y después del reinicio |
| 6.4 | Corromper `leaderboard.json` a mano (texto basura) y arrancar | Se aparta como `.broken`, aviso en el log, se recupera del `.bak` si existe (si no, vacío) y sin crash | log, ficheros de la carpeta |
| 6.5 | Al acabar cada ronda | El top 10 global aparece 10 s con el estilo del top 5, tu fila resaltada, y coincide con el JSON; se oculta si empieza otra ronda antes | captura y JSON |
| 6.6 | `MinPlayersToScore: 0` | La ronda no arranca: `E_CFG_MIN_PLAYERS` en el log | log |

## F7 · KO corto

Decisión de Guillermo del 25-nov a las 21:57: opción (a). El KO dura unos 2 s, sin saltarse la protección vanilla de 2 s.

| # | Paso | Resultado esperado | Qué recoger |
|---|---|---|---|
| 7.1 | Dejar KO a B (por ejemplo, con munición de goma o un golpe de culata) | B está consciente a ≤ 2,5 s del inicio | log `[NexoDM][KO]` con la hora de inicio y la de la subida de shock; vídeo o capturas con hora |
| 7.2 | Darle otro golpe a B entre 1 y 2 s de KO | Aun así se levanta antes de unos 3 s | log |
| 7.3 | Matar a B mientras está KO | Muere con normalidad; la cámara del asesino y el respawn funcionan | captura y log |
| 7.4 | B se desconecta estando KO | No se despierta; se aplica la muerte vanilla por combat-log | log del servidor |
| 7.5 | Revisar el log del servidor | Sin errores de script | `script_*.log` |

## F8 · HUD final (sustituye a F5)

Referencia: `dev/mocks/hud-nexo-mezcla.html` (v4).

| # | Paso | Resultado esperado | Qué recoger |
|---|---|---|---|
| 8.1 | Entrar en una ronda | Arriba a la izquierda, el KDA (bajas, muertes, asistencias) sin puntos | captura 1080p y 1440p |
| 8.2 | Matar a alguien | Al matar no sale +100 ni +50; el KDA sube; en el killfeed (arriba a la derecha) se ve el icono del arma del asesino | captura |
| 8.3 | Matar con un arma que el cliente no tiene cerca (asesino lejos) | El icono sale igual (entidad local) o, si no, la silueta de respaldo, sin errores | captura y log del cliente |
| 8.4 | Abajo a la izquierda | Solo la postura y la barra de aguante vanilla; el arma se ve en la quickbar vanilla abajo al centro | captura |
| 8.5 | Abajo a la derecha | Solo los iconos vanilla de sangre y salud, sin números; ni agua, ni comida, ni temperatura | captura sano y herido |
| 8.6 | Mantener T | Top 5 centrado con Bajas, Muertes y Asistencias; tu fila resaltada solo si estás en el top 5; al soltar desaparece | captura |
| 8.7 | Acabar una ronda | 10 s de clasificación de la ronda («Fin de partida · gana X», mapa, objetivo) y después 10 s del top 10 global, con el mismo aspecto | capturas |
| 8.8 | Ronda con muchos jugadores | Comprobar cuántas filas caben a 1080p (ver `dev/F8B-REPORT.md`) | captura |
| 8.9 | Salir al menú y volver a entrar | Sin restos del HUD ni de los iconos del killfeed; sin errores | log del cliente |