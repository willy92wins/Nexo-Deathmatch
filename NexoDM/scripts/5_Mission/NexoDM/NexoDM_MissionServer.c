modded class MissionServer
{
    // GG-01v5: the fresh character on first connect AND on post-death respawn fires OnClientNewEvent
    // (missionserver.c:316-319), but OnClientReadyEvent fires ONLY on the initial connect handshake -
    // NOT on a post-death respawn. So fresh-character routing hangs off OnClientNewEvent (fires on both).
    // The "naked + invisible weapon to others" desync was a fresh-character appearance-replication gap:
    // equipping a participant AFTER creation (t=0 GG-01, +1.5s v3, DisableSimulation toggle v4) never
    // replicated the loadout to observers. v5 instead equips DURING creation, the way vanilla does:
    // OnClientNewEvent overrides the spawn pos so the character is BORN at the gungame spawn (no
    // double-load lag), and the StartingEquipSetup override (called inside super, before the engine
    // releases the character) wipes the vanilla default and equips the tier preset. Post-birth routing
    // then only resets death stats + syncs HUD; OnPlayerRespawn skips re-equip via the bornEquipped
    // guard (fallback to old post-creation equip if StartingEquipSetup did not run). OnClientReadyEvent
    // keeps a redundant idempotent OnPlayerReady for the initial-connect voting-dialog RPC.
    protected string m_GG_BirthSteamId;
    protected int m_GG_BirthTier;

    // Unsupervised 24/7 mode: arm the first autostart attempt at server boot; the round manager
    // re-arms after every round from ReturnToIdle. Together they cover the periodic-restart cycle.
    // R2: MissionServer.OnInit() in scripts/5_mission/mission/missionserver.c:83.
    override void OnInit()
    {
        super.OnInit();

#ifdef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (!game.IsDedicatedServer())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
        {
            string bootReason = "server boot";
            manager.ScheduleAutostartIfEnabled(bootReason);
        }
#endif
    }

    // R2: MissionServer.OnClientNewEvent(PlayerIdentity,vector,ParamsReadContext) in scripts/5_mission/mission/missionserver.c:536.
    override PlayerBase OnClientNewEvent(PlayerIdentity identity, vector pos, ParamsReadContext ctx)
    {
        vector birthPos = pos;

#ifdef SERVER
        // GG-01v5: before super creates the character, if this is a participant respawn in an ACTIVE
        // round, override the spawn pos so the character is BORN at the gungame spawn (no
        // vanilla-spawn-then-teleport double map load) and stash the tier so the StartingEquipSetup
        // override equips it during creation. The stash is read+consumed within this same synchronous
        // OnClientNewEvent -> super -> StartingEquipSetup flow (DayZ processes one player at a time).
        m_GG_BirthSteamId = string.Empty;
        m_GG_BirthTier = 0;
        DayZGame preGame = GetGame();
        if (preGame && preGame.IsDedicatedServer() && identity)
        {
            NexoDM_RoundManager preMgr = NexoDM_RoundManager.GetInstance();
            if (preMgr)
            {
                vector ggSpawn = vector.Zero;
                int ggTier = 0;
                if (preMgr.ResolveParticipantBirthSpawn(identity.GetPlainId(), ggSpawn, ggTier))
                {
                    birthPos = ggSpawn;
                    m_GG_BirthSteamId = identity.GetPlainId();
                    m_GG_BirthTier = ggTier;
                }
            }
        }
#endif

        PlayerBase newPlayer = super.OnClientNewEvent(identity, birthPos, ctx);

#ifdef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (game && game.IsDedicatedServer() && identity && newPlayer)
        {
            NexoDM_EnsureAnnounceSubscription();

            // GG-01v6: equip the participant HERE (post-super, still inside the synchronous
            // ClientNewEvent frame, before the character replicates to other clients) when the birth
            // stash is still set. On servers with cfgplayerspawnpoints.json the vanilla
            // OnClientNewEvent returns at missionserver.c:563 BEFORE EquipCharacter, so
            // StartingEquipSetup never runs and the v5 birth-equip never happened -> bornEquipped
            // stayed false -> OnPlayerRespawn ran the desyncing teleport+wipe+equip fallback. The
            // stash doubles as a "not yet equipped" flag: StartingEquipSetup clears it when it ran
            // the equip (no spawn points), so this block is skipped then (no double-equip).
            if (m_GG_BirthSteamId != string.Empty && identity.GetPlainId() == m_GG_BirthSteamId)
            {
                NexoDM_RoundManager birthMgr = NexoDM_RoundManager.GetInstance();
                if (birthMgr)
                    birthMgr.EquipParticipantAtBirth(newPlayer, m_GG_BirthSteamId, m_GG_BirthTier);

                // Consume the stash so neither this block nor a later character creation re-triggers.
                m_GG_BirthSteamId = string.Empty;
                m_GG_BirthTier = 0;
            }

            // GG-01v5/v6: post-birth routing on the next tick (FRESH_CHAR_ROUTE_DELAY_MS=0). Teleport+equip
            // already happened at birth (pos override + birth-equip above or in StartingEquipSetup); this
            // only resets death stats + syncs the HUD. OnPlayerRespawn skips re-equip via the bornEquipped guard.
            // R2: ScriptCallQueue.CallLater(func,int,bool,...) in scripts/2_gamelib/tools.c:61.
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(NexoDM_RouteFreshChar, NexoDM_Constants.FRESH_CHAR_ROUTE_DELAY_MS, false, identity, newPlayer);
        }
#endif

        return newPlayer;
    }

    // GG-01v5: equip a participant DURING character creation. Vanilla calls StartingEquipSetup from
    // EquipCharacter (missionserver.c:523), inside super.OnClientNewEvent, AFTER applying the default
    // loadout and BEFORE the engine releases the character to the network - the same point where the
    // base game dresses characters so all clients see them clothed. If OnClientNewEvent stashed a
    // birth plan for THIS character, wipe the vanilla default and equip the tier preset here, so
    // observers receive the respawned participant already dressed (fixes the GG-01..v4 naked desync).
    // R2: MissionServer.StartingEquipSetup(PlayerBase,bool) in scripts/5_mission/mission/missionserver.c:527.
    override void StartingEquipSetup(PlayerBase player, bool clothesChosen)
    {
        super.StartingEquipSetup(player, clothesChosen);

#ifdef SERVER
        if (!player)
            return;

        if (m_GG_BirthSteamId == string.Empty)
            return;

        PlayerIdentity id = player.GetIdentity();
        if (!id || id.GetPlainId() != m_GG_BirthSteamId)
            return;

        NexoDM_RoundManager mgr = NexoDM_RoundManager.GetInstance();
        if (mgr)
            mgr.EquipParticipantAtBirth(player, m_GG_BirthSteamId, m_GG_BirthTier);

        // Consume the stash so a later character creation in this session does not re-trigger.
        m_GG_BirthSteamId = string.Empty;
        m_GG_BirthTier = 0;
#endif
    }

    // GG-01v5 post-birth target: the teleport + equip already happened at birth (pos override +
    // StartingEquipSetup). This resets death stats + syncs role/tier/HUD for a freshly created
    // character (connect or respawn). Guards cover the next-tick window: the player may have
    // disconnected (null) or died again before this fires. OnPlayerRespawn skips re-equip for a
    // born-equipped participant (bornEquipped guard); fallback equips here if birth-equip did not run.
    void NexoDM_RouteFreshChar(PlayerIdentity identity, PlayerBase player)
    {
#ifdef SERVER
        if (!identity || !player)
            return;

        // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
        if (!player.IsAlive())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (!manager)
            return;

        manager.OnPlayerRespawn(identity, player);
        manager.OnPlayerReady(identity, player);
#endif
    }

    // GG-01v3: initial-connect handshake only (does NOT fire on respawn). Keeps a redundant,
    // idempotent OnPlayerReady because this is the moment the client is loaded and ready to receive
    // the voting-dialog RPC; the respawn/equip routing lives in NexoDM_RouteFreshChar.
    // R2: MissionServer.OnClientReadyEvent(PlayerIdentity,PlayerBase) in scripts/5_mission/mission/missionserver.c:586.
    override void OnClientReadyEvent(PlayerIdentity identity, PlayerBase player)
    {
        super.OnClientReadyEvent(identity, player);

#ifdef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (!game.IsDedicatedServer())
            return;

        NexoDM_EnsureAnnounceSubscription();

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
            manager.OnPlayerReady(identity, player);
#endif
    }

    // GG-01: intentionally NO manager routing here. This event delivers the PRE-respawn character;
    // vanilla only uses it to kill an unconscious/restrained character choosing respawn
    // (missionserver.c:598-609). Equipping or teleporting from here acts on the corpse — the NEW
    // character is routed through the fresh-character mark above.
    // R2: MissionServer.OnClientRespawnEvent(PlayerIdentity,PlayerBase) in scripts/5_mission/mission/missionserver.c:598.
    override void OnClientRespawnEvent(PlayerIdentity identity, PlayerBase player)
    {
        super.OnClientRespawnEvent(identity, player);
    }

    // R2: MissionServer.OnClientReconnectEvent(PlayerIdentity,PlayerBase) in scripts/5_mission/mission/missionserver.c:620.
    override void OnClientReconnectEvent(PlayerIdentity identity, PlayerBase player)
    {
        super.OnClientReconnectEvent(identity, player);

#ifdef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (!game.IsDedicatedServer())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
            manager.OnPlayerReconnect(identity, player);
#endif
    }

    // GG-08: PlayerDisconnected is the no-return point of the logout flow; identity may already be
    // deleted here, and the player object leaves the world a few calls later (missionserver.c:677-711).
    // Defer the crown recompute 1s — the same deferral vanilla uses for SendPlayerList (:710) — so
    // FindPlayerBySteamId no longer sees the leaver when leaders re-evaluate.
    // R2: MissionServer.PlayerDisconnected(PlayerBase,PlayerIdentity,string) in scripts/5_mission/mission/missionserver.c:677.
    override void PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)
    {
        super.PlayerDisconnected(player, identity, uid);

#ifdef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (!game.IsDedicatedServer())
            return;

        // R2: ScriptCallQueue.CallLater(func,int,bool,...) in scripts/2_gamelib/tools.c:61.
        game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(NexoDM_RecalcLeadersDeferred, 1000, false);
#endif
    }

    // GG-08: deferred target for PlayerDisconnected above. CallLater on an own method is the
    // pattern already used by the mod (NexoDM_PlayerBase.c deferred JOIN open).
    protected void NexoDM_RecalcLeadersDeferred()
    {
#ifdef SERVER
        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
            manager.OnPlayerGoneRecalculate();
#endif
    }

    protected static bool s_GG_AnnounceSubscribed;

    // Subscribe once to the 3_Game announce bus and forward to LBmaster chat. Guarded by
    // #ifndef LB_DISABLE_CHAT so a no-chat build doesn't reference the absent LBChatHandler.
    protected void NexoDM_EnsureAnnounceSubscription()
    {
#ifdef SERVER
#ifndef LB_DISABLE_CHAT
        if (s_GG_AnnounceSubscribed)
            return;

        s_GG_AnnounceSubscribed = true;
        // R2: ScriptInvoker.Insert in scripts/2_gamelib/tools.c:120.
        NexoDM_AnnounceBus.GetEvent().Insert(NexoDM_OnAnnounce);
#endif
#endif
    }

    protected void NexoDM_OnAnnounce(NexoDM_AnnouncePayload payload)
    {
#ifdef SERVER
#ifndef LB_DISABLE_CHAT
        if (!payload)
            return;

        // R2: LBChatHandler.SendSimpleChatMessage(PlayerIdentity,string,bool) in
        // LBmaster_Groups/scripts/5_Mission/LBmaster_Groups/System/LBChatHandler.c:65 (static).
        LBChatHandler.SendSimpleChatMessage(payload.recipient, payload.message, payload.global);
#endif
#endif
    }
}
