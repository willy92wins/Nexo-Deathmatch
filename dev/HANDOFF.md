<!-- LIVE-STATE:START -->
# LFGungame — Estado vivo · snapshot 2026-06-14 (sesión de fixes OFFLINE; PENDIENTE rebuild + test; siguiente sesión = REVIEW a fondo contra product-spec)

> **TODO lo de abajo está aplicado SOLO EN FUENTES — sin rebuild / firmar / desplegar / probar.** La sesión nueva arranca **lanzando la skill de revisión completa + revisión a fondo contra `product-spec.md` (contrato + intencionalidad / cláusula de desafío)** — decidido con el usuario 2026-06-14. NO re-implementar; revisar, fix de hallazgos, LUEGO rebuild + test in-game.

## Última verificación real
Test in-game 2026-06-13. Los cambios de ESTA sesión (06-14) **no los ha compilado el engine** — primer compile-check = RPT de arranque (varios `CallLater` con params nuevos; si se queja, partir en helpers de 2 params).

## Cambios de esta sesión (en fuentes, 06-14)
1. **Auto fire-mode: REVERTIDO del todo.** Desincronizaba armas multi-modo (raw `SetMuzzleMode` en server+cliente salta el sync del evento FSM `SyncEventToRemote`, weapon_base.c:297; confirmado por sub-agente). Eliminado: enum `SET_WEAPON_AUTO`, bucle de modo server, handlers cliente, miembros retry. Fix correcto (camino evento FSM, server-only) documentado pero **NO implementado** (opción B elegida por el usuario).
2. **Chambereo + cargador interno.** `MakeWeaponReady` (ex-`MakeWeaponReadyAuto`, RoundManager): chambea; si cargador interno VACÍO (escopeta/Mosin) → `FillInnerMagazine("",CHAMBER)` (lleno + chambeado, misma munición; guarda anti-overfill). **DIFERIDO** a `GG_ReadyWeapon` (siguiente tick, FSM estable) → arregla "algunas armas sin chamberear" en kills en vivo.
3. **Hotbar.** Arma→tecla 1, cuchillo→tecla 2, venda→tecla 3 (spawnea `BandageDressing` aparte). Todo DIFERIDO (`GG_ReadyWeapon` arma; `GG_BindQuickbar` cuchillo/venda).
4. **Respawn (regresión) — fix.** Causa: los binds/chambereo SÍNCRONOS corrían dentro de `StartingEquipSetup` (creación del personaje) y rompían el nacimiento GG-01v5 → caía a spawn vanilla + teleport + lagazo. Diferido todo fuera de la creación → debería recuperar el nacimiento en el punto de GunGame. **HIPÓTESIS — sin confirmar in-game.**
5. **Localización HUD.** `RoundTag`, `CountLabel`, rol (`#STR_LFGUNGAME_ROLE_PARTICIPANT`), "Arma"→`#STR_LFGUNGAME_HUD_WEAPON` (clave NUEVA en stringtable.csv), corona (`#STR_LFGUNGAME_RANK_FIRST`) → resueltos vía `Widget.TranslateString` (enwidgets.c:112). El `#STR` baked de la layout NO resuelve en HUD-workspace (solo en menús/UIScriptedMenu) → mostraba `STR_...` crudo. Arregla el `STR_` que veían clientes inglés en countdown/HUD/corona.
6. **Mensajes de chat:** sin tocar (en español, por decisión del usuario).

## Issues abiertos / pendiente in-game
- [ALTA] Respawn recuperado (nace en punto GG, sin lagazo) — CONFIRMAR (era la regresión).
- [ALTA] Chambereo fiable en todos los paths: nacimiento / respawn / tier-up por kill normal **y de cuchillo** (knife-kill: killer +1 vía `ChangeTier`→`ApplyTierLoadout`→`MakeWeaponReady`; víctima −1 sin arma, se la da el respawn).
- [MEDIA] Hotbar 1/2/3. **Caveats:** classname venda hardcodeada `"BandageDressing"` (puede diferir de la del preset de ropa); la venda es DUPLICADA (spawn aparte) — el usuario aprobó "spawnear aparte"; valorar bindear la del preset.
- [MEDIA] Localización HUD para clientes inglés — CONFIRMAR.
- [MEDIA] **Pantalla SÍ/NO en `STR_` para un amigo (cliente inglés):** diagnóstico = lo más probable **MOD DESACTUALIZADO** en su cliente (clave cruda = ausente; DayZ casa columnas por NOMBRE, el orden no produce clave cruda; el JoinMenu usa `SetText("#STR_")` que vanilla resuelve en 38 sitios). Acción: que re-descargue limpio. Si un cliente inglés ACTUALIZADO sigue viéndolo → reordenar stringtable.csv a canónico (`original,english,…,spanish`) — **NO hecho**.
- [BAJA] Autostart: bug de re-arme post-ronda SIGUE sin diagnosticar (falta log con `Autostart="1"`, delay corto, server vacío, 1 ronda). No tocado esta sesión.

## Invariantes CERRADAS — NO re-tocar / NO re-bakear
- **Respawn GG-01v5** (OnClientNewEvent birth-pos + StartingEquipSetup + bornEquipped guard) NO se tocó; la regresión fue por ops síncronas añadidas en la creación, ahora diferidas. No re-bakear el flujo.
- **Auto fire-mode = DROPPED** (opción B). No re-implementar sin el camino evento-FSM (server-only, sin RPC).
- Fixes 06-11/12/13 (countdown/barra, knife-credit gate, ESC scoreboard, dorado líder, sweep+CleanupDeathFinal, RecalculateLeaders, feature Autostart, GG-01v5) — sin cambios.

## Próxima acción (decidida con el usuario)
1. **Sesión nueva = REVIEW a fondo:** lanzar la skill de revisión completa + revisar TODOS los cambios de esta sesión contra `product-spec.md` (contrato + intencionalidad). Archivos tocados: `LFGungame/scripts/4_World/LFGungame/LFGungame_RoundManager.c`, `.../LFGungame_PlayerBase.c`, `.../LFGungame_HUD.c`, `LFGungame/stringtable.csv`, `LFGungame/scripts/3_Game/LFGungame/LFGungame_RPC.c`.
2. Tras review (y fix de hallazgos): rebuild + firmar + desplegar + test in-game (respawn / chambereo / hotbar / localización).

## Punteros
- Contrato: `product-spec.md` · Handoff narrativo: `30_Sessions/2026-06-14-LFGungame-rollback-auto-chamber-hotbar-loc-respawn.md`
<!-- LIVE-STATE:END -->

---

> Log histórico: el detalle cronológico por sesión vive en `%USERPROFILE%\ObsidianVault\AI\30_Sessions\` (handoffs `*LFGungame*`). Este archivo solo mantiene el snapshot de arriba, sobrescrito en cada cierre (workflow.md §"Arranque de sesión").
