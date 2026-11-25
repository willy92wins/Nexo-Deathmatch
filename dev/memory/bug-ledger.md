# Bug Ledger — LFGungame

Historia durable de bugs que no deben regresar.

## Severity Vocabulary

- `crash` · `exception` · `corruption` · `degradation` · `cosmetic` (ver definición en [`workflow.md`](../../00_System/workflow.md)/template).

## Entries

### 2026-06-10 — RV-1 · `StartGame()` borra pendings de cleanup ajenos a la ronda nueva (arma de tier filtrada al encadenar rondas)

- Severity: `corruption` (config-independiente, frecuencia baja — requiere encadenar rondas con un desconectado-vivo entre medias).
- Area: `LFGungame_RoundManager.c` — `StartGame()` `:173-175` (`m_PendingPublicCleanup.Clear()`, parte del fix F-02 v2) + path B6 de `OnPlayerReady` (late-join: ropa sin wipe, por contrato).
- Mecanismo (verificado por review adversarial Codex 2026-06-10 sobre el diff R1): participante se desconecta VIVO con loadout de tier mid-ronda → fin de ronda lo inserta en `m_PendingPublicCleanup` (ausente) → se arranca OTRA ronda antes de que vuelva → `StartGame()` hace `Clear()` → al reconectar durante la ronda nueva entra por B6 (entry nuevo PUBLIC, "no wipe, no teleport") → conserva el arma de gungame de la ronda anterior.
- Por qué no se parchea a ciegas: el `Clear()` existe deliberadamente (F-02 v2) para que un pending viejo no dispare wipe+teleport sorpresa en una ronda futura; quitarlo reabre eso, y el consume en VOTING haría `return` saltándose el prompt. El fix bueno necesita diseño (p.ej. consumir-on-StartGame los pendings de jugadores ONLINE y/o pending con id de ronda).
- Adjudicación usuario 2026-06-10: **flag post-test** — irrelevante para el test de evento con rondas controladas.
- Evidence: review Codex 2026-06-10 (RV-1, output sesión de fix); plan `LFGungame_dev/plans/2026-06-10-remediation-pretest.md`.
- Status: OPEN, deferred post-test.

### 2026-06-02 — `RULE_VOTE_ADMIN` queda como dead-ref tras la enmienda "admin puede participar"

- Severity: `cosmetic` / dead-ref (no bloquea compile ni runtime; `static const string` sin consumidor).
- Area: `scripts/3_Game/LFGungame/LFGungame_Constants.c:85`.
- Contexto: la enmienda "admin puede participar" (2026-06-02) borró el único uso de `RULE_VOTE_ADMIN` en `LFGungame_PlayerBase.c` (`LFGungame_HandleVoteRPC`, ex-`:118-122`): antes el RPC de voto rechazaba a los admin con `LFGungame_WarnRpc(RULE_VOTE_ADMIN)`; ahora los admin votan como cualquiera. La constante ([`Constants.c:85`](../../30_Research/redfalcon-rffs-heli/Core_scripts/3_Game/constants.c)) se conservó deliberadamente (R20 anti-refactor: no borrar en el mismo paso que el cambio de contrato).
- Verificación (host-direct, Grep tool): `RULE_VOTE_ADMIN` en `LFGungame/` = 1 ocurrencia (declaración [`Constants.c:85`](../../30_Research/redfalcon-rffs-heli/Core_scripts/3_Game/constants.c)), 0 usos. `IsAdminSteamId` sigue vivo (def + 4 ramas noop nuevas + `EnsurePublicEntriesForConnectedPlayers`).
- Fix sugerido: limpieza futura (no urgente) — borrar [`Constants.c:85`](../../30_Research/redfalcon-rffs-heli/Core_scripts/3_Game/constants.c) con grep negativo previo de `RULE_VOTE_ADMIN` en todo `LFGungame/`. NO tocar ahora (fuera de alcance de la enmienda, R25).
- Evidence: handoff [`30_Sessions/2026-06-02-LFGungame-enmienda-admin-participa-claude.md`](../../30_Sessions/2026-06-02-LFGungame-enmienda-admin-participa-claude.md); diff P1 de la enmienda.
- Status: OPEN, deferred (cosmético).

### 2026-06-02 - S5.5 housekeeping diferido: `m_CountLabel` / `m_CountLabelEN` son refs cacheadas sin uso script-side

- Severity: `cosmetic` / housekeeping (no bloquea compile ni runtime; las etiquetas existen en el layout y se muestran por widget).
- Area: `scripts/4_World/LFGungame/LFGungame_HUD.c`.
- Contexto: durante S5.5 se podaron `CountBar`, `RoleBadge`, `m_CountBar`, `m_RoleBadge`, `m_StartTotal` y `UpdateCountdownBar` por el cambio de contrato C5. El plan prohibia borrar `m_CountLabel` / `m_CountLabelEN` en este correctivo porque son scope de S6/design pass.
- Fix sugerido: en S6, revisar si `m_CountLabel` / `m_CountLabelEN` necesitan manipulacion runtime; si no, borrar los member refs y cache locals correspondientes con grep negativo de `CountLabel` / `CountLabelEN` en `.c` antes de tocar layout.
- Evidence: S5.5 plan `%USERPROFILE%\OneDrive\Documentos\DayZ Projects\LFGungame_dev\plans\2026-06-02-S5.5-correctivo-KB1-KB2.md`; post-S5.5 `LFGungame_HUD.c` conserva `m_CountLabel` / `m_CountLabelEN` por scope.
- Status: OPEN, deferred S6.

### 2026-05-27 — BUG-001 · `ResetStats()` se llama dentro de `EndRound()` (vacía stats antes de que S7 calcule ganador/scoreboard)

- Severity: `degradation` (pre-bug — detectado en R21 ligera S1 antes de que S7 lo manifieste in-game).
- Area: lifecycle de la ronda en `LFGungame_RoundManager`.
- Reproduction (proyectada para S7): al terminar una ronda (admin stop o RoundDuration), S7 debe calcular ganador (I1/I2), pintar scoreboard final (G3), y aplicar tiebreak `tier→kills→assists→deaths` (I2). Si `ResetStats()` se llama dentro de `EndRound()` antes de la transición, el map de stats ya está vacío cuando S7 lo lee → ganador indefinido y scoreboard vacío.
- Root cause: `EndRound()` invoca `ResetStats()` inmediatamente al entrar en ENDED (`LFGungame_RoundManager.c:154-155`, S1 impl 2026-05-27). El patrón correcto es que las stats sobrevivan ENDED y se limpien al volver a IDLE (siguiente `StartGame()` o callback del `m_EndStateTimer`).
- Fix: mover la llamada a `ResetStats()` de `EndRound()` al callback de transición ENDED→IDLE (el que dispara `m_EndStateTimer`). Mantener el reset en `StartGame()` (G1) es idempotente y correcto.
- Regression check: cuando S7 implemente `BuildScoreboard()`/anuncio de ganador, añadir test offline que (1) prepare stats no vacías, (2) llame `EndRound("test")`, (3) verifique que el scoreboard/ganador se computan con datos antes de que el timer ENDED→IDLE limpie.
- Evidence: `%USERPROFILE%\OneDrive\Documentos\DayZ Projects\LFGungame\scripts\4_World\LFGungame\LFGungame_RoundManager.c:154-155` (call-site original); review en `LFGungame_dev/reviews/codex-review-inbox.md` §"R21 ligera S1 — Claude 2026-05-27" P1-1.
- Fix delivery: mini-correctivo S1.5 (Codex), prompt en sesión 2026-05-27.
- **Status: RESOLVED 2026-05-27** vía correctivo S1.5. `ResetStats()` ahora solo se invoca en `StartGame()` (`LFGungame_RoundManager.c:114`) y en `ReturnToIdle()` (`:198`). Verificado por grep negativo en `EndRound()`. Regression check sigue pendiente hasta que S7 implemente `BuildScoreboard()`.

### 2026-05-31 — BUG-002 · `LFGungame_Constants.c` truncado por guardado corrupto (clase sin cerrar + 5 constantes perdidas) → compile fail módulo Game

- Severity: `corruption` (build-blocking: el módulo Game no compila → el mod no arranca).
- Area: `scripts/3_Game/lfgungame/LFGungame_Constants.c`.
- Síntoma: "Can't compile Game script module! ... lfgungame_constants.c(88): Opened scope at the end of file, missing '}'".
- Root cause: archivo truncado a media línea (l.88 = `    st`), dejando la clase `LFGungame_Constants` (abierta en l.23) sin cerrar al EOF. El truncado además borró 5 constantes usadas en `LFGungame_RoundManager.c`: `RULE_KILL_RECORDED`, `RULE_DEATH_RECORDED`, `RULE_KNIFE_DEATH_RECORDED`, `RULE_ASSIST_RECORDED`, `RULE_RESPAWN_EQUIPPED`.
- Fix: restauradas las 5 constantes (l.88-92) + cierre de clase `}` (l.93). Valores reconstruidos por convención del archivo (valor = nombre sin prefijo `RULE_`), sin git/backup → ver [`assumptions.md`](assumptions.md) 2026-05-31. Verificado vía Read host (API Windows): clase cerrada, 0 referencias `LFGungame_Constants.*` sin definir.
- Ítems abiertos → **RESUELTOS en revisión independiente 2026-05-31** (host-direct, R2):
  - (a) **valores `RULE_*` reconstruidos**: inocuos. `LogRule()` (`LFGungame_RoundManager.c:1681`; gemelo `LFGungame_Config.c:511`) usa `ruleId` solo como clave de rate-limit (`:1692,1700`) + etiqueta en `Print()` al RPT (`:1702-1707`); nunca se parsea ni compara. `RoundManager.c:519` llama `LogRule("TIER_CHANGED", …)` con literal no-constante → free-form. Sin consumidor externo (grep 0 matches de los 5 strings en `LFGungame_dev`; sin scripts `.py/.ps1/...`). Valores siguen la convención del archivo (reglas no-`E_CFG` = nombre sin `RULE_`). El string exacto NO afecta corrección. Ver [`assumptions.md`](assumptions.md) (resuelto 2026-05-31).
  - (b) **enum `NONE` duplicado**: benigno. Enforce = enums con scope, acceso solo cualificado (`EnumName.MEMBER`); "multiple declaration" aplica a vars locales en bloque, no a miembros homónimos de enums distintos (sintaxis oficial BI). Código usa 11 refs cualificadas, 0 bare. NO es "already defined".
- Compile-readiness (revisión 2026-05-31): clase cierra en `lfgungame_constants.c:93`; balance de llaves 4/4; las 76 refs `LFGungame_Constants.*` de los 12 `.c` resuelven a miembros definidos. Truncado **aislado** a este archivo (12 `.c` verificados host-direct cierran limpios). NOTA: el mount bash sirvió vistas stale del proyecto entero (refuerza gotcha OneDrive); autoridad = Read/Grep host.
- Verificación pendiente (usuario): recompilar módulo Game + leer RPT. No ejecutable desde Cowork; static-clean no garantiza otros módulos.
- Evidence: handoff [`30_Sessions/2026-05-31-LFGungame-constants-truncation-fix.md`](../../30_Sessions/2026-05-31-LFGungame-constants-truncation-fix.md) + revisión independiente 2026-05-31 (esta sesión).
- **Status: FIX VERIFICADO offline (revisión 2026-05-31); pendiente confirmar compile in-tool del usuario.**

### 2026-05-31 — BUG-003 · Cadena de compile errors tras BUG-002 (violación de capa 3→4 + colisión de nombre con clase vanilla)

- Severity: `corruption` (build-blocking: dos módulos no compilaban en cadena).
- Area: `scripts/3_Game/.../LFGungame_Config.c` y `scripts/4_World/.../LFGungame_RoundManager.c`.
- (a) **Módulo Game**: `LFGungame_Config.c:258 ValidatePresetField` referenciaba `LB_PresetLoader` (clase **4_World** de LBmaster) desde **3_Game** → "Can't compile Game script module! ... Can't find variable 'LB_PresetLoader'". La visibilidad de capa la fija **dónde se DEFINE** el método, no el caller (lo llama `RoundManager.StartGame` 4_World vía `Validate()`). Fix: quitados los 2 bloques que usaban `LB_PresetLoader.Get`/`.GetPreset()`; conservado el empty-check; el spawn en `RoundManager.c:751-760 / 1212-1237` ya null-checkea presets. **Cambio de comportamiento**: preset inexistente ya no bloquea `StartGame` (degradación silenciosa al spawnear); fail-closed en 4_World queda como follow-up opcional.
- (b) **Módulo World**: método propio `LogManager(string detail)` (`RoundManager.c:1673`) colisionaba con la **clase vanilla `LogManager`** → "Too many parameters for 'LogManager' method" en cada call-site. Fix: renombrado a `LogRound` (13 ocurrencias: 1 def + 12 calls). Verificado host-direct: 0 `LogManager` / 13 `LogRound`.
- Barrido de capas (a petición del usuario): 3_Game no referencia 4/5; 4_World no referencia 5 (solo un comentario de prior-art). Limpio.
- Evidence: compile errors in-tool del usuario; handoff [`30_Sessions/2026-05-31-LFGungame-compile-chain-y-crash-ui.md`](../../30_Sessions/2026-05-31-LFGungame-compile-chain-y-crash-ui.md).
- **Status: RESUELTO 2026-05-31 (ambos). Pendiente confirmar compile completo del usuario.**

### 2026-05-31 — BUG-004 · Crash al entrar: `CreateWidgets` sobre layouts inexistentes (HUD + JoinMenu)

- Severity: `crash` (CTD al entrar; el mod no es jugable).
- Area: `scripts/4_World/.../LFGungame_HUD.c` + `LFGungame_JoinMenu.c`; falta `LFGungame/gui/`.
- Síntoma: callstack RPT `Class 'LFGungame_HUD' Function 'LFGungame_HUD'` → `lfgungame_hud.c:34` `CreateWidgets("LFGungame/gui/layouts/lfgungame_hud.layout")`, llamado desde `LFGungame_MissionGameplay.OnInit:8`.
- Root cause: NO existe la carpeta `gui/` ni ningún `.layout` en el mod compilable. `CreateWidgets` con un layout inexistente **revienta dentro de la llamada nativa** (el `if(!m_Root)` posterior nunca se ejecuta). `JoinMenu` (`join.c:36`, `lfgungame_join.layout`) tiene el mismo crash latente al abrir el voto. El código de UI se mergeó sin adoptar los layouts (S6/S7 pendiente); nunca se había entrado in-game (validation-matrix toda pendiente).
- Recursos: kit en `LFGungame_dev/_kit/Gungame/gui/layouts/gungame_*.layout` (4 layouts), sin adoptar. El kit join layout ya tiene `BtnYes/BtnNo/BtnYesBg` + los SetText targets que `JoinMenu.c` espera; falta `CountdownText`; claves `#STR_GG_*` ≠ `#STR_LFGUNGAME_*`. HUD necesita textura `crown.edds` (ausente → `Bad texture ''`/`AlphaMaskTexture ''` en el RPT).
- Fix: **diferido** a sesión dedicada de UI (prompt en `LFGungame_dev/reviews/2026-05-31-prompt-sesion-ui-completa.md`). Opción B (adoptar layouts del kit) confirmada por el usuario.
- **UPDATE 2026-05-31 (sesión UI, Claude) — hallazgo de FORMATO**: entre el handoff y esta sesión aparecieron 2 layouts (`lfgungame_hud.layout`, `lfgungame_join.layout`) en el compilable, pero en **formato XML** (`<GUI><class type=...>`) — el del kit-mockup, NO el **Enfusion brace** (`WidgetClass Name { props { children } }`) que parsea el `CreateWidgets` nativo de DayZ. Confirmado contra 3 fuentes: [`aim.layout`](../BRDK_BRDM_2/research/extracted/addon/layout/aim.layout) real extraído de `BRDK_BRDM_2` (brace), el parser del proyecto `DayZ_UI_Research` (opera sobre brace), y la skill `dayz-ui-development/references/layout-format.md` ("`.layout` files use Enfusion format, NOT XML"). Los **nombres de widget y claves `#STR`** del XML SÍ eran correctos (coincidían con los `.c`), pero el **formato** habría reventado igual → el "fix" parcial no resolvía el crash, solo movía el punto de fallo. **Trampa durable**: la verificación de reconciliación nombres/claves sale VERDE con el formato equivocado; hay que verificar el formato aparte.
- **Fix aplicado (esta sesión)**: convertidos los **4** layouts a Enfusion brace (`lfgungame_hud` + `lfgungame_join` sobrescritos; `lfgungame_scoreboard` + `lfgungame_scoreboard_row` creados desde el kit). Reglas skill: `{ }` por widget, 4 exact flags (`hexact/vexact pos/size 0` = proporcional), `ImageWidgetClass` para fondos visibles (no `PanelWidget`, invisible), sin `imageTexture crown.edds` (`CrownIcon visible 0` + texto fallback `1st`). Añadidas claves `#STR_LFGUNGAME_RANK_FIRST` + `#STR_LFGUNGAME_SCOREBOARD_TITLE`. Escritura `wb+fsync` (OneDrive-safe), verificación host-direct.
- **Verificación estática GREEN (R26, offline)**: (1) ambos `CreateWidgets` paths → archivos existentes en brace; (2) las 22 búsquedas `FindAnyWidget` de HUD(8)+Join(14) resuelven a widgets presentes; (3) los 11 `#STR` de layouts + los 2 runtime de `.c` (`ROLE_PARTICIPANT/PUBLIC`) existen en stringtable; (4) brace-balance OK (20/24/17/11), 0 chars XML; (5) `gui/` empaqueta solo (`$PBOPREFIX$=LFGungame`, `config.cpp` no registra `.layout`).
- **Follow-ups (NO bloquean el crash)**: (1) rellenos de panel (`Dim/ModalPanel/Btn*Bg/BoardPanel/RowBg`) NO renderizan color hasta que el `.c` haga `LoadImageFile("#(argb,8,8,3)color(1,1,1,1,CO)")`+`SetColor` o se shippee textura pixel → **design pass** (lane Codex/Claude-design). El join in-game será FUNCIONAL (labels visibles, botones clicables, countdown) pero sin fondos. (2) `DescText` sin wrap (token `multiline` omitido por no verificado). (3) `RowsContainer` sin token `Vertical`. (4) Scoreboard sin `.c` consumidor hasta **S7** (archivos pre-stage; nombres de widget preservados para que el futuro `LFGungame_Scoreboard.c` los encuentre). (5) `crown.edds` pendiente diseño.
- **R21 estructural (Codex, 2026-05-31) — VEREDICTO apto, 0 P1**: los 4 layouts en brace Enfusion, sin XML residual, llaves balanceadas, sin NUL/truncado, paths case-exact; HUD/JoinMenu resuelven todos sus widgets. Verificado por paridad contra copia local unpackeada (`%USERPROFILE%\OneDrive\Documentos\DayZ Projects\gui\layouts`), [`aim.layout`](../BRDK_BRDM_2/research/extracted/addon/layout/aim.layout) (BRDK_BRDM_2) y mods reales locales (LFPowerGrid); `P:\` no montado en su entorno. Inbox: `LFGungame_dev/reviews/codex-review-inbox.md`. Hallazgos ruteados:
  - **P2 → S7** (no se arregla ahora; el `.c` no existe aún): al portar `GungameScoreboard.c` del kit, `RowBg` es ahora `ImageWidgetClass` pero el kit lo castea a `PanelWidget` (`PanelWidget.Cast` → null → no aplica el highlight del líder). Fix correcto en S7: `ImageWidget.Cast(row.FindAnyWidget("RowBg"))` + `LoadImageFile("#(argb,8,8,3)color(1,1,1,1,CO)")`+`SetColor`. NO revertir el layout a `PanelWidget`: PanelWidget es invisible (skill), no renderizaría el color igualmente.
  - **P3 → design pass / backlog cosmético**: `DescText` perdió el `multiline` del XML; no rompe carga pero puede cortar texto visualmente. En el design pass: añadir el token verificado contra vanilla, o cambiar `DescText` a `RichTextWidgetClass` (wrap nativo).
- **In-game 2026-05-31 (RPT `DayZ_x64_2026-05-31_17-59-14`)**: los layouts LFGungame **cargaron sin error** (0 ocurrencias de "Cannot open layout LFGungame/..."; comparar LFPowerGrid que SÍ falló su layout en l.505-506). El mod corrió >30s in-world. **El crash original de BUG-004 (HUD CreateWidgets sobre layout inexistente/XML) está RESUELTO in-game.** Hubo un CTD nativo a los ~30s del spawn (`18:00:23 ENGINE (F): Crashed`; Fault address `Unknown module`; world deerisle) pero **SIN firma LFGungame**: 0 frames LFGungame en cualquier callstack. Las únicas excepciones de script del log son de OTROS mods: BaseBuildingPlus `missiongameplay.c:333` (GetHumanInventory sobre player null en OnKeyPress, ×116) y DayZExpansion Core `dayzgame.c:24` OnUpdate. Footprint LFGungame en todo el RPT = solo carga de [`inputs.xml`](../BRDK_BRDM_2/research/extracted/addon/inputs.xml) (l.22) + registro de addon (l.4480). **Atribución del CTD: NO determinable como LFGungame desde este RPT** (R31/R35: native fault, sin callstack de script, sin nuestra firma); patrón consistente con inestabilidad de modpack (DeerIsle+Expansion+BBP, lleno de errores pre-existentes). Discriminadores pendientes con usuario: (1) ¿se lanzó `!gungame` o solo spawn? (sin logs LFGungame → probable que la ronda nunca arrancó → nuestra lógica idle); (2) ¿crasheaba el modpack antes de añadir/actualizar LFGungame?; (3) test decisivo: quitar LFGungame y entrar → ¿sigue crasheando? y/o LFGungame + solo LBmaster en mapa vanilla.
  - **Finding para el design pass (panel fills)**: la textura procedural `#(argb,8,8,3)color(1,1,1,1,CO)` **falla en 1.29** ("Bad texture name" / `LoadImageFile can't load`, l.497-504 — observado en LFPowerGrid). El recipe canónico de la skill `dayz-ui-development` para fondos de color NO sirve tal cual en esta versión/engine; el design pass de los rellenos necesitará textura pixel shippeada (.edds) o un formato alternativo verificado in-game. (Candidato a SP — skill-patch.)
- **Audit de código always-on cliente (2026-05-31, sin ciclo de usuario)**: usuario confirma modpack **estable antes** + **no lanzó `!gungame`** → lógica de ronda idle. Lo único de LFGungame que corre en cliente sin ronda = HUD pasivo (cargó OK, OnUpdate no-op sin countdown) + 3 `RegisterNetSyncVariableInt` en `PlayerBase()` (en constructor = correcto per enforce-script-reference) + parse de config. `LFGungame_RoundManager` **NO se instancia en cliente** (`GetInstance` gated `IsDedicatedServer`, `RoundManager.c:90-103`); `EEKilled/EEHitBy` están `#ifdef SERVER` (fuera del build cliente); `MissionServer` es server-only; `OnRPC` idle sin ronda (RPCs no llegan). **No se halló mecanismo de CTD nativo en nuestro código.** Sin límite de syncvars documentado en el skill (la única regla es timing en constructor, que cumplimos). Hipótesis (NO confirmada, R35): perturbación del modpack frágil al añadir addon + modded PlayerBase/MissionGameplay + 3 syncvars, aflorando un crash latente ajeno; alternativa: coincidencia (muestra "estable" pequeña). **Discriminador decisivo pendiente: test de aislamiento (quitar LFGungame → entrar; luego LFGungame+solo LBmaster en mapa vanilla).** Artefacto extra disponible: minidump `%USERPROFILE%\AppData\Local\DayZ\DayZ_x64_2026-05-31_18-00-23.mdmp`.
- **Status: layouts VALIDADOS in-game (cargan sin crash; crash original BUG-004 resuelto). CTD nativo separado a los ~30s SIN firma LFGungame y SIN mecanismo hallado en nuestro código → probable perturbación/coincidencia de modpack, pendiente test de aislamiento. Follow-ups P2/P3 + finding procedural-texture ruteados.**

### 2026-06-02 — BUG-005 · Compile fail módulo Game ("quoted string not closed" en constants.c); BOM sospechado, diagnóstico inicial por bash STALE (corregido)

- Severity: `corruption` (build-blocking reportado por el usuario). Hermano de BUG-002 (mismo archivo/módulo, otra causa).
- Síntoma: "Can't compile Game script module! scripts/3_Game/gui/spacers/autoheightspacer.c(130): CParser: quoted string not closed on line 1, module LFGungame/scripts/3_Game/lfgungame/lfgungame_constants.c" (autoheightspacer.c = vanilla, red herring del cascade).
- **Diagnóstico inicial erróneo en MÉTODO**: escaneo byte-level por **bash** halló "BOM en 4 archivos S0". Pero el mount bindfs estaba **STALE** (reportó Constants.c 88 l / 3978 B; el host Read-tool = 93 l intactas, clase cerrada). El de-BOM por bash escribió a la copia divergente del mount → **NO-OP sobre el host**. Ver LL-084 + gotcha bindfs. (El vault YA documentaba este stale en [`cowork-entorno-y-tooling-gotchas.md`](../../20_Knowledge/cowork-entorno-y-tooling-gotchas.md) 2026-05-31; no lo releí antes de fiarme de bash.)
- Estado real verificado host-direct (Read tool): [`Constants.c`](../../30_Research/redfalcon-rffs-heli/Core_scripts/3_Game/constants.c) (93 l, clase cerrada, 5 const S5 OK), `Config.c` (528 l, intacto), `config.cpp` (65 l), [`mod.cpp`](../DayZ_Vehicle_Skill/outputs/test_kt/mod.cpp) — TODOS intactos, sin string sin cerrar ni truncación visibles. NO se pudo confirmar host-direct si hay BOM (Read decodifica).
- Acción correctiva (host-direct, Write tool): reescritos [`mod.cpp`](../DayZ_Vehicle_Skill/outputs/test_kt/mod.cpp) y `LFGungame_Constants.c` verbatim (garantiza sin BOM en el archivo nombrado por el error). `config.cpp` y `Config.c` Read-verificados intactos (no reescritos por riesgo de transcripción de 17 KB; el rebuild los testea).
- Verificación pendiente (usuario = GROUND TRUTH): rebuild del módulo Game. Si compila → resuelto. Si vuelve a fallar (mismo o nuevo archivo/línea) → diagnóstico host-direct en la sesión nueva (Read de cada `.c` del módulo; NADA de bash para estos archivos).
- Evidence: screenshot del usuario 2026-06-02; handoff [`30_Sessions/2026-06-02-LFGungame-cierre-y-bug-backlog.md`](../../30_Sessions/2026-06-02-LFGungame-cierre-y-bug-backlog.md).
- **Status: RESUELTO 2026-06-02.** El rebuild del usuario cargó Game/World/Mission **limpio** (RPT `DayZServer_x64_2026-06-02_15-30-35`: 0 "Can't compile"; el ciclo de ronda corrió entero `voting`→`active`→`freeze`→`combat`, líneas 90027/92958/94052). El de-BOM host-direct ([`mod.cpp`](../DayZ_Vehicle_Skill/outputs/test_kt/mod.cpp)+[`Constants.c`](../../30_Research/redfalcon-rffs-heli/Core_scripts/3_Game/constants.c)) resolvió el compile-fail; el archivo/línea del error (`autoheightspacer.c`) era red herring del cascade. Método confirmado: Read/Write host (no bash) para OneDrive (LL-084).

### 2026-06-03 — Revisión independiente de P9–P11 + debug: 1 P1 + 4 P2/P3 (TODOS arreglados en la misma sesión)

- Severity: P1 = `degradation`/contract-gap; resto `degradation`/`cosmetic`. Ninguno llegó al test (arreglados offline).
- Contexto: subagente Claude general-purpose (Codex R21 se atascó >1h en `findstr` recursivos → cancelado) revisó el batch scoreboard/HUD/leader-sync/E2.
- **P1-1 scoreboard sin gate** (`RoundManager.OnScoreboardRequest` + `MissionGameplay.LFGungame_ToggleScoreboard`): cualquier jugador lo abría y el server servía la tabla a público/sin-ronda (refresco-vacío-eterno). FIX: gate cliente (`m_GG_Role==PARTICIPANT`) + server (`m_State==ACTIVE` + sender participante).
- **P2-1 cuchillo one-hit sin crédito** (asimétrico): `SetHealth(0)` → `EEKilled(killer=null)` (verificado: [`playerbase.c`](../../30_Research/redfalcon-rffs-heli/Core_scripts/4_World/PlayerBase.c) CheckDeath→SimulateDeath) → matador sin +1 PERO víctima −1. FIX: `lastKnifeKillerId` guardado en `OnPlayerDamaged`, recuperado en `OnPlayerKilled` cuando killer=null. Cubre +1 y victoria.
- **P2-3 rank scoreboard = orden de inserción del map** → selección-sort server-side por `IsBetterByTiebreak` (rank 1 = líder).
- **P3-1** nulls de los widgets nuevos en `HUD.Destroy`. **P3-2** scoreboard overlay pasivo (`UseMouse/UseKeyboard` false → no congela movimiento, el toggle L rebindeable funciona).
- **P2-4 perf O(P²)** `RecalculateLeaders` en `BeginRound`: ACEPTADO (≤15 jugadores, trivial, mantiene la R7-safety de recalcular en cada `SetTierSync`).
- Confirmado OK por la revisión: orden RPC Write/Read alineado, bitstream NetSync, bus payload, CLOSE_DIALOG simétrico, 0 violaciones Enforce.
- Evidence: handoff [`30_Sessions/2026-06-03-LFGungame-finish-offline-P7-P13.md`](../../30_Sessions/2026-06-03-LFGungame-finish-offline-P7-P13.md).
- **Status: TODOS arreglados y verificados offline 2026-06-03. Pendiente confirmación in-game (E2 one-hit + atribución = único `[verify in-game]`).**

### 2026-06-10 — Auditoría pre-test (Claude/Fable): GG-01 respawn cableado al personaje VIEJO + 9 findings nuevos + F-25 huérfano del plan

- Severity: GG-01/GG-02 `corruption` (Alta) · GG-03/GG-05 `degradation` · GG-04 `corruption` (Media) · GG-06/07/08 `cosmetic` · GG-09 `corruption`-edge (Baja) · GG-10 operativo show-stopper.
- Area: `LFGungame_MissionServer.c` (wiring de eventos) + `LFGungame_RoundManager.c` (respawn/pending/votación) + build/deploy.
- **GG-01 (la raíz):** todo el pipeline de respawn (`OnPlayerRespawn` — F1/F2 mid-round y consumo F-02 post-ronda) corre desde `OnClientRespawnEvent`, que en vanilla entrega el personaje PRE-respawn (`missionserver.c:598-609` mata al unconscious "choosing to respawn"); el personaje NUEVO nace en `OnClientNewEvent` (`:536` CreateCharacter), NO hookeado. Prior art: HM_Starter_Kit (`alwaysGiveKitOnRespawn` en OnClientNewEvent) + LBmaster Spawn-Loadouts. Confirmado por agente adversarial con mandato de refutarlo. Pasó la auditoría preprod (8 Opus + cross-actor) y 2 reviews Codex porque todos verificaron la FIRMA del evento, no la SEMÁNTICA del parámetro (→ LL-131).
- **GG-02:** violación R7 del fix F-02 v2 — `m_PendingPublicCleanup` se consume en OnPlayerRespawn/:914-927 y OnPlayerReconnect/:1089-1105 pero NO en OnPlayerReady → rejoin vivo post-ronda conserva el loadout de tier permanentemente.
- **F-25 sin clasificar:** los grupos A–D del remediation-plan 06-07 cubren 26/27 findings; F-25 (cuchillo dentro de GunPreset se acumula por tier-up) quedó fuera. Sigue vigente (`ShouldDeleteWeaponLoadoutItem` preserva todo `IsKindOf(Knife)`). Tratamiento mínimo: regla de config (GunPresets sin la clase del Knife).
- **GG-10:** PBO de `LFGungame\` (06-05) y `TEMP\LFGungame_prod` (06-02) NO contienen los fixes de la remediación (grep binario negativo de `IsEntryOnline`/`m_PendingPublicCleanup`/`LogPlaceholder`). Rebuild+sign+deploy obligatorios pre-test.
- Verificado en positivo: WARN-1/WARN-3 de Codex arreglados en fuentes; clusters 3-4 aplicados (F-20 no); matriz contrato 28/33 OK offline.
- Evidence: `LFGungame_dev\_review_flags\audit-pretest\2026-06-10-REPORTE-AUDITORIA-PRETEST.md` (fuente única) + handoff [`30_Sessions/2026-06-10-LFGungame-auditoria-pretest.md`](../../30_Sessions/2026-06-10-LFGungame-auditoria-pretest.md).
- Status: **OPEN — todos.** Plan de remediación = próxima sesión (prompt en `LFGungame_dev\reviews\2026-06-10-prompt-sesion-fix.md`; clusters R1=GG-01+02+03, R2=GG-04+05, R3=cosméticos).

### 2026-06-14 — Auto fire-mode forzado desincroniza armas multi-modo

- Severity: `degradation` (el arma se "buguea" para algunos jugadores; no crash). Reportado in-game por el usuario.
- Area: `LFGungame_RoundManager.MakeWeaponReadyAuto` (server: bucle `SetMuzzleMode` + RPC `SET_WEAPON_AUTO`) + `LFGungame_PlayerBase` (handler cliente).
- Mecanismo (sub-agente general-purpose 2026-06-14, con citas): el fire-mode se sincroniza SOLO por el evento FSM `WeaponEventSetNextMuzzleMode`→`SyncEventToRemote` (`weapon_base.c:297`). Forzarlo con raw `SetMuzzleMode` independiente en server Y cliente (+ ciclar modos en arma recién spawneada) deja server/cliente divergentes en modo/burst en armas multi-modo (semi/burst/auto). Armas de 1 modo (pistola/bolt) caen en modo 0 y coinciden → no se buguean. `Synchronize()` NO lleva el modo.
- Fix: **rollback total del auto** (opción B, decidido con el usuario). El fix correcto (evento FSM server-only) queda documentado en verified-apis pero NO implementado.
- Evidence: handoff [`30_Sessions/2026-06-14-LFGungame-rollback-auto-chamber-hotbar-loc-respawn.md`](../../30_Sessions/2026-06-14-LFGungame-rollback-auto-chamber-hotbar-loc-respawn.md).
- Status: RESUELTO 2026-06-14 vía rollback (auto descartado).

### 2026-06-14 — Regresión de respawn: nace en spawn vanilla + teleport (no en el punto de GunGame)

- Severity: `degradation` (lagazo + doble carga; el respawn GG-01v5 que ya funcionaba dejó de nacer en el punto de GunGame).
- Area: el bind de hotbar/chamber SÍNCRONO (añadido 06-14) corría dentro de `EquipParticipantAtBirth`→`EquipPlayer` durante `MissionServer.StartingEquipSetup` (creación del personaje).
- Mecanismo (HIPÓTESIS, fuerte correlación temporal — el respawn iba bien ANTES de añadir el hotbar): una op síncrona de quickbar/chamber durante la creación rompe el birth-equip → `EquipParticipantAtBirth` no llega a marcar `m_GG_BirthEquipped` (`RoundManager.c:2125`) → `bornEquipped=false` → `OnPlayerRespawn` ejecuta el fallback viejo (teleport+wipe+equip, `:1055-1062`). El flujo GG-01v5 (OnClientNewEvent birth-pos + StartingEquipSetup) NO se tocó.
- Fix: **diferir todo** (chamber + binds) fuera de la creación vía `CallLater(...,0)`.
- Evidence: report del usuario 2026-06-14; handoff de la sesión.
- Status: FIX OFFLINE 2026-06-14, **PENDIENTE confirmar in-game** (es hipótesis; si persiste = mirar RPT de un respawn).

### 2026-06-14 — Algunas armas salen sin chamberear en kills (en vivo)

- Severity: `degradation` (intermitente; el arma del tier-up no siempre queda chambeada).
- Area: `MakeWeaponReady` chambeaba SÍNCRONO justo tras `SpawnPreset`.
- Mecanismo: en un kill en vivo (killer mid-combat: `OnPlayerKilled`→`ChangeTier`→`ApplyTierLoadout`→`WipeWeaponsOnly`→`SpawnPreset`→chamber), el FSM del arma recién intercambiada está transitorio cuando corre `FillChamber` → a veces no toma. El knife-kill SÍ pasa por esta lógica (killer +1 vía `ChangeTier`, igual que un kill normal; la víctima −1 recibe su arma en el respawn).
- Fix: diferir el chamber a `GG_ReadyWeapon` (siguiente tick, FSM estable).
- Evidence: report del usuario 2026-06-14; handoff.
- Status: FIX OFFLINE 2026-06-14, **PENDIENTE in-game**.

### 2026-06-14 — Clientes inglés ven `STR_...` crudo (countdown/HUD/corona; pantalla join)

- Severity: `cosmetic` (texto sin localizar; no funcional).
- Area: HUD (`LFGungame_HUD.c`) + (reportado) pantalla join.
- Mecanismo: el `#STR` baked en `text` de la `.layout` NO resuelve en el HUD-workspace (`workspace.CreateWidgets`), solo en menús (`UIScriptedMenu`). RoundTag/CountLabel/rol/Arma/corona dependían del baked → clave cruda. (El JoinMenu usa `SetText("#STR_")`, que SÍ resuelve.)
- Fix HUD: resolver por script con `Widget.TranslateString` (enwidgets.c:112) en CacheWidgets/UpdateTier.
- Join `STR_` (un amigo, cliente inglés): diagnóstico = lo más probable MOD DESACTUALIZADO en su cliente (clave cruda = ausente; DayZ casa columnas del stringtable por NOMBRE — el orden no produce clave cruda; el JoinMenu usa `SetText("#STR_")` que vanilla resuelve). Acción: re-descargar. Si un cliente inglés ACTUALIZADO sigue viéndolo → reordenar stringtable.csv a canónico (NO hecho).
- Evidence: captura del usuario 2026-06-14; handoff.
- Status: HUD = FIX OFFLINE (pendiente in-game). Join = OPEN (probable cliente desactualizado, sin confirmar).


## NDM-01 · `MaxPlayers` se valida pero ya no limita nada (2026-11-25, abierto, sin decidir)

- Estado: abierto. No se toca hasta decidir.
- Desde F1 (se quitaron el voto y el rol de público), `MaxPlayers` solo se lee y se valida (≥ 2) en
  `NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c` (validación ~:190, getter ~:303). Ningún camino de la ronda limita con él a los
  participantes; antes el límite lo ponía el voto de LFGungame.
- Consecuencia: el config por defecto (`MaxPlayers: 15`) no impide rondas de más de 15. Por eso F6 no compara
  `MinPlayersToScore` con `MaxPlayers` (decisión del 25-nov).
- A decidir: quitar `MaxPlayers` del config o volver a usarlo para limitar la entrada a la ronda.