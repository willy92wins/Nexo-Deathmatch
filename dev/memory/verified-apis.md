# Verified APIs — LFGungame

Solo APIs verificadas contra source real (montado en `...\DayZ Projects\`) o doc oficial.
Verificado el **2026-05-27** contra el source de LBmaster montado.

## Entries

- API: `LBChatHandler.ChatCommandExists(string cmd)` / `HasPermission(PlayerIdentity sender, string cmd)` / `OnChatCommand(PlayerIdentity sender, string cmd, TStringArray args)`
  Source: `LBmaster_Groups/scripts/5_Mission/LBmaster_Groups/System/LBChatHandler.c:22 / :36 / :52`
  Verified: 2026-05-27
  Scope: client+server (5_Mission)
  Correct use: `modded class LBChatHandler` que sobreescribe los **tres** métodos; cada uno llama `super`. `ChatCommandExists` devuelve true para `"gungame"`; `HasPermission` decide quién puede; `OnChatCommand` ejecuta. El `cmd` llega **sin** el prefijo `!` (patrón del ejemplo `!tp` → `cmd=="tp"`).
  Caveats: hay que sobreescribir los 3 o no funciona. `[verify]` que el dispatcher usa prefijo `!`.
  Example caller: chat in-game `!gungame`.

- API: `LBAdmins.HasPermission(string permission, PlayerIdentity player, bool cannotUseGrantAllPermissions = false)`
  Source: `LBmaster_Core/scripts/3_Game/LBmaster_Core/Config/LBAdmins.c:464` (overloads `Man`/`string` en :455/:472; `HasPermissionActive` en :443/447/451)
  Verified: 2026-05-27
  Scope: shared
  Correct use: comprobar permiso de admin para un comando. Para LFGungame el gate elegido es **SteamID ∈ `Admin[]` del config**; `LBAdmins.HasPermission` queda como alternativa si se quiere usar el sistema de permisos LBmaster.
  Caveats: decidir en fase 1 cuál de los dos gates se usa (config Admin[] vs permiso LBmaster). El contrato fija Admin[].

- API: `LBChatHandler.SendSimpleChatMessage(PlayerIdentity player, string message, bool global = false)`
  Source: `LBmaster_Groups/scripts/5_Mission/LBmaster_Groups/System/LBChatHandler.c` (método `static`, ~:66)
  Verified: 2026-05-27
  Scope: server (envía RPC de chat)
  Correct use: enviar mensaje a un jugador (`player` + `global=false`) o a todos (`player=null` + `global=true`). Útil para "partida llena" y anuncio de ganador.
  Caveats: el mensaje debe llevar un `+` delante (lo añade internamente con `"+" + message`).

- API: `LB_PresetLoader.Get.GetPreset(string name)` → `LB_PresetBase`
  Source: `LBmaster_Core/scripts/4_World/LBmaster_Core/Config/LB_PresetLoader.c:146` (`GetPresetAt` en :132; `FindPresets(TStringArray)` en :189)
  Verified: 2026-05-27
  Scope: 4_World (server para spawnear loadouts)
  Correct use: `LB_PresetBase p = LB_PresetLoader.Get.GetPreset("NombrePreset");` → null si no existe. `FindPresets(arr)` resuelve una lista de nombres a `array<LB_PresetBase>`.
  Caveats: comprobar null antes de usar.

- API: `LB_PresetBase.SpawnPreset(PlayerBase player, EntityAI parent, vector altPos = vector.Zero, float radius = 0.0)` y `static LB_PresetBase.SpawnPresets(PlayerBase player, array<LB_PresetBase> presets, EntityAI parent, vector altPos = vector.Zero, float radius = 0.0)`
  Source: `LBmaster_Core/scripts/4_World/LBmaster_Core/Struct/LB_PresetBase.c:138` (SpawnPreset) y `:92`/`:96` (SpawnPresets, overloads)
  Verified: 2026-05-27
  Scope: server
  Correct use: spawnear un preset (ropa o armas) sobre el jugador. `player` para quickbar/items en manos; `parent` = contenedor (puede ser el propio player). Para varios: `SpawnPresets(player, LB_PresetLoader.Get.FindPresets({"P1","P2"}), parent)`.
  Caveats: las *chances* aplican a `SpawnPresets` *plural* (`:96`) y a *linked sub-presets*. **`SpawnPreset` singular del root es determinista** (verificado 2026-05-27 vía prior-art validation, ver [`research/2026-05-27-prior-art-validation.md`](research/2026-05-27-prior-art-validation.md)). Los *linked sub-presets* dentro del preset (cargo/attachments/mags) sí deben estar a chance 1/100%. El borrado del preset previo al cambiar de tier lo hace NUESTRO código.
  Example caller: al spawnear participante → `GetPreset(ClothingPreset).SpawnPreset(player, player)` + `GetPreset(GunPresets[tier]).SpawnPreset(player, player)`.
- API: `JsonFileLoader<T>.LoadFile(string filename, out T data, out string errorMessage)` / `SaveFile(string filename, T data, out string errorMessage)`
  Source: `scripts/3_game/tools/jsonfileloader.c:7` / `:42`
  Verified: 2026-05-28
  Scope: 3_Game
  Correct use: cargar/guardar config JSON serializable, comprobando el booleano de retorno y logueando `errorMessage` en fallo.
  Caveats: las variantes `JsonLoadFile`/`JsonSaveFile` existen en `:105`/`:134` pero estan marcadas deprecated en el propio source.

- API: `FileExist(string name)` / `MakeDirectory(string name)`
  Source: `scripts/1_core/proto/ensystem.c:397` / `:525`
  Verified: 2026-05-28
  Scope: core
  Correct use: comprobar `$profile:` antes de cargar config; crear carpeta de perfil si falta. `OpenFile` documenta prefijos `$profile`, `$saves`, `$mission` en `scripts/1_core/proto/ensystem.c:401`.
  Caveats: patron local verificado en mods existentes: `FileExist(dir)` seguido de `MakeDirectory(dir)` para carpetas de perfil.

- API: `DayZGame GetGame()` / `DayZGame.GetTime()` / `DayZGame.ConfigIsExisting(string path)`
  Source: `scripts/3_game/gameplay.c:636`; `scripts/3_game/global/game.c:1510`; `scripts/3_game/global/game.c:611`
  Verified: 2026-05-28
  Scope: 3_Game
  Correct use: `GetGame()` se null-checkea antes de usar; `GetTime()` sirve para rate-limit en ms; `ConfigIsExisting("CfgVehicles " + className)` valida classnames del config.
  Caveats: para guards de autoridad sigue prohibido `IsServer()`; usar `#ifdef SERVER`/`IsDedicatedServer()` cuando toque S1+.

- API: `DayZGame.IsDedicatedServer()` -> `bool`
  Source: `scripts/3_game/global/game.c:1127`
  Verified: 2026-05-28
  Scope: shared runtime guard
  Correct use: guard runtime para logica server-authoritative junto a `#ifdef SERVER`; en cliente debe devolver false y no instanciar managers de estado.
  Caveats: no sustituir por `IsServer()` ni `IsClient()` para autoridad.

- API: `PlayerIdentity.GetPlainId()` -> `string`
  Source: `scripts/3_game/gameplay.c:370`
  Verified: 2026-05-28
  Scope: identidad runtime
  Correct use: clave runtime en memoria para `Admin[]`, stats y assists durante la ronda.
  Caveats: el comentario vanilla dice que no se use en DB/logs persistentes (`GetId()` es el id apto para DB/logs en `gameplay.c:367-368`).

- API: `Timer.Run(float duration, Managed obj, string fn_name, Param params = NULL, bool loop = false)` / `Timer.Stop()` / `TimerBase.IsRunning()`
  Source: `scripts/3_game/tools/tools.c:595`; `scripts/3_game/tools/tools.c:617`; `scripts/3_game/tools/tools.c:264`
  Verified: 2026-05-28
  Scope: 3_Game/4_World timers
  Correct use: timers de una sola ejecucion para ventana de voto y reset de estado; pasar nombre de callback como string y parar solo si `IsRunning()`.
  Caveats: no usar `CallLater` repetido para timers de ronda por el bug de precision documentado en el workflow.

- API: `string.Trim()` / `ToInt()` / `ToFloat()` / `ToLower()` / `Split(...)` / `Length()` / `Substring(...)` / `IndexOf(...)`
  Source: `scripts/1_core/proto/enstring.c:304` / `:20` / `:46` / `:171` / `:396` / `:199` / `:113` / `:240`
  Verified: 2026-05-28
  Scope: core
  Correct use: parsing de strings del config (`"1200"` -> int, `"10"` -> float, `"x,y,z"` -> partes por coma). Validar formato antes de `ToInt`/`ToFloat` porque strings no numericos pueden parsear a 0.
  Caveats: `string.ToVector()` existe en `scripts/1_core/proto/enstring.c:59`, pero espera formato con espacios; S0 usa `Split(",")` + `Vector(float,float,float)` para validar el formato exacto `x,y,z`.

- Pattern: **Singleton estático para bridges cross-layer 5_Mission→4_World**
  Source: decision-log 2026-05-28 + research/2026-05-28-prior-art-validation.md
  Verified: 2026-05-28 (verificado por Codex contra layer-order constraints; aprobado)
  Scope: 4_World (la clase singleton vive aquí; MissionGameplay en 5_Mission la instancia, PlayerBase/Manager en 4_World la consume)
  Correct use: cuando una pieza de UI o estado vive lógicamente en MissionGameplay (5_Mission) pero necesita ser consumida desde 4_World (PlayerBase, RoundManager), pon la pieza en una clase Managed en 4_World con `static s_Instance` + `static Get()`. El constructor seta s_Instance; el destructor lo limpia. MissionGameplay crea/destruye la instancia en su lifecycle (OnInit/OnMissionFinish). 4_World consume con `MyClass.Get()` null-checkeado.
  Caveats: 1 instancia única por sesión (suficiente para HUD, anunciadores, etc.). El destructor DEBE limpiar s_Instance para evitar dangling refs entre rondas. No vale para objetos que deben coexistir en múltiples instancias.
  Example caller (LFGungame_HUD): `LFGungame_HUD hud = LFGungame_HUD.Get(); if (hud) hud.StartCountdown(seconds, role);` desde `PlayerBase.OnRPC` rama cliente del RPC `COUNTDOWN`.

- API: `class UIScriptedMenu extends UIMenuPanel` — base de menus scripted DayZ
  Source: `scripts/3_game/tools/uiscriptedmenu.c:66`
  Verified: 2026-05-28
  Scope: **3_Game** (¡importante: NO en 5_Mission!). Las subclases pueden vivir en 3_Game, 4_World o 5_Mission.
  Correct use: `class LFGungame_X extends UIScriptedMenu { override Widget Init() {...} override bool OnClick(...) {...} override bool OnKeyDown(...) {...} }`. Se abre con `GetGame().GetUIManager().EnterScriptedMenu(int id, UIMenuPanel parent)`; el id se registra en `modded class MissionGameplay::CreateScriptedMenu(int id)`.
  Caveats: la convención de poner menus en `5_Mission/Gui/` es organizativa, NO por compile-order. Prior art en disco que los pone en 4_World: `BaseBuildingPlus/scripts/4_World/.../CraftingBookUI.c:1`, `MapLink/scripts/4_World/gui/DepaturePointMenu.c:3`, `TutorialCarLock/scripts/4_World/GUI/TutorialCarLockMenu.c:1`. Para LFGungame `JoinMenu` vive en 4_World para que `PlayerBase.OnRPC` pueda castear sin layer issue.
  Example caller: `LFGungame_JoinMenu m = LFGungame_JoinMenu.Cast(GetGame().GetUIManager().EnterScriptedMenu(LFGungame_MenuIDs.JOIN, NULL)); if (m) m.SetCountdown(p.param1);`

- API: `DayZGame.GetWorkspace()` / `WorkspaceWidget.CreateWidgets(string layout, Widget parentWidget = NULL, bool immedUpdate = true)` / `Widget.FindAnyWidget(string pathname)` / `Widget.SetColor(int color)` / `TextWidget.SetText(string text, bool immedUpdate = true)`
  Source: `scripts/3_game/global/game.c:84`; `scripts/1_core/proto/enwidgets.c:181`; `scripts/1_core/proto/enwidgets.c:169`; `scripts/1_core/proto/enwidgets.c:144`; `scripts/1_core/proto/enwidgets.c:195`
  Verified: 2026-05-28
  Scope: 3_Game UI/widget runtime.
  Correct use: en `UIScriptedMenu.Init()`, null-check `GetGame()` y `GetWorkspace()` antes de `CreateWidgets`; guardar el `layoutRoot`; buscar widgets por nombre exacto con `FindAnyWidget`; castear el tipo concreto (`ButtonWidget.Cast`, `TextWidget.Cast`) y null-check antes de `SetText`/`SetColor`.
  Caveats: los nombres del `.layout` son case-sensitive. Si un widget no existe, el menú debe degradar sin romper el flujo de voto.

- API: `Vector(float x, float y, float z)` / `Print(void var)`
  Source: `scripts/1_core/proto/enmath3d.c:18`; `scripts/1_core/proto/endebug.c:96`
  Verified: 2026-05-28
  Scope: core
  Correct use: construir vectors desde coords parseadas y emitir warnings rate-limited con `rule_id`.
  Caveats: usar `Print` con rate-limit para no spamear RPT.

- API: `EntityAI.RegisterNetSyncVariableInt(string variableName, int minValue = 0, int maxValue = 0)` / `EntityAI.SetSynchDirty()` / `EntityAI.OnVariablesSynchronized()`
  Source: `scripts/3_game/entities/entityai.c:2865`; `scripts/3_game/entities/entityai.c:3069`; `scripts/3_game/entities/entityai.c:3074`
  Verified: 2026-05-28
  Scope: 3_Game/4_World replicated entities
  Correct use: registrar SyncVars en el constructor o `Init()` del `PlayerBase` modded; tras mutar `m_GG_Role` en server, llamar `SetSynchDirty()` para forzar sincronizacion.
  Caveats: `PlayerBase.OnVariablesSynchronized()` existe en `scripts/4_world/entities/manbase/playerbase.c:5886`; siempre llamar `super`.

- API: `PlayerBase.OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)`
  Source: `scripts/4_world/entities/manbase/playerbase.c:5450`
  Verified: 2026-05-28
  Scope: target object RPC
  Correct use: `DayZGame.OnRPC` enruta al `target.OnRPC`; si `ScriptRPC.Send` usa target `PlayerBase`, el handler simetrico puede vivir en `modded class PlayerBase` para C2S y S2C.
  Caveats: `super.OnRPC(sender, rpc_type, ctx)` primero. Validar payload con `ctx.Read(...)` fail-closed antes de mutar estado.

- API: `ScriptRPC.Send(Object target, int rpc_type, bool guaranteed, PlayerIdentity recipient = null)` / `Serializer.Write(void var)` / `Serializer.Read(void var)`
  Source: `scripts/3_game/gameplay.c:117`; `scripts/1_core/proto/serializer.c:57`; `scripts/1_core/proto/serializer.c:58`
  Verified: 2026-05-28
  Scope: RPC C2S/S2C
  Correct use: C2S VOTE: target `GetGame().GetPlayer()`, recipient `null`. S2C OPEN_DIALOG: target `PlayerBase` destino, recipient `PlayerIdentity` concreta, guaranteed `true`.
  Caveats: mantener enums RPC minimos por fase; S2 solo declara `VOTE` y `OPEN_DIALOG`.

- API: `MissionServer.OnClientReadyEvent(PlayerIdentity identity, PlayerBase player)` / `MissionServer.OnClientReconnectEvent(PlayerIdentity identity, PlayerBase player)`
  Source: `scripts/5_mission/mission/missionserver.c:586`; `scripts/5_mission/mission/missionserver.c:620`
  Verified: 2026-05-28
  Scope: server lifecycle hooks
  Correct use: overrides con `super` primero; delegar en manager para clasificar ready/reconnect sin borrar entries.
  Caveats: `OnClientRespawnEvent` queda fuera de S2.

- API: `DayZGame.GetPlayers(out array<Man> players)` / `PlayerIdentity.GetPlayer()`
  Source: `scripts/3_game/global/game.c:947`; `scripts/3_game/gameplay.c:374-375`
  Verified: 2026-05-28
  Scope: server player lookup
  Correct use: iterar jugadores conectados para prompts iniciales y fallback de target `OPEN_DIALOG`; `GetPlayer()` puede resolver el `PlayerBase` asociado al identity.
  Caveats: no borrar entries al desconectar; D-7 exige restaurar por SteamID al reconectar.

- API: `Timer.GetRemaining()`
  Source: `scripts/3_game/tools/tools.c:318`
  Verified: 2026-05-28
  Scope: vote window UX
  Correct use: computar segundos restantes al abrir o reabrir dialog de join.
  Caveats: el timer cliente del dialog es solo UX; el cierre autoritativo es el timer server.

- API: `Object.SetPosition(vector)` / `CGame.SurfaceY(float x, float z)` / `Math.RandomFloat(float min, float max)` / `Vector(float x, float y, float z)`
  Source: `scripts/3_game/entities/object.c:300`; `scripts/3_game/global/game.c:1162`; `scripts/1_core/proto/enmath.c:91`; `scripts/1_core/proto/enmath3d.c:18`
  Verified: 2026-05-29
  Scope: S3 spawn teleport.
  Correct use: elegir base por round-robin desde coords de config, sumar `RandomFloat(-radius, radius)` en X/Z, corregir Y con `SurfaceY(x,z)`, teleport con `player.SetPosition(spawn)`.
  Caveats: validar/fallback de config ocurre en `LFGungame_Config`; si una lista llega vacia por regresion, usar origen con `SurfaceY(0,0)` como degradacion fail-closed.

- API: `Entity.DisableSimulation(bool)` / `Object.SetAllowDamage(bool)`
  Source: `scripts/3_game/entities/entity.c:3`; `scripts/3_game/entities/object.c:1192`
  Verified: 2026-05-29
  Scope: S3 freeze inicial de ronda.
  Correct use: al comenzar ACTIVE, `DisableSimulation(true)` + `SetAllowDamage(false)` por jugador procesado; tras el timer de 10s, revertir con `DisableSimulation(false)` + `SetAllowDamage(true)`.
  Caveats: prior art vanilla en `scripts/5_mission/mission/missionbenchmark.c:373-375` usa la misma combinacion para inmovilizar/proteger jugador.

- API: `CGame.ObjectDelete(Object)` / `Man.GetHumanInventory()` / `HumanInventory.GetEntityInHands()` / `EntityAI.GetInventory()` / `GameInventory.AttachmentCount()` / `GameInventory.GetAttachmentFromIndex(int)` / `GameInventory.GetCargo()` / `CargoBase.GetItemCount()` / `CargoBase.GetItem(int)`
  Source: `scripts/3_game/global/game.c:704`; `scripts/3_game/entities/man.c:79`; `scripts/3_game/systems/inventory/humaninventory.c:16`; `scripts/3_game/entities/entityai.c:1834`; `scripts/3_game/systems/inventory/inventory.c:205`; `scripts/3_game/systems/inventory/inventory.c:220`; `scripts/3_game/systems/inventory/inventory.c:138`; `scripts/3_game/systems/inventory/cargo.c:28`; `scripts/3_game/systems/inventory/cargo.c:32`
  Verified: 2026-05-29
  Scope: S3 manual inventory wipe.
  Correct use: borrar item en manos, iterar attachments hacia atras, borrar cargo de contenedores hacia atras y usar `ObjectDelete`; no usar `DestroyInventory`.
  Caveats: prior art HM Battle Royale: `_recovered/HM_Core_Module/Scripts/HM_Battle_Royale/MissionServer.c:1158-1193`.

- API: `GameInventory.CreateInInventory(string type)`
  Source: `scripts/3_game/systems/inventory/inventory.c:876`
  Verified: 2026-05-29
  Scope: S3 knife como item individual.
  Correct use: tras spawnear clothing + gun preset del participante, `player.GetInventory().CreateInInventory(Config.GetKnife())` si `Knife` no esta vacio.
  Caveats: retorna `EntityAI` o null; S3 no necesita conservar la referencia.

- API: `Widget.Unlink()` / `ProgressBarWidget.SetCurrent(float)` / `MissionGameplay.OnInit()` / `MissionGameplay.OnMissionFinish()` / `MissionGameplay.OnUpdate(float)`
  Source: `scripts/1_core/proto/enwidgets.c:173`; `scripts/1_core/proto/enwidgets.c:364`; `scripts/5_mission/mission/missiongameplay.c:96`; `scripts/5_mission/mission/missiongameplay.c:257`; `scripts/5_mission/mission/missiongameplay.c:278`
  Verified: 2026-05-29
  Scope: S3 HUD countdown lifecycle.
  Correct use: `MissionGameplay` crea `LFGungame_HUD` en `OnInit`, lo actualiza en `OnUpdate`, y llama `Destroy()`/null en `OnMissionFinish`; el HUD destruye su root con `Unlink()` y actualiza `CountBar` con porcentaje 0-100.
  Caveats: `PlayerBase` vive en 4_World, asi que no puede depender de getters definidos en 5_Mission; consumir HUD via singleton 4_World `LFGungame_HUD.Get()`.

- API: `Object.IsKindOf(string type)` / `CGame.ObjectIsKindOf(Object object, string cfg_parent_name)`
  Source: `scripts/3_game/entities/object.c:517`; `scripts/3_game/global/game.c:1465`
  Verified: 2026-05-29
  Scope: S4 whitelist de loadout y S5 discriminacion del cuchillo.
  Correct use: comparar por herencia de config, no por igualdad exacta de classname. Para S4, preservar el cuchillo configurado con `item.IsKindOf(Config.GetKnife())` antes de clasificar arma/municion; para S5, detectar fuente letal de cuchillo con `source.IsKindOf(Config.GetKnife())`.
  Caveats: `IsKindOf` recibe nombres de config, no nombres de variables ni friendly names.

- API: bases vanilla para whitelist de armas y municion: `Weapon_Base`, `Magazine_Base`, `Ammunition_Base`
  Source: `scripts/4_world/entities/firearms/weapon_base.c:36-38`; `scripts/4_world/entities/itembase/magazine/magazine.c:1`; `scripts/4_world/entities/itembase/magazine/ammunitionpiles.c:2`
  Verified: 2026-05-29
  Scope: S4 `WipeWeaponsOnly`.
  Correct use: `Weapon_Base` identifica armas de fuego; `Magazine_Base` identifica cargadores y tambien cubre pilas de municion porque `Ammunition_Base` extiende `Magazine_Base`; `Ammunition_Base` puede usarse como check explicito/documental de municion.
  Caveats: attachments como opticas/supresores no comparten una unica base de config util para borrar de forma segura. S4 no intenta borrar cualquier `Inventory_Base`; borra el arbol completo debajo del arma detectada y borra mags/ammo sueltos. Si un preset de arma crea attachments sueltos como root/cargo independiente fuera del arbol del arma, eso queda como `[ASSUMPTION->verify in-game]` o requiere inventory-diff.

- API/behavior: `CGame.ObjectDelete(Object)` no documenta cascada script-visible de hijos de inventario
  Source: `scripts/3_game/global/game.c:704`; prior art de wipe manual en `LFGungame/scripts/4_World/LFGungame/LFGungame_RoundManager.c:647-725`
  Verified: 2026-05-29
  Scope: S4 borrado conservador de arma anterior.
  Correct use: no depender de cascada implicita. Para armas detectadas, recorrer attachments y cargo hacia atras y borrar explicitamente hijos antes de `ObjectDelete(parent)`.
  Caveats: al ser `ObjectDelete` native, el source script solo expone la firma. No se pudo verificar desde script si el engine hace cascada por si mismo.

- API: `PlayerBase.EEKilled(Object killer)`
  Source: `scripts/4_world/entities/manbase/playerbase.c:1183`
  Verified: 2026-05-29
  Scope: S5 kill attribution hook.
  Correct use: override en `modded class PlayerBase`, `super.EEKilled(killer)` primero, guard `#ifdef SERVER` + `IsDedicatedServer()`, y delegar al manager con `manager.OnPlayerKilled(this, killer)`.
  Caveats: `killer` es `Object`; resolver el jugador responsable por cast directo a `PlayerBase` o por `EntityAI.GetHierarchyRootPlayer()` cuando sea arma/tool en jerarquia. No hacer victoria ni force-kill en S5.

- API: `PlayerBase.EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)`
  Source: `scripts/4_world/entities/manbase/playerbase.c:1224`
  Verified: 2026-05-29
  Scope: S5 assists y clasificacion de cuchillo.
  Correct use: override con firma literal, `super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef)` primero, guard server, y delegar `source` al manager con `OnPlayerDamaged(this, source)`.
  Caveats: `source` se usa para registrar `damagedBy` y memorizar `lastDamageWasKnife`. `[ASSUMPTION->verify in-game]` que el `EEHitBy` letal se ejecuta antes de `EEKilled`.

- API: `EntityAI.GetHierarchyRootPlayer()` -> `Man`
  Source: `scripts/3_game/entities/entityai.c:876-877`
  Verified: 2026-05-29
  Scope: S5 resolver arma/tool/proyectil a jugador responsable.
  Correct use: si el `Object`/`EntityAI` no castea directo a `PlayerBase`, castear a `EntityAI`, llamar `GetHierarchyRootPlayer()`, y castear el `Man` resultante a `PlayerBase`.
  Caveats: devuelve null si no hay jugador raiz; ese caso representa muerte ambiental/fuente no atribuible y no debe mutar killer stats.

- API: `MissionServer.OnClientRespawnEvent(PlayerIdentity identity, PlayerBase player)`
  Source: `scripts/5_mission/mission/missionserver.c:598`
  Verified: 2026-05-29
  Scope: S5 respawn/re-equip lifecycle.
  Correct use: override en `modded class MissionServer`, `super.OnClientRespawnEvent(identity, player)` primero, guard `#ifdef SERVER` + `IsDedicatedServer()`, y delegar en `manager.OnPlayerRespawn(identity, player)`.
  Caveats: el re-equip vive en `LFGungame_RoundManager` (4_World); `MissionServer` solo llama hacia el manager.

- API: `set<Class T>` / `set<string>.Count()` / `Clear()` / `Find(T)` / `Get(int)` / `Insert(T)`
  Source: `scripts/1_core/proto/enscript.c:722-739`
  Verified: 2026-05-29
  Scope: S5 `LFGungame_PlayerEntry.damagedBy`.
  Correct use: `ref set<string> damagedBy` como member field del entry; inicializar con `new set<string>()`; usar `Find(attackerId) < 0` antes de `Insert(attackerId)`, iterar por indice con `Count()`/`Get(i)`, y resetear con `Clear()` solo en respawn.
  Caveats: se evita `foreach` y se evita depender de duplicacion nativa: `Find` antes de `Insert` garantiza un atacante unico por vida de la victima.

- API: `MissionServer.OnClientRespawnEvent(PlayerIdentity, PlayerBase)` — SEMÁNTICA del parámetro
  Source: `scripts/5_mission/mission/missionserver.c:353-365` (dispatch) + `:598-609` (handler vanilla)
  Verified: 2026-06-10 (auditoría pre-test + agente adversarial)
  Scope: GG-01 — wiring de respawn del mod.
  Correct use: el `player` recibido es el personaje PRE-respawn (cadáver/unconscious; vanilla lo mata si unconscious/restrained — "is choosing to respawn", `pluginadminlog.c:195-198`). NO usar este evento para equipar/teleportar al personaje del respawn.
  Caveats: el personaje NUEVO se crea en `OnClientNewEvent` (`missionserver.c:536-584`, CreateCharacter + EquipCharacter); New y Ready son rutas ALTERNAS (ambos cases ejecutan el mismo post-setup `InvokeOnConnect`+`SendPlayerList`, `:316-351`). Prior art que equipa "on respawn": HM_Starter_Kit `MissionServer.c:133-176` y LBmaster_Core `MissionServer.c:135-141` — ambos en OnClientNewEvent, 0 mods en OnClientRespawnEvent.

- API: `string.ToLower()`
  Source: `scripts/1_core/proto/enstring.c:160-171`
  Verified: 2026-06-10
  Scope: `LFGungame_Config.GetKnifeOneHit()`.
  Correct use: MUTA in-place y devuelve int (length) — el patrón `value.ToLower(); if (value == "true")` es correcto; acepta "True"/"TRUE" del usuario.
  Caveats: no es función pura; no encadenar.

- API: `Timer.GetRemaining()`
  Source: `scripts/3_game/tools/tools.c:318`
  Verified: 2026-06-10
  Scope: `GetVotingSecondsLeft()` → countdown del join dialog.
  Correct use: float de segundos restantes del timer en curso; combinar con `IsRunning()` y `Math.Ceil`.
  Caveats: —

- API: `LB_PresetBase.SpawnPreset(PlayerBase, EntityAI, vector, float)` — determinismo
  Source: `LBmaster_Core/scripts/4_World/LBmaster_Core/Struct/LB_PresetBase.c:138-157` (vs `SpawnPresets` plural `:92-136`)
  Verified: 2026-06-10 (re-verificación con lectura del cuerpo)
  Scope: los 3 call-sites de spawn de presets del mod.
  Correct use: el singular NO aplica chances de SELECCIÓN (eso es del plural/global chance); spawnea siempre.
  Caveats: `minSpawnTries`/`maxSpawnTries` del preset son REPETICIONES del spawn (`:139-141`, RandomIntInclusive) — los presets de arma/ropa del gungame deben tener tries 1/1; los sub-presets `linked` siguen con chance propia (gotcha contractual ya documentado).

- API: `MissionServer.OnClientNewEvent(PlayerIdentity identity, vector pos, ParamsReadContext ctx)` → PlayerBase
  Source: `scripts/5_mission/mission/missionserver.c:536-584` (switch del evento `:316-332`)
  Verified: 2026-06-10 (cuerpo + prior art, LL-131)
  Scope: 5_Mission (modded MissionServer) — hook R1 del fix GG-01.
  Correct use: override que captura `super.OnClientNewEvent(...)` y opera sobre el RETORNO = personaje NUEVO (respawn tras muerte Y primera conexión). Devolver siempre lo que devolvió super. Prior art: HM_Starter_Kit `MissionServer.c:133`, LBmaster_Core `MissionServer.c:135` (también lo overridea — la cadena modded se encadena).
  Caveats: super puede devolver null (CreateCharacter falla) — null-check antes de enrutar. `OnClientRespawnEvent` (`:598-609`) entrega el personaje PRE-respawn (vanilla solo lo usa para matar unconscious/restrained): NUNCA equipar/teleportar desde ahí.

- API: `MissionServer.PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)`
  Source: `scripts/5_mission/mission/missionserver.c:677-711`
  Verified: 2026-06-10
  Scope: 5_Mission — hook GG-08 (recálculo de coronas al irse un jugador).
  Correct use: punto de NO-retorno del logout (LogoutCancel ya no puede revertir). Trabajo dependiente de "el player ya no está" debe diferirse ~1s (vanilla difiere `SendPlayerList` +1000ms, `:710`) porque el player sigue en mundo hasta `DisconnectPlayer` (`:708`).
  Caveats: `identity` puede estar YA borrada al entrar (`:679`) — usar `uid`, no `identity`.

- API: `ScriptCallQueue.Remove(func fn)` y `CallLater(func fn, int delay, bool repeat, ...)`
  Source: `scripts/2_gamelib/tools.c:65` y `:61`
  Verified: 2026-06-10
  Scope: client (cancel del deferred-open GG-05) + server (recálculo diferido GG-08).
  Correct use: `GetGame().GetCallQueue(CALL_CATEGORY_X).Remove(MiMetodo)` cancela TODAS las llamadas encoladas a ese func; no-op si no hay ninguna (seguro llamarlo siempre). Usar la MISMA categoría que el CallLater original.
  Caveats: func de método propio = patrón verificado en el mod; func de método de OTRA instancia no verificado — preferir un método local que delegue.

- API: `Object.IsAlive()`
  Source: `scripts/3_game/entities/object.c:523`
  Verified: 2026-06-10
  Scope: gates GG-04 (OnPlayerVote / BeginRound) y skip de muertos en cleanup (`RoundManager.c:1293` ya lo usaba).
  Correct use: `player && player.IsAlive()` — un PlayerBase muerto (cadáver pre-respawn) sigue teniendo identity y FindPlayerBySteamId lo encuentra; el gate de vida es aparte del de presencia.
  Caveats: —

- API: Fire-mode de arma: `Weapon.GetCurrentMuzzle()` / `GetMuzzleModeCount(int)` / `SetMuzzleMode(int,int)` / `SetNextMuzzleMode(int)` / `GetCurrentMode(int)` / `SetCurrentMode(int,int)` / `GetCurrentModeAutoFire(int)` / `GetCurrentModeBurstSize(int)` / `Synchronize()`
  Source: `scripts/4_world/entities/core/inherited/weapon.c:35/43/44/45/48/49/47/46/11`
  Verified: 2026-06-14
  Scope: 4_World armas.
  Correct use: `GetCurrentModeAutoFire(muzzle)` indica si el modo ACTUAL es full-auto (no hay query por índice → `SetMuzzleMode` y luego comprobar).
  Caveats (CRÍTICO): **`SetMuzzleMode`/`SetCurrentMode` solo cambian la copia LOCAL; NO se replican.** El fire-mode se sincroniza ÚNICAMENTE por el evento FSM `WeaponEventSetNextMuzzleMode`→`ProcessWeaponEvent`→`SyncEventToRemote(e)` (`weapon_base.c:295-311`; fires solo si el parent es server-player `:1240-1255`; el cliente lo replica en `dayzplayerinventory.c:2473-2509`). `Synchronize()` NO lleva el modo (solo estado FSM stable: chamber/anim/selection; el modo solo persiste vía `OnStoreSave/Load` `:633-636/519-529`). El bucle de auto-fire se predice en el CLIENTE (`dayzplayerimplement.c:1150`). → forzar auto con raw `SetMuzzleMode` en server+cliente DESINCRONIZA armas multi-modo (auto DESCARTADO 06-14, ver decision-log/bug-ledger).

- API: Chamber/carga: `Weapon_Base.FillChamber(string ammoType="", int flags=WeaponWithAmmoFlags.CHAMBER)` / `FillInnerMagazine(string="", int=CHAMBER)` / `SpawnAmmo(string="", int=CHAMBER)` / `SpawnAttachedMagazine(...)` / `RandomizeFSMState()`
  Source: `scripts/4_world/entities/firearms/weapon_base.c:929 / :839 / :748 / :771 / :675`; enum `WeaponWithAmmoFlags.CHAMBER = 1` en `:16`.
  Verified: 2026-06-14
  Scope: 4_World armas (server).
  Correct use: `FillChamber()` chambea una bala chambeable aleatoria; idempotente (no-op si el chamber ya está lleno, `FillSpecificChamber :988` exige `IsChamberEmpty`). `FillInnerMagazine("",CHAMBER)` llena el cargador interno (escopeta/Mosin) + chambea con UNA misma munición. Ambas llaman `RandomizeFSMState`+`Synchronize` internamente al hacer algo.
  Caveats: `FillInnerMagazine` empuja el MÁXIMO de cartuchos → sobre-llena un cargador parcial; llamar solo si está VACÍO (`GetInternalMagazineCartridgeCount(muzzle)==0`). `SpawnAttachedMagazine` re-adjunta mag → no usar si el preset ya adjuntó uno. Chambear en arma recién spawneada con FSM transitorio (kill en vivo) puede fallar → DIFERIR 1 tick (06-14).

- API: `Weapon.HasInternalMagazine(int)` / `GetInternalMagazineCartridgeCount(int)` / `IsInternalMagazineFull(int)`
  Source: `scripts/4_world/entities/core/inherited/weapon.c:106/111/122`
  Verified: 2026-06-14
  Scope: 4_World armas.
  Correct use: discriminar arma de cargador interno (escopeta/Mosin) vs detachable-mag antes de llenar. `IsInternalMagazineFull` = max==current.

- API: `Man.GetEntityInHands()` → EntityAI
  Source: `scripts/3_game/entities/man.c:81`
  Verified: 2026-06-14
  Scope: 3_Game (Man; lo hereda PlayerBase).
  Correct use: obtener el arma en manos (los presets de gungame spawnean el arma con `spawnIn:"hands"`). Castear a `Weapon_Base`; un tier melee (cuchillo) castea a null.
  Caveats: si las manos están ocupadas al spawnear, el preset cae a inventario → puede devolver null/item equivocado en un kill en vivo. Re-fetch diferido es más fiable.

- API: `PlayerBase.SetQuickBarEntityShortcut(EntityAI entity, int index, bool force=false)`
  Source: `scripts/4_world/entities/manbase/playerbase.c:2232`
  Verified: 2026-06-14
  Scope: 4_World (server-authoritative; sincroniza el quickbar al cliente).
  Correct use: bindear un item a un slot del hotbar (index 0 = tecla 1). Server-side.
  Caveats (CRÍTICO): **NO llamar SÍNCRONO durante `MissionServer.StartingEquipSetup`** (creación del personaje en el birth-equip GG-01v5) — rompe el birth-equip y el participante cae al fallback vanilla-spawn+teleport (regresión 06-14). DIFERIR vía `CallLater(...,0)` al siguiente tick (post-creación).

- API: `static Widget.TranslateString(string stringId)` → string
  Source: `scripts/1_core/proto/enwidgets.c:112`
  Verified: 2026-06-14
  Scope: core UI (cliente).
  Correct use: resolver una clave `#STR_...` al idioma del cliente y `SetText` el resultado.
  Caveats (CRÍTICO): el `#STR` **baked en el `text` de una `.layout` NO resuelve** cuando el widget se crea por `workspace.CreateWidgets()` (HUD-workspace) — muestra la clave cruda `STR_...`. SÍ resuelve en menús (`UIScriptedMenu`) y vía `TextWidget.SetText("#STR_")` runtime (patrón vanilla en 38 sitios). → labels localizados del HUD: setearlos por script con `TranslateString`/`SetText("#STR_")`, no confiar en el baked (06-14: RoundTag/CountLabel/corona salían crudos para clientes inglés).
