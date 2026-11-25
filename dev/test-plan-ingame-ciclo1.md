# Test plan in-game — LFGungame · Ciclo 1

> Diseñado para **1 sola sesión** (R5): cubre todo lo que quedó offline en estado
> `[verify]` / `[verify in-game]`. Generado 2026-06-03 tras verificar comandos,
> tiempos, keybind y mensajes contra el código real (R2). Refs `path:line` al pie.
>
> Entorno: **server real dedicado** (LBmaster solo compila ahí; la IP local no está
> whitelisted). Build con `-packonly` (NO binarize). NO hay auto-deploy: lo lanzas tú.

---

## Paso 0 — Setup del `config.json` (PREREQUISITOS — show-stoppers)

Ubicación en el server: **`$profile:LFGungame\config.json`** (se crea solo con
defaults la primera vez que arranca el mod; luego lo editas y reinicias).

| # | Prerequisito | Por qué es bloqueante | Verificación |
|---|---|---|---|
| 0.1 | **Tu SteamID64 real** en `Admin[]` (sustituir los `7656XXXXXXXXXX`) | `!gungame`/`!ggsim` exigen `SteamID ∈ Admin[]`; con el placeholder **ningún comando responde** | mandar `!gungame` y ver el ack en chat |
| 0.2 | **`GunPresets[]` que existan en LBmaster_Core** (≥2, orden `[0]`=T1 … `[n-1]`=Tmax) | si el preset no existe, el spawn no equipa arma (no crashea, pero te quedas sin arma de tier) **y el tier-up que no resuelve tampoco borra el arma anterior (F-06)** | tras spawn, tienes el arma T1 en mano |
| 0.2b | **Cada item dentro de cada preset con `individualChance: 1` + `chance: 1.0`** (hallazgo test 2026-06-11): con el default `individualChance: 0` el preset spawnea UN item aleatorio del pool, no todos (`LB_PresetBase.c:180-195`). Arma con `spawnIn: "hands"`, cargador hijo del arma con `spawnIn: "mag"`, spares `spawnIn: "root"` | si no, el loadout sale incompleto (solo arma, o solo cargador) | tras spawn, arma cargada en mano + spares en inventario |
| 0.3 | **`ClothingPreset` real** + **`Knife` válido** (`HuntingKnife` vanilla vale) | sin ropa/cuchillo válidos el loadout queda incompleto; el cuchillo se valida contra `CfgVehicles`; **al ClothingPreset le aplica 0.2b igual** (si no, spawnea una sola prenda) | tras spawn, llevas ropa + cuchillo |
| 0.4 | **`RespawnCoordinates` / `PublicCoordinates` de TU mapa, con la Y correcta** | el spawn usa la **Y exacta del config** (ya NO snapea al terreno); coords de otro mapa = caída/bajo el suelo | tras spawn, apareces donde esperas, a la altura correcta |
| 0.5 | **`MaxPlayers = "2"`** para esta sesión (mínimo permitido; default 15) | con 2 jugadores la ronda arranca **al llenarse**, sin esperar los 60s de votación | el Paso 3 arranca al votar ambos |
| 0.6 | **Ningún `GunPresets[]` contiene la clase configurada en `Knife`** (regla F-25) | el cuchillo se da aparte y el borrado de loadout lo preserva venga de donde venga → un GunPreset con cuchillo **acumula un cuchillo extra por tier-up** | revisar los presets en LBmaster antes de arrancar |

> El default `RoundDuration` es 1200s (20 min): de sobra; no lo toques.
> `KnifeOneHit` ya viene `"true"` por defecto.

---

## Paso 1 — Build + deploy + checks (cubre el RIESGO Nº1: errores de script)

1. **Build `-packonly`** (NO AddonBuilder binarize — binarize suelta los `.c` y el PBO
   queda sin lógica). Tu método habitual de build/deploy al server.
2. **Check PBO no-vacío + lleva los fixes 06-10** (host, no bash): `Grep` dentro del
   `.pbo` desplegado de **`LFGungame_RoundManager`** (scripts dentro → build OK) y de los
   dos marcadores de la remediación: **`m_PendingPublicCleanup`** y **`OnClientNewEvent`**.
   Si falta cualquiera → el PBO desplegado es viejo (GG-10), rebuild con `-packonly`.
   (Referencia de tamaño: ~120 KB+; el grep es el check autoritativo, no el tamaño.)
3. **Arrancar el server real → leer el RPT** y confirmar **0 errores de script**:
   - Buscar en el RPT: `Can't compile`, `Compile error`, cualquier línea con `LFGungame`
     en un error, y warnings de carga de módulo.
   - Confirmar que el config se creó: aparece `$profile:LFGungame\config.json` y NO hay
     líneas `[LFGungame][Config] E_CFG_*` (esas señalan config inválido — p.ej.
     `E_CFG_KNIFE_INVALID`, `E_CFG_GUN_PRESETS_EMPTY`, `E_CFG_COORD_INVALID`).
   - Buscar **`PLACEHOLDER`** en el RPT al arrancar: cada línea señala un campo del
     config todavía con el valor por defecto (algún prerequisito 0.1–0.5 sin hacer).

**Gate**: si el RPT tiene errores de script, PARAR aquí y traérmelos — el resto del test
no aplica hasta que compile limpio.

---

## Paso 2 — Pre-vuelo en SOLO (con `!ggsim`, sin nadie más)

Entra tú solo al server. Cada fila: comando → qué observar → criterio.

| # | Acción | Qué debe pasar | Criterio |
|---|---|---|---|
| 2.1 | `!gungame` | Ack en chat **"Juego de Armas iniciado."** + se te abre el cuadro **"¿Te unes al juego de armas?"** (SÍ/NO). Pulsa **SÍ** | A1/A2, B1/B2 |
| 2.2 | esperar **~60s** (votación; en solo no se llena `MaxPlayers`) | te **teletransporta** a `RespawnCoordinates` (a la **Y exacta** del config), te **congela 10s** con cuenta atrás central **10→0** (número en rojo los últimos 3s), y al acabar ves el HUD **"Arma 1/N"** abajo-izq + **corona** (eres único → líder) | C1/C3/C5, H1/H2 |
| 2.3 | `!ggsim tierup` ×3 | el HUD sube **"Arma 2/N" → "3/N" → "4/N"**; cambia el arma en mano a cada tier; la corona sigue | D1/D2, H1 |
| 2.4 | `!ggsim fillstats` → pulsa **tecla L** | abre el **scoreboard**: 1 fila (tú), **Kills 7 / Asistencias 3 / Muertes 2**, Tier **"Arma 4/N"**. Pulsa **L** otra vez → cierra | G3/G4/G5 |
| 2.5 | `!ggsim win` | **fin de ronda**: anuncio en chat global **"Ganador del Juego de Armas: <tu nombre>"**, te limpian las armas y te teletransportan a `PublicCoordinates` (quedas público), se cierra cualquier diálogo | I1/I3/I4 |

> Alternativa para abortar sin ganar: `!gungamestop` (ack **"Juego de Armas detenido."**).
> Tras `!ggsim win` o stop, el modo vuelve a IDLE y puedes relanzar `!gungame`.

**Notas de tiempos** (verificadas): votación **60s**, freeze **10s**. El pre-vuelo
completo son ~80s de arranque + lo que tardes en teclear los `!ggsim`.

---

## Paso 3 — Con 2 personas (5-10 min) — lo que `!ggsim` no puede simular

Con `MaxPlayers=2`: tú (admin) lanzas `!gungame`; **ambos** recibís el prompt → **SÍ**
los dos → la ronda arranca **al llenarse** (sin esperar 60s).

| # | Acción | Qué debe pasar | Criterio |
|---|---|---|---|
| 3.1 | **Kill real con arma** (A mata a B con el arma de su tier) | A sube +1 (HUD "Arma X/N"); B **respawnea en su MISMO tier**; en el cuerpo de B y en el suelo (~2 m) **no queda arma de fuego/cargador/munición**; cuchillo y ropa de B intactos | D1, F1/F2, **F4** |
| 3.2 | **Cuchillo real** (A cuchillea a B) — `[verify in-game]`, el único con riesgo | B muere **de un golpe**; B **baja −1 tier** (si estaba en T1, se queda en T1 con loadout T1); A sube **+1 tier** | **E2**, E3/E4 |
| 3.3 | **Discriminador GG-01** (en el mismo kill de 3.1, mirar DÓNDE reaparece B) | B reaparece **dentro de la arena, con su tier y loadout** — NO en la costa con ropa vanilla; **ningún arma de tier suelta** junto a un punto de spawn de la arena | F1/F2 (fix GG-01) |
| 3.4 | **`!gungamestop` durante la votación** (lanza `!gungame`, espera a que B tenga el prompt abierto, stop antes de que vote) | el cuadro JOIN de B **se cierra solo** y NO reaparece ~200ms después; tras el stop nadie conserva HUD "Arma 1/N" ni scoreboard abrible en IDLE | GG-05/GG-06 |

**El riesgo real de 3.2 no es la muerte** (`SetHealth(0)` mata fiable) **sino la
atribución del +1 al matador**: la muerte scripteada llega con `killer=null`, y hay un
fallback (`lastKnifeKillerId`) que le da el crédito. **Confirma que A efectivamente
sube +1** tras el cuchillazo. Si no sube → es el fallback; tráeme el `script_*.log`.

---

## Paso 4 — Logs/datos a recolectar (todo de una, R5)

- **RPT completo** del server tras la sesión.
- **`script_*.log`** del server.
- Capturas: HUD (**"Arma X/N"** + corona), **scoreboard** (Paso 2.4), **countdown** (Paso 2.2).
- Coords del player justo tras el spawn del Paso 2.2 (confirmar la **Y**).
- De cualquier paso que falle: la línea `[LFGungame]` / `[LFGungame][Config]` relevante.
- Check del fix GG-01: si tras un respawn real aparece `RESPAWN_EQUIPPED` en el log pero
  el jugador NO está equipado dentro de la arena, el server cree que equipó al personaje
  equivocado — traerme RPT + `script_*.log` de ese momento.

---

## Riesgos y Plan B

| Riesgo | Síntoma | Plan B |
|---|---|---|
| Config no editado (0.1–0.4) | comandos sin respuesta / spawn sin arma / spawn mal ubicado | corregir `config.json` + reiniciar server (es lo más probable si algo "no hace nada") |
| Build binarizado | el mod carga pero **nada** responde y el grep del Paso 1.2 falla | rebuild `-packonly` |
| Cuchillo no mata de un golpe (E2) | B sobrevive al cuchillazo | `KnifeOneHit` ya está en `true`; si aun así no mata, es tuning de daño melee → lo reviso offline con el log |
| Cuchillo mata pero A no sube (E2 atribución) | B muere, B baja, pero A no sube | revisar fallback `lastKnifeKillerId` con `script_*.log` |
| Server no dedicado | TODO el flujo es no-op (`CanMutateState` exige dedicado) | usar el server real dedicado (no host local) |

---

## Después del test (NO ahora)
- Si todo pasa: marcar A–I como ✓ en `product-spec.md` y **revertir `!ggsim`**
  (buscar `=== LFGUNGAME DEBUG` en `LFGungame_ChatHandler.c` + `RoundManager.DebugSim`).
- Pendiente aparte: design-pass P12 (fuente vanilla + corchetes finos).

---

## Refs verificadas (R2)
- Comandos + permiso admin/dedicado: `LFGungame/scripts/5_Mission/LFGungame/LFGungame_ChatHandler.c:46,61,78,110-147`.
- `DebugSim` (tierup/tierdown/fillstats/win): `LFGungame/scripts/4_World/LFGungame/LFGungame_RoundManager.c:1307-1356`.
- Arranque: `StartGame` :128 / votación 60s `StartVotingTimer` :1888-1896 / `BeginRound` spawn+freeze 10s :454-528 / spawn Y-del-config :1404-1408 / `EndRound` :171-225 / `CanMutateState` dedicado :2276-2291.
- Keybind L (toggle, rebindeable, solo participantes): `inputs.xml:11`, `LFGungame/scripts/5_Mission/LFGungame/LFGungame_MissionGameplay.c:42-87`.
- Config + defaults placeholder: `LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c` + `LFGungame_Constants.c:25-101`.
- Textos/mensajes: `stringtable.csv`, `LFGungame_Constants.c:97-101`.
