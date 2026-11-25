# CLAUDE.md — LFGungame

> Contexto específico del proyecto. Claude (Cowork) y Codex lo leen al entrar.
> Reglas globales del pipeline (roles, doble revisión, grill, R1-R26) NO van aquí:
> están en `%USERPROFILE%\ObsidianVault\AI\00_System\workflow.md` (autocargado).
> Memoria de proyecto en vault: `%USERPROFILE%\ObsidianVault\AI\10_Projects\LFGungame\`.

## Resumen
Mod de **GunGame** para DayZ. Se activa con el comando de chat LBmaster `!gungame`
(solo Admins). Los jugadores reciben una pregunta de unirse (SÍ/NO, 60s); los que
aceptan (hasta `MaxPlayers`) son **participantes**, el resto **público**. Al arrancar
se teletransporta a todos, se congela 10s con cuenta atrás, y empieza la progresión
por tiers de armas: cualquier kill que hagas te sube de nivel; si te matan con cuchillo
bajas uno. Gana quien consigue un kill estando en el tier máximo.

## Definición de Producto Final
**Qué es "terminado" → [`product-spec.md`](product-spec.md)** — contrato de aceptación
(reglas, criterios verificables R26, fuera de alcance R25, dependencias verificadas).
Leerlo antes de planificar cualquier fase. El Grill de plan (Modo B) exige que cada
fase trace a un criterio de ahí (workflow.md §"Grill — Modo A/B", gate DPF).
Cerrado por Grill de producto con el usuario el **2026-05-27**.

## Stack técnico
- Runtime: DayZ Enforce Script. Mod de **scripts puros** (sin assets 3D propios).
- Dependencias DURAS (verificadas en source montado):
  - `LBmaster_Groups` → comando de chat (`LBChatHandler`).
  - `LBmaster_Core` → presets de loadout (`LB_PresetLoader` / `LB_PresetBase`).
- Required addons: `DZ_Data`, `DZ_Scripts`, `LBmaster_Core`, `LBmaster_Groups`.
- PBO prefix: `LFGungame`.

## Estructura del repo
```
LFGungame/         (compilable, va al PBO)
├── $PBOPREFIX$        → "LFGungame"
├── mod.cpp            → metadata + dependencies de display
├── config.cpp         → CfgPatches (requiredAddons) + CfgMods (3 módulos de script)
├── inputs.xml         → keybind UALFGungameScoreboard (default kL, scoreboard toggle)
├── stringtable.csv    → localización (join prompt, scoreboard, winner)
├── gui/layouts/       → (pendiente) join prompt, countdown, scoreboard, tier HUD
└── scripts/
    ├── 3_Game/LFGungame/    → config JSON (struct + parsing + creación no-lazy)
    ├── 4_World/LFGungame/   → gestor de partida server-authoritative + hooks muerte/daño
    └── 5_Mission/LFGungame/ → comando !gungame (modded LBChatHandler) + GUI cliente
LFGungame_dev/     (este, NO va al PBO)
├── CLAUDE.md · AGENTS.md · product-spec.md
├── plans/ · decisions/ · reviews/ · research/ · tools/ · _backups/
```

## APIs verificadas (R2 — NO presuponer)
Ver detalle con `path:line` en el vault: `AI/10_Projects/LFGungame/verified-apis.md`.
- Comando: override `LBChatHandler::ChatCommandExists/HasPermission/OnChatCommand`
  (`LBmaster_Groups/scripts/5_Mission/LBmaster_Groups/System/LBChatHandler.c:22/36/52`,
  siempre llamando `super`). Permiso vía `LBAdmins.HasPermission(perm, sender)`
  (`LBmaster_Core/scripts/3_Game/LBmaster_Core/Config/LBAdmins.c:464`).
- Mensajes de chat: `LBChatHandler.SendSimpleChatMessage(PlayerIdentity, string, bool global)`.
- Presets: `LB_PresetLoader.Get.GetPreset("Nombre")`
  (`LBmaster_Core/scripts/4_World/LBmaster_Core/Config/LB_PresetLoader.c:146`) →
  `preset.SpawnPreset(player, parent, altPos, radius)` (`.../Struct/LB_PresetBase.c:138`)
  o `LB_PresetBase.SpawnPresets(player, LB_PresetLoader.Get.FindPresets(arr), parent)` (`:92`).

## Gotchas conocidos
- **Items de un preset LBmaster con `individualChance=false` (el default) entran en un
  pool del que spawnea UNO solo** (`LB_PresetBase.c:180-195`, `break` tras el elegido;
  default en `:4`). Para loadout determinista, CADA item hijo necesita
  `individualChance: 1` + `chance: 1.0` (tirada independiente por item, `:196-202`).
  Patrón arma lista para disparar (`LB_ItemPreset.c:7-13`): arma `spawnIn: "hands"`,
  cargador como hijo del item-arma `spawnIn: "mag"`, spares `spawnIn: "root"`.
  Aplica a GunPresets Y ClothingPreset. (Corregido 2026-06-11 tras test ciclo 1;
  la nota anterior "chance 1/100%" era insuficiente.)
- **NO editar `.py` directamente en OneDrive** (inyecta bytes nulos / trunca) → heredoc
  a `/tmp`, ejecutar, `cp` resultado (gotcha transversal del vault).
- **Daño one-hit del cuchillo**: el tuning de daño melee en DayZ puede requerir prueba
  in-game → criterio marcado `[verify in-game]` en el contrato.
- Comando `!gungame`: confirmar contra el source que el dispatcher de LBGroups usa
  prefijo `!` y que el `cmd` llega sin prefijo (patrón del ejemplo `!tp` → cmd "tp").

## Convención UI (2026-05-28 — actualizado)

**Framework**: **vanilla `UIScriptedMenu` + widgets nativos** (`FindAnyWidget`) para las 4
piezas. `JM_CF_Scripts` (Dabs MVC) sigue como dep low-cost pero NO se usa por defecto;
queda como fallback opcional si alguna pieza no funciona bien en vanilla.

**Dirección visual** (del kit `Gungame.zip` del usuario, recibido 2026-05-28):
- Estética **Tactical** (sin redondeo, esquinas en corchete `[ ]`).
- Paleta **ámbar Survivor**: `COL_ACCENT` `#E6A84B`, `COL_GOLD` `#F0C45C` (corona),
  `COL_TEXT/MUTED/DIM` para jerarquía gris cálida, `COL_PANEL` oliva 94% alpha,
  `COL_DANGER` rojo últimos 3s del countdown.
- Tipografía Chakra Petch / mono.
- **Bilingüe ES+EN visible simultaneamente**: cada label = `<TextWidget>` con
  `#STR_LFGUNGAME_*` (ES localizable) + segundo `<TextWidget>` con EN hardcoded debajo
  en gris atenuado.

**Reparto**:
- Lógica (`.c` con `extends UIScriptedMenu` o `Managed` para HUD): Codex.
- Layout `.layout` + tokens visuales: adaptación del kit (vanilla widgets, prefix
  `LFGungame_`, claves `#STR_LFGUNGAME_*`); el ajuste fino visual queda como design pass
  posterior con Claude (Cowork) + plugin `design` si hace falta refinar.
- Assets propios (crown.edds): pendiente diseño. Hasta que exista,
  `<ImageWidget Visible="false">` y el texto "1.º"/"1st" funciona como fallback (igual
  que el kit).

**Source de referencia**: kit vendored en `LFGungame_dev/_kit/Gungame/` (host-accesible,
generado por Claude Design según indicación del usuario 2026-05-28). 16 archivos: 4 layouts,
8 scripts (3 en 3_Game + 5 en 5_Mission), config + stringtable + textures README. Survey
durable + decisiones D-K1/D-K2/D-K3 en
`vault AI/10_Projects/LFGungame/research/2026-05-28-ui-kit-survey.md`.

Piezas pendientes de adopción: JoinDialog (correctivo S2.5), Countdown (S3), HUD
tier+corona (S6), Scoreboard (S7).

## Estado actual
> Fuente de verdad viva: **[`HANDOFF.md`](HANDOFF.md)** (header LIVE-STATE, inyectado por hook al arrancar sesión). Esta sección solo da el trazo grueso.
- 2026-06-10: **mod terminado OFFLINE + remediación aplicada en fuentes** (P7-P13 fin de ronda/scoreboard/HUD + clusters 1-4 de la auditoría preprod + WARN-1/3). Auditoría pre-test 2026-06-10: **GG-01 bloqueante** (respawn cableado al personaje viejo — hook `OnClientNewEvent` ausente) + GG-02..10 + F-25; PBOs desfasados. Última verificación in-game real: RPT 2026-06-02 (ciclo completo limpio).
- Siguiente: sesión de fix (prompt en `reviews/2026-06-10-prompt-sesion-fix.md`) → rebuild+sign+deploy → test in-game ciclo 1. Findings: `_review_flags/audit-pretest/2026-06-10-REPORTE-AUDITORIA-PRETEST.md`.

## Enlaces
- Contrato: `product-spec.md`
- Plans: `plans/` · Reviews: `reviews/codex-review-inbox.md`
- Memoria vault: `%USERPROFILE%\ObsidianVault\AI\10_Projects\LFGungame\`
  (brief, verified-apis, assumptions, validation-matrix, bug-ledger, decisions/, research/)
