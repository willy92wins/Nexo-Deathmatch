class NexoDM_PlayerEntry : Managed
{
    string steamId;
    int role;
    int kills;
    int assists;
    int deaths;
    int points;
    int lastPointsChangeMs;
    ref set<string> damagedBy;
    bool lastDamageWasKnife;
    int lastKnifeHitTimeMs;     // S5.5 KB1-B obs
    string lastKnifeKillerId;   // E2 kill-credit fallback (scripted knife death -> EEKilled killer=null)
    string lastAttackerId;      // Option A: last participant who damaged this player (any weapon); credits the "downer" when the death reports no killer (respawn-suicide while KO / bleed-out)
    int lastAttackerTimeMs;     // GetTime() of the lastAttackerId hit; recency-gated by ATTACKER_CREDIT_WINDOW_MS

    void NexoDM_PlayerEntry()
    {
        steamId = string.Empty;
        role = NexoDM_Role.NONE;
        kills = 0;
        assists = 0;
        deaths = 0;
        points = 0;
        lastPointsChangeMs = 0;
        damagedBy = new set<string>();
        lastDamageWasKnife = false;
        lastKnifeHitTimeMs = 0;     // S5.5 KB1-B obs
        lastKnifeKillerId = string.Empty;
        lastAttackerId = string.Empty;
        lastAttackerTimeMs = 0;
    }
}

/*
Round flow:
IDLE | admin start or autostart with valid config | ACTIVE | every connected player becomes a participant, BeginRound.
IDLE | start with invalid config | IDLE | fail closed.
ACTIVE | connect or reconnect without entry | ACTIVE | role PARTICIPANT.
ACTIVE | timeout or admin stop | ENDED | then IDLE.
*/
class NexoDM_RoundManager : Managed
{
    protected static ref NexoDM_RoundManager s_Instance;

    protected int m_State;
    protected ref NexoDM_Config m_Config;
    protected ref Timer m_RoundTimer;
    protected ref Timer m_EndStateTimer;
    protected ref Timer m_FreezeTimer;
    protected ref Timer m_AutostartTimer;
    protected ref map<string, ref NexoDM_PlayerEntry> m_PlayerEntries;
    protected ref map<string, int> m_WarnLog;
    // GG-01v5: steamIds whose fresh respawn character was equipped DURING creation
    // (StartingEquipSetup -> EquipParticipantAtBirth). OnPlayerRespawn consumes this to skip the
    // post-creation teleport+wipe+equip path that desynced the loadout to observers.
    protected ref set<string> m_GG_BirthEquipped;
    protected int m_SpawnRoundRobinIdx;
    protected int m_ActiveMapIndex;
    protected int m_PreviousMapIndex;
    protected bool m_FreezeActive;
    protected int m_RoundEndsAtMs;
    protected int m_HudTop5LastSendMs;
    protected int m_HudTop5Count;
    protected bool m_HudTop5Pending;
    protected ref array<string> m_HudTop5Ids;
    protected ref array<int> m_HudTop5Points;
    protected ref array<int> m_HudTop5Kills;
    protected ref array<int> m_HudTop5Deaths;

    void NexoDM_RoundManager()
    {
#ifdef SERVER
        m_State = NexoDM_State.IDLE;
        m_RoundTimer = new Timer();
        m_EndStateTimer = new Timer();
        m_FreezeTimer = new Timer();
        m_AutostartTimer = new Timer();
        m_PlayerEntries = new map<string, ref NexoDM_PlayerEntry>;
        m_WarnLog = new map<string, int>;
        m_GG_BirthEquipped = new set<string>();
        m_SpawnRoundRobinIdx = -1;
        m_ActiveMapIndex = 0;
        m_PreviousMapIndex = -1;
        m_FreezeActive = false;
        m_RoundEndsAtMs = 0;
        m_HudTop5LastSendMs = 0;
        m_HudTop5Count = -1;
        m_HudTop5Pending = false;
        m_HudTop5Ids = new array<string>;
        m_HudTop5Points = new array<int>;
        m_HudTop5Kills = new array<int>;
        m_HudTop5Deaths = new array<int>;
#endif
    }

    static NexoDM_RoundManager GetInstance()
    {
#ifdef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return null;

        if (!game.IsDedicatedServer())
            return null;

        if (!s_Instance)
        {
            s_Instance = new NexoDM_RoundManager();
            s_Instance.Init();
        }

        return s_Instance;
#else
        return null;
#endif
    }

    void Init()
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        EnsureCollections();
        EnsureTimers();
#endif
    }

    void StartGame()
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != NexoDM_State.IDLE)
        {
            string ignoredMessage = "start ignored: round is not idle";
            LogRound(ignoredMessage);
            return;
        }

        StopTimer(m_EndStateTimer);
        // A start (manual or automatic) consumes any armed autostart attempt; ReturnToIdle
        // re-arms the unsupervised loop when the round is over.
        StopTimer(m_AutostartTimer);

        // F3 p1: load into a local so a failed Load() does not clobber a previously loaded
        // config; ScheduleAutostartIfEnabled below still needs GetAutostart() to re-arm.
        // R2: NexoDM_Config.Load() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:80.
        NexoDM_Config loadedConfig = NexoDM_Config.Load();
        if (!loadedConfig)
        {
            string loadMessage = "start rejected: config load failed";
            LogRound(loadMessage);
            m_State = NexoDM_State.IDLE;
            ScheduleAutostartIfEnabled("retry after rejected start");
            return;
        }
        m_Config = loadedConfig;

        // R2: NexoDM_Config.Validate() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:150.
        if (!m_Config.Validate())
        {
            string validateMessage = "start rejected: config validation failed";
            LogRound(validateMessage);
            m_State = NexoDM_State.IDLE;
            ScheduleAutostartIfEnabled("retry after rejected start");
            return;
        }

        // 4_World half of E_CFG_PRESET: 3_Game only checks non-empty names. CAMBIAR_* defaults do not resolve.
        if (!LoadoutPresetsExist())
        {
            string presetMessage = "start rejected: config preset missing";
            LogRound(presetMessage);
            m_State = NexoDM_State.IDLE;
            ScheduleAutostartIfEnabled("retry after rejected start");
            return;
        }

        // Placeholder names (CAMBIAR_*) in the RPT at round start.
        m_Config.LogPlaceholderWarnings();

        ResetStats();
        // No previous spawn point this round. -1 so the first pick is not treated as a repeat of index 0.
        m_SpawnRoundRobinIdx = -1;
        EnrollConnectedParticipants();
        m_State = NexoDM_State.ACTIVE;
        BeginRound();
        string startedMessage = "round entered active";
        LogRound(startedMessage);
#endif
    }

    void EndRound(string reason, string winnerSteamId)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State == NexoDM_State.IDLE)
        {
            string idleMessage = "end ignored: already idle";
            LogRound(idleMessage);
            return;
        }

        if (m_State == NexoDM_State.ENDED)
        {
            string endedMessage = "end ignored: already ended";
            LogRound(endedMessage);
            return;
        }

        // Capture state before flipping to ENDED: only a round that reached ACTIVE announces a winner.
        bool wasActive = (m_State == NexoDM_State.ACTIVE);

        StopTimer(m_RoundTimer);
        StopTimer(m_EndStateTimer);
        StopTimer(m_FreezeTimer);

        m_State = NexoDM_State.ENDED;
        m_FreezeActive = false;
        RevertFreezeIfApplied();

        // F3 p6: choose the next round's active map now; BeginRound of the next round
        // teleports/equips everyone on m_ActiveMapIndex.
        if (wasActive && m_Config)
        {
            // R2: NexoDM_Config.GetValidMaps() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:327.
            array<ref NexoDM_ParsedMap> validMaps = m_Config.GetValidMaps();
            if (validMaps && validMaps.Count() > 0)
            {
                int mapCount = validMaps.Count();
                int finishedMapIndex = m_ActiveMapIndex;
                int oldPreviousIndex = m_PreviousMapIndex;
                m_ActiveMapIndex = PickNextMapIndex(mapCount, finishedMapIndex, oldPreviousIndex);
                m_PreviousMapIndex = finishedMapIndex;
            }
        }

        if (wasActive)
        {
            // Winner announce must run while entries still exist; ReturnToIdle
            // clears them 0.1s after ScheduleIdleReset below.
            AnnounceWinner(reason, winnerSteamId);
            // Drop the deferred flush before sending the final table, so it cannot fire in the next round.
            CancelHudTop5Flush();
            FlushHudTop5();
            // Round is already ENDED: remaining 0, active no. Participants still have their role.
            BroadcastHudRound();
        }

        // #6: strip weapons/mags/ammo from every connected participant now, so
        // nothing is kept through IDLE (BeginRound ground cleanup stays as-is).
        EnsureCollections();
        int wipeIndex = 0;
        for (wipeIndex = 0; wipeIndex < m_PlayerEntries.Count(); wipeIndex = wipeIndex + 1)
        {
            NexoDM_PlayerEntry wipeEntry = m_PlayerEntries.GetElement(wipeIndex);
            if (!wipeEntry || wipeEntry.role != NexoDM_Role.PARTICIPANT)
                continue;
            string wipeSteamId = m_PlayerEntries.GetKey(wipeIndex);
            PlayerBase wipePlayer = FindPlayerBySteamId(wipeSteamId);
            if (wipePlayer)
                WipeWeaponsOnly(wipePlayer);
        }

        // Drop the synced participant role so the scoreboard gate closes with the round.
        EnsureCollections();
        int roleIndex = 0;
        for (roleIndex = 0; roleIndex < m_PlayerEntries.Count(); roleIndex = roleIndex + 1)
        {
            NexoDM_PlayerEntry roleEntry = m_PlayerEntries.GetElement(roleIndex);
            if (roleEntry && roleEntry.role != NexoDM_Role.NONE)
            {
                string roleSteamId = m_PlayerEntries.GetKey(roleIndex);
                ApplyRoleToOnlinePlayer(roleSteamId, NexoDM_Role.NONE);
            }
        }

        string safeReason = reason;
        if (safeReason == string.Empty)
            safeReason = "unknown";

        string message = "round ended: ";
        message = message + safeReason;
        LogRound(message);
        ScheduleIdleReset();
#endif
    }

    int GetState()
    {
        return m_State;
    }

    array<string> GetParticipantIds()
    {
        array<string> ids = new array<string>();
#ifdef SERVER
        EnsureCollections();
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (entry && entry.role == NexoDM_Role.PARTICIPANT)
            {
                string steamId = m_PlayerEntries.GetKey(index);
                ids.Insert(steamId);
            }
        }
#endif
        return ids;
    }

    // F-01: an entry only counts while its player is actually connected. A disconnected
    // participant's entry persists (entries are purged only on round reset), so liveness must be
    // checked at read time to avoid ghost slots / leaders / winners. O(players) per call; the
    // round is capped at MaxPlayers so the O(P^2) in the read loops below is trivial (P2-4 accepted).
    protected bool IsEntryOnline(NexoDM_PlayerEntry e)
    {
#ifdef SERVER
        if (!e)
            return false;
        if (e.steamId == string.Empty)
            return false;
        PlayerBase online = FindPlayerBySteamId(e.steamId);
        if (!online)
            return false;
        // R2: PlayerBase.IsPlayerDisconnected() in scripts/4_world/entities/manbase/playerbase.c:2638.
        if (online.IsPlayerDisconnected())
            return false;
        return true;
#else
        return false;
#endif
    }

    int CountParticipants()
    {
        int count = 0;
#ifdef SERVER
        EnsureCollections();
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (entry && entry.role == NexoDM_Role.PARTICIPANT && IsEntryOnline(entry))
                count = count + 1;
        }
#endif
        return count;
    }

    bool IsAdminSteamId(string steamId)
    {
#ifdef SERVER
        if (steamId == string.Empty)
            return false;

        if (!EnsureConfig())
            return false;

        // R2: NexoDM_Config.GetAdmins() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:253.
        array<string> admins = m_Config.GetAdmins();
        if (!admins)
            return false;

        int adminIndex = 0;
        for (adminIndex = 0; adminIndex < admins.Count(); adminIndex = adminIndex + 1)
        {
            string adminId = admins.Get(adminIndex);
            if (adminId == steamId)
                return true;
        }
#endif
        return false;
    }

    void ResetStats()
    {
#ifdef SERVER
        EnsureCollections();
        m_PlayerEntries.Clear();
        if (m_GG_BirthEquipped)
            m_GG_BirthEquipped.Clear();
#endif
    }

    void BeginRound()
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != NexoDM_State.ACTIVE)
            return;

        if (!EnsureConfig())
        {
            string noConfigMessage = "begin round rejected: config unavailable";
            LogRound(noConfigMessage);
            return;
        }

        EnsureTimers();
        EnsureCollections();
        StopTimer(m_FreezeTimer);

        // F3 p7: no weapon from the previous round survives on the ground of its arena.
        CleanupPreviousMapGroundWeapons();

        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry)
                continue;

            if (entry.role == NexoDM_Role.NONE)
                continue;

            PlayerBase player = FindPlayerBySteamId(entry.steamId);
            if (!player)
            {
                string missingMessage = "begin skipped disconnected player ";
                missingMessage = missingMessage + entry.steamId;
                LogRound(missingMessage);
                continue;
            }

            // GG-04: never teleport/equip/freeze a corpse. The NEW character is routed into the
            // round by the OnClientNewEvent respawn path (GG-01).
            // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
            if (!player.IsAlive())
            {
                string deadMessage = "begin skipped dead player ";
                deadMessage = deadMessage + entry.steamId;
                LogRound(deadMessage);
                continue;
            }

            vector spawn = PickRandomSpawn();
            // R2: Object.SetPosition(vector) in scripts/3_game/entities/object.c:300.
            player.SetPosition(spawn);

            // F4: wipe lives inside EquipPlayer, after both presets resolve.
            EquipPlayer(player, entry.role);

            if (entry.role == NexoDM_Role.PARTICIPANT)
            {
                // R2: Entity.DisableSimulation(bool) in scripts/3_game/entities/entity.c:3.
                player.DisableSimulation(true);
                // R2: Object.SetAllowDamage(bool) in scripts/3_game/entities/object.c:1192.
                player.SetAllowDamage(false);
            }

            if (entry.role == NexoDM_Role.PARTICIPANT)
            {
                PlayerIdentity identity = player.GetIdentity();
                if (identity)
                    BroadcastCountdown(identity, 10);
            }
        }

        m_FreezeActive = true;
        float freezeSeconds = 10.0;
        string callbackName = "EndFreeze";
        // R2: Timer.Run(float,Managed,string,Param,bool) in scripts/3_game/tools/tools.c:595.
        m_FreezeTimer.Run(freezeSeconds, this, callbackName, NULL, false);

        // I2: start the round-duration countdown; OnRoundTimeout ends the round by tiebreak.
        int roundDuration = m_Config.GetRoundDuration();
        float roundSeconds = roundDuration;
        string timeoutCallback = "OnRoundTimeout";
        m_RoundTimer.Run(roundSeconds, this, timeoutCallback, NULL, false);
        int nowMs = 0;
        if (GetGame())
            nowMs = GetGame().GetTime();
        m_RoundEndsAtMs = nowMs + (roundDuration * 1000);
        CancelHudTop5Flush();
        m_HudTop5Count = -1;
        m_HudTop5LastSendMs = nowMs - 1000;

        int hudIndex = 0;
        for (hudIndex = 0; hudIndex < m_PlayerEntries.Count(); hudIndex = hudIndex + 1)
        {
            NexoDM_PlayerEntry hudEntry = m_PlayerEntries.GetElement(hudIndex);
            if (!hudEntry || hudEntry.role != NexoDM_Role.PARTICIPANT)
                continue;
            PlayerBase hudPlayer = FindPlayerBySteamId(hudEntry.steamId);
            if (!hudPlayer || !hudPlayer.IsAlive())
                continue;
            hudPlayer.NexoDM_ResetVitalsSent();
            SendHudJoinState(hudPlayer, hudEntry);
        }
        BroadcastHudSelf();
#endif
    }

    void OnPlayerDamaged(PlayerBase victim, EntityAI source)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != NexoDM_State.ACTIVE)
            return;

        if (!victim)
            return;

        PlayerIdentity victimIdentity = victim.GetIdentity();
        if (!victimIdentity)
            return;

        // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
        string victimId = victimIdentity.GetPlainId();
        if (victimId == string.Empty)
            return;

        NexoDM_PlayerEntry victimEntry = GetEntry(victimId);
        if (!victimEntry)
            return;

        if (victimEntry.role != NexoDM_Role.PARTICIPANT)
            return;

        if (!victimEntry.damagedBy)
            victimEntry.damagedBy = new set<string>();

        string knifeAttackerId = string.Empty;
        bool attackerIsParticipant = false;
        PlayerBase attacker = ResolveSourcePlayer(source);
        if (attacker)
        {
            PlayerIdentity attackerIdentity = attacker.GetIdentity();
            if (attackerIdentity)
            {
                string attackerId = attackerIdentity.GetPlainId();
                if (attackerId != string.Empty && attackerId != victimId)
                {
                    NexoDM_PlayerEntry attackerEntry = GetEntry(attackerId);
                    if (attackerEntry && attackerEntry.role == NexoDM_Role.PARTICIPANT)
                    {
                        attackerIsParticipant = true;
                        knifeAttackerId = attackerId;
                        // R2: set.Find/Insert in scripts/1_core/proto/enscript.c:730/:739.
                        if (victimEntry.damagedBy.Find(attackerId) < 0)
                            victimEntry.damagedBy.Insert(attackerId);
                    }
                }
            }
        }

        bool isKnife = false;
        if (attackerIsParticipant && EnsureConfig())
        {
            // R2: NexoDM_Config.GetKnife() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:270.
            string knife = m_Config.GetKnife();
            // R2: Object.IsKindOf(string) in scripts/3_game/entities/object.c:517.
            if (source && knife != string.Empty && source.IsKindOf(knife))
                isKnife = true;
        }

        victimEntry.lastDamageWasKnife = isKnife;
        if (isKnife)
        {
            // S5.5 KB1-B obs: marca de tiempo del ultimo cuchillazo (R2: DayZGame.GetTime() game.c:1510).
            victimEntry.lastKnifeHitTimeMs = GetGame().GetTime();
            // E2 kill-credit fallback: a scripted SetHealth(0) death yields EEKilled(killer=null),
            // so remember the knife attacker to re-credit the kill in OnPlayerKilled.
            victimEntry.lastKnifeKillerId = knifeAttackerId;
        }

        // Option A downed-kill credit: remember the last participant attacker (ANY weapon) + time, to
        // re-credit the kill in OnPlayerKilled when the death reports no killer - an unconscious player
        // choosing Respawn (OnClientRespawnEvent -> SetHealth(0), missionserver.c:608) or a bleed-out.
        // Recency-gated there (ATTACKER_CREDIT_WINDOW_MS); a clean finisher sets a valid killer so the
        // fallback never overrides them.
        // R2: DayZGame.GetTime() in scripts/3_game/global/game.c:1510.
        if (attackerIsParticipant && knifeAttackerId != string.Empty)
        {
            victimEntry.lastAttackerId = knifeAttackerId;
            victimEntry.lastAttackerTimeMs = GetGame().GetTime();
        }

        // E2 [verify in-game]: one-hit knife. Force the victim's health to 0 so any knife hit is
        // lethal when KnifeOneHit is set. SetHealth("","",0) reliably kills; the kill is re-credited
        // to the attacker in OnPlayerKilled via lastKnifeKillerId when EEKilled gives killer=null.
        // R2: EntityAI.SetHealth("","",0) in scripts/4_world/entities/explosivesbase/plastic_explosive.c:247.
        if (isKnife && m_Config.GetKnifeOneHit())
            victim.SetHealth("", "", 0);
#endif
    }

    void OnPlayerKilled(PlayerBase victim, Object killer)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != NexoDM_State.ACTIVE)
            return;

        if (!victim)
            return;

        // Points and killer cam need a live identity. Anti-loot does not: a logout death
        // can arrive with GetIdentity() == null. The steam id cached at equip still identifies
        // the participant.
        PlayerIdentity victimIdentity = victim.GetIdentity();
        string victimId = string.Empty;
        if (victimIdentity)
        {
            // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
            victimId = victimIdentity.GetPlainId();
        }

        bool scoreThisDeath = false;
        NexoDM_PlayerEntry victimEntry = null;
        if (victimIdentity && victimId != string.Empty)
        {
            victimEntry = GetEntry(victimId);
            if (!victimEntry)
                return;

            if (victimEntry.role != NexoDM_Role.PARTICIPANT)
                return;

            scoreThisDeath = true;
        }
        else
        {
            bool deadParticipant = false;
            if (victim.m_GG_Role == NexoDM_Role.PARTICIPANT)
                deadParticipant = true;

            if (!deadParticipant && victim.m_GG_SteamId != string.Empty)
            {
                NexoDM_PlayerEntry cachedEntry = GetEntry(victim.m_GG_SteamId);
                if (cachedEntry && cachedEntry.role == NexoDM_Role.PARTICIPANT)
                    deadParticipant = true;
            }

            if (!deadParticipant)
                return;
        }

        bool roundEndsByKills = false;
        string winningKillerId = string.Empty;
        if (scoreThisDeath)
        {
        victimEntry.deaths = victimEntry.deaths + 1;
        string deathDetail = "participant death recorded";
        LogRule(NexoDM_Constants.RULE_DEATH_RECORDED, deathDetail);

        PlayerBase killerPlayer = ResolveObjectPlayer(killer);
        string killerId = string.Empty;
        if (killerPlayer)
        {
            PlayerIdentity killerIdentity = killerPlayer.GetIdentity();
            if (killerIdentity)
                killerId = killerIdentity.GetPlainId();
        }

        // E2 fallback: a one-hit knife death (scripted SetHealth 0) reports EEKilled killer as
        // null OR as the victim itself (self-attributed scripted death), so both cases recover
        // the recorded knife attacker to credit the kill (kills counter).
        if ((killerId == string.Empty || killerId == victimId) && victimEntry.lastDamageWasKnife && victimEntry.lastKnifeKillerId != string.Empty)
        {
            killerId = victimEntry.lastKnifeKillerId;
            killerPlayer = FindPlayerBySteamId(killerId);
        }

        // Option A downed-kill credit: a no-killer death that was NOT a knife (or where the knife branch
        // found nothing) - an unconscious player choosing Respawn (OnClientRespawnEvent SetHealth(0)) or a
        // bleed-out after being downed by gunfire - credits the last participant attacker if recent. Only
        // fires while the killer is still null/self, so a clean finisher (valid killer) is never overridden.
        if ((killerId == string.Empty || killerId == victimId) && victimEntry.lastAttackerId != string.Empty)
        {
            // R2: DayZGame.GetTime() in scripts/3_game/global/game.c:1510.
            int sinceLastHitMs = GetGame().GetTime() - victimEntry.lastAttackerTimeMs;
            if (sinceLastHitMs >= 0 && sinceLastHitMs < NexoDM_Constants.ATTACKER_CREDIT_WINDOW_MS)
            {
                killerId = victimEntry.lastAttackerId;
                killerPlayer = FindPlayerBySteamId(killerId);
            }
        }

        PlayerBase creditedKiller = null;
        if (killerPlayer && killerId != string.Empty && killerId != victimId)
        {
            NexoDM_PlayerEntry killerEntry = GetEntry(killerId);
            if (killerEntry && killerEntry.role == NexoDM_Role.PARTICIPANT)
            {
                killerEntry.kills = killerEntry.kills + 1;
                AddPoints(killerEntry, NexoDM_Constants.POINTS_PER_KILL);
                creditedKiller = killerPlayer;
                winningKillerId = killerId;
                string killDetail = "participant kill recorded";
                LogRule(NexoDM_Constants.RULE_KILL_RECORDED, killDetail);
                AwardAssists(victimEntry, killerId);

                // F3 p4: first to KillsToWin ends the round at once.
                if (EnsureConfig() && killerEntry.kills >= m_Config.GetKillsToWin())
                    roundEndsByKills = true;
            }
        }

        // F3 p3: tell the victim who killed them (credited killer, incl. fallbacks above;
        // null/self -> 0,0 on the wire). Skipped on the round-ending kill: no respawn follows.
        if (!roundEndsByKills)
            SendKillerCam(victim, creditedKiller);

        PublishKillHud(victim, victimEntry, creditedKiller, killerId, killer);

        // S5.5 KB1-A: consume-once - evita un credito stale entre kill y respawn (P2-2).
        victimEntry.lastDamageWasKnife = false;
        victimEntry.lastKnifeKillerId = string.Empty;
        // Option A: consume the downed-credit attribution too, so it is not reused on a later death.
        victimEntry.lastAttackerId = string.Empty;
        victimEntry.lastAttackerTimeMs = 0;
        // F-15: clear the assist set at death too (it ran AwardAssists above). The set was only
        // cleared on respawn; a death not followed by a respawn-clear (round end / disconnect)
        // would otherwise re-award stale assists on the entry's next death.
        if (victimEntry.damagedBy)
            victimEntry.damagedBy.Clear();
        }

        // Anti-loot: strip firearms/mags/ammo from the corpse now and
        // sweep any dropped on the ground shortly after, so the round weapon can't be looted.
        // Participants only. The knife (everyone has one), clothing and
        // the body are kept - WipeWeaponsOnly preserves the knife via ShouldDeleteWeaponLoadoutItem.
        // Also runs when identity is already gone (logout death).
        ScheduleCorpseAntiLoot(victim);

        // F3 p4: deferred to the end so death cleanup above is already scheduled.
        if (roundEndsByKills)
        {
            string killsReason = "kills_to_win";
            EndRound(killsReason, winningKillerId);
        }
#endif
    }

    // Strip the corpse now, sweep the ground at 2 s, and delete the body only after the killer
    // camera can finish: max(10 s, RespawnDelay + 5 s).
    protected void ScheduleCorpseAntiLoot(PlayerBase victim)
    {
#ifdef SERVER
        if (!victim)
            return;

        vector deathPos = victim.GetPosition();
        WipeWeaponsOnly(victim);
        // R2: ScriptCallQueue.CallLater(func,int,bool,...) pattern in scripts/2_gamelib/tools.c:46.
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(CleanupDeathSite, 2000, false, deathPos);

        int holdSeconds = 10;
        if (m_Config)
        {
            // R2: NexoDM_Config.GetRespawnDelay() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:298.
            int camSeconds = m_Config.GetRespawnDelay() + 5;
            if (camSeconds > holdSeconds)
                holdSeconds = camSeconds;
        }
        int delayMs = holdSeconds * 1000;
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(CleanupDeathFinal, delayMs, false, victim, deathPos);
#endif
    }

    // F3 p7: delete loose firearms/mags/ammo on the ground of the PREVIOUS round's arena.
    // Zone = bounding box of that map's spawn points + 50 m, queried as centre + radius
    // (half the box diagonal + 50) through the same vanilla query the death sweep uses.
    void CleanupPreviousMapGroundWeapons()
    {
#ifdef SERVER
        if (!m_Config)
            return;

        if (m_PreviousMapIndex < 0)
            return;

        // R2: NexoDM_Config.GetValidMaps() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:327.
        array<ref NexoDM_ParsedMap> maps = m_Config.GetValidMaps();
        if (!maps || m_PreviousMapIndex >= maps.Count())
            return;

        NexoDM_ParsedMap prevMap = maps.Get(m_PreviousMapIndex);
        if (!prevMap || !prevMap.SpawnGroups)
            return;

        float minX = 0.0;
        float maxX = 0.0;
        float minZ = 0.0;
        float maxZ = 0.0;
        float sumY = 0.0;
        int pointTotal = 0;
        int gIdx = 0;
        for (gIdx = 0; gIdx < prevMap.SpawnGroups.Count(); gIdx = gIdx + 1)
        {
            NexoDM_ParsedSpawnGroup grp = prevMap.SpawnGroups.Get(gIdx);
            if (!grp || !grp.Points)
                continue;
            int pIdx = 0;
            for (pIdx = 0; pIdx < grp.Points.Count(); pIdx = pIdx + 1)
            {
                vector pt = grp.Points.Get(pIdx);
                if (pointTotal == 0)
                {
                    minX = pt[0];
                    maxX = pt[0];
                    minZ = pt[2];
                    maxZ = pt[2];
                }
                else
                {
                    if (pt[0] < minX)
                        minX = pt[0];
                    if (pt[0] > maxX)
                        maxX = pt[0];
                    if (pt[2] < minZ)
                        minZ = pt[2];
                    if (pt[2] > maxZ)
                        maxZ = pt[2];
                }
                sumY = sumY + pt[1];
                pointTotal = pointTotal + 1;
            }
        }

        if (pointTotal == 0)
            return;

        float centerX = (minX + maxX) * 0.5;
        float centerZ = (minZ + maxZ) * 0.5;
        float centerY = sumY / pointTotal;
        // R2: Math.Sqrt(float) in scripts/1_core/proto/enmath.c:305.
        float halfDiag = Math.Sqrt(((maxX - minX) * (maxX - minX)) + ((maxZ - minZ) * (maxZ - minZ))) * 0.5;
        float radius = halfDiag + 50.0;
        // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
        vector center = Vector(centerX, centerY, centerZ);

        DayZGame game = GetGame();
        if (!game)
            return;

        array<Object> objects = new array<Object>();
        array<CargoBase> proxyCargos = new array<CargoBase>();
        // R2: CGame.GetObjectsAtPosition3D(vector,float,out array<Object>,out array<CargoBase>) in scripts/3_game/global/game.c:930.
        game.GetObjectsAtPosition3D(center, radius, objects, proxyCargos);

        int objIndex = 0;
        for (objIndex = 0; objIndex < objects.Count(); objIndex = objIndex + 1)
        {
            ItemBase item = ItemBase.Cast(objects.Get(objIndex));
            // R2: EntityAI.GetHierarchyRootPlayer() in scripts/3_game/entities/entityai.c:877.
            if (item && !item.GetHierarchyRootPlayer() && ShouldDeleteWeaponLoadoutItem(item))
            {
                // R2: CGame.ObjectDelete(Object) in scripts/3_game/global/game.c:705.
                game.ObjectDelete(item);
            }
        }
#endif
    }

    // Death-site sweep: delete loose firearms/mags/ammo dropped on the ground near the death
    // (the knife and non-weapon loot are kept). Called ~2s after a participant death so a
    // weapon released from the hands has settled. Radius 5 m (in-game test 2026-06-11).
    void CleanupDeathSite(vector pos)
    {
#ifdef SERVER
        DayZGame game = GetGame();
        if (!game)
            return;

        array<Object> objects = new array<Object>();
        array<CargoBase> proxyCargos = new array<CargoBase>();
        // R2: CGame.GetObjectsAtPosition3D(vector,float,out array<Object>,out array<CargoBase>) in scripts/5_mission/gui/inventorynew/vicinityitemmanager.c:245.
        game.GetObjectsAtPosition3D(pos, 5.0, objects, proxyCargos);

        int objIndex = 0;
        for (objIndex = 0; objIndex < objects.Count(); objIndex = objIndex + 1)
        {
            ItemBase item = ItemBase.Cast(objects.Get(objIndex));
            // Loose ground weapon only: no owning player, and a firearm/mag/ammo (not the knife).
            // R2: EntityAI.GetHierarchyRootPlayer() in scripts/4_world/plugins/pluginbase/pluginrecipesmanager.c:407.
            if (item && !item.GetHierarchyRootPlayer() && ShouldDeleteWeaponLoadoutItem(item))
            {
                // R2: CGame.ObjectDelete(Object) in scripts/3_game/global/game.c:704.
                game.ObjectDelete(item);
            }
        }
#endif
    }

    // Final death cleanup (+10s): deletes the corpse (clothes and knife go with it) and sweeps
    // stray firearms/mags/ammo once more around the body's settled position - a ragdolled body
    // and its flung weapon can end up far from where the kill registered. Intentionally has no
    // round-state guard: a corpse from a round that just ended still gets removed.
    void CleanupDeathFinal(PlayerBase corpse, vector deathPos)
    {
#ifdef SERVER
        DayZGame game = GetGame();
        if (!game)
            return;

        vector sweepPos = deathPos;
        if (corpse)
        {
            // Never delete a living character (paranoia guard; the ref is captured at kill time).
            // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
            if (corpse.IsAlive())
                return;

            sweepPos = corpse.GetPosition();
            // Recursive delete (attachments/cargo first, then the body) - same helper the
            // weapon wipe uses, valid for any entity tree.
            DeleteWeaponLoadoutEntityTree(corpse, game);
        }

        CleanupDeathSite(sweepPos);
        // R2: vector.Distance(vector,vector) in scripts/1_core/proto/enconvert.c:196.
        if (vector.Distance(sweepPos, deathPos) > 2.0)
            CleanupDeathSite(deathPos);
#endif
    }

    void AwardAssists(NexoDM_PlayerEntry victimEntry, string killerId)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (!victimEntry)
            return;

        if (!victimEntry.damagedBy)
            return;

        int index = 0;
        for (index = 0; index < victimEntry.damagedBy.Count(); index = index + 1)
        {
            // R2: set.Count/Get in scripts/1_core/proto/enscript.c:724/:731.
            string attackerId = victimEntry.damagedBy.Get(index);
            if (attackerId == killerId)
                continue;

            NexoDM_PlayerEntry attackerEntry = GetEntry(attackerId);
            if (attackerEntry && attackerEntry.role == NexoDM_Role.PARTICIPANT)
            {
                attackerEntry.assists = attackerEntry.assists + 1;
                AddPoints(attackerEntry, NexoDM_Constants.POINTS_PER_ASSIST);
                string assistDetail = "participant assist recorded";
                LogRule(NexoDM_Constants.RULE_ASSIST_RECORDED, assistDetail);
            }
        }
#endif
    }

    void OnPlayerRespawn(PlayerIdentity identity, PlayerBase player)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (!identity || !player)
            return;

        // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
        string steamId = identity.GetPlainId();
        if (steamId == string.Empty)
            return;

        NexoDM_PlayerEntry entry = GetEntry(steamId);
        if (!entry)
            return;

        if (entry.role != NexoDM_Role.PARTICIPANT)
            return;

        if (m_State != NexoDM_State.ACTIVE)
            return;

        if (!EnsureConfig())
            return;

        // GG-01v5: a participant fresh character is born at the gungame spawn (pos override in
        // MissionServer.OnClientNewEvent) and equipped DURING creation (StartingEquipSetup ->
        // EquipParticipantAtBirth), so observers receive it already dressed. If that birth-equip ran,
        // skip the post-creation teleport+wipe+equip here (that was the GG-01..v4 path that desynced)
        // and only sync the HUD + reset death stats. Fallback (birth-equip did not run, e.g. a server
        // with custom spawn gear that bypasses StartingEquipSetup): equip here the old way.
        bool bornEquipped = false;
        if (m_GG_BirthEquipped)
        {
            int beIdx = m_GG_BirthEquipped.Find(steamId);
            if (beIdx >= 0)
            {
                m_GG_BirthEquipped.Remove(beIdx);
                bornEquipped = true;
            }
        }

        if (!bornEquipped)
        {
            vector spawn = PickRandomSpawn();
            // R2: Object.SetPosition(vector) in scripts/3_game/entities/object.c:300.
            player.SetPosition(spawn);
            // F4: wipe lives inside EquipPlayer, after both presets resolve.
            EquipPlayer(player, entry.role);
        }

        if (!entry.damagedBy)
            entry.damagedBy = new set<string>();

        // R2: set.Clear in scripts/1_core/proto/enscript.c:725.
        entry.damagedBy.Clear();
        entry.lastDamageWasKnife = false;
        entry.lastKnifeKillerId = string.Empty;
        entry.lastAttackerId = string.Empty;
        entry.lastAttackerTimeMs = 0;

        // F3 p8: a fresh character born during the freeze window stays frozen until EndFreeze.
        ApplyFreezeToPlayer(player);
        player.NexoDM_ResetVitalsSent();
        SendHudJoinState(player, entry);

        string respawnDetail = "participant respawn equipped";
        LogRule(NexoDM_Constants.RULE_RESPAWN_EQUIPPED, respawnDetail);
#endif
    }

    // Makes the freshly-spawned weapon ready: chamber (or fully load an EMPTY internal magazine for
    // shotguns/Mosin) + hotbar bind, DEFERRED one tick (GG_ReadyWeapon). Doing it synchronously right
    // after SpawnPreset is unsafe twice over: (1) on a participant birth-equip MakeWeaponReady runs
    // inside MissionServer.StartingEquipSetup (character creation), where a sync quickbar write broke
    // the gungame birth-spawn; (2) on a LIVE kill the just-swapped weapon's FSM is transient and the
    // chamber sometimes did not take ("some weapons unchambered" on knife kills). Re-fetching next tick
    // gives a stable weapon. (Full-auto force removed: raw SetMuzzleMode on both sides bypassed the FSM
    // sync path and desynced multi-mode weapons.) Server-only.
    protected void MakeWeaponReady(PlayerBase player)
    {
#ifdef SERVER
        if (!player)
            return;

        // R2: ScriptCallQueue.CallLater(func,int,bool,...) in scripts/2_gamelib/tools.c:61.
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(GG_ReadyWeapon, 0, false, player);
#endif
    }

    // Next-tick equip callbacks bail out if the player is gone, pending delete, disconnected,
    // without identity, or the round already left ACTIVE.
    protected bool GG_DeferredEquipStillActive(PlayerBase player)
    {
        if (!player)
            return false;

        // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
        if (!player.IsAlive())
            return false;

        // R2: EntityAI.IsSetForDeletion() in scripts/3_game/entities/entityai.c:812.
        if (player.IsSetForDeletion())
            return false;

        // R2: PlayerBase.IsPlayerDisconnected() in scripts/4_world/entities/manbase/playerbase.c:2638.
        if (player.IsPlayerDisconnected())
            return false;

        if (!player.GetIdentity())
            return false;

        if (m_State != NexoDM_State.ACTIVE)
            return false;

        return true;
    }

    // Deferred target of MakeWeaponReady (next tick = stable FSM): chamber the in-hands weapon (or
    // fully load an EMPTY internal magazine + chamber with one consistent ammo type) and bind it to
    // hotbar slot 1. Server-only.
    void GG_ReadyWeapon(PlayerBase player)
    {
#ifdef SERVER
        if (!GG_DeferredEquipStillActive(player))
            return;

        // The weapon usually spawns in hands, but LBmaster may have placed it in the inventory.
        // Fall back to scanning
        // the player for the (single, post-wipe) Weapon_Base so it is chambered + hotbar-bound wherever
        // it landed. R2: Man.GetEntityInHands() in scripts/3_game/entities/man.c:81.
        Weapon_Base wpn = Weapon_Base.Cast(player.GetEntityInHands());
        if (!wpn)
            wpn = FindPlayerWeapon(player);
        if (!wpn)
            return;

        // F4 r1 #2: a weapon found only by FindPlayerWeapon goes to the hands when they are empty, so the
        // participant spawns holding it. R2: Man.ServerTakeEntityToHands(EntityAI) in
        // scripts/3_game/entities/man.c:197.
        if (!player.GetEntityInHands() && wpn.GetHierarchyRootPlayer() == player)
            player.ServerTakeEntityToHands(wpn);
        int muzzle = wpn.GetCurrentMuzzle();
        // R2: Weapon.HasInternalMagazine/GetInternalMagazineCartridgeCount in
        // scripts/4_world/entities/core/inherited/weapon.c:106/111. Only fill an EMPTY internal mag:
        // FillInnerMagazine pushes max cartridges and would overfill a partially-loaded one.
        if (wpn.HasInternalMagazine(muzzle) && wpn.GetInternalMagazineCartridgeCount(muzzle) == 0)
        {
            // Pick a LETHAL chamberable ammo. FillInnerMagazine("") picks a RANDOM entry from the weapon's
            // chamberableFrom, and a shotgun's pool includes Ammo_12gaRubberSlug (non-lethal) - players
            // could not kill with it. GG_PickLethalChamberAmmo returns the first non-rubber entry, or ""
            // (let the engine randomize) for weapons whose pool has no rubber.
            string lethalAmmo = GG_PickLethalChamberAmmo(wpn);
            // R2: Weapon_Base.FillInnerMagazine(string,int) in weapon_base.c:839 — the given (lethal) ammo
            // fills the inner mag AND the chamber (WeaponWithAmmoFlags.CHAMBER, weapon_base.c:16).
            wpn.FillInnerMagazine(lethalAmmo, WeaponWithAmmoFlags.CHAMBER);
        }
        else
        {
            // R2: Weapon_Base.FillChamber(string ammoType = "", ...) in scripts/4_world/entities/firearms/weapon_base.c:921.
            // Same lethal pick as the internal-mag branch so "" cannot roll Ammo_12gaRubberSlug.
            string chamberAmmo = GG_PickLethalChamberAmmo(wpn);
            wpn.FillChamber(chamberAmmo);
        }

        // R2: Weapon.Synchronize() in scripts/4_world/entities/core/inherited/weapon.c:11.
        wpn.Synchronize();

        // R2: PlayerBase.SetQuickBarEntityShortcut(EntityAI,int,bool) in scripts/4_world/entities/manbase/playerbase.c:2232.
        player.SetQuickBarEntityShortcut(wpn, 0, true);
#endif
    }

    // Picks a LETHAL chamberable ammo class for the weapon, avoiding non-lethal rounds. The engine's
    // random pick (FillInnerMagazine/FillChamber with "") draws from the weapon's chamberableFrom, and a
    // shotgun's pool includes Ammo_12gaRubberSlug (non-lethal). Returns the first chamberableFrom entry
    // whose class name does not contain "Rubber", or "" (let the engine randomize) when the config is
    // empty or every entry is filtered out. Server-only.
    protected string GG_PickLethalChamberAmmo(Weapon_Base wpn)
    {
#ifdef SERVER
        if (!wpn)
            return "";

        // R2: CGame.ConfigGetTextArray(string,out TStringArray) in scripts/3_game/global/game.c:557;
        // chamberableFrom holds the weapon's compatible cartridge classes (dayzgame.c:3794).
        TStringArray chamberable = new TStringArray();
        string cfgPath = "CfgWeapons " + wpn.GetType() + " chamberableFrom";
        GetGame().ConfigGetTextArray(cfgPath, chamberable);

        int i = 0;
        for (i = 0; i < chamberable.Count(); i = i + 1)
        {
            string ammoName = chamberable.Get(i);
            // R2: string.Contains(string) in scripts/1_core/proto/enstring.c:286.
            if (ammoName != string.Empty && !ammoName.Contains("Rubber"))
                return ammoName;
        }
#endif
        return "";
    }

    // Returns the first Weapon_Base carried by the player (hands, attachments or cargo). After
    // WipeWeaponsOnly the only firearm on a participant is the freshly spawned weapon, so this
    // reliably locates it even when LBmaster placed it in the inventory instead of the hands
    // (knife-kill: the knife occupied the hands). Server-only.
    protected Weapon_Base FindPlayerWeapon(PlayerBase player)
    {
#ifdef SERVER
        if (!player)
            return null;

        // R2: EntityAI.GetInventory() in scripts/3_game/entities/entityai.c:1834.
        GameInventory inv = player.GetInventory();
        if (!inv)
            return null;

        array<EntityAI> items = new array<EntityAI>();
        // R2: GameInventory.EnumerateInventory(InventoryTraversalType,out array<EntityAI>) in scripts/3_game/systems/inventory/inventory.c:127; InventoryTraversalType.PREORDER in scripts/3_game/gameplay.c:7.
        inv.EnumerateInventory(InventoryTraversalType.PREORDER, items);
        int i = 0;
        for (i = 0; i < items.Count(); i = i + 1)
        {
            Weapon_Base w = Weapon_Base.Cast(items.Get(i));
            if (w)
                return w;
        }
#endif
        return null;
    }

    void OnPlayerReady(PlayerIdentity identity, PlayerBase player)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (!identity || !player)
            return;

        // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
        string steamId = identity.GetPlainId();
        if (steamId == string.Empty)
            return;

        if (m_State == NexoDM_State.IDLE || m_State == NexoDM_State.ENDED)
            return;

        if (m_State == NexoDM_State.ACTIVE)
        {
            NexoDM_PlayerEntry activeEntry = GetEntry(steamId);
            if (!activeEntry)
            {
                activeEntry = GetOrCreateEntry(steamId);
                activeEntry.role = NexoDM_Role.PARTICIPANT;
                if (player.IsAlive() && EnsureConfig())
                {
                    vector spawn = PickRandomSpawn();
                    player.SetPosition(spawn);
                    // F4: wipe lives inside EquipPlayer, after both presets resolve.
                    EquipPlayer(player, activeEntry.role);
                }
            }

            ApplyRoleToPlayer(player, activeEntry.role);
            // F3 p8: late join during the freeze window freezes like everyone else.
            if (activeEntry.role == NexoDM_Role.PARTICIPANT)
            {
                ApplyFreezeToPlayer(player);
                player.NexoDM_ResetVitalsSent();
                SendHudJoinState(player, activeEntry);
                BroadcastHudSelf();
            }
        }
#endif
    }

    void OnPlayerReconnect(PlayerIdentity identity, PlayerBase player)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (!identity || !player)
            return;

        // #9: a player frozen at disconnect stays frozen in the reconnect cache;
        // if no freeze is active now, thaw them (freeze paths below re-freeze if needed).
        if (!m_FreezeActive)
        {
            // R2: Entity.DisableSimulation(bool) in scripts/3_game/entities/entity.c:3.
            player.DisableSimulation(false);
            // R2: Object.SetAllowDamage(bool) in scripts/3_game/entities/object.c:1192.
            player.SetAllowDamage(true);
        }

        // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
        string steamId = identity.GetPlainId();
        if (steamId == string.Empty)
            return;

        NexoDM_PlayerEntry entry = GetEntry(steamId);
        if (entry)
        {
            // F3 p8: reconnect in the EndRound->ReturnToIdle window (state ENDED, entries
            // not yet cleared) must end with role NONE, not the stale PARTICIPANT.
            if (m_State == NexoDM_State.ENDED || m_State == NexoDM_State.IDLE)
            {
                entry.role = NexoDM_Role.NONE;
                ApplyRoleToPlayer(player, NexoDM_Role.NONE);
                return;
            }

            ApplyRoleToPlayer(player, entry.role);
            if (entry.role == NexoDM_Role.PARTICIPANT && m_State == NexoDM_State.ACTIVE)
            {
                // F-07: re-equip so the reconnected participant's inventory matches the round loadout.
                if (EnsureConfig() && player.IsAlive())
                {
                    // F4: wipe lives inside EquipPlayer, after both presets resolve.
                    // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
                    EquipPlayer(player, entry.role);
                }
                // F3 p8: reconnect during the freeze window freezes like everyone else.
                ApplyFreezeToPlayer(player);
                player.NexoDM_ResetVitalsSent();
                SendHudJoinState(player, entry);
                BroadcastHudSelf();
            }
            return;
        }

        if (m_State == NexoDM_State.ACTIVE)
        {
            entry = GetOrCreateEntry(steamId);
            entry.role = NexoDM_Role.PARTICIPANT;
            if (player.IsAlive() && EnsureConfig())
            {
                vector spawn = PickRandomSpawn();
                player.SetPosition(spawn);
                // F4: wipe lives inside EquipPlayer, after both presets resolve.
                EquipPlayer(player, entry.role);
            }
            ApplyRoleToPlayer(player, entry.role);
            // F3 p8: new participant via reconnect during the freeze window freezes too.
            ApplyFreezeToPlayer(player);
            player.NexoDM_ResetVitalsSent();
            SendHudJoinState(player, entry);
            BroadcastHudSelf();
            return;
        }

        // F1 (was m_PendingPublicCleanup, GG-07/F-22): a participant who logged out during a round and
        // reconnects after it ended gets the same PlayerBase back with its synced role still PARTICIPANT;
        // reset it so the client does not keep the scoreboard open and polling while IDLE.
        ApplyRoleToPlayer(player, NexoDM_Role.NONE);
#endif
    }

    protected void AnnounceWinner(string reason, string winnerSteamId)
    {
#ifdef SERVER
        string adminStop = "admin_stop";
        if (reason == adminStop)
            return;

        string winnerName = GetPlayerName(winnerSteamId);
        string message;
        if (winnerName == string.Empty)
        {
            message = NexoDM_Constants.MSG_NO_WINNER;
        }
        else
        {
            message = NexoDM_Constants.MSG_WINNER_PREFIX;
            message = message + winnerName;
        }

        // 3_Game bus -> 5_Mission listener -> LBChatHandler.SendSimpleChatMessage (broadcast).
        NexoDM_AnnounceBus.Raise(null, message, true);
#endif
    }

    // G3-G5: answer a client's SCOREBOARD_REQ with the participant table (server-authoritative).
    // Serialized as count + per-row [name, kills, assists, deaths, points]; the client
    // reads the same order in NexoDM_PlayerBase.NexoDM_HandleScoreboardDataRPC.
    void OnScoreboardRequest(PlayerIdentity sender)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (!sender)
            return;

        // R2: PlayerIdentity.GetPlayer() in scripts/3_game/gameplay.c:374-375.
        PlayerBase target = PlayerBase.Cast(sender.GetPlayer());
        if (!target)
        {
            // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
            string steamId = sender.GetPlainId();
            target = FindPlayerBySteamId(steamId);
        }

        if (!target)
            return;

        EnsureCollections();

        // G5/access gate: only serve the participant table during an active round, and only to a
        // participant (blocks the empty-board-refresh-forever when idle).
        if (m_State != NexoDM_State.ACTIVE)
            return;

        NexoDM_PlayerEntry senderEntry = GetEntry(sender.GetPlainId());
        if (!senderEntry || senderEntry.role != NexoDM_Role.PARTICIPANT)
            return;

        // Collect participants then rank them by the tiebreak (kills>assists>deaths) so the
        // scoreboard Rank column is meaningful. Selection sort, best first (count <= MaxPlayers).
        array<NexoDM_PlayerEntry> ranked = new array<NexoDM_PlayerEntry>;
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry counted = m_PlayerEntries.GetElement(index);
            // F-01: only rank participants whose player is connected (no ghost rows).
            if (counted && counted.role == NexoDM_Role.PARTICIPANT && IsEntryOnline(counted))
                ranked.Insert(counted);
        }

        int a = 0;
        int b = 0;
        for (a = 0; a < ranked.Count(); a = a + 1)
        {
            int best = a;
            for (b = a + 1; b < ranked.Count(); b = b + 1)
            {
                if (IsBetterByTiebreak(ranked.Get(b), ranked.Get(best)))
                    best = b;
            }
            if (best != a)
            {
                NexoDM_PlayerEntry swap = ranked.Get(a);
                ranked.Set(a, ranked.Get(best));
                ranked.Set(best, swap);
            }
        }

        ScriptRPC rpc = new ScriptRPC();
        int count = ranked.Count();
        // R2: Serializer.Write(void) in scripts/1_core/proto/serializer.c:57 (raw primitives/strings).
        rpc.Write(count);

        for (a = 0; a < count; a = a + 1)
        {
            NexoDM_PlayerEntry entry = ranked.Get(a);
            string name = GetPlayerName(entry.steamId);
            int rKills = entry.kills;
            int rAssists = entry.assists;
            int rDeaths = entry.deaths;
            int rPoints = entry.points;

            rpc.Write(name);
            rpc.Write(rKills);
            rpc.Write(rAssists);
            rpc.Write(rDeaths);
            rpc.Write(rPoints);
        }

        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        rpc.Send(target, NexoDM_RPC.SCOREBOARD_DATA, true, sender);
#endif
    }

    // F3 p6: pure map rotation. Depends only on its arguments and Math.RandomInt.
    // count>=3: random index that is neither current nor previous; count==2: the other;
    // count<=1: the same (0). Out-of-range current (first round) picks any index.
    int PickNextMapIndex(int count, int current, int previous)
    {
        if (count <= 1)
            return 0;

        if (current < 0 || current >= count)
        {
            // R2: Math.RandomInt(int,int) max-exclusive in scripts/1_core/proto/enmath.c:38.
            return Math.RandomInt(0, count);
        }

        if (count == 2)
        {
            if (current == 0)
                return 1;
            return 0;
        }

        int picked = current;
        int guard = 0;
        while (guard < 64)
        {
            // R2: Math.RandomInt(int,int) max-exclusive in scripts/1_core/proto/enmath.c:38.
            picked = Math.RandomInt(0, count);
            if (picked != current && picked != previous)
                return picked;
            guard = guard + 1;
        }
        // #7: deterministic fallback - first index distinct from current and previous.
        int fallback = 0;
        for (fallback = 0; fallback < count; fallback = fallback + 1)
        {
            if (fallback != current && fallback != previous)
                return fallback;
        }
        return picked;
    }

    protected NexoDM_ParsedMap GetActiveMap()
    {
        if (!m_Config)
            return null;

        // R2: NexoDM_Config.GetValidMaps() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:327.
        array<ref NexoDM_ParsedMap> maps = m_Config.GetValidMaps();
        if (!maps || maps.Count() == 0)
            return null;

        int idx = m_ActiveMapIndex;
        if (idx < 0 || idx >= maps.Count())
            idx = 0;
        return maps.Get(idx);
    }

    protected vector PickRandomSpawn()
    {
        DayZGame game = GetGame();
        if (!game)
        {
            // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
            return Vector(0, 0, 0);
        }

        // R2: NexoDM_Config.GetValidMaps() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:321.
        array<ref NexoDM_ParsedMap> maps = m_Config.GetValidMaps();
        if (!maps || maps.Count() == 0)
        {
            // R2: CGame.SurfaceY(float,float) in scripts/3_game/global/game.c:1162.
            float originY = game.SurfaceY(0, 0);
            // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
            return Vector(0, originY, 0);
        }

        // F3 p6: the active arena is m_ActiveMapIndex (rotated in EndRound).
        NexoDM_ParsedMap activeMap = GetActiveMap();
        if (!activeMap || !activeMap.SpawnGroups || activeMap.SpawnGroups.Count() == 0)
        {
            float emptyY = game.SurfaceY(0, 0);
            return Vector(0, emptyY, 0);
        }

        int groupCount = activeMap.SpawnGroups.Count();
        // R2: Math.RandomInt(min,max) is max-exclusive (enforce-script-reference Math section).
        int groupIndex = Math.RandomInt(0, groupCount);
        NexoDM_ParsedSpawnGroup group = activeMap.SpawnGroups.Get(groupIndex);
        if (!group || !group.Points || group.Points.Count() == 0)
        {
            float groupY = game.SurfaceY(0, 0);
            return Vector(0, groupY, 0);
        }

        int pointCount = group.Points.Count();
        int pointIndex = Math.RandomInt(0, pointCount);
        int flatIndex = FlatSpawnIndex(activeMap, groupIndex, pointIndex);
        int totalPoints = CountSpawnPoints(activeMap);
        if (totalPoints > 1 && flatIndex == m_SpawnRoundRobinIdx)
        {
            pointIndex = pointIndex + 1;
            if (pointIndex >= pointCount)
            {
                pointIndex = 0;
                if (groupCount > 1)
                {
                    groupIndex = groupIndex + 1;
                    if (groupIndex >= groupCount)
                        groupIndex = 0;
                    group = activeMap.SpawnGroups.Get(groupIndex);
                    if (group && group.Points && group.Points.Count() > 0)
                        pointCount = group.Points.Count();
                }
            }
            if (group && group.Points && pointIndex >= 0 && pointIndex < group.Points.Count())
                flatIndex = FlatSpawnIndex(activeMap, groupIndex, pointIndex);
        }
        m_SpawnRoundRobinIdx = flatIndex;

        vector base = group.Points.Get(pointIndex);
        float radius = activeMap.SpawnRadius;
        float minRadius = 0.0 - radius;
        // R2: Math.RandomFloat(float,float) in scripts/1_core/proto/enmath.c:91.
        float dx = Math.RandomFloat(minRadius, radius);
        float dz = Math.RandomFloat(minRadius, radius);
        float x = base[0] + dx;
        float z = base[2] + dz;
        // Y stays the configured point. SurfaceY would drop players under the road mesh.
        float y = base[1];
        // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
        return Vector(x, y, z);
    }

    protected int CountSpawnPoints(NexoDM_ParsedMap mapCfg)
    {
        int total = 0;
        if (!mapCfg || !mapCfg.SpawnGroups)
            return total;

        int groupIndex = 0;
        for (groupIndex = 0; groupIndex < mapCfg.SpawnGroups.Count(); groupIndex = groupIndex + 1)
        {
            NexoDM_ParsedSpawnGroup group = mapCfg.SpawnGroups.Get(groupIndex);
            if (group && group.Points)
                total = total + group.Points.Count();
        }
        return total;
    }

    protected int FlatSpawnIndex(NexoDM_ParsedMap mapCfg, int groupIndex, int pointIndex)
    {
        int flat = pointIndex;
        if (!mapCfg || !mapCfg.SpawnGroups)
            return flat;

        int i = 0;
        for (i = 0; i < groupIndex && i < mapCfg.SpawnGroups.Count(); i = i + 1)
        {
            NexoDM_ParsedSpawnGroup group = mapCfg.SpawnGroups.Get(i);
            if (group && group.Points)
                flat = flat + group.Points.Count();
        }
        return flat;
    }

    // E_CFG_PRESET in 4_World. 3_Game has already rejected empty names. A missing LBmaster preset rejects the round.
    protected bool LoadoutPresetsExist()
    {
        if (!m_Config)
            return false;

        // No Trim here: look up the exact string EquipPlayer will spawn, so a name that only resolves after
        // trimming fails closed instead of passing validation and spawning players without the preset.
        string clothing = m_Config.GetClothingPreset();
        array<string> loadouts = m_Config.GetLoadouts();
        bool resolved = true;

        if (!LB_PresetLoader.Get)
        {
            LogMissingPreset(NexoDM_Constants.KEY_CLOTHING_PRESET, clothing);
            if (loadouts)
            {
                int missingIndex = 0;
                for (missingIndex = 0; missingIndex < loadouts.Count(); missingIndex = missingIndex + 1)
                    LogMissingPreset(NexoDM_Constants.KEY_LOADOUTS, loadouts.Get(missingIndex));
            }
            return false;
        }

        // R2: LB_PresetLoader.GetPreset(string) in LBmaster_Core/scripts/4_World/LBmaster_Core/Config/LB_PresetLoader.c:146.
        LB_PresetBase clothingPreset = LB_PresetLoader.Get.GetPreset(clothing);
        if (!clothingPreset)
        {
            LogMissingPreset(NexoDM_Constants.KEY_CLOTHING_PRESET, clothing);
            resolved = false;
        }

        if (loadouts)
        {
            int nameIndex = 0;
            for (nameIndex = 0; nameIndex < loadouts.Count(); nameIndex = nameIndex + 1)
            {
                string loadoutName = loadouts.Get(nameIndex);
                LB_PresetBase loadoutPreset = LB_PresetLoader.Get.GetPreset(loadoutName);
                if (!loadoutPreset)
                {
                    LogMissingPreset(NexoDM_Constants.KEY_LOADOUTS, loadoutName);
                    resolved = false;
                }
            }
        }

        return resolved;
    }

    // One line per missing preset. Prefix is the config log, not RoundManager, and it is not rate-limited.
    protected void LogMissingPreset(string fieldName, string presetName)
    {
        string trimmedName = presetName.Trim();
        string message = NexoDM_Constants.LOG_PREFIX;
        message = message + NexoDM_Constants.RULE_PRESET;
        message = message + " ";
        message = message + fieldName;
        message = message + " ";
        message = message + trimmedName;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }

    protected void WipeInventory(PlayerBase player)
    {
        if (!player)
            return;

        DayZGame game = GetGame();
        if (!game)
            return;

        // HM prior art: hands to attachments to cargo wipe in _recovered/HM_Core_Module/Scripts/HM_Battle_Royale/MissionServer.c:1158-1193.
        // R2: Man.GetHumanInventory() in scripts/3_game/entities/man.c:79.
        HumanInventory humanInventory = player.GetHumanInventory();
        if (humanInventory)
        {
            // R2: HumanInventory.GetEntityInHands() in scripts/3_game/systems/inventory/humaninventory.c:16.
            EntityAI handItem = humanInventory.GetEntityInHands();
            if (handItem)
            {
                // R2: CGame.ObjectDelete(Object) in scripts/3_game/global/game.c:704.
                game.ObjectDelete(handItem);
            }
        }

        // R2: EntityAI.GetInventory() in scripts/3_game/entities/entityai.c:1834.
        GameInventory inventory = player.GetInventory();
        if (!inventory)
            return;

        // R2: GameInventory.AttachmentCount() in scripts/3_game/systems/inventory/inventory.c:205.
        int attachmentCount = inventory.AttachmentCount();
        int attachmentIndex = 0;
        for (attachmentIndex = attachmentCount - 1; attachmentIndex >= 0; attachmentIndex = attachmentIndex - 1)
        {
            // R2: GameInventory.GetAttachmentFromIndex(int) in scripts/3_game/systems/inventory/inventory.c:220.
            EntityAI attachment = inventory.GetAttachmentFromIndex(attachmentIndex);
            if (attachment)
            {
                WipeEntityCargo(attachment);
                // R2: CGame.ObjectDelete(Object) in scripts/3_game/global/game.c:704.
                game.ObjectDelete(attachment);
            }
        }

        WipeEntityCargo(player);
    }

    void WipeWeaponsOnly(PlayerBase player)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (!player)
            return;

        if (!m_Config)
            return;

        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: Man.GetHumanInventory() in scripts/3_game/entities/man.c:79.
        HumanInventory humanInventory = player.GetHumanInventory();
        if (humanInventory)
        {
            // R2: HumanInventory.GetEntityInHands() in scripts/3_game/systems/inventory/humaninventory.c:16.
            EntityAI handItem = humanInventory.GetEntityInHands();
            if (ShouldDeleteWeaponLoadoutItem(handItem))
            {
                DeleteWeaponLoadoutEntityTree(handItem, game);
            }
            else
            {
                WipeWeaponLoadoutChildren(handItem, game);
            }
        }

        WipeWeaponLoadoutChildren(player, game);
#endif
    }

    protected void WipeEntityCargo(EntityAI container)
    {
        if (!container)
            return;

        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: EntityAI.GetInventory() in scripts/3_game/entities/entityai.c:1834.
        GameInventory inventory = container.GetInventory();
        if (!inventory)
            return;

        // R2: GameInventory.GetCargo() in scripts/3_game/systems/inventory/inventory.c:138.
        CargoBase cargo = inventory.GetCargo();
        if (!cargo)
            return;

        // R2: CargoBase.GetItemCount() in scripts/3_game/systems/inventory/cargo.c:28.
        int cargoCount = cargo.GetItemCount();
        int cargoIndex = 0;
        for (cargoIndex = cargoCount - 1; cargoIndex >= 0; cargoIndex = cargoIndex - 1)
        {
            // R2: CargoBase.GetItem(int) in scripts/3_game/systems/inventory/cargo.c:32.
            EntityAI cargoItem = cargo.GetItem(cargoIndex);
            if (cargoItem)
            {
                // R2: CGame.ObjectDelete(Object) in scripts/3_game/global/game.c:704.
                game.ObjectDelete(cargoItem);
            }
        }
    }

    protected void WipeWeaponLoadoutChildren(EntityAI container, DayZGame game)
    {
        if (!container)
            return;

        if (!game)
            return;

        // R2: EntityAI.GetInventory() in scripts/3_game/entities/entityai.c:1834.
        GameInventory inventory = container.GetInventory();
        if (!inventory)
            return;

        // R2: GameInventory.AttachmentCount() in scripts/3_game/systems/inventory/inventory.c:205.
        int attachmentCount = inventory.AttachmentCount();
        int attachmentIndex = 0;
        for (attachmentIndex = attachmentCount - 1; attachmentIndex >= 0; attachmentIndex = attachmentIndex - 1)
        {
            // R2: GameInventory.GetAttachmentFromIndex(int) in scripts/3_game/systems/inventory/inventory.c:220.
            EntityAI attachment = inventory.GetAttachmentFromIndex(attachmentIndex);
            if (!attachment)
                continue;

            if (ShouldDeleteWeaponLoadoutItem(attachment))
            {
                DeleteWeaponLoadoutEntityTree(attachment, game);
            }
            else
            {
                WipeWeaponLoadoutChildren(attachment, game);
            }
        }

        // R2: GameInventory.GetCargo() in scripts/3_game/systems/inventory/inventory.c:138.
        CargoBase cargo = inventory.GetCargo();
        if (!cargo)
            return;

        // R2: CargoBase.GetItemCount() in scripts/3_game/systems/inventory/cargo.c:28.
        int cargoCount = cargo.GetItemCount();
        int cargoIndex = 0;
        for (cargoIndex = cargoCount - 1; cargoIndex >= 0; cargoIndex = cargoIndex - 1)
        {
            // R2: CargoBase.GetItem(int) in scripts/3_game/systems/inventory/cargo.c:32.
            EntityAI cargoItem = cargo.GetItem(cargoIndex);
            if (!cargoItem)
                continue;

            if (ShouldDeleteWeaponLoadoutItem(cargoItem))
            {
                DeleteWeaponLoadoutEntityTree(cargoItem, game);
            }
            else
            {
                WipeWeaponLoadoutChildren(cargoItem, game);
            }
        }
    }

    protected void DeleteWeaponLoadoutEntityTree(EntityAI item, DayZGame game)
    {
        if (!item)
            return;

        if (!game)
            return;

        // ObjectDelete is native and does not document script-visible cascade for inventory children,
        // so S4 deletes weapon children explicitly before deleting the parent.
        // R2: EntityAI.GetInventory() in scripts/3_game/entities/entityai.c:1834.
        GameInventory inventory = item.GetInventory();
        if (inventory)
        {
            // R2: GameInventory.AttachmentCount() in scripts/3_game/systems/inventory/inventory.c:205.
            int attachmentCount = inventory.AttachmentCount();
            int attachmentIndex = 0;
            for (attachmentIndex = attachmentCount - 1; attachmentIndex >= 0; attachmentIndex = attachmentIndex - 1)
            {
                // R2: GameInventory.GetAttachmentFromIndex(int) in scripts/3_game/systems/inventory/inventory.c:220.
                EntityAI attachment = inventory.GetAttachmentFromIndex(attachmentIndex);
                DeleteWeaponLoadoutEntityTree(attachment, game);
            }

            // R2: GameInventory.GetCargo() in scripts/3_game/systems/inventory/inventory.c:138.
            CargoBase cargo = inventory.GetCargo();
            if (cargo)
            {
                // R2: CargoBase.GetItemCount() in scripts/3_game/systems/inventory/cargo.c:28.
                int cargoCount = cargo.GetItemCount();
                int cargoIndex = 0;
                for (cargoIndex = cargoCount - 1; cargoIndex >= 0; cargoIndex = cargoIndex - 1)
                {
                    // R2: CargoBase.GetItem(int) in scripts/3_game/systems/inventory/cargo.c:32.
                    EntityAI cargoItem = cargo.GetItem(cargoIndex);
                    DeleteWeaponLoadoutEntityTree(cargoItem, game);
                }
            }
        }

        // R2: CGame.ObjectDelete(Object) in scripts/3_game/global/game.c:704.
        game.ObjectDelete(item);
    }

    protected bool ShouldDeleteWeaponLoadoutItem(EntityAI item)
    {
        if (!item)
            return false;

        if (m_Config)
        {
            // R2: NexoDM_Config.GetKnife() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:270.
            string knife = m_Config.GetKnife();
            // R2: Object.IsKindOf(string) in scripts/3_game/entities/object.c:517.
            if (knife != string.Empty && item.IsKindOf(knife))
                return false;
        }

        string weaponBase = "Weapon_Base";
        // R2: Weapon_Base bound to config class in scripts/4_world/entities/firearms/weapon_base.c:36-38.
        if (item.IsKindOf(weaponBase))
            return true;

        string magazineBase = "Magazine_Base";
        // R2: Magazine_Base typedef in scripts/4_world/entities/itembase/magazine/magazine.c:1.
        if (item.IsKindOf(magazineBase))
            return true;

        string ammunitionBase = "Ammunition_Base";
        // R2: Ammunition_Base extends Magazine_Base in scripts/4_world/entities/itembase/magazine/ammunitionpiles.c:2.
        if (item.IsKindOf(ammunitionBase))
            return true;

        string ammoBox = "Box_Base";
        // R2: Box_Base (ammo boxes) in scripts/4_world/entities/itembase/ammoboxes.c:1-16.
        if (item.IsKindOf(ammoBox))
            return true;

        return false;
    }

    protected bool EquipPlayer(PlayerBase player, int role)
    {
        if (!player)
            return false;

        if (!m_Config)
            return false;

        if (role != NexoDM_Role.PARTICIPANT)
            return false;

        // F4: resolve clothing and one random loadout BEFORE wiping. A miss logs and leaves the
        // inventory untouched (SpawnPreset only adds).
        // R2: NexoDM_Config.GetClothingPreset() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:265.
        string clothing = m_Config.GetClothingPreset();
        // R2: NexoDM_Config.GetLoadouts() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:270.
        array<string> guns = m_Config.GetLoadouts();
        if (clothing == string.Empty)
        {
            LogRule("PRESET_RESOLVE_FAILED_CLOTHING", clothing);
            return false;
        }

        if (!guns || guns.Count() <= 0)
        {
            LogRule("PRESET_RESOLVE_FAILED_GUN", "");
            return false;
        }

        // R2: Math.RandomInt(int,int) max-exclusive in scripts/1_core/proto/enmath.c:38.
        int gunIndex = Math.RandomInt(0, guns.Count());
        string gunName = guns.Get(gunIndex);

        if (!LB_PresetLoader.Get)
        {
            LogRule("PRESET_LOADER_UNAVAILABLE_CLOTHING", clothing);
            LogRule("PRESET_LOADER_UNAVAILABLE_GUN", gunName);
            return false;
        }

        // R2: LB_PresetLoader.GetPreset(string) in LBmaster_Core/scripts/4_World/LBmaster_Core/Config/LB_PresetLoader.c:146.
        LB_PresetBase clothingPreset = LB_PresetLoader.Get.GetPreset(clothing);
        if (!clothingPreset)
        {
            LogRule("PRESET_RESOLVE_FAILED_CLOTHING", clothing);
            return false;
        }

        LB_PresetBase gunPreset = LB_PresetLoader.Get.GetPreset(gunName);
        if (!gunPreset)
        {
            LogRule("PRESET_RESOLVE_FAILED_GUN", gunName);
            return false;
        }

        WipeInventory(player);
        // R2: LB_PresetBase.SpawnPreset(PlayerBase,EntityAI) in LBmaster_Core/scripts/4_World/LBmaster_Core/Struct/LB_PresetBase.c:138.
        clothingPreset.SpawnPreset(player, player);
        gunPreset.SpawnPreset(player, player);
        // Chamber + load the freshly spawned weapon and bind it to the hotbar.
        MakeWeaponReady(player);

        // Knife (hotbar slot 2) + bandage (hotbar slot 3) are inventory items - NOT visible to other
        // clients - so both their CREATION and their bind are DEFERRED off this frame (3a). On a
        // participant birth EquipPlayer runs inside character creation; spawning them synchronously
        // there added to the server-frame stall every respawn (all clients hitched ~1s). Only the
        // visible loadout (clothing + weapon-in-hands) stays synchronous for the GG-01v5/v6 appearance.
        // R2: ScriptCallQueue.CallLater(func,int,bool,...) in scripts/2_gamelib/tools.c:61.
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(GG_GiveKnifeAndBandage, 0, false, player);

        PlayerIdentity equippedIdentity = player.GetIdentity();
        if (equippedIdentity)
        {
            // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
            player.m_GG_SteamId = equippedIdentity.GetPlainId();
        }
        return true;
    }

    // Deferred (next tick) creation + hotbar bind of the knife (slot 2) and bandage (slot 3). These are
    // inventory items, invisible to other clients, so deferring their CREATION off the character-creation
    // frame is desync-safe and trims the birth-frame server stall (3a). Bind is inline here: by next tick
    // the FSM is stable and the character is released, so the sync-quickbar-during-birth hazard that
    // originally forced the bind to be deferred does not apply. Server-only.
    void GG_GiveKnifeAndBandage(PlayerBase player)
    {
#ifdef SERVER
        if (!GG_DeferredEquipStillActive(player))
            return;

        if (!m_Config)
            return;

        // R2: EntityAI.GetInventory() in scripts/3_game/entities/entityai.c:1834.
        GameInventory inventory = player.GetInventory();
        if (!inventory)
            return;

        // R2: NexoDM_Config.GetKnife() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:276.
        string knife = m_Config.GetKnife();
        if (knife != string.Empty)
        {
            // F4: reuse the preset knife when one is already on the player; do not spawn a second.
            EntityAI knifeItem = GG_TakeSingleOfType(player, knife);
            if (!knifeItem)
            {
                // R2: GameInventory.CreateInInventory(string) in scripts/3_game/systems/inventory/inventory.c:876.
                knifeItem = inventory.CreateInInventory(knife);
            }
            // R2: PlayerBase.SetQuickBarEntityShortcut(EntityAI,int,bool) in scripts/4_world/entities/manbase/playerbase.c:2232.
            if (knifeItem)
                player.SetQuickBarEntityShortcut(knifeItem, 1, true);
        }

        // Bandage on hotbar slot 3 (index 2). Independent of the Knife config (m1). Scope recorded in
        // product-spec Changelog 2026-06-14 (C3/F2 amended). F4: a BandageDressing already spawned by
        // the preset is bound instead of creating another.
        string bandageType = "BandageDressing";
        EntityAI bandageItem = GG_TakeSingleOfType(player, bandageType);
        if (!bandageItem)
        {
            // R2: GameInventory.CreateInInventory(string) in scripts/3_game/systems/inventory/inventory.c:876.
            bandageItem = inventory.CreateInInventory(bandageType);
        }
        if (bandageItem)
            player.SetQuickBarEntityShortcut(bandageItem, 2, true);
#endif
    }

    // F4: first carried item of typeName stays; any further match is deleted so the hotbar slot
    // binds a single object. Returns null when the player has none. Server-only.
    protected EntityAI GG_TakeSingleOfType(PlayerBase player, string typeName)
    {
#ifdef SERVER
        if (!player)
            return null;

        if (typeName == string.Empty)
            return null;

        // R2: EntityAI.GetInventory() in scripts/3_game/entities/entityai.c:1834.
        GameInventory inv = player.GetInventory();
        if (!inv)
            return null;

        array<EntityAI> items = new array<EntityAI>();
        // R2: GameInventory.EnumerateInventory(InventoryTraversalType,out array<EntityAI>) in scripts/3_game/systems/inventory/inventory.c:127; InventoryTraversalType.PREORDER in scripts/3_game/gameplay.c:7.
        inv.EnumerateInventory(InventoryTraversalType.PREORDER, items);

        array<EntityAI> matches = new array<EntityAI>();
        int scanIndex = 0;
        for (scanIndex = 0; scanIndex < items.Count(); scanIndex = scanIndex + 1)
        {
            EntityAI candidate = items.Get(scanIndex);
            // R2: Object.IsKindOf(string) in scripts/3_game/entities/object.c:517.
            if (candidate && candidate != player && candidate.IsKindOf(typeName))
                matches.Insert(candidate);
        }

        if (matches.Count() == 0)
            return null;

        EntityAI kept = matches.Get(0);
        DayZGame game = GetGame();
        int extraIndex = 1;
        for (extraIndex = 1; extraIndex < matches.Count(); extraIndex = extraIndex + 1)
        {
            EntityAI extraItem = matches.Get(extraIndex);
            if (!extraItem || !game)
                continue;

            // A duplicate sitting inside another duplicate goes with that parent's tree delete.
            bool underOtherMatch = false;
            int priorIndex = 1;
            for (priorIndex = 1; priorIndex < extraIndex; priorIndex = priorIndex + 1)
            {
                if (GG_EntityIsUnder(extraItem, matches.Get(priorIndex)))
                    underOtherMatch = true;
            }
            if (underOtherMatch)
                continue;

            DeleteWeaponLoadoutEntityTree(extraItem, game);
        }

        return kept;
#else
        return null;
#endif
    }

    // True when item hangs under ancestor (attachment or cargo). Used so a nested duplicate is not
    // deleted twice. Server-only.
    protected bool GG_EntityIsUnder(EntityAI item, EntityAI ancestor)
    {
        if (!item || !ancestor)
            return false;

        // R2: EntityAI.GetHierarchyParent() in scripts/3_game/entities/entityai.c (same file as GetHierarchyRootPlayer, cited at :877 above).
        EntityAI above = item.GetHierarchyParent();
        int guard = 0;
        while (above && guard < 32)
        {
            if (above == ancestor)
                return true;
            above = above.GetHierarchyParent();
            guard = guard + 1;
        }
        return false;
    }

    // GG-01v5 birth-equip resolution: decide whether a fresh character about to be created belongs to a
    // participant in an ACTIVE round, and if so return the gungame spawn so MissionServer can
    // create the character AT that spawn (no double-load lag) and equip it during creation. Only mutation:
    // enrolling a player with no entry as participant (F1); PickRandomSpawn's random pick is consumed once by the caller (OnPlayerRespawn
    // skips its own PickRandomSpawn for this player via the bornEquipped guard).
    bool ResolveParticipantBirthSpawn(string steamId, out vector spawn)
    {
        spawn = vector.Zero;
#ifdef SERVER
        if (!CanMutateState())
            return false;

        if (steamId == string.Empty)
            return false;

        if (m_State != NexoDM_State.ACTIVE)
            return false;

        // F1: with no public role, a player joining an ACTIVE round is a participant from birth, so
        // enroll them here and they are born at the arena already equipped (GG-01v5), instead of being
        // born at a vanilla spawn and teleported + re-equipped on the next tick by OnPlayerReady.
        NexoDM_PlayerEntry entry = GetEntry(steamId);
        if (!entry)
        {
            entry = GetOrCreateEntry(steamId);
            entry.role = NexoDM_Role.PARTICIPANT;
        }

        if (entry.role != NexoDM_Role.PARTICIPANT)
            return false;

        if (!EnsureConfig())
            return false;

        spawn = PickRandomSpawn();
        return true;
#else
        return false;
#endif
    }

    // GG-01v5: equip a participant DURING character creation (called from MissionServer.StartingEquipSetup,
    // before the engine releases the character to the network), so observers receive it already dressed.
    // Wipes the vanilla default loadout EquipCharacter just applied, then equips the gun preset. Marks
    // the steamId so the post-birth OnPlayerRespawn skips the (desyncing) teleport+wipe+equip path.
    void EquipParticipantAtBirth(PlayerBase player, string steamId)
    {
#ifdef SERVER
        if (!player)
            return;

        // F4: wipe lives inside EquipPlayer, after both presets resolve.
        bool equipped = EquipPlayer(player, NexoDM_Role.PARTICIPANT);
        if (!equipped)
            return;

        if (!m_GG_BirthEquipped)
            m_GG_BirthEquipped = new set<string>();
        if (steamId != string.Empty && m_GG_BirthEquipped.Find(steamId) < 0)
            m_GG_BirthEquipped.Insert(steamId);
#endif
    }

    protected void BroadcastCountdown(PlayerIdentity recipient, int seconds)
    {
#ifdef SERVER
        if (!recipient)
            return;

        if (seconds <= 0)
            return;

        // R2: PlayerIdentity.GetPlayer() in scripts/3_game/gameplay.c:374-375.
        PlayerBase targetPlayer = PlayerBase.Cast(recipient.GetPlayer());
        if (!targetPlayer)
        {
            // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
            string steamId = recipient.GetPlainId();
            targetPlayer = FindPlayerBySteamId(steamId);
        }

        if (!targetPlayer)
        {
            LogRule("COUNTDOWN_NO_TARGET", "target player not found");
            return;
        }

        ScriptRPC rpc = new ScriptRPC();
        Param1<int> payload = new Param1<int>(seconds);
        // R2: Serializer.Write(void) in scripts/1_core/proto/serializer.c:57.
        rpc.Write(payload);
        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        rpc.Send(targetPlayer, NexoDM_RPC.COUNTDOWN, true, recipient);
#endif
    }

    void EndFreeze()
    {
#ifdef SERVER
        m_FreezeActive = false;
        SetFreezeForEntries(false);
        string message = "round freeze ended; entering live combat";
        LogRound(message);
#endif
    }

    // F3 p8: a participant joining during the initial freeze is frozen/immune like the rest.
    protected void ApplyFreezeToPlayer(PlayerBase player)
    {
#ifdef SERVER
        if (!player)
            return;
        if (!m_FreezeActive)
            return;
        if (m_State != NexoDM_State.ACTIVE)
            return;
        // R2: Entity.DisableSimulation(bool) in scripts/3_game/entities/entity.c:3.
        player.DisableSimulation(true);
        // R2: Object.SetAllowDamage(bool) in scripts/3_game/entities/object.c:1192.
        player.SetAllowDamage(false);
#endif
    }

    protected void SetFreezeForEntries(bool frozen)
    {
#ifdef SERVER
        EnsureCollections();

        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry)
                continue;

            // Only participants are ever frozen (BeginRound gates the freeze on PARTICIPANT).
            if (entry.role != NexoDM_Role.PARTICIPANT)
                continue;

            PlayerBase player = FindPlayerBySteamId(entry.steamId);
            if (!player)
                continue;

            // R2: Entity.DisableSimulation(bool) in scripts/3_game/entities/entity.c:3.
            player.DisableSimulation(frozen);

            if (frozen)
            {
                // R2: Object.SetAllowDamage(bool) in scripts/3_game/entities/object.c:1192.
                player.SetAllowDamage(false);
            }
            else
            {
                // R2: Object.SetAllowDamage(bool) in scripts/3_game/entities/object.c:1192.
                player.SetAllowDamage(true);
            }
        }
#endif
    }

    protected void ScheduleIdleReset()
    {
#ifdef SERVER
        EnsureTimers();
        StopTimer(m_EndStateTimer);
        float resetSeconds = 0.1;
        string callbackName = "ReturnToIdle";
        // R2: Timer.Run(float,Managed,string,Param,bool) in scripts/3_game/tools/tools.c:595.
        m_EndStateTimer.Run(resetSeconds, this, callbackName);
#endif
    }

    void ReturnToIdle()
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State == NexoDM_State.ENDED)
        {
            // BUG-001 (vault bug-ledger): stats survive ENDED so S7 can compute winner/scoreboard before IDLE clears.
            ResetStats();
            m_State = NexoDM_State.IDLE;
            string idleMessage = "round returned to idle";
            LogRound(idleMessage);

            // Unsupervised 24/7 loop: every transition back to idle re-arms the next round.
            ScheduleAutostartIfEnabled("round ended");
        }
#endif
    }

    // Unsupervised 24/7 mode (config Autostart + AutostartDelaySeconds): arms a StartGame attempt
    // after server boot (MissionServer.OnInit) and after each round returns to idle. StartGame's
    // own IDLE/config gates stay authoritative; this only schedules the attempt.
    void ScheduleAutostartIfEnabled(string reasonTag)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != NexoDM_State.IDLE)
            return;

        EnsureTimers();
        StopTimer(m_AutostartTimer);

        // F3 p1: config never loaded (Load() failed): arm a default-delay retry anyway.
        // OnAutostartTimer re-checks GetAutostart() once a config loads, so this cannot
        // start a round while Autostart is disabled.
        if (!EnsureConfig())
        {
            float fallbackDelay = NexoDM_Constants.DEFAULT_AUTOSTART_DELAY_VALUE;
            string fallbackCallback = "OnAutostartTimer";
            // R2: Timer.Run(float,Managed,string,Param,bool) in scripts/3_game/tools/tools.c:595.
            m_AutostartTimer.Run(fallbackDelay, this, fallbackCallback, NULL, false);
            string fallbackMessage = "autostart armed with default delay (config unavailable) (";
            fallbackMessage = fallbackMessage + reasonTag;
            fallbackMessage = fallbackMessage + ")";
            LogRound(fallbackMessage);
            return;
        }

        if (!m_Config.GetAutostart())
            return;

        int delaySeconds = m_Config.GetAutostartDelaySeconds();
        float delay = delaySeconds;
        string callbackName = "OnAutostartTimer";
        // R2: Timer.Run(float,Managed,string,Param,bool) in scripts/3_game/tools/tools.c:595.
        m_AutostartTimer.Run(delay, this, callbackName, NULL, false);

        string armedMessage = "autostart armed +";
        armedMessage = armedMessage + delaySeconds.ToString();
        armedMessage = armedMessage + "s (";
        armedMessage = armedMessage + reasonTag;
        armedMessage = armedMessage + ")";
        LogRound(armedMessage);
#endif
    }

    void OnAutostartTimer()
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != NexoDM_State.IDLE)
        {
            // A manually started round is in progress; its ReturnToIdle re-arms the loop.
            string busyMessage = "autostart skipped: round in progress";
            LogRound(busyMessage);
            return;
        }

        // F3 p1: a default-delay retry armed while the config was unloadable must not
        // start a round once a config loads with Autostart disabled.
        if (!EnsureConfig())
        {
            ScheduleAutostartIfEnabled("retry after rejected start");
            return;
        }
        if (!m_Config.GetAutostart())
        {
            string disabledMessage = "autostart skipped: disabled in config";
            LogRound(disabledMessage);
            return;
        }

        string firingMessage = "autostart firing";
        LogRound(firingMessage);
        StartGame();

        // A rejected start (config load/validate failure) leaves the state IDLE: re-arm so a
        // live config fix resumes the unsupervised loop without waiting for the next restart.
        if (m_State == NexoDM_State.IDLE)
            ScheduleAutostartIfEnabled("retry after rejected start");
#endif
    }

    void OnRoundTimeout()
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        // Fail-closed: only an ACTIVE round can time out. Also guards the victory/timeout race
        // (a victory already moved state to ENDED, so this becomes a no-op).
        if (m_State != NexoDM_State.ACTIVE)
            return;

        string winnerId = ComputeWinnerByTiebreak();
        string detail = "round timed out";
        LogRule(NexoDM_Constants.RULE_TIMEOUT_RECORDED, detail);

        string timeoutReason = "timeout";
        EndRound(timeoutReason, winnerId);
#endif
    }

    protected string ComputeWinnerByTiebreak()
    {
#ifdef SERVER
        EnsureCollections();

        string bestId = string.Empty;
        NexoDM_PlayerEntry best = null;
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry)
                continue;

            if (entry.role != NexoDM_Role.PARTICIPANT)
                continue;

            // F-01: a disconnected participant must not win the tiebreak (ghost winner).
            if (!IsEntryOnline(entry))
                continue;

            if (!best || IsBetterByTiebreak(entry, best))
            {
                best = entry;
                bestId = m_PlayerEntries.GetKey(index);
            }
        }

        return bestId;
#else
        return string.Empty;
#endif
    }

    // F3 p5: the instant of the last points change drives the final tiebreak (earlier wins).
    protected void AddPoints(NexoDM_PlayerEntry entry, int amount)
    {
#ifdef SERVER
        if (!entry)
            return;
        entry.points = entry.points + amount;
        // R2: DayZGame.GetTime() in scripts/3_game/global/game.c:1510.
        if (GetGame())
            entry.lastPointsChangeMs = GetGame().GetTime();
#endif
    }

    // F3 p3 (server): send the victim the credited killer's NetworkID + RespawnDelay.
    // No credited killer (or self-kill) goes out as 0,0: the client films its own corpse.
    protected void SendKillerCam(PlayerBase victim, PlayerBase killerPlayer)
    {
#ifdef SERVER
        if (!victim)
            return;

        if (!EnsureConfig())
            return;

        PlayerIdentity victimIdentity = victim.GetIdentity();
        if (!victimIdentity)
            return;

        int lo = 0;
        int hi = 0;
        if (killerPlayer && killerPlayer != victim)
        {
            // R2: Object.GetNetworkID(out int,out int) in scripts/3_game/entities/object.c:819.
            killerPlayer.GetNetworkID(lo, hi);
        }

        int respawnDelay = m_Config.GetRespawnDelay();
        ScriptRPC rpc = new ScriptRPC();
        Param3<int, int, int> payload = new Param3<int, int, int>(lo, hi, respawnDelay);
        // R2: Serializer.Write(void) in scripts/1_core/proto/serializer.c:57.
        rpc.Write(payload);
        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        rpc.Send(victim, NexoDM_RPC.KILLER_CAM, true, victimIdentity);
#endif
    }

    protected void SendHudJoinState(PlayerBase player, NexoDM_PlayerEntry entry)
    {
#ifdef SERVER
        if (!player || !entry)
            return;
        SendHudRoundTo(player);
        SendHudSelf(player, entry);
        SendHudTop5To(player);
        RequestHudTop5();
#endif
    }

    protected int HudRoundSecondsLeft()
    {
#ifdef SERVER
        if (m_State != NexoDM_State.ACTIVE)
            return 0;
        DayZGame game = GetGame();
        if (!game)
            return 0;
        int leftMs = m_RoundEndsAtMs - game.GetTime();
        if (leftMs <= 0)
            return 0;
        return leftMs / 1000;
#else
        return 0;
#endif
    }

    protected int HudKillsToWin()
    {
#ifdef SERVER
        if (!m_Config)
            return 0;
        return m_Config.GetKillsToWin();
#else
        return 0;
#endif
    }

    protected void SendHudRoundTo(PlayerBase player)
    {
#ifdef SERVER
        if (!player)
            return;
        PlayerIdentity identity = player.GetIdentity();
        if (!identity)
            return;

        int activeFlag = 0;
        if (m_State == NexoDM_State.ACTIVE)
            activeFlag = 1;
        Param3<int, int, int> payload = new Param3<int, int, int>(HudRoundSecondsLeft(), HudKillsToWin(), activeFlag);
        ScriptRPC rpc = new ScriptRPC();
        // R2: Serializer.Write(void) in scripts/1_core/proto/serializer.c:57.
        rpc.Write(payload);
        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        rpc.Send(player, NexoDM_RPC.HUD_ROUND, true, identity);
#endif
    }

    protected void BroadcastHudRound()
    {
#ifdef SERVER
        EnsureCollections();
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry || entry.role != NexoDM_Role.PARTICIPANT)
                continue;
            SendHudRoundTo(FindPlayerBySteamId(entry.steamId));
        }
#endif
    }

    protected void SendHudSelf(PlayerBase player, NexoDM_PlayerEntry entry)
    {
#ifdef SERVER
        if (!player || !entry)
            return;
        PlayerIdentity identity = player.GetIdentity();
        if (!identity)
            return;

        array<NexoDM_PlayerEntry> ranked = CollectRankedParticipants();
        int rank = 0;
        int index = 0;
        for (index = 0; index < ranked.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry row = ranked.Get(index);
            if (row && row.steamId == entry.steamId)
            {
                rank = index + 1;
                break;
            }
        }

        Param4<int, int, int, int> payload = new Param4<int, int, int, int>(entry.points, entry.kills, entry.deaths, rank);
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(payload);
        rpc.Send(player, NexoDM_RPC.HUD_SELF, true, identity);
#endif
    }

    void OnParticipantDisconnected(string steamId)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;
        if (m_State != NexoDM_State.ACTIVE)
            return;
        if (steamId == string.Empty)
            return;

        NexoDM_PlayerEntry entry = GetEntry(steamId);
        if (!entry || entry.role != NexoDM_Role.PARTICIPANT)
            return;

        // IsEntryOnline now skips IsPlayerDisconnected, so top 5 and ranks drop this player.
        RequestHudTop5();
        BroadcastHudSelf();
#endif
    }

    protected void CancelHudTop5Flush()
    {
#ifdef SERVER
        m_HudTop5Pending = false;
        DayZGame game = GetGame();
        if (!game)
            return;
        // R2: ScriptCallQueue.Remove(func) in scripts/2_gamelib/tools.c:65.
        game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(FlushHudTop5);
#endif
    }

    protected void BroadcastHudSelf()
    {
#ifdef SERVER
        EnsureCollections();
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry || entry.role != NexoDM_Role.PARTICIPANT)
                continue;
            if (!IsEntryOnline(entry))
                continue;
            SendHudSelf(FindPlayerBySteamId(entry.steamId), entry);
        }
#endif
    }

    protected void PublishKillHud(PlayerBase victim, NexoDM_PlayerEntry victimEntry, PlayerBase creditedKiller, string killerId, Object killerObject)
    {
#ifdef SERVER
        if (!victimEntry)
            return;

        string killerName = string.Empty;
        if (creditedKiller)
        {
            PlayerIdentity killerIdentity = creditedKiller.GetIdentity();
            if (killerIdentity)
                killerName = killerIdentity.GetName();
        }

        string victimName = string.Empty;
        if (victim)
        {
            PlayerIdentity victimIdentity = victim.GetIdentity();
            if (victimIdentity)
                victimName = victimIdentity.GetName();
        }

        string weaponClass = HudWeaponClass(killerObject, creditedKiller, victimEntry);
        BroadcastHudKill(killerName, victimName, weaponClass);
        // Points changed: every participant's rank may have moved.
        BroadcastHudSelf();
        RequestHudTop5();
#endif
    }

    protected string HudWeaponClass(Object killerObject, PlayerBase creditedKiller, NexoDM_PlayerEntry victimEntry)
    {
#ifdef SERVER
        if (victimEntry && victimEntry.lastDamageWasKnife && m_Config)
        {
            string knife = m_Config.GetKnife();
            if (knife != string.Empty)
                return knife;
        }

        EntityAI entity = EntityAI.Cast(killerObject);
        if (entity && !PlayerBase.Cast(entity))
        {
            // R2: Object.GetType() in scripts/3_game/entities/object.c:467.
            string typed = entity.GetType();
            if (typed != string.Empty)
                return typed;
        }

        if (creditedKiller)
        {
            // R2: Man.GetEntityInHands() in scripts/3_game/entities/man.c:81.
            EntityAI inHands = creditedKiller.GetEntityInHands();
            if (inHands)
                return inHands.GetType();
        }
#endif
        return string.Empty;
    }

    protected void BroadcastHudKill(string killerName, string victimName, string weaponClass)
    {
#ifdef SERVER
        EnsureCollections();
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry || entry.role != NexoDM_Role.PARTICIPANT)
                continue;
            PlayerBase player = FindPlayerBySteamId(entry.steamId);
            if (!player)
                continue;
            PlayerIdentity identity = player.GetIdentity();
            if (!identity)
                continue;
            ScriptRPC rpc = new ScriptRPC();
            rpc.Write(killerName);
            rpc.Write(victimName);
            rpc.Write(weaponClass);
            rpc.Send(player, NexoDM_RPC.HUD_KILL, true, identity);
        }
#endif
    }

    protected void RequestHudTop5()
    {
#ifdef SERVER
        array<NexoDM_PlayerEntry> ranked = CollectRankedParticipants();
        int rowCount = ranked.Count();
        if (rowCount > 5)
            rowCount = 5;
        if (HudTop5Unchanged(ranked, rowCount))
            return;

        DayZGame game = GetGame();
        int nowMs = 0;
        if (game)
            nowMs = game.GetTime();
        if (nowMs - m_HudTop5LastSendMs >= 1000)
        {
            BroadcastHudTop5();
            return;
        }

        if (m_HudTop5Pending)
            return;
        m_HudTop5Pending = true;
        int waitMs = 1000 - (nowMs - m_HudTop5LastSendMs);
        if (waitMs < 1)
            waitMs = 1;
        if (!game)
            return;
        // R2: ScriptCallQueue.CallLater(func,int,bool,...) in scripts/2_gamelib/tools.c:46.
        game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(FlushHudTop5, waitMs, false);
#endif
    }

    void FlushHudTop5()
    {
#ifdef SERVER
        m_HudTop5Pending = false;
        if (m_State != NexoDM_State.ACTIVE && m_State != NexoDM_State.ENDED)
            return;
        BroadcastHudTop5();
#endif
    }

    protected void BroadcastHudTop5()
    {
#ifdef SERVER
        array<NexoDM_PlayerEntry> ranked = CollectRankedParticipants();
        int rowCount = ranked.Count();
        if (rowCount > 5)
            rowCount = 5;
        if (HudTop5Unchanged(ranked, rowCount))
            return;

        RememberHudTop5(ranked, rowCount);
        DayZGame game = GetGame();
        if (game)
            m_HudTop5LastSendMs = game.GetTime();

        EnsureCollections();
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry || entry.role != NexoDM_Role.PARTICIPANT)
                continue;
            SendHudTop5To(FindPlayerBySteamId(entry.steamId));
        }
#endif
    }

    protected void SendHudTop5To(PlayerBase player)
    {
#ifdef SERVER
        if (!player)
            return;
        PlayerIdentity identity = player.GetIdentity();
        if (!identity)
            return;

        array<NexoDM_PlayerEntry> ranked = CollectRankedParticipants();
        int rowCount = ranked.Count();
        if (rowCount > 5)
            rowCount = 5;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(rowCount);
        int index = 0;
        for (index = 0; index < rowCount; index = index + 1)
        {
            NexoDM_PlayerEntry entry = ranked.Get(index);
            string name = string.Empty;
            int points = 0;
            int kills = 0;
            int deaths = 0;
            if (entry)
            {
                name = GetPlayerName(entry.steamId);
                points = entry.points;
                kills = entry.kills;
                deaths = entry.deaths;
            }
            rpc.Write(name);
            rpc.Write(points);
            rpc.Write(kills);
            rpc.Write(deaths);
        }
        rpc.Send(player, NexoDM_RPC.HUD_TOP5, true, identity);
#endif
    }

    protected bool HudTop5Unchanged(array<NexoDM_PlayerEntry> ranked, int rowCount)
    {
        if (m_HudTop5Count < 0)
            return false;
        if (m_HudTop5Count != rowCount)
            return false;
        int index = 0;
        for (index = 0; index < rowCount; index = index + 1)
        {
            NexoDM_PlayerEntry entry = ranked.Get(index);
            if (!entry)
                return false;
            if (m_HudTop5Ids.Get(index) != entry.steamId)
                return false;
            if (m_HudTop5Points.Get(index) != entry.points)
                return false;
            if (m_HudTop5Kills.Get(index) != entry.kills)
                return false;
            if (m_HudTop5Deaths.Get(index) != entry.deaths)
                return false;
        }
        return true;
    }

    protected void RememberHudTop5(array<NexoDM_PlayerEntry> ranked, int rowCount)
    {
        m_HudTop5Count = rowCount;
        m_HudTop5Ids.Clear();
        m_HudTop5Points.Clear();
        m_HudTop5Kills.Clear();
        m_HudTop5Deaths.Clear();
        int index = 0;
        for (index = 0; index < rowCount; index = index + 1)
        {
            NexoDM_PlayerEntry entry = ranked.Get(index);
            string steamId = string.Empty;
            int points = 0;
            int kills = 0;
            int deaths = 0;
            if (entry)
            {
                steamId = entry.steamId;
                points = entry.points;
                kills = entry.kills;
                deaths = entry.deaths;
            }
            m_HudTop5Ids.Insert(steamId);
            m_HudTop5Points.Insert(points);
            m_HudTop5Kills.Insert(kills);
            m_HudTop5Deaths.Insert(deaths);
        }
    }

    // Same order as OnScoreboardRequest: online participants, IsBetterByTiebreak, best first.
    protected array<NexoDM_PlayerEntry> CollectRankedParticipants()
    {
        array<NexoDM_PlayerEntry> ranked = new array<NexoDM_PlayerEntry>;
#ifdef SERVER
        EnsureCollections();
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            NexoDM_PlayerEntry counted = m_PlayerEntries.GetElement(index);
            if (counted && counted.role == NexoDM_Role.PARTICIPANT && IsEntryOnline(counted))
                ranked.Insert(counted);
        }

        int a = 0;
        int b = 0;
        for (a = 0; a < ranked.Count(); a = a + 1)
        {
            int best = a;
            for (b = a + 1; b < ranked.Count(); b = b + 1)
            {
                if (IsBetterByTiebreak(ranked.Get(b), ranked.Get(best)))
                    best = b;
            }
            if (best != a)
            {
                NexoDM_PlayerEntry swap = ranked.Get(a);
                ranked.Set(a, ranked.Get(best));
                ranked.Set(best, swap);
            }
        }
#endif
        return ranked;
    }

    protected bool IsBetterByTiebreak(NexoDM_PlayerEntry candidate, NexoDM_PlayerEntry current)
    {
        // F3 p5: points desc, then fewer deaths, then earlier last points change.
        if (candidate.points != current.points)
            return candidate.points > current.points;

        if (candidate.deaths != current.deaths)
            return candidate.deaths < current.deaths;

        int candidateTime = candidate.lastPointsChangeMs;
        int currentTime = current.lastPointsChangeMs;
        if (candidateTime == currentTime)
            return false;
        // 0 means "never scored": it loses to any real timestamp.
        if (candidateTime == 0)
            return false;
        if (currentTime == 0)
            return true;
        return candidateTime < currentTime;
    }

    protected string GetPlayerName(string steamId)
    {
#ifdef SERVER
        if (steamId == string.Empty)
            return string.Empty;

        PlayerBase player = FindPlayerBySteamId(steamId);
        if (!player)
            return string.Empty;

        PlayerIdentity identity = player.GetIdentity();
        if (!identity)
            return string.Empty;

        // R2: PlayerIdentity.GetName() in scripts/3_game/gameplay.c:362 (PlayerIdentityBase).
        return identity.GetName();
#else
        return string.Empty;
#endif
    }

    protected void EnrollConnectedParticipants()
    {
#ifdef SERVER
        DayZGame game = GetGame();
        if (!game)
            return;

        array<Man> players = new array<Man>();
        // R2: DayZGame.GetPlayers(out array<Man>) in scripts/3_game/global/game.c:947.
        game.GetPlayers(players);

        int index = 0;
        for (index = 0; index < players.Count(); index = index + 1)
        {
            PlayerBase player = PlayerBase.Cast(players.Get(index));
            if (!player)
                continue;

            PlayerIdentity identity = player.GetIdentity();
            if (!identity)
                continue;

            string steamId = identity.GetPlainId();
            if (steamId == string.Empty)
                continue;

            NexoDM_PlayerEntry entry = GetOrCreateEntry(steamId);
            entry.role = NexoDM_Role.PARTICIPANT;
            ApplyRoleToPlayer(player, entry.role);
        }
#endif
    }

    protected NexoDM_PlayerEntry GetOrCreateEntry(string steamId)
    {
        EnsureCollections();
        NexoDM_PlayerEntry entry = GetEntry(steamId);
        if (entry)
            return entry;

        entry = new NexoDM_PlayerEntry();
        entry.steamId = steamId;
        m_PlayerEntries.Set(steamId, entry);
        return entry;
    }

    protected NexoDM_PlayerEntry GetEntry(string steamId)
    {
        EnsureCollections();
        if (steamId == string.Empty)
            return null;

        if (!m_PlayerEntries.Contains(steamId))
            return null;

        return m_PlayerEntries.Get(steamId);
    }

    protected bool EnsureConfig()
    {
#ifdef SERVER
        if (m_Config)
            return true;

        // R2: NexoDM_Config.Load() in NexoDM/scripts/3_Game/NexoDM/NexoDM_Config.c:80.
        m_Config = NexoDM_Config.Load();
        if (m_Config)
            return true;
#endif
        return false;
    }

    protected PlayerBase FindPlayerBySteamId(string steamId)
    {
#ifdef SERVER
        if (steamId == string.Empty)
            return null;

        DayZGame game = GetGame();
        if (!game)
            return null;

        array<Man> players = new array<Man>();
        // R2: DayZGame.GetPlayers(out array<Man>) in scripts/3_game/global/game.c:947.
        game.GetPlayers(players);

        int index = 0;
        for (index = 0; index < players.Count(); index = index + 1)
        {
            PlayerBase player = PlayerBase.Cast(players.Get(index));
            if (!player)
                continue;

            PlayerIdentity identity = player.GetIdentity();
            if (!identity)
                continue;

            string playerSteamId = identity.GetPlainId();
            if (playerSteamId == steamId)
                return player;
        }
#endif
        return null;
    }

    protected PlayerBase ResolveObjectPlayer(Object obj)
    {
        if (!obj)
            return null;

        PlayerBase directPlayer = PlayerBase.Cast(obj);
        if (directPlayer)
            return directPlayer;

        EntityAI entity = EntityAI.Cast(obj);
        if (!entity)
            return null;

        // R2: EntityAI.GetHierarchyRootPlayer() in scripts/3_game/entities/entityai.c:877.
        Man rootMan = entity.GetHierarchyRootPlayer();
        PlayerBase rootPlayer = PlayerBase.Cast(rootMan);
        return rootPlayer;
    }

    protected PlayerBase ResolveSourcePlayer(EntityAI source)
    {
        if (!source)
            return null;

        PlayerBase directPlayer = PlayerBase.Cast(source);
        if (directPlayer)
            return directPlayer;

        // R2: EntityAI.GetHierarchyRootPlayer() in scripts/3_game/entities/entityai.c:877.
        Man rootMan = source.GetHierarchyRootPlayer();
        PlayerBase rootPlayer = PlayerBase.Cast(rootMan);
        return rootPlayer;
    }

    protected void ApplyRoleToOnlinePlayer(string steamId, int role)
    {
#ifdef SERVER
        PlayerBase player = FindPlayerBySteamId(steamId);
        if (player)
            ApplyRoleToPlayer(player, role);
#endif
    }

    protected void ApplyRoleToPlayer(PlayerBase player, int role)
    {
#ifdef SERVER
        if (!player)
            return;

        player.m_GG_Role = role;
        // R2: EntityAI.SetSynchDirty() in scripts/3_game/entities/entityai.c:3069.
        player.SetSynchDirty();
#endif
    }

    protected void RevertFreezeIfApplied()
    {
#ifdef SERVER
        StopTimer(m_FreezeTimer);
        SetFreezeForEntries(false);
#endif
    }

    protected bool CanMutateState()
    {
#ifdef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return false;

        if (!game.IsDedicatedServer())
            return false;

        return true;
#else
        return false;
#endif
    }

    protected void EnsureCollections()
    {
        if (!m_PlayerEntries)
            m_PlayerEntries = new map<string, ref NexoDM_PlayerEntry>;
        if (!m_WarnLog)
            m_WarnLog = new map<string, int>;
    }

    protected void EnsureTimers()
    {
        if (!m_RoundTimer)
            m_RoundTimer = new Timer();
        if (!m_EndStateTimer)
            m_EndStateTimer = new Timer();
        if (!m_FreezeTimer)
            m_FreezeTimer = new Timer();
        if (!m_AutostartTimer)
            m_AutostartTimer = new Timer();
    }

    protected void StopTimer(Timer timer)
    {
        if (!timer)
            return;

        // R2: Timer.IsRunning() in scripts/3_game/tools/tools.c:264; Timer.Stop() in scripts/3_game/tools/tools.c:617.
        if (timer.IsRunning())
            timer.Stop();
    }

    protected void LogRound(string detail)
    {
        string message = "[NexoDM] ";
        message = message + detail;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }

    protected void LogRule(string ruleId, string detail)
    {
        EnsureCollections();

        int now = 0;
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.GetTime() in scripts/3_game/global/game.c:1510.
        if (GetGame())
            now = GetGame().GetTime();

        // Same gate as NexoDM_Config.LogRule: config validation lines are not rate-limited.
        bool unguarded = false;
        if (ruleId == NexoDM_Constants.RULE_MAP_OTHER_WORLD)
            unguarded = true;
        // R2: string.IndexOf(string) in scripts/1_core/proto/enstring.c:240.
        if (ruleId.IndexOf("E_CFG_") == 0)
            unguarded = true;

        if (!unguarded)
        {
            int last = 0;
            // R2: map.Find(TKey,out TValue) in scripts/1_core/proto/enscript.c:858.
            if (m_WarnLog.Find(ruleId, last))
            {
                int elapsed = now - last;
                if (elapsed >= 0 && elapsed < NexoDM_Constants.LOG_RULE_INTERVAL_MS)
                    return;
            }

            // R2: map.Set(TKey,TValue) in scripts/1_core/proto/enscript.c:883.
            m_WarnLog.Set(ruleId, now);
        }

        string message = "[NexoDM][RoundManager] ";
        message = message + ruleId;
        message = message + " ";
        message = message + detail;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }
}
