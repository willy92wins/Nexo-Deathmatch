# F4 — loadouts en cada spawn

## 1. Loadout al azar
Hecho en `EquipPlayer`: un solo nombre de `GetLoadouts()` con `Math.RandomInt(0, guns.Count())` (máximo exclusivo). Fuera el comentario `// F4: loadout al azar` y el `Get(0)`.

- Sorteo: [NexoDM_RoundManager.c](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1806)
- Getters: `GetClothingPreset` [NexoDM_Config.c](NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:265), `GetLoadouts` [:270](NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:270)

## 2. Orden resolver → vaciar → ropa → loadout → arma lista
`EquipPlayer` resuelve ropa y el loadout sorteado. Si el nombre va vacío, la lista no tiene entradas, no hay `LB_PresetLoader` o `GetPreset` devuelve null, hace `LogRule` y sale **sin** `WipeInventory`. Si los dos resuelven: `WipeInventory` → `SpawnPreset` ropa → `SpawnPreset` loadout → `MakeWeaponReady`.

- Orden: [NexoDM_RoundManager.c:1788](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1788) a [:1837](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1837)
- Un solo `EquipPlayer` por camino (el vaciado ya no está en el llamador):
  - `BeginRound` [:422](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:422)
  - Nacimiento: `StartingEquipSetup` [NexoDM_MissionServer.c:131](NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionServer.c:131) o, si el vanilla no llama a `EquipCharacter`, [NexoDM_MissionServer.c:90](NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionServer.c:90). El stash `m_GG_BirthSteamId` se consume en el camino que corre, así que no entran los dos. `EquipParticipantAtBirth` [:2007](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:2007)
  - `OnPlayerRespawn` solo si `bornEquipped` es falso [:914](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:914); si el nacimiento ya equipó, quita el id y no vuelve a equipar [:897](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:897)
  - Entrada tardía `OnPlayerReady` [:1095](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1095) (solo si no había ficha)
  - Reconexión con ficha [:1150](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1150) y sin ficha [:1167](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1167); cada rama hace un `return` antes de la otra

## 3. Hotbar sin duplicados
`GG_GiveKnifeAndBandage` (tick siguiente, igual que antes) busca el cuchillo de `GetKnife()` y un `BandageDressing`. Si ya hay uno, no crea otro y ata ese a la ranura 2 (índice 1) o 3 (índice 2). Si hay más de uno, se queda el primero del inventario y el resto se borra con el árbol de `DeleteWeaponLoadoutEntityTree` (un duplicado dentro de otro duplicado no se borra otra vez). El arma sigue en la ranura 1 (índice 0) en `GG_ReadyWeapon`.

- Cuchillo y venda: [:1870](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1870)–[:1899](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1899)
- `GG_TakeSingleOfType`: [:1905](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1905)
- Arma: [:1004](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1004)

## 4. Antiloot
Sin cambios. En `OnPlayerKilled`, solo participante y ronda ACTIVE: `WipeWeaponsOnly` al momento (arma, cargador y munición; el cuchillo se conserva en `ShouldDeleteWeaponLoadoutItem`), barrido a 5 m a los 2 s (`CleanupDeathSite`) y a los 10 s `CleanupDeathFinal` borra el cuerpo y vuelve a barrer.

- Programación: [:659](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:659)–[:664](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:664)
- Radio 5 m: [:783](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:783)
- Borrado del cuerpo: [:804](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:804)

## LO QUE NO PUDE VERIFICAR
- Compilación Enforce y partida: no hay shell en este encargo y no se lanzó el juego. No hay prueba de que el índice aleatorio cambie de un spawn a otro, ni de que un preset inexistente deje el inventario vanilla intacto.
- Contenido real de los presets LBmaster (si la ropa ya trae cuchillo o `BandageDressing`, o más de un arma). La hotbar se leyó en código, no en un jugador.
- Línea exacta de `EntityAI.GetHierarchyParent` en `P:\`: no se abrió el fuente vanilla en esta sesión. El comentario del módulo no cita un número de línea inventado.
- Que `GetObjectsAtPosition3D` a 5 m recoja un arma que la física haya lanzado más lejos antes del segundo barrido (el de los 10 s usa la posición del cuerpo si se movió, y también el punto de la muerte si distan más de 2 m).

## Corrección 1

- **#1.** `OnPlayerKilled` ya no sale si `GetIdentity()` es null. Puntos y cámara siguen dentro de `scoreThisDeath` (hace falta identidad y ficha de participante): [NexoDM_RoundManager.c:564](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:564). Sin identidad, el antiloot corre si `m_GG_Role` es participante o el steam id cacheado tiene ficha de participante: [:586](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:586). El id se guarda al equipar de verdad en `player.m_GG_SteamId`: [NexoDM_PlayerBase.c:5](NexoDM/scripts/4_World/NexoDM/NexoDM_PlayerBase.c:5), asignación [NexoDM_RoundManager.c:1934](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1934).
- **#2.** No moví el arma a las manos. En este repo y en `F4-REVIEW-R1.md` no hay una API verificada de inventario para pasar a manos un ítem ya existente (`GetHumanInventory` / `GetEntityInHands` solo leen; `CreateInInventory` crea). `GG_ReadyWeapon` sigue recamarando y atando el arma que encuentra `FindPlayerWeapon`: comentario [NexoDM_RoundManager.c:1052](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1052).
- **#3.** La rama sin cargador interno llama `FillChamber(chamberAmmo)` con `GG_PickLethalChamberAmmo`: [NexoDM_RoundManager.c:1074](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1074). Cita de la revisión: `Weapon_Base.FillChamber(string)` en `scripts/4_world/entities/firearms/weapon_base.c:921`.
- **#4.** `GG_DeferredEquipStillActive` corta si el jugador es null, no está vivo, `IsSetForDeletion()` (`entityai.c:812`), `IsPlayerDisconnected()` (`playerbase.c:2638`), no tiene identidad o la ronda no está ACTIVE: [NexoDM_RoundManager.c:1007](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1007). Lo usan `GG_ReadyWeapon` [:1039](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1039) y `GG_GiveKnifeAndBandage` [:1947](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1947). `NexoDM_RouteFreshChar` repite esas guardas y `GetState() != ACTIVE`: [NexoDM_MissionServer.c:150](NexoDM/scripts/5_Mission/NexoDM/NexoDM_MissionServer.c:150).
- **#5.** `ScheduleCorpseAntiLoot` programa el borrado del cuerpo a `max(10, RespawnDelay + 5)` segundos: [NexoDM_RoundManager.c:702](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:702). El barrido de 2 s no cambia.
- **#6.** `ResetStats` vacía `m_GG_BirthEquipped`: [NexoDM_RoundManager.c:362](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:362). `EquipPlayer` devuelve `bool` [:1830](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1830). `EquipParticipantAtBirth` solo inserta el steam id si ese bool es true: [:2123](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:2123).
- **#7.** La reconexión con ficha solo llama a `EquipPlayer` si `player.IsAlive()`: [NexoDM_RoundManager.c:1224](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1224).
- **#9.** `ShouldDeleteWeaponLoadoutItem` también borra `Box_Base` (`ammoboxes.c:1-16`): [NexoDM_RoundManager.c:1852](NexoDM/scripts/4_World/NexoDM/NexoDM_RoundManager.c:1852). El cuchillo de config sigue excluido antes.

No compilé Enforce ni lo probé en partida. El #8 y el #10 de la revisión no se tocaron.
