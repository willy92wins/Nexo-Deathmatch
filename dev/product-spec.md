# product-spec.md — LFGungame

> **Definición de Producto Final (DPF)** — el contrato de "qué es terminado".
> Lo lee toda sesión al entrar (referenciado desde `CLAUDE.md`/`AGENTS.md`). Es el
> target estable; el progreso vivo va en handoffs del vault (`30_Sessions/`), NO aquí.
> Cerrado con el **Grill de producto** (workflow.md §"Grill — Modo A") el **2026-05-27**,
> confirmado con el usuario en 4 rondas (ver `decisions/decision-log.md` D1).

## Qué es "terminado" (alcance acordado — v1)

Mod de **GunGame** para DayZ, server-authoritative, activado por el comando de chat
LBmaster `!gungame` (solo Admins). Una ronda completa: pregunta de unirse → asignación
participante/público → teletransporte + congelación 10s con cuenta atrás → progresión
por tiers de armas (cualquier kill de un participante = +1; el cuchillo además baja −1 a
la víctima) → victoria al matar en el tier máximo (o fin por tiempo) → anuncio de ganador + retorno a
público. Incluye scoreboard (tecla L), contador de tier + corona en HUD, y config JSON
no-lazy. **Confirmado con usuario 2026-05-27.**

## Dependencias (verificadas — R2, source montado)

- **`LBmaster_Groups`** — comando de chat. Override de `LBChatHandler`
  (`ChatCommandExists`/`HasPermission`/`OnChatCommand`, siempre `super`):
  `LBmaster_Groups/scripts/5_Mission/LBmaster_Groups/System/LBChatHandler.c:22/36/52`.
  Mensajes: `LBChatHandler.SendSimpleChatMessage(PlayerIdentity, string, bool global)`.
- **`LBmaster_Core`** — presets de loadout: `LB_PresetLoader.Get.GetPreset(name)`
  (`.../Config/LB_PresetLoader.c:146`), `FindPresets(TStringArray)` (`:189`);
  `LB_PresetBase.SpawnPreset(player, parent, altPos, radius)` (`.../Struct/LB_PresetBase.c:138`),
  `static SpawnPresets(...)` (`:92`). Permisos: `LBAdmins.HasPermission(perm, sender)`
  (`.../LBAdmins.c:464`).
- **Gotcha contractual (refinado 2026-05-27 vía prior-art validation)**: las *chances*
  solo aplican a `SpawnPresets` *plural* (`LB_PresetBase.c:96`) y a *linked sub-presets*
  dentro de un preset. **`SpawnPreset` singular del root** (el patrón que usa el plan)
  **es determinista**: el preset raíz spawnea siempre. Sí hay que documentar al usuario
  que cualquier *sub-preset linked* (cargo, attachments, mags) dentro de sus presets
  debe tener chance 1/100% si quiere que sea determinista.

## Esquema del config JSON

Ubicación: **`$profile:LFGungame\config.json`**. **Creación no-lazy**: si no existe la
carpeta `LFGungame` y/o el archivo al ejecutar, se crean con valores por defecto.

```jsonc
{
  "Admin": ["7656XXXXXXXXXX", "7656XXXXXXXXXX"],   // nunca participan; no notificados ni movidos. También = quién puede usar !gungame
  "ClothingPreset": "Preset1",                      // preset LBmaster de ropa (participantes y público)
  "GunPresets": ["GunPreset1","GunPreset2","GunPreset3"], // ORDEN: [0]=T1 ... [n-1]=Tmax
  "Knife": "HuntingKnife",                           // clase del cuchillo; dado a cada participante, NO se borra al subir
  "KnifeOneHit": "true",                             // opcional: intentar daño one-hit (ver criterio [verify in-game])
  "RoundDuration": "1200",                           // segundos
  "MaxPlayers": "15",
  "RespawnCoordinates": ["18939,3828,3993","x,y,z"],
  "RespawnRadius": "10",
  "PublicCoordinates": ["18235,876,8567","x,y,z"],
  "PublicRespawnRadius": "10"
}
```

Parsing: valores numéricos vienen como **string** → parsear a número (`"1200"`→1200).
Coords `"x,y,z"` → vector. (El esquema mantiene strings por fidelidad al input del usuario.)

## Cláusula de desafío (el criterio sirve a la intención)

Cada criterio de aceptación de abajo es la forma *verificable* de una **intención** (el `Intent` declarado en su sección). El criterio binario es el gate de aceptación y **no se cumple a la letra rompiendo el espíritu**. Si al planificar o implementar una fase detectas que:

- (a) un criterio **choca** con su `Intent`,
- (b) hay un camino **más simple** que satisface el `Intent` igual o mejor, o
- (c) el `Intent` revela un **criterio que falta**,

estás **obligado** a surfacearlo en el **Grill de plan (Modo B)** ANTES de implementar — no como justificación a posteriori. El usuario adjudica; todo cambio de criterio se anota en el *Changelog de alcance*. La intención **no sustituye** al criterio binario: lo explica y autoriza a renegociarlo con visto bueno.

## Criterios de aceptación verificables (R26)

> Estado: ✓ hecho · ⏳ en curso · ❓ sin empezar · [verify] correcto offline, falta in-game.
> **2026-05-27: todo ❓ (solo scaffold + contrato; sin implementación).**

### A — Activación y config

> **Intent**: arrancar el modo es una acción de admin con efecto global; debe ser imposible que un no-admin lo dispare o lo rompa, y la config debe existir siempre — sin ella la ronda no es ni justa ni reproducible.

| # | Criterio | Cómo se verifica | Estado |
|---|----------|------------------|--------|
| A1 | `!gungame` registrado vía modded `LBChatHandler` (3 overrides + super) | el comando se reconoce in-game; logs LBmaster | [verify] (S1 impl 2026-05-27) |
| A2 | Permiso fail-closed: solo SteamID ∈ `Admin[]` arranca; otros → no-op | no-admin envía `!gungame` → nada ocurre | [verify] (S1 impl 2026-05-27) |
| A3 | Config no-lazy: crea carpeta `LFGungame` + `config.json` con defaults si falta | borrar config → arrancar → reaparece con defaults | [verify] (S0 impl 2026-05-27) |
| A4 | Parsing string→número y `"x,y,z"`→vector correcto | unit/log de los valores parseados | [verify] (S0 impl 2026-05-27) |
| A5 | `!gungamestop` cancela una ronda en curso (admin) | en ronda activa, admin envía stop → ronda termina | [verify] (S1 impl 2026-05-27, EndRound skeleton) |

### B — Unirse y asignación de roles

> **Intent** (alineado a la letra 2026-06-10, OBS-1 — el evento transforma a todos los presentes): cada jugador elige conscientemente si **compite**; quien dice NO (o no contesta) pasa a **público**: teletransportado a su zona con la ropa del evento y el inventario real borrado (C2/C4+B3). Los únicos exentos son los **admins** (rol `NONE`, intactos — enmienda 2026-06-02). El rol (participante / público / `NONE`) decide TODO lo que sigue, así que asignarlo mal corrompe la ronda entera.

| # | Criterio | Cómo se verifica | Estado |
|---|----------|------------------|--------|
| B1 | Admins (`Admin[]`) reciben el prompt como cualquiera; **SÍ** con cupo → participan igual que un jugador (teleport+wipe+loadout T1+cuchillo+freeze+countdown+tiers); **NO / timeout / lleno / late-join** → noop (rol `NONE`: ni teleport ni cambios de inventario/posición) | admin SÍ → participante completo; admin NO/timeout → queda intacto donde estaba | [verify] (enmienda impl 2026-06-02; falta in-game) |
| B2 | A todos los no-admin les sale el cuadro "¿Quieres participar?" con botones SÍ/NO | in-game: aparece el widget | [verify] (S2 impl 2026-05-28) |
| B3 | Ventana de 60s; sin respuesta → NO/público y se cierra el widget | esperar 60s sin click → queda público | [verify] (S2 impl 2026-05-28) |
| B4 | **No-admin** SÍ → participante si participantes < `MaxPlayers`; si lleno → público + notificación "partida llena". **Admin** SÍ con lleno → noop (rol `NONE`, no público) | llenar a MaxPlayers; no-admin SÍ → público+aviso; admin SÍ → queda intacto | [verify] (S2 2026-05-28; enmienda admin 2026-06-02; falta in-game) |
| B5 | Teletransporte al cumplirse 60s **o** al alcanzar `MaxPlayers` (lo que ocurra antes) | llenar antes de 60s → arranca ya | [verify] (S2 impl 2026-05-28) |
| B6 | Late-join **no-admin** (conecta con ronda en curso) → público (solo ClothingPreset), sin prompt. Late-join **admin** → noop (rol `NONE`, no público, sin mover) | no-admin a media ronda → público; admin a media ronda → intacto | [verify] (S2 2026-05-28; enmienda admin 2026-06-02; falta in-game) |

### C — Arranque (teleport + freeze + countdown)

> **Intent**: todos arrancan en igualdad — misma señal de salida, nadie dispara antes de tiempo. El freeze debe inmovilizar de verdad (sin moverse, disparar ni girar la cámara) sin romper otros inputs; el público es espectador y no se congela.

| # | Criterio | Cómo se verifica | Estado |
|---|----------|------------------|--------|
| C1 | Participantes → coord **aleatoria** de `RespawnCoordinates` dentro de `RespawnRadius` | spawns dispersos en las coords | [verify] (S3 impl 2026-05-29) |
| C2 | Público → coord aleatoria de `PublicCoordinates` dentro de `PublicRespawnRadius` | ídem para público | [verify] (S3 impl 2026-05-29) |
| C3 | A participantes: borrar TODO lo que lleven → dar `ClothingPreset` + gun preset T1 + `Knife` + **venda** (`BandageDressing`) | inventario tras spawn = eso (ropa+arma+cuchillo+venda); hotbar: arma→1, cuchillo→2, venda→3 | [verify] (S3 2026-05-29; venda+hotbar 2026-06-14) |
| C4 | A público: borrar loot → dar **solo** `ClothingPreset` (sin armas ni cuchillo) | inventario público = solo ropa | [verify] (S3 impl 2026-05-29) |
| C5 | **Solo participantes**: congelación 10s + cuenta atrás central 10→0 (solo etiqueta `Iniciando en/Starting in` + número, centrado; **sin barra, sin anillo, sin role badge**; número en rojo los últimos 3s). El **público NO se congela** ni ve el countdown (solo se teletransporta a su zona con ropa, C2/C4). Solo al inicio | in-game: participantes inmóviles 10s con número; público libre y sin countdown | [verify] (S5.5 impl 2026-06-02: barra/role-badge fuera, freeze+countdown solo participante; R21 doble apto) |

### D — Progresión de tiers

> **Intent**: subir de tier es el motor del juego y debe sentirse limpio — al cambiar de arma no quedan restos del tier anterior que den ventaja; el cuchillo (herramienta de castigo) sobrevive a los cambios; la ropa no se resetea gratis.

| # | Criterio | Cómo se verifica | Estado |
|---|----------|------------------|--------|
| D1 | **Cualquier kill** sube +1 tier al asesino (gana si estaba en el máximo). El cuchillo además degrada a la víctima (E3). *(Aclarado 2026-05-27: el loadout controlado hace que "cualquier kill" ≈ "kill con arma de tier o cuchillo"; solo hace falta detectar SI fue el cuchillo, no clasificar el arma.)* | cualquier kill de un participante → +1 tier | [verify] (S5 impl 2026-05-29) |
| D2 | Al cambiar de tier (↑/↓): borrar todos los objetos que coincidan con el gun preset **previo dado** + dar el nuevo | inventario tras cambio = nuevo preset, sin restos del anterior | [verify] (S4 impl 2026-05-29) |
| D3 | El cuchillo (`Knife`) **persiste** entre tiers (no se borra al subir/bajar) | el cuchillo sigue tras varios cambios de tier | [verify] (S4 impl 2026-05-29) |
| D4 | La ropa NO se re-da en cambio de tier (solo en spawn/respawn) | cambio de tier no resetea ropa | [verify] (S4 impl 2026-05-29) |

### E — Cuchillo

> **Intent**: el cuchillo es la mecánica de **castigo/degradación** (baja un tier a la víctima), no el arma principal. Debe ser letal DENTRO del modo para que cuchillar sea viable, **sin romper el cuchillo vanilla fuera** de la ronda. Solo lo llevan participantes.

| # | Criterio | Cómo se verifica | Estado |
|---|----------|------------------|--------|
| E1 | Cada participante recibe el `Knife` configurado al spawnear | inventario tras spawn incluye el cuchillo | [verify] (S3 impl 2026-05-29) |
| E2 | Cualquier golpe del `Knife` mata de un golpe a cualquier jugador, cualquier zona `[intent: instakill solo DENTRO del modo — gatear el daño, no maximizar el daño melee global y romper el cuchillo vanilla fuera]` | in-game: 1 golpe = muerte | [verify in-game] (tuning de daño melee) |
| E3 | Kill con cuchillo: asesino **+1 tier** (gana si estaba en máximo) **y** víctima **−1 tier** | knife-kill → killer sube, víctima baja | [verify] (S5 impl 2026-05-29) |
| E4 | Víctima en T1 cuchilleada → se queda en T1 (no baja) y se le re-da el loadout T1 | knife-kill a un T1 → sigue T1 | [verify] (S5 impl 2026-05-29) |
| E5 | El cuchillo lo tienen solo participantes (no el público) | público no tiene cuchillo | [verify] (S5 impl 2026-05-29) |

### F — Muerte y respawn

> **Intent**: morir no te saca de la partida ni regala loot que rompa la progresión. Vuelves con el loadout que te toca por tu tier, y tu arma de tier no queda en el suelo para que otro la lootee y se salte la escalera.

| # | Criterio | Cómo se verifica | Estado |
|---|----------|------------------|--------|
| F1 | Muerte por algo que NO sea cuchillo → víctima respawnea con su **mismo tier** | matar con bala → la víctima vuelve al mismo tier | [verify] (S5 impl 2026-05-29) |
| F2 | Respawn (cualquier causa): borrar + `ClothingPreset` + gun preset del tier actual + `Knife` + **venda** (`BandageDressing`) | inventario tras respawn = eso; hotbar: arma→1, cuchillo→2, venda→3 | [verify] (S5 2026-05-29; venda+hotbar 2026-06-14) |
| F3 | Solo cuentan jugadores **participantes**: matar a público no cambia tiers ni stats | matar público → sin efecto gungame | [verify] (S5 impl 2026-05-29) |
| F4 | **Limpieza de armas al morir (integridad anti-loot)**: al morir un participante se le quitan **armas de fuego + cargadores + munición** del cuerpo (`WipeWeaponsOnly`) y se barren las que hayan caído al suelo a ~2m → no queda arma de tier para lootear y saltarse la progresión. Se **conservan** cuchillo (uniforme), ropa y cuerpo | in-game: tras una muerte, sin armas de fuego en el cuerpo ni en el suelo a ~2m; cuchillo/ropa/cuerpo intactos | [verify in-game] (impl 2026-06-03) |

### G — Estadísticas y scoreboard

> **Intent**: el marcador refleja la ronda actual de forma justa (kills, assists, deaths, tier) y solo de quien juega; no es un ranking persistente. La asistencia premia haber contribuido a una baja sin ser el rematador.

| # | Criterio | Cómo se verifica | Estado |
|---|----------|------------------|--------|
| G1 | Stats por partida, en memoria, reset al arrancar cada `!gungame` (no persisten tras reinicio) | nueva ronda → stats a 0 | [verify] (S1 ResetStats impl 2026-05-27) |
| G2 | Asistencia = causaste daño a la víctima desde su último spawn y no fuiste el rematador | dañar + otro remata → +1 assist a ti, +1 kill al rematador | [verify] (S5 impl 2026-05-29) |
| G3 | Scoreboard (tecla L, **toggle**, configurable en controles) centrado: columnas Player/Kills/Assists/Deaths/Tier | pulsar L abre/cierra; filas por jugador | ❓ |
| G4 | Columna Tier en formato "Arma X/N" (índice gun actual / total) | p.ej. T4 de 21 → "Arma 4/21" | ❓ |
| G5 | El scoreboard muestra **solo participantes** | público no aparece | ❓ |

### H — HUD (contador de tier + corona)

> **Intent**: cada participante ve de un vistazo en qué tier va y quién lidera, sin abrir el scoreboard. La corona señala al/los líder(es) y crea el objetivo visible de la partida.

| # | Criterio | Cómo se verifica | Estado |
|---|----------|------------------|--------|
| H1 | Contador de tier siempre visible **abajo a la izquierda** para participantes (mismo valor "Arma X/N") | visible durante toda la ronda | ❓ |
| H2 | Corona dibujada **encima del contador** para el jugador con el tier más alto o empatado | el/los líder(es) muestran corona | ❓ |

### I — Victoria y fin de ronda

> **Intent**: la partida tiene un final inequívoco — ganar exige rematar en el tier máximo, o (por tiempo) decide el desempate objetivo. Al cerrar, todos vuelven a público y limpios, sin restos del modo que contaminen el mundo.

| # | Criterio | Cómo se verifica | Estado |
|---|----------|------------------|--------|
| I1 | Victoria: un participante hace **cualquier kill** estando en el **tier máximo** → fin de partida | kill en Tmax → termina | ❓ |
| I2 | Fin por `RoundDuration` sin ganador: gana **mayor tier**; desempate **más kills → más assists → menos deaths** | simular timeout → ganador por la regla | ❓ |
| I3 | Anuncio del ganador a todos (chat global + notificación) | mensaje visible para todos | ❓ |
| I4 | Al terminar (victoria/timeout/stop): limpiar loot de participantes + teletransportar a `PublicCoordinates` (quedan público) | tras fin, participantes en zona pública sin loot de juego | ❓ |

## Fuera de alcance (explícito) — R25

v1 incluye **solo** lo de los criterios A–I. Quedan **FUERA**:
- Kill feed / registro de muertes en pantalla.
- Sonidos o música propios del modo.
- Modos por equipos.
- Ranking persistente en BD / estadísticas entre partidas o entre reinicios.
- Web o estadísticas externas.
- Anti-cheat adicional.

**Confirmado con usuario 2026-05-27**: "esto es el spec de v1, lo demás está fuera".

## Decisiones diferidas (no bloquean el contrato; se resuelven en fase 1)

- **Framework GUI** — **RE-RESUELTO 2026-05-28** (D-K1): **vanilla `UIScriptedMenu` + widgets
  nativos** (`FindAnyWidget`) para las 4 piezas (Join, HUD, Scoreboard, countdown). Revierte
  la decisión Dabs MVC del 2026-05-27 tras recibir el kit del usuario en vanilla. `JM_CF_Scripts`
  permanece en `requiredAddons` como fallback opcional (low cost, queda disponible si algo
  necesita Dabs). Prior art ahora: el propio kit `Gungame.zip` + cómo LBmaster usa UIScriptedMenu.
- **Mecanismo de congelación** (C5): cómo inmovilizar al jugador 10s (deshabilitar inputs,
  fijar posición, etc.) — decisión de implementación.
- **Detección de "arma del tier actual" en el kill** (D1): cómo identificar el arma usada
  (clase del arma del killer vs contenido del gun preset) — decisión de implementación,
  con criterio verificable ya fijado.

## Ítems a verificar antes/durante implementación

- `[verify]` **E2**: daño one-hit del cuchillo — el tuning de daño melee en DayZ puede
  requerir prueba in-game; `KnifeOneHit` expone el knob.
- `[ASSUMPTION→verify]` **A1**: que el dispatcher de LBGroups usa prefijo `!` y entrega el
  `cmd` sin prefijo (patrón del ejemplo `!tp`→cmd `"tp"`) — confirmar en el source del
  dispatcher de LBmaster_Groups.
- `[verify]` Comportamiento de `SpawnPreset`/`SpawnPresets` respecto a items existentes
  (asumimos que añade; el borrado del preset previo lo hace nuestro código, D2).

## Changelog de alcance

- **2026-05-27** — alcance v1 fijado y confirmado (Grill de producto, 4 rondas). Añadido
  campo de config `Knife` (+ `KnifeOneHit` opcional) durante el grill. Resuelta
  contradicción de posición del contador de tier → **abajo a la izquierda**. Regla de
  cuchillo confirmada como doble efecto (+1 asesino / −1 víctima). Desempate de fin de
  ronda: tier → kills → assists → deaths.
- **2026-05-27** — framework GUI resuelto: **Dabs MVC + CF** (deps duras) + overlay vanilla
  para HUD/countdown. Confirmado por usuario tras research fase 0. (Dep única `JM_CF_Scripts`:
  Dabs MVC vive dentro de CF, no es addon aparte.)
- **2026-05-27** — **D1 ampliado** (grill de plan): **cualquier kill** sube de tier (antes
  "solo arma de tier o cuchillo"). El cuchillo sigue degradando a la víctima (E3 sin cambio).
  Simplifica la implementación: solo detectar si la fuente letal fue el cuchillo. Confirmado por usuario.
- **2026-05-28** — **Framework UI pivotado a vanilla `UIScriptedMenu`** (revierte D-3 Dabs MVC).
  El kit del usuario (`Gungame.zip`) viene en vanilla; el implementador es débil en Dabs.
  `JM_CF_Scripts` se mantiene como dep low-cost (fallback disponible). Survey en
  `vault research/2026-05-28-ui-kit-survey.md`. Implica correctivo S2.5 (rewrite del dialog).
- **2026-05-28** — **Localización bilingüe ES+EN visible simultaneamente** (D-K3). Cada label
  UI emite dos TextWidget: ES localizable (`#STR_LFGUNGAME_*`) + EN hardcoded debajo en gris.
- **2026-05-28** — **Dirección visual fijada**: estética **Tactical** + paleta **ámbar
  Survivor** del kit (`COL_ACCENT #E6A84B`, `COL_GOLD #F0C45C`, `COL_PANEL` oliva oscuro,
  `COL_DANGER` rojo últimos 3s). Tipografía Chakra Petch / mono. Esquinas en corchete `[ ]`,
  sin redondeo.
- **2026-05-29** — **Countdown HUD simplificado + freeze solo-participantes** (iteración en vivo).
  La cuenta atrás = solo `Iniciando en/Starting in` + número, **centrado**, SIN barra de progreso,
  SIN anillo circular, SIN role badge (el número sigue en rojo los últimos 3s). Se muestra **SOLO
  a participantes**; el **público NO ve el countdown ni se congela** (revierte el "colectivo" de C5;
  el público solo se teletransporta + ropa). Implica tweak de `lfgungame_hud.layout` +
  `LFGungame_HUD.c` + gating de countdown/freeze a participantes en `BeginRound` → fold en el próximo
  correctivo (S5.5/S6). Aparte y aún pendientes: persistencia del contador de arma abajo-izq (S6) +
  nombre de arma amigable (D-K2).
- **2026-06-02 — Enmienda al flujo de unión (IMPLEMENTADA 2026-06-02 por Claude Code; falta in-game)**: el prompt de unirse se
  muestra a **TODOS los conectados, admins incluidos** (hoy los admin se excluyen del prompt). **Admin SÍ** →
  participa **igual que cualquiera** (teleport a `RespawnCoordinates` + wipe + loadout T1+cuchillo + freeze +
  countdown + tiers + stats). **Admin NO / timeout / cancelar / partida llena / late-join** → **noop**: NO se
  teletransporta a público, NO se le toca posición ni inventario; el rol queda `NONE` (no `PUBLIC`). Permite que
  los admin jueguen/prueben sin arriesgar cambios de posición si cancelan. El permiso para **lanzar** `!gungame`
  (A2) **no cambia** (solo SteamID ∈ `Admin[]`); el admin que lanza también recibe su propio prompt (puede unirse
  a su propia ronda). Bordes confirmados con usuario 2026-06-02: admin+lleno → noop; admin late-join → noop.
  Afecta A2 (sin cambio funcional)/B1/B4/B6 — se enmendarán al implementar. Anclas de implementación: quitar el
  rechazo de voto admin (`LFGungame_PlayerBase.c:118-122`, `RULE_VOTE_ADMIN`); incluir admins al asignar
  `PROMPT_PENDING`; en `BeginRound` y en el path late-join, **saltar** a los admin no-participantes (no moverlos).
- **2026-06-03** — **Contrato pasa a formato dual-layer (Ricardian)**: añadida la *Cláusula de desafío* (el criterio sirve a la intención) + banner `Intent` por grupo A–I + intent inline en E2. Los criterios de aceptación R26, su verificación y su Estado **no cambian** (solo se añade la capa de intención). Cierra el hueco letra↔espíritu (*evil compliance*) y habilita que el implementador desafíe un criterio o proponga algo mejor en el Grill Modo B. Confirmado con usuario 2026-06-03.
- **2026-06-03 — Mod terminado OFFLINE (P7–P13)**: fin de ronda (I1 victoria en tier máx / I2 timeout+desempate / I3 anuncio / I4 cleanup-a-público), scoreboard (G3–G5, tecla L rebindeable, RPC server-auth **ordenado por ranking**), HUD tier+corona+sync (H1–H2, NetSync `m_GG_IsLeader`), B4 chat (bus `ScriptInvoker` 3_Game→5_Mission), E2 cuchillo (force-kill `SetHealth(0)` + **fallback de crédito** `lastKnifeKillerId` — REVIERTE la decisión 2026-05-27 de "aumento de daño del item"). Comando debug `!ggsim` (REVERT antes de release). Higiene PBO. P12 (fuente/corchetes) diferido a design-pass. Revisión adversarial independiente: 1 P1 + 4 P2/P3, **todos arreglados**. Todo OFFLINE; pendiente test in-game (1 sesión, casi todo en solo con `!ggsim`). **I1–I4/G3–G5/H1–H2 → `[verify]`; E2 `[verify in-game]`.** Handoff `30_Sessions/2026-06-03-LFGungame-finish-offline-P7-P13`.
- **2026-06-10 — OBS-1 adjudicada: la letra gana sobre la frase del Intent B** (cláusula de desafío, caso (a)). La frase original ("nadie acaba teletransportado ni con el inventario borrado sin haber dicho que sí") contradecía C2/C4+B3 confirmados (changelog 05-27 + enmienda 06-02): un no-admin que vota NO / no contesta SÍ acaba público (teleport + wipe). El usuario confirma que la transformación de todos los presentes es el diseño querido (server de evento); el Intent B se reescribe alineado a la letra. Criterios B1–B6 sin cambio. Aparte, misma sesión: regla operativa **F-25** añadida al test plan paso 0.6 (los `GunPresets[]` no deben contener la clase de `Knife` — evita un cuchillo acumulado por tier-up).
- **2026-06-14 — Venda + hotbar añadidos al alcance (confirmado con usuario en review R21)**: cada participante recibe además una **venda** (`BandageDressing`) y los 3 items se bindean al hotbar (arma→1, cuchillo→2, venda→3). Enmienda C3 y F2 (el inventario ya no es "exactamente ropa+arma+cuchillo"). Motivo: QoL de combate (curarse rápido). Pendiente `[verify in-game]`: que la venda no se duplique con la del `ClothingPreset` — si el preset ya trae venda, bindear esa en vez de spawnear (decisión diferida). Misma sesión: fix **GG-01v6** del respawn (equip en `OnClientNewEvent` post-`super`, no en `StartingEquipSetup`, que el vanilla se salta con `cfgplayerspawnpoints`) — sin cambio de criterio (sirve la Intent F/GG-01v5). Origen: review del diff 06-14, `reviews/codex-review-inbox.md`.
- **2026-06-14 — Crédito de kill "downed" (Opción A, confirmado con usuario)**: refina D1/F1. Si una víctima participante muere **sin killer reportado** (muerte scripted: jugador inconsciente que elige Respawn → `OnClientRespawnEvent` `SetHealth(0)`; o desangrado), se acredita el kill (+1 tier/victoria) al **último participante que la dañó** (el que la dejó inconsciente), con **ventana de reciencia de 120s** (`ATTACKER_CREDIT_WINDOW_MS`). Casos garantizados: (1) si otro **remata** con disparo real, el killer es válido → el rematador conserva el kill (el fallback NO lo pisa); (2) si la víctima hace click en respawn estando inconsciente, el crédito va al que la downeó. Generaliza el fallback de cuchillo existente (aditivo, sin tocarlo). `[verify in-game]`. Edge acotado: recuperarse del KO y morir al entorno en <120s acreditaría al último atacante.
