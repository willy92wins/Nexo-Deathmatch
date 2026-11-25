# F8 · Icono del arma en el killfeed (investigación, 2026-11-25)

Hecha con un subagente de lectura sobre `P:\scripts`, `P:\gui` y `P:\DZ` (DayZ 1.30). Las citas `ruta:línea` hay que
verificarlas al implementar.

**Conclusión:** en vanilla no hay ninguna imagen 2D por arma; los iconos del inventario son un render 3D de una entidad con
`ItemPreviewWidget`. Si el arma del asesino no existe en el cliente, se pinta una **entidad local cacheada por classname**.

- **Qué hay en vanilla:**
  - API: `ItemPreviewWidget.SetItem(EntityAI)`, `SetView(int)`, `SetForceFlipEnable` (`3_game\gameplay.c:286-302`) y `PreviewWidget.SetModelOrientation/SetModelPosition` (`:276-284`).
  - Patrón del icono de inventario: `5_mission\gui\inventorynew\containeditems\icon.c:1381-1387` hace `SetItem`, luego `SetModelOrientation("0 0 0")` y luego `SetView(obj.GetViewIndex())`.
  - Ninguna arma vanilla tiene `picture=` (grep en `P:\DZ\weapons`: 0). `dayz_inventory.imageset` solo trae siluetas por categoría (`pistol`, `shoulderright`...), que sirven de respaldo.
- **Entidad local:**
  - Se crea con `g_Game.CreateObjectEx(cls, "0 0 0", ECE_LOCAL | ECE_CREATEPHYSICS | ECE_TRACE)` y luego se hace `dBodyDestroy`, `DisableSimulation(true)` y `SetAllowDamage(false)`. Es lo que hace la consola de scripts: `5_mission\gui\scriptconsoleitemstab.c:682-709`, solo DEVELOPER.
  - Flags en `3_game\ce\centraleconomy.c` (`ECE_LOCAL` :25, `ECE_NOLIFETIME` :30).
  - Se borra con `Delete()` u `ObjectDelete` (`game.c:705`).
  - Fuera de DEVELOPER, `DayZGame.m_IsPreviewSpawn` no rebaja el VME de validación del cañón (`weaponstablestate.c:183-195`). Con armas vanilla no debería salir; con armas de mods mal configuradas, sí.
- **Recomendación:**
  - Un `map<string, EntityAI>` en el cliente, con tope LRU de 10-20 entradas, que solo crea entidades si el classname existe en config y `IsKindOf(cls, "Weapon_Base")`.
  - Se vacía al cerrar la misión y al destruir el HUD.
  - Si la creación falla, el respaldo es la silueta `set:dayz_inventory image:<categoría>` o el `displayName`.
  - El `displayName` se busca en `CfgWeapons`, después en `CfgVehicles` y después en `CfgMagazines`. `MiscGameplayFunctions.GetItemDisplayName` solo mira `CfgVehicles`.
- **Límites:**
  - No se ven los accesorios del asesino: el arma sale con su modelo base.
  - La primera vez que aparece cada arma puede dar un tirón al cargar el modelo.
  - No está medido el coste de render de 5 filas.
