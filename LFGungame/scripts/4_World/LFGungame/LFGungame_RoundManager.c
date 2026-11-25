class LFGungame_PlayerEntry : Managed
{
    string steamId;
    int role;
    int promptState;
    int tier;
    int kills;
    int assists;
    int deaths;
    ref set<string> damagedBy;
    bool lastDamageWasKnife;
    int lastKnifeHitTimeMs;     // S5.5 KB1-B obs
    string lastKnifeKillerId;   // E2 kill-credit fallback (scripted knife death -> EEKilled killer=null)
    string lastAttackerId;      // Option A: last participant who damaged this player (any weapon); credits the "downer" when the death reports no killer (respawn-suicide while KO / bleed-out)
    int lastAttackerTimeMs;     // GetTime() of the lastAttackerId hit; recency-gated by ATTACKER_CREDIT_WINDOW_MS

    void LFGungame_PlayerEntry()
    {
        steamId = string.Empty;
        role = LFGungame_Role.NONE;
        promptState = LFGungame_PromptState.NONE;
        tier = 0;
        kills = 0;
        assists = 0;
        deaths = 0;
        damagedBy = new set<string>();
        lastDamageWasKnife = false;
        lastKnifeHitTimeMs = 0;     // S5.5 KB1-B obs
        lastKnifeKillerId = string.Empty;
        lastAttackerId = string.Empty;
        lastAttackerTimeMs = 0;
    }
}

/*
S1/S2 state matrix:
Current | Event | Destination | Notes
IDLE | admin start with valid config | VOTING | resets entries, prompts no-admins.
IDLE | admin start with invalid config | IDLE | fail closed.
VOTING | vote YES with connected participant slots free | VOTING | role PARTICIPANT, prompt ANSWERED.
VOTING | vote YES with connected participant slots full | VOTING | role PUBLIC, prompt ANSWERED.
VOTING | vote NO | VOTING | role PUBLIC, prompt ANSWERED.
VOTING | vote timeout | ACTIVE | pending entries become PUBLIC.
VOTING | connected participants reach MaxPlayers | ACTIVE | voting closes early.
ACTIVE | late join without entry | ACTIVE | role PUBLIC, no prompt.
ACTIVE | reconnect with entry | ACTIVE | existing role reapplied.
ENDED | any join or vote | ENDED | ignored until reset timer returns to IDLE.

S2 voting matrix:
Input | Final role | Final promptState
YES with slot | PARTICIPANT | ANSWERED
YES when full | PUBLIC | ANSWERED
NO | PUBLIC | ANSWERED
timeout without response | PUBLIC | ANSWERED
reconnect mid-VOTING with pending entry | unchanged until answer | PROMPT_PENDING
late-join mid-ACTIVE | PUBLIC | NONE

S2 lifecycle matrix:
State | Event | Admin | Result
IDLE | ready/reconnect | yes or no | no entry mutation.
VOTING | ready | yes | no prompt, no entry mutation.
VOTING | ready | no | create or reuse prompt pending, open dialog.
VOTING | reconnect | yes | no prompt, no entry mutation.
VOTING | reconnect | no | reapply role; reopen only if prompt pending.
ACTIVE | ready/reconnect | yes | no entry mutation.
ACTIVE | ready/reconnect | no with entry | reapply existing role.
ACTIVE | ready/reconnect | no without entry | create PUBLIC without prompt.
ENDED | ready/reconnect | yes or no | no entry mutation.
*/
class LFGungame_RoundManager : Managed
{
    protected static ref LFGungame_RoundManager s_Instance;

    protected int m_State;
    protected ref LFGungame_Config m_Config;
    protected ref Timer m_RoundTimer;
    protected ref Timer m_VotingTimer;
    protected ref Timer m_EndStateTimer;
    protected ref Timer m_FreezeTimer;
    protected ref Timer m_AutostartTimer;
    protected ref map<string, ref LFGungame_PlayerEntry> m_PlayerEntries;
    protected ref map<string, int> m_WarnLog;
    // F-02: steamIds of participants skipped by round-end cleanup (dead/absent). Lives OUTSIDE
    // m_PlayerEntries because that map is cleared by ResetStats 0.1s after the round ends, well
    // before the dead-at-end player respawns; consumed (consume-once) at EVERY player entry
    // point: OnPlayerRespawn, OnPlayerReady, OnPlayerReconnect (GG-02).
    protected ref set<string> m_PendingPublicCleanup;
    // GG-01v5: steamIds whose fresh respawn character was equipped DURING creation
    // (StartingEquipSetup -> EquipParticipantAtBirth). OnPlayerRespawn consumes this to skip the
    // post-creation teleport+wipe+equip path that desynced the loadout to observers.
    protected ref set<string> m_GG_BirthEquipped;
    protected int m_SpawnRoundRobinIdx;

    void LFGungame_RoundManager()
    {
#ifdef SERVER
        m_State = LFGungame_State.IDLE;
        m_RoundTimer = new Timer();
        m_VotingTimer = new Timer();
        m_EndStateTimer = new Timer();
        m_FreezeTimer = new Timer();
        m_AutostartTimer = new Timer();
        m_PlayerEntries = new map<string, ref LFGungame_PlayerEntry>;
        m_WarnLog = new map<string, int>;
        m_PendingPublicCleanup = new set<string>();
        m_GG_BirthEquipped = new set<string>();
        m_SpawnRoundRobinIdx = 0;
#endif
    }

    static LFGungame_RoundManager GetInstance()
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
            s_Instance = new LFGungame_RoundManager();
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

        if (m_State != LFGungame_State.IDLE)
        {
            string ignoredMessage = "start ignored: round is not idle";
            LogRound(ignoredMessage);
            return;
        }

        StopTimer(m_EndStateTimer);
        // A start (manual or automatic) consumes any armed autostart attempt; ReturnToIdle
        // re-arms the unsupervised loop when the round is over.
        StopTimer(m_AutostartTimer);

        // R2: LFGungame_Config.Load() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:25.
        m_Config = LFGungame_Config.Load();
        if (!m_Config)
        {
            string loadMessage = "start rejected: config load failed";
            LogRound(loadMessage);
            m_State = LFGungame_State.IDLE;
            return;
        }

        // R2: LFGungame_Config.Validate() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:87.
        if (!m_Config.Validate())
        {
            string validateMessage = "start rejected: config validation failed";
            LogRound(validateMessage);
            m_State = LFGungame_State.IDLE;
            return;
        }

        // F-13/F-14: surface leftover placeholder config (admin 7656XXXX / default presets / default
        // coords) in the RPT at round start, so a half-configured install is diagnosable.
        m_Config.LogPlaceholderWarnings();

        ResetStats();
        // F-02: a new round invalidates any leftover pending public-cleanup from the prior round.
        m_PendingPublicCleanup.Clear();
        // F-20v2: reset the "last spawn coord used" tracker at round start (spawn pick is now
        // uniform-random, not round-robin — see PickRandomSpawn).
        m_SpawnRoundRobinIdx = 0;
        m_State = LFGungame_State.VOTING;
        StartVotingTimer();
        PromptConnectedPlayers();
        string startedMessage = "round entered voting";
        LogRound(startedMessage);
#endif
    }

    void EndRound(string reason, string winnerSteamId)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State == LFGungame_State.IDLE)
        {
            string idleMessage = "end ignored: already idle";
            LogRound(idleMessage);
            return;
        }

        if (m_State == LFGungame_State.ENDED)
        {
            string endedMessage = "end ignored: already ended";
            LogRound(endedMessage);
            return;
        }

        // Capture state before flipping to ENDED: only a round that reached ACTIVE has
        // teleported/equipped participants, so the I4 cleanup must not run for a VOTING stop
        // (it would wipe players' real loot that was never replaced by a gungame loadout).
        bool wasActive = (m_State == LFGungame_State.ACTIVE);

        StopTimer(m_RoundTimer);
        StopTimer(m_VotingTimer);
        StopTimer(m_EndStateTimer);
        StopTimer(m_FreezeTimer);

        m_State = LFGungame_State.ENDED;
        RevertFreezeIfApplied();

        // Close any join dialog still open on a client (admin stop during VOTING leaves them
        // open until the client-side 60s timer expires; BroadcastOpenDialog cannot close them).
        CloseOrphanedDialogs();

        if (wasActive)
        {
            // I3 winner announce + I4 cleanup must run while entries still exist; ReturnToIdle
            // clears them 0.1s after ScheduleIdleReset below.
            AnnounceWinner(reason, winnerSteamId);
            CleanupParticipantsToPublic();
        }
        else
        {
            // GG-06: a stop during VOTING never ran BeginRound/CleanupParticipantsToPublic, but
            // voters already carry a synced role from OnPlayerVote. Reset it so no client keeps
            // the tier card / openable scoreboard into IDLE. Tier/crown never synced in VOTING.
            EnsureCollections();
            int roleIndex = 0;
            for (roleIndex = 0; roleIndex < m_PlayerEntries.Count(); roleIndex = roleIndex + 1)
            {
                LFGungame_PlayerEntry roleEntry = m_PlayerEntries.GetElement(roleIndex);
                if (roleEntry && roleEntry.role != LFGungame_Role.NONE)
                {
                    string roleSteamId = m_PlayerEntries.GetKey(roleIndex);
                    ApplyRoleToOnlinePlayer(roleSteamId, LFGungame_Role.NONE);
                }
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
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (entry && entry.role == LFGungame_Role.PARTICIPANT)
            {
                string steamId = m_PlayerEntries.GetKey(index);
                ids.Insert(steamId);
            }
        }
#endif
        return ids;
    }

    array<string> GetPublicIds()
    {
        array<string> ids = new array<string>();
#ifdef SERVER
        EnsureCollections();
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (entry && entry.role == LFGungame_Role.PUBLIC)
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
    protected bool IsEntryOnline(LFGungame_PlayerEntry e)
    {
#ifdef SERVER
        if (!e)
            return false;
        if (e.steamId == string.Empty)
            return false;
        return FindPlayerBySteamId(e.steamId) != null;
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
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (entry && entry.role == LFGungame_Role.PARTICIPANT && IsEntryOnline(entry))
                count = count + 1;
        }
#endif
        return count;
    }

    int CountPublic()
    {
        int count = 0;
#ifdef SERVER
        EnsureCollections();
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (entry && entry.role == LFGungame_Role.PUBLIC)
                count = count + 1;
        }
#endif
        return count;
    }

    bool HasPromptPending(string steamId)
    {
#ifdef SERVER
        LFGungame_PlayerEntry entry = GetEntry(steamId);
        if (entry && entry.promptState == LFGungame_PromptState.PROMPT_PENDING)
            return true;
#endif
        return false;
    }

    bool IsAdminSteamId(string steamId)
    {
#ifdef SERVER
        if (steamId == string.Empty)
            return false;

        if (!EnsureConfig())
            return false;

        // R2: LFGungame_Config.GetAdmins() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:152.
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
#endif
    }

    void OnPlayerVote(string steamId, bool yes)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != LFGungame_State.VOTING)
            return;

        if (!EnsureConfig())
            return;

        LFGungame_PlayerEntry entry = GetEntry(steamId);
        if (!entry)
            return;

        if (entry.promptState != LFGungame_PromptState.PROMPT_PENDING)
            return;

        // GG-04: a dead player (pre-respawn) can still have the join prompt open. Refuse the vote
        // and keep the entry PROMPT_PENDING, so the NEW character gets the prompt re-broadcast on
        // ready (VOTING branch of OnPlayerReady); accepting would let BeginRound process a corpse.
        // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
        PlayerBase voter = FindPlayerBySteamId(steamId);
        if (!voter || !voter.IsAlive())
        {
            string deadVoteDetail = "vote rejected: voter dead or absent";
            LogRule(LFGungame_Constants.RULE_VOTE_BAD_STATE, deadVoteDetail);
            return;
        }

        bool isAdmin = IsAdminSteamId(steamId);
        if (yes)
        {
            // R2: LFGungame_Config.GetMaxPlayers() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:194.
            if (CountParticipants() < m_Config.GetMaxPlayers())
            {
                entry.role = LFGungame_Role.PARTICIPANT;
            }
            else if (isAdmin)
            {
                // Admin voted yes on a full round: stay NONE (noop), never forced to public.
                entry.role = LFGungame_Role.NONE;
            }
            else
            {
                entry.role = LFGungame_Role.PUBLIC;
                // B4: notify the late voter the match is full, via the 3_Game announce bus
                // (4_World cannot call LBChatHandler in 5_Mission directly).
                PlayerIdentity fullIdentity = GetIdentityForSteamId(steamId);
                if (fullIdentity)
                {
                    string fullMsg = LFGungame_Constants.MSG_GAME_FULL;
                    LFGungame_AnnounceBus.Raise(fullIdentity, fullMsg, false);
                }
                string fullDetail = "yes vote converted to public; match full notice sent";
                LogRule(LFGungame_Constants.RULE_B4_GAME_FULL_CHAT_DEFERRED, fullDetail);
            }
        }
        else
        {
            if (isAdmin)
            {
                // Admin declined: stay NONE (noop), not public.
                entry.role = LFGungame_Role.NONE;
            }
            else
            {
                entry.role = LFGungame_Role.PUBLIC;
            }
        }

        entry.promptState = LFGungame_PromptState.ANSWERED;
        ApplyRoleToOnlinePlayer(steamId, entry.role);

        // R2: LFGungame_Config.GetMaxPlayers() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:194.
        if (CountParticipants() >= m_Config.GetMaxPlayers())
            CloseVotingEarly();
#endif
    }

    void OnVotingClosed()
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != LFGungame_State.VOTING)
            return;

        StopTimer(m_VotingTimer);
        EnsurePublicEntriesForConnectedPlayers();

        // F-08 (WARN-2): no point running a full RoundDuration with nobody playing. Abort if voting
        // closed with zero participants. Done BEFORE the pending->answered loop below so EndRound's
        // CloseOrphanedDialogs still sees PROMPT_PENDING entries and closes those clients' join dialogs.
        if (CountParticipants() == 0)
        {
            string emptyMessage = "voting closed with 0 participants; aborting round";
            LogRound(emptyMessage);
            string noPartReason = "no_participants";
            string emptyWinner = string.Empty;
            EndRound(noPartReason, emptyWinner);
            return;
        }

        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (entry && entry.promptState == LFGungame_PromptState.PROMPT_PENDING)
            {
                string steamId = m_PlayerEntries.GetKey(index);
                if (IsAdminSteamId(steamId))
                {
                    // Admin who never answered before voting closed: noop (NONE), not public.
                    entry.role = LFGungame_Role.NONE;
                }
                else
                {
                    entry.role = LFGungame_Role.PUBLIC;
                }
                entry.promptState = LFGungame_PromptState.ANSWERED;
                ApplyRoleToOnlinePlayer(steamId, entry.role);
            }
        }

        m_State = LFGungame_State.ACTIVE;
        BeginRound();
        string activeMessage = "round entered active";
        LogRound(activeMessage);
#endif
    }

    void BeginRound()
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != LFGungame_State.ACTIVE)
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

        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry)
                continue;

            if (entry.role == LFGungame_Role.NONE)
                continue;

            PlayerBase player = FindPlayerBySteamId(entry.steamId);
            if (!player)
            {
                string missingMessage = "begin skipped disconnected player ";
                missingMessage = missingMessage + entry.steamId;
                LogRound(missingMessage);
                continue;
            }

            // GG-04: never teleport/equip/freeze a corpse (a voter can die during VOTING). The
            // entry keeps its voted role; the NEW character is routed into the round by the
            // OnClientNewEvent respawn path (GG-01).
            // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
            if (!player.IsAlive())
            {
                string deadMessage = "begin skipped dead player ";
                deadMessage = deadMessage + entry.steamId;
                LogRound(deadMessage);
                continue;
            }

            vector spawn = PickRandomSpawn(entry.role);
            // R2: Object.SetPosition(vector) in scripts/3_game/entities/object.c:300.
            player.SetPosition(spawn);

            WipeInventory(player);
            EquipPlayer(player, entry.role, entry.tier);

            if (entry.role == LFGungame_Role.PARTICIPANT)
            {
                // R2: Entity.DisableSimulation(bool) in scripts/3_game/entities/entity.c:3.
                player.DisableSimulation(true);
                // R2: Object.SetAllowDamage(bool) in scripts/3_game/entities/object.c:1192.
                player.SetAllowDamage(false);
            }

            SetTierSync(player, entry.tier);

            if (entry.role == LFGungame_Role.PARTICIPANT)
            {
                PlayerIdentity identity = player.GetIdentity();
                if (identity)
                    BroadcastCountdown(identity, 10);
            }
        }

        float freezeSeconds = 10.0;
        string callbackName = "EndFreeze";
        // R2: Timer.Run(float,Managed,string,Param,bool) in scripts/3_game/tools/tools.c:595.
        m_FreezeTimer.Run(freezeSeconds, this, callbackName, NULL, false);

        // I2: start the round-duration countdown; OnRoundTimeout ends the round by tiebreak.
        int roundDuration = m_Config.GetRoundDuration();
        float roundSeconds = roundDuration;
        string timeoutCallback = "OnRoundTimeout";
        m_RoundTimer.Run(roundSeconds, this, timeoutCallback, NULL, false);
#endif
    }

    void ChangeTier(PlayerBase player, int delta)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (!player)
            return;

        if (!EnsureConfig())
            return;

        PlayerIdentity id = player.GetIdentity();
        if (!id)
            return;

        // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
        string steamId = id.GetPlainId();
        if (steamId == string.Empty)
            return;

        LFGungame_PlayerEntry entry = GetEntry(steamId);
        if (!entry)
            return;

        if (entry.role != LFGungame_Role.PARTICIPANT)
            return;

        // R2: LFGungame_Config.GetGunPresets() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:163.
        array<string> guns = m_Config.GetGunPresets();
        if (!guns)
            return;

        int maxIndex = guns.Count() - 1;
        if (maxIndex < 0)
            return;

        int newTier = entry.tier + delta;
        if (newTier < 0)
            newTier = 0;
        if (newTier > maxIndex)
            newTier = maxIndex;

        // No-op when the clamp produced no change (e.g. +1 already at max tier): skip the
        // wasted full weapon re-equip. Victory at max tier is handled in OnPlayerKilled.
        if (newTier == entry.tier)
            return;

        entry.tier = newTier;
        ApplyTierLoadout(player, newTier);
        SetTierSync(player, newTier);

        string ruleId = "TIER_CHANGED";
        string detail = "participant tier changed";
        LogRule(ruleId, detail);
#endif
    }

    void OnPlayerDamaged(PlayerBase victim, EntityAI source)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != LFGungame_State.ACTIVE)
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

        LFGungame_PlayerEntry victimEntry = GetEntry(victimId);
        if (!victimEntry)
            return;

        if (victimEntry.role != LFGungame_Role.PARTICIPANT)
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
                    LFGungame_PlayerEntry attackerEntry = GetEntry(attackerId);
                    if (attackerEntry && attackerEntry.role == LFGungame_Role.PARTICIPANT)
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
            // R2: LFGungame_Config.GetKnife() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:169.
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

        if (m_State != LFGungame_State.ACTIVE)
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

        LFGungame_PlayerEntry victimEntry = GetEntry(victimId);
        if (!victimEntry)
            return;

        if (victimEntry.role != LFGungame_Role.PARTICIPANT)
            return;

        victimEntry.deaths = victimEntry.deaths + 1;
        string deathDetail = "participant death recorded";
        LogRule(LFGungame_Constants.RULE_DEATH_RECORDED, deathDetail);

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
        // the recorded knife attacker to credit the kill (kills counter / +1 tier / victory).
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
            if (sinceLastHitMs >= 0 && sinceLastHitMs < LFGungame_Constants.ATTACKER_CREDIT_WINDOW_MS)
            {
                killerId = victimEntry.lastAttackerId;
                killerPlayer = FindPlayerBySteamId(killerId);
            }
        }

        bool victory = false;
        string winnerId = string.Empty;
        if (killerPlayer && killerId != string.Empty && killerId != victimId)
        {
            LFGungame_PlayerEntry killerEntry = GetEntry(killerId);
            if (killerEntry && killerEntry.role == LFGungame_Role.PARTICIPANT)
            {
                killerEntry.kills = killerEntry.kills + 1;
                string killDetail = "participant kill recorded";
                LogRule(LFGungame_Constants.RULE_KILL_RECORDED, killDetail);

                int maxIndex = GetMaxTierIndex();
                // I1: a kill made WHILE already at the top tier wins (the +1 would clamp to a no-op).
                if (maxIndex >= 0 && killerEntry.tier >= maxIndex)
                {
                    victory = true;
                    winnerId = killerId;
                    string victoryDetail = "victory kill at max tier";
                    LogRule(LFGungame_Constants.RULE_VICTORY_RECORDED, victoryDetail);
                }
                else
                {
                    ChangeTier(killerPlayer, 1);
                }
                AwardAssists(victimEntry, killerId);
            }
        }

        if (victimEntry.lastDamageWasKnife)
        {
            int gapMs = GetGame().GetTime() - victimEntry.lastKnifeHitTimeMs;   // S5.5 KB1-B obs
            int newTier = victimEntry.tier - 1;
            if (newTier < 0)
                newTier = 0;

            victimEntry.tier = newTier;
            // Keep the victim's synced tier coherent (leader recalc / HUD); respawn re-syncs anyway.
            SetTierSync(victim, newTier);
            string knifeDetail = "knife death tier penalty recorded gap_ms=";   // S5.5 KB1-B obs
            knifeDetail = knifeDetail + gapMs.ToString();                        // S5.5 KB1-B obs
            LogRule(LFGungame_Constants.RULE_KNIFE_DEATH_RECORDED, knifeDetail);
        }

        // S5.5 KB1-A: consume-once - evita -1 stale entre kill y respawn (P2-2).
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

        // Anti-loot (gungame integrity): strip firearms/mags/ammo from the corpse now and
        // sweep any dropped on the ground shortly after, so the tier weapon can't be looted
        // to skip progression. Participants only. The knife (everyone has one), clothing and
        // the body are kept - WipeWeaponsOnly preserves the knife via ShouldDeleteWeaponLoadoutItem.
        vector deathPos = victim.GetPosition();
        WipeWeaponsOnly(victim);
        // R2: ScriptCallQueue.CallLater(func,int,bool,...) pattern in scripts/2_gamelib/tools.c:46.
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(CleanupDeathSite, 2000, false, deathPos);
        // Final cleanup: delete the corpse and re-sweep stray weapons once physics has settled.
        // Runs regardless of later round-state changes (arena hygiene, no lootable tier weapons).
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(CleanupDeathFinal, 10000, false, victim, deathPos);

        // I1: end the round when this kill was a victory, after all stats/penalties are finalized.
        if (victory)
        {
            string victoryReason = "victory";
            EndRound(victoryReason, winnerId);
        }
#endif
    }

    // Death-site sweep: delete loose firearms/mags/ammo dropped on the ground near the death
    // (the knife and non-weapon loot are kept). Called ~2s after a participant death so a
    // weapon released from the hands has settled. The corpse's own weapons are already stripped
    // by WipeWeaponsOnly at kill time. Radius 5 m: the dying character's momentum flings the
    // hands weapon well past the old 2 m sphere (in-game test 2026-06-11: strays survived and
    // were lootable). Only ownerless gun/mag/ammo match, so the wide sphere cannot hit gear
    // carried by living players.
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

    void AwardAssists(LFGungame_PlayerEntry victimEntry, string killerId)
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

            LFGungame_PlayerEntry attackerEntry = GetEntry(attackerId);
            if (attackerEntry && attackerEntry.role == LFGungame_Role.PARTICIPANT)
            {
                attackerEntry.assists = attackerEntry.assists + 1;
                string assistDetail = "participant assist recorded";
                LogRule(LFGungame_Constants.RULE_ASSIST_RECORDED, assistDetail);
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

        // F-02: a participant dead/absent at round end was recorded in m_PendingPublicCleanup
        // (CleanupParticipantsToPublic only teleports/wipes the living). Finish their I4 here on their
        // post-round respawn. The set survives ResetStats (the entry does NOT, so this is consumed from
        // the set BEFORE the entry lookup below). Consume-once: removed on use.
        if (m_PendingPublicCleanup)
        {
            int pendingIdx = m_PendingPublicCleanup.Find(steamId);
            if (pendingIdx >= 0)
            {
                m_PendingPublicCleanup.Remove(pendingIdx);
                if (EnsureConfig())
                {
                    vector publicSpawn = PickRandomSpawn(LFGungame_Role.PUBLIC);
                    player.SetPosition(publicSpawn);
                    WipeInventory(player);
                    EquipPlayer(player, LFGungame_Role.PUBLIC, 0);
                    SetTierSync(player, 0);
                }
                return;
            }
        }

        LFGungame_PlayerEntry entry = GetEntry(steamId);
        if (!entry)
            return;

        if (entry.role != LFGungame_Role.PARTICIPANT)
            return;

        if (m_State != LFGungame_State.ACTIVE)
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
            vector spawn = PickRandomSpawn(entry.role);
            // R2: Object.SetPosition(vector) in scripts/3_game/entities/object.c:300.
            player.SetPosition(spawn);
            WipeInventory(player);
            EquipPlayer(player, entry.role, entry.tier);
        }
        SetTierSync(player, entry.tier);

        if (!entry.damagedBy)
            entry.damagedBy = new set<string>();

        // R2: set.Clear in scripts/1_core/proto/enscript.c:725.
        entry.damagedBy.Clear();
        entry.lastDamageWasKnife = false;
        entry.lastKnifeKillerId = string.Empty;
        entry.lastAttackerId = string.Empty;
        entry.lastAttackerTimeMs = 0;

        string respawnDetail = "participant respawn equipped";
        LogRule(LFGungame_Constants.RULE_RESPAWN_EQUIPPED, respawnDetail);
#endif
    }

    void ApplyTierLoadout(PlayerBase player, int tier)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (!player)
            return;

        if (!m_Config)
            return;

        // F-06: resolve the gun preset BEFORE wiping the current weapon, so a failed loader/preset
        // (e.g. a misconfigured tier) does not leave the player disarmed mid-fight after a tier-up.
        // R2: LFGungame_Config.GetGunPresets() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:163.
        array<string> guns = m_Config.GetGunPresets();
        if (!guns)
            return;

        if (tier < 0)
            return;

        if (tier >= guns.Count())
            return;

        string gunName = guns.Get(tier);
        if (!LB_PresetLoader.Get)
        {
            LogRule("PRESET_LOADER_UNAVAILABLE_TIER", gunName);
            return;
        }

        // R2: LB_PresetLoader.GetPreset(string) in LBmaster_Core/scripts/4_World/LBmaster_Core/Config/LB_PresetLoader.c:146.
        LB_PresetBase gunPreset = LB_PresetLoader.Get.GetPreset(gunName);
        if (!gunPreset)
        {
            LogRule("PRESET_RESOLVE_FAILED_TIER", gunName);
            return;
        }

        // Only now that the new preset is resolved do we destroy the previous tier weapon.
        WipeWeaponsOnly(player);
        // Free the hands so the tier weapon spawns INTO them. LBmaster spawnIn:"hands"
        // (LB_ItemPreset.CreateInHands, LB_ItemPreset.c:144-148) falls back to the inventory when the
        // hands are occupied; after a KNIFE kill the knife is still held (WipeWeaponsOnly keeps it),
        // which left the new weapon in the bag - unchambered and unbound (GG_ReadyWeapon read the
        // knife in hands and bailed). Moving the held item to the inventory keeps its hotbar shortcut.
        // R2: HumanInventory.GetEntityInHands() in scripts/3_game/systems/inventory/humaninventory.c:16.
        EntityAI heldBeforeSpawn = player.GetHumanInventory().GetEntityInHands();
        if (heldBeforeSpawn)
        {
            // R2: GameInventory.TakeEntityToInventory(InventoryMode,FindInventoryLocationType,EntityAI) in scripts/3_game/systems/inventory/inventory.c:1012; InventoryMode.SERVER :26.
            player.GetInventory().TakeEntityToInventory(InventoryMode.SERVER, FindInventoryLocationType.ANY, heldBeforeSpawn);
        }
        // R2: LB_PresetBase.SpawnPreset(PlayerBase,EntityAI) in LBmaster_Core/scripts/4_World/LBmaster_Core/Struct/LB_PresetBase.c:138.
        gunPreset.SpawnPreset(player, player);
        // Chamber + load the freshly spawned tier weapon and bind it to the hotbar (see MakeWeaponReady).
        MakeWeaponReady(player);
#endif
    }

    // Makes the freshly-spawned tier weapon ready: chamber (or fully load an EMPTY internal magazine for
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

    // Deferred target of MakeWeaponReady (next tick = stable FSM): chamber the in-hands tier weapon (or
    // fully load an EMPTY internal magazine + chamber with one consistent ammo type) and bind it to
    // hotbar slot 1. Server-only.
    void GG_ReadyWeapon(PlayerBase player)
    {
#ifdef SERVER
        if (!player)
            return;

        // m3: the player may have died/disconnected between the spawn and this next-tick callback.
        // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
        if (!player.IsAlive())
            return;

        // The tier weapon usually spawns in hands, but on a knife-kill the hands move (ApplyTierLoadout)
        // can lag a tick so LBmaster may have placed the weapon in the inventory. Fall back to scanning
        // the player for the (single, post-wipe) Weapon_Base so it is chambered + hotbar-bound wherever
        // it landed. R2: Man.GetEntityInHands() in scripts/3_game/entities/man.c:81.
        Weapon_Base wpn = Weapon_Base.Cast(player.GetEntityInHands());
        if (!wpn)
            wpn = FindPlayerWeapon(player);
        if (!wpn)
            return;

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
            // R2: Weapon_Base.FillChamber() in weapon_base.c:929. Idempotent (FillSpecificChamber :988).
            // Detachable-mag weapons keep the magazine the LBmaster preset attached; just chamber.
            wpn.FillChamber();
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
    // WipeWeaponsOnly the only firearm on a participant is the freshly spawned tier weapon, so this
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

    void CloseVotingEarly()
    {
#ifdef SERVER
        if (m_State != LFGungame_State.VOTING)
            return;

        OnVotingClosed();
#endif
    }

    // GG-08: leader crowns only recompute on tier changes (SetTierSync), so a leader logging out
    // keeps a stale crown until someone's tier moves. Called deferred (+1s) from
    // MissionServer.PlayerDisconnected, after the engine removed the player from the world, so
    // IsEntryOnline re-evaluates without the leaver.
    void OnPlayerGoneRecalculate()
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (m_State != LFGungame_State.ACTIVE)
            return;

        RecalculateLeaders();
#endif
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

        // F-02/GG-02 (R7): the pending public-cleanup must be consumed at EVERY player entry point.
        // A participant absent at round end who relogs with a new session (alive persisted
        // character) arrives via ClientReady; without this consume they keep the tier loadout
        // permanently and the stale pending fires a surprise wipe on an unrelated future
        // respawn/reconnect. Placed BEFORE the IDLE/ENDED early-return: post-round state is IDLE.
        // Consume-once: removed on use. No role re-sync needed here — a ClientReady character is
        // re-created on connect, so its syncvars start clean (unlike the Reconnect path).
        if (m_PendingPublicCleanup)
        {
            int pendingIdx = m_PendingPublicCleanup.Find(steamId);
            if (pendingIdx >= 0)
            {
                m_PendingPublicCleanup.Remove(pendingIdx);
                if (EnsureConfig())
                {
                    vector publicSpawn = PickRandomSpawn(LFGungame_Role.PUBLIC);
                    player.SetPosition(publicSpawn);
                    WipeInventory(player);
                    EquipPlayer(player, LFGungame_Role.PUBLIC, 0);
                    SetTierSync(player, 0);
                }
                return;
            }
        }

        if (m_State == LFGungame_State.IDLE || m_State == LFGungame_State.ENDED)
            return;

        if (m_State == LFGungame_State.VOTING)
        {
            LFGungame_PlayerEntry votingEntry = GetOrCreateEntry(steamId);
            if (votingEntry.promptState == LFGungame_PromptState.NONE)
            {
                votingEntry.role = LFGungame_Role.NONE;
                votingEntry.promptState = LFGungame_PromptState.PROMPT_PENDING;
            }

            ApplyRoleToPlayer(player, votingEntry.role);
            if (votingEntry.promptState == LFGungame_PromptState.PROMPT_PENDING)
                BroadcastOpenDialog(identity, GetVotingSecondsLeft());
            return;
        }

        if (m_State == LFGungame_State.ACTIVE)
        {
            LFGungame_PlayerEntry activeEntry = GetEntry(steamId);
            if (!activeEntry)
            {
                // Admin joining mid-round without an entry: noop, never forced to public.
                if (IsAdminSteamId(steamId))
                    return;

                activeEntry = GetOrCreateEntry(steamId);
                activeEntry.role = LFGungame_Role.PUBLIC;
                activeEntry.promptState = LFGungame_PromptState.NONE;
                // B6: late-join non-admin -> public with ClothingPreset only (no wipe, no teleport).
                EquipPlayer(player, LFGungame_Role.PUBLIC, activeEntry.tier);
            }

            ApplyRoleToPlayer(player, activeEntry.role);
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

        // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
        string steamId = identity.GetPlainId();
        if (steamId == string.Empty)
            return;

        // F-02: an absent-at-round-end participant recorded in m_PendingPublicCleanup may come back via
        // reconnect (alive) rather than respawn. Consume the pending here too -> teleport + clothe public.
        if (m_PendingPublicCleanup)
        {
            int pendingIdx = m_PendingPublicCleanup.Find(steamId);
            if (pendingIdx >= 0)
            {
                m_PendingPublicCleanup.Remove(pendingIdx);
                if (EnsureConfig())
                {
                    vector publicSpawn = PickRandomSpawn(LFGungame_Role.PUBLIC);
                    player.SetPosition(publicSpawn);
                    WipeInventory(player);
                    EquipPlayer(player, LFGungame_Role.PUBLIC, 0);
                    SetTierSync(player, 0);
                }
                // GG-07: the reconnecting character is the SAME server-side object (logout window),
                // so it still carries the ended round's role/leader syncvars. Reset both even when
                // config is unavailable; ApplyRoleToPlayer's SetSynchDirty covers the leader write.
                player.m_GG_IsLeader = 0;
                ApplyRoleToPlayer(player, LFGungame_Role.NONE);
                return;
            }
        }

        LFGungame_PlayerEntry entry = GetEntry(steamId);
        if (entry)
        {
            ApplyRoleToPlayer(player, entry.role);
            // F5: restore synced tier on reconnect (a fresh PlayerBase starts m_GG_Tier=0).
            if (entry.role == LFGungame_Role.PARTICIPANT && m_State == LFGungame_State.ACTIVE)
            {
                SetTierSync(player, entry.tier);
                // F-07: re-equip the tier loadout so the reconnected participant's inventory matches
                // their synced tier (otherwise they keep whatever the character persisted, diverging
                // from data/HUD). [verify in-game]: confirm reconnect timing accepts inventory ops
                // here and that this does not double with an engine-restored loadout.
                if (EnsureConfig())
                {
                    WipeInventory(player);
                    EquipPlayer(player, entry.role, entry.tier);
                }
            }
            if (m_State == LFGungame_State.VOTING && entry.promptState == LFGungame_PromptState.PROMPT_PENDING)
                BroadcastOpenDialog(identity, GetVotingSecondsLeft());
            return;
        }

        if (m_State == LFGungame_State.ACTIVE)
        {
            // Admin reconnecting mid-round without an entry: noop, never forced to public.
            if (IsAdminSteamId(steamId))
                return;

            entry = GetOrCreateEntry(steamId);
            entry.role = LFGungame_Role.PUBLIC;
            entry.promptState = LFGungame_PromptState.NONE;
            // B6: late-join (reconnect, no entry) non-admin -> public with ClothingPreset only.
            EquipPlayer(player, LFGungame_Role.PUBLIC, entry.tier);
            ApplyRoleToPlayer(player, entry.role);
        }
#endif
    }

    void BroadcastOpenDialog(PlayerIdentity recipient, int secondsLeft)
    {
#ifdef SERVER
        if (!recipient)
        {
            return;
        }

        if (secondsLeft <= 1)
        {
            LogRule("OPEN_DIALOG_VOTING_CLOSING", "open dialog skipped because voting is closing");
            return;
        }

        if (secondsLeft < 0)
            secondsLeft = 0;

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
            LogRule(LFGungame_Constants.RULE_OPEN_DIALOG_NO_TARGET, "target player not found");
            return;
        }

        ScriptRPC rpc = new ScriptRPC();
        Param1<int> payload = new Param1<int>(secondsLeft);
        // R2: Serializer.Write(void) in scripts/1_core/proto/serializer.c:57.
        rpc.Write(payload);
        // GGDBG (REVERT) ----------------------------------------------------
        string ggdbgSent = "server BOD sending OPEN_DIALOG recipient=";
        ggdbgSent = ggdbgSent + recipient.GetPlainId();
        ggdbgSent = ggdbgSent + " secs=";
        ggdbgSent = ggdbgSent + secondsLeft.ToString();
        LFGungame_Debug.Log(ggdbgSent);
        // END GGDBG ---------------------------------------------------------
        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        rpc.Send(targetPlayer, LFGungame_RPC.OPEN_DIALOG, true, recipient);
#endif
    }

    void BroadcastCloseDialog(PlayerIdentity recipient)
    {
#ifdef SERVER
        if (!recipient)
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
            return;

        ScriptRPC rpc = new ScriptRPC();
        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        rpc.Send(targetPlayer, LFGungame_RPC.CLOSE_DIALOG, true, recipient);
#endif
    }

    protected void CloseOrphanedDialogs()
    {
#ifdef SERVER
        EnsureCollections();

        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry)
                continue;

            if (entry.promptState != LFGungame_PromptState.PROMPT_PENDING)
                continue;

            PlayerBase player = FindPlayerBySteamId(entry.steamId);
            if (!player)
                continue;

            PlayerIdentity identity = player.GetIdentity();
            if (identity)
                BroadcastCloseDialog(identity);
        }
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
            message = LFGungame_Constants.MSG_NO_WINNER;
        }
        else
        {
            message = LFGungame_Constants.MSG_WINNER_PREFIX;
            message = message + winnerName;
        }

        // 3_Game bus -> 5_Mission listener -> LBChatHandler.SendSimpleChatMessage (broadcast).
        LFGungame_AnnounceBus.Raise(null, message, true);
#endif
    }

    protected void CleanupParticipantsToPublic()
    {
#ifdef SERVER
        if (!EnsureConfig())
            return;

        EnsureCollections();

        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry)
                continue;

            if (entry.role != LFGungame_Role.PARTICIPANT)
                continue;

            PlayerBase player = FindPlayerBySteamId(entry.steamId);
            // Skip a dead participant (e.g. the just-killed victim on a victory kill): teleporting
            // and wiping a corpse mid-death is pointless and risky. The role reset below still applies.
            // R2: EntityAI.IsAlive() used on players in scripts/3_game/entities/entityai.c:788.
            if (player && player.IsAlive())
            {
                vector spawn = PickRandomSpawn(LFGungame_Role.PUBLIC);
                // R2: Object.SetPosition(vector) in scripts/3_game/entities/object.c:300.
                player.SetPosition(spawn);
                WipeInventory(player);
                EquipPlayer(player, LFGungame_Role.PUBLIC, 0);
                SetTierSync(player, 0);
            }
            else
            {
                // F-02: a dead/absent participant can't be cleaned now. Record their steamId so their
                // next respawn/reconnect teleports + re-equips them as public (I4 for the dead subset).
                // Stored outside m_PlayerEntries so it survives ResetStats (the entry does not).
                if (m_PendingPublicCleanup)
                    m_PendingPublicCleanup.Insert(entry.steamId);
            }

            entry.role = LFGungame_Role.PUBLIC;
            ApplyRoleToOnlinePlayer(entry.steamId, LFGungame_Role.PUBLIC);
        }

        string cleanupDetail = "participants returned to public";
        LogRule(LFGungame_Constants.RULE_ROUND_CLEANUP, cleanupDetail);
#endif
    }

    // G3-G5: answer a client's SCOREBOARD_REQ with the participant table (server-authoritative).
    // Serialized as count + per-row [name, tier, tierMax, kills, assists, deaths]; the client
    // reads the same order in LFGungame_PlayerBase.LFGungame_HandleScoreboardDataRPC.
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
        // participant (blocks public/spectators and the empty-board-refresh-forever when idle).
        if (m_State != LFGungame_State.ACTIVE)
            return;

        LFGungame_PlayerEntry senderEntry = GetEntry(sender.GetPlainId());
        if (!senderEntry || senderEntry.role != LFGungame_Role.PARTICIPANT)
            return;

        int tierMax = 0;
        if (EnsureConfig())
        {
            // R2: LFGungame_Config.GetGunPresets() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:163.
            array<string> guns = m_Config.GetGunPresets();
            if (guns)
                tierMax = guns.Count();
        }

        // Collect participants then rank them by the tiebreak (tier>kills>assists>deaths) so the
        // scoreboard Rank column is meaningful. Selection sort, best first (count <= MaxPlayers).
        array<LFGungame_PlayerEntry> ranked = new array<LFGungame_PlayerEntry>;
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry counted = m_PlayerEntries.GetElement(index);
            // F-01: only rank participants whose player is connected (no ghost rows).
            if (counted && counted.role == LFGungame_Role.PARTICIPANT && IsEntryOnline(counted))
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
                LFGungame_PlayerEntry swap = ranked.Get(a);
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
            LFGungame_PlayerEntry entry = ranked.Get(a);
            string name = GetPlayerName(entry.steamId);
            int rTier = entry.tier;
            int rKills = entry.kills;
            int rAssists = entry.assists;
            int rDeaths = entry.deaths;

            rpc.Write(name);
            rpc.Write(rTier);
            rpc.Write(tierMax);
            rpc.Write(rKills);
            rpc.Write(rAssists);
            rpc.Write(rDeaths);
        }

        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        rpc.Send(target, LFGungame_RPC.SCOREBOARD_DATA, true, sender);
#endif
    }

    // === LFGUNGAME DEBUG (REVERT BEFORE RELEASE) ============================================
    // Solo-test sim: lets an admin PARTICIPANT drive HUD/victory/scoreboard without a 2nd player.
    // Usage (after the admin joins a round): !ggsim tierup | tierdown | fillstats | win
    // Remove this method AND its !ggsim handler in LFGungame_ChatHandler before release.
    void DebugSim(PlayerIdentity sender, string action)
    {
#ifdef SERVER
        if (!CanMutateState())
            return;

        if (!sender)
            return;

        // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
        string steamId = sender.GetPlainId();
        PlayerBase player = FindPlayerBySteamId(steamId);
        LFGungame_PlayerEntry entry = GetEntry(steamId);
        if (!player || !entry)
            return;

        if (action == "tierup")
        {
            ChangeTier(player, 1);
            return;
        }

        if (action == "tierdown")
        {
            ChangeTier(player, -1);
            return;
        }

        if (action == "fillstats")
        {
            entry.kills = 7;
            entry.assists = 3;
            entry.deaths = 2;
            return;
        }

        if (action == "win")
        {
            int maxIndex = GetMaxTierIndex();
            if (maxIndex >= 0)
            {
                entry.tier = maxIndex;
                SetTierSync(player, maxIndex);
            }
            string winReason = "victory";
            EndRound(winReason, steamId);
            return;
        }
#endif
    }
    // === END LFGUNGAME DEBUG ================================================================

    protected vector PickRandomSpawn(int role)
    {
        array<vector> spawnList;
        float radius = 0.0;

        if (role == LFGungame_Role.PARTICIPANT)
        {
            // R2: LFGungame_Config.GetRespawnCoords() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:218.
            spawnList = m_Config.GetRespawnCoords();
            // R2: LFGungame_Config.GetRespawnRadius() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:202.
            radius = m_Config.GetRespawnRadius();
        }
        else
        {
            // R2: LFGungame_Config.GetPublicRespawnCoords() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:224.
            spawnList = m_Config.GetPublicRespawnCoords();
            // R2: LFGungame_Config.GetPublicRespawnRadius() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:210.
            radius = m_Config.GetPublicRespawnRadius();
        }

        DayZGame game = GetGame();
        if (!game)
        {
            // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
            return Vector(0, 0, 0);
        }

        if (!spawnList || spawnList.Count() == 0)
        {
            // R2: CGame.SurfaceY(float,float) in scripts/3_game/global/game.c:1162.
            float originY = game.SurfaceY(0, 0);
            // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
            return Vector(0, originY, 0);
        }

        // F-20v2: uniform-random coord pick. Was a round-robin index (reset to 0 each round) that
        // clustered players on the first coords of the list — physically close on the map. Reuse
        // m_SpawnRoundRobinIdx as "last coord used" to avoid two back-to-back spawns on the same coord.
        // R2: Math.RandomInt(min,max) is max-exclusive (enforce-script-reference Math section).
        int count = spawnList.Count();
        int idx = Math.RandomInt(0, count);
        if (count > 1 && idx == m_SpawnRoundRobinIdx)
            idx = (idx + 1) % count;
        m_SpawnRoundRobinIdx = idx;

        vector base = spawnList.Get(idx);
        float minRadius = 0.0 - radius;
        // R2: Math.RandomFloat(float,float) in scripts/1_core/proto/enmath.c:91.
        float dx = Math.RandomFloat(minRadius, radius);
        float dz = Math.RandomFloat(minRadius, radius);
        float x = base[0] + dx;
        float z = base[2] + dz;
        // C1 exact spawn: use the CONFIGURED Y of the coord (base[1]), NOT the terrain.
        // game.SurfaceY would snap to ground and drop players under structures / the map.
        float y = base[1];
        // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
        return Vector(x, y, z);
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
            // R2: LFGungame_Config.GetKnife() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:169.
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

        return false;
    }

    protected void EquipPlayer(PlayerBase player, int role, int tier)
    {
        if (!player)
            return;

        if (!m_Config)
            return;

        // R2: LFGungame_Config.GetClothingPreset() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:158.
        string clothing = m_Config.GetClothingPreset();
        if (clothing != string.Empty)
        {
            if (!LB_PresetLoader.Get)
            {
                LogRule("PRESET_LOADER_UNAVAILABLE_CLOTHING", clothing);
            }
            else
            {
                // R2: LB_PresetLoader.GetPreset(string) in LBmaster_Core/scripts/4_World/LBmaster_Core/Config/LB_PresetLoader.c:146.
                LB_PresetBase clothingPreset = LB_PresetLoader.Get.GetPreset(clothing);
                if (clothingPreset)
                {
                    // R2: LB_PresetBase.SpawnPreset(PlayerBase,EntityAI) in LBmaster_Core/scripts/4_World/LBmaster_Core/Struct/LB_PresetBase.c:138.
                    clothingPreset.SpawnPreset(player, player);
                }
                else
                {
                    LogRule("PRESET_RESOLVE_FAILED_CLOTHING", clothing);
                }
            }
        }

        if (role != LFGungame_Role.PARTICIPANT)
            return;

        int safeTier = tier;
        if (safeTier < 0)
            safeTier = 0;

        // R2: LFGungame_Config.GetGunPresets() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:163.
        array<string> guns = m_Config.GetGunPresets();
        if (guns && safeTier < guns.Count())
        {
            string gunName = guns.Get(safeTier);
            if (!LB_PresetLoader.Get)
            {
                LogRule("PRESET_LOADER_UNAVAILABLE_GUN", gunName);
            }
            else
            {
                LB_PresetBase gunPreset = LB_PresetLoader.Get.GetPreset(gunName);
                if (gunPreset)
                {
                    gunPreset.SpawnPreset(player, player);
                    // Chamber + load the freshly spawned tier weapon and bind it to the hotbar.
                    MakeWeaponReady(player);
                }
                else
                    LogRule("PRESET_RESOLVE_FAILED_GUN", gunName);
            }
        }

        // Knife (hotbar slot 2) + bandage (hotbar slot 3) are inventory items - NOT visible to other
        // clients - so both their CREATION and their bind are DEFERRED off this frame (3a). On a
        // participant birth EquipPlayer runs inside character creation; spawning them synchronously
        // there added to the server-frame stall every respawn (all clients hitched ~1s). Only the
        // visible loadout (clothing + weapon-in-hands) stays synchronous for the GG-01v5/v6 appearance.
        // R2: ScriptCallQueue.CallLater(func,int,bool,...) in scripts/2_gamelib/tools.c:61.
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(GG_GiveKnifeAndBandage, 0, false, player);
    }

    // Deferred (next tick) creation + hotbar bind of the knife (slot 2) and bandage (slot 3). These are
    // inventory items, invisible to other clients, so deferring their CREATION off the character-creation
    // frame is desync-safe and trims the birth-frame server stall (3a). Bind is inline here: by next tick
    // the FSM is stable and the character is released, so the sync-quickbar-during-birth hazard that
    // originally forced the bind to be deferred does not apply. Server-only.
    void GG_GiveKnifeAndBandage(PlayerBase player)
    {
#ifdef SERVER
        if (!player)
            return;

        // m3: skip a player who died/disconnected in the one-tick gap since the equip.
        // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
        if (!player.IsAlive())
            return;

        if (!m_Config)
            return;

        // R2: EntityAI.GetInventory() in scripts/3_game/entities/entityai.c:1834.
        GameInventory inventory = player.GetInventory();
        if (!inventory)
            return;

        // R2: LFGungame_Config.GetKnife() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:169.
        string knife = m_Config.GetKnife();
        if (knife != string.Empty)
        {
            // R2: GameInventory.CreateInInventory(string) in scripts/3_game/systems/inventory/inventory.c:876.
            EntityAI knifeItem = inventory.CreateInInventory(knife);
            // R2: PlayerBase.SetQuickBarEntityShortcut(EntityAI,int,bool) in scripts/4_world/entities/manbase/playerbase.c:2232.
            if (knifeItem)
                player.SetQuickBarEntityShortcut(knifeItem, 1, true);
        }

        // Bandage on hotbar slot 3 (index 2). Independent of the Knife config (m1). Scope recorded in
        // product-spec Changelog 2026-06-14 (C3/F2 amended). [verify in-game] not double with a bandage
        // already in the ClothingPreset.
        // R2: GameInventory.CreateInInventory(string) in scripts/3_game/systems/inventory/inventory.c:876.
        EntityAI bandageItem = inventory.CreateInInventory("BandageDressing");
        if (bandageItem)
            player.SetQuickBarEntityShortcut(bandageItem, 2, true);
#endif
    }

    // GG-01v5 birth-equip resolution: decide whether a fresh character about to be created belongs to a
    // participant in an ACTIVE round, and if so return the gungame spawn + tier so MissionServer can
    // create the character AT that spawn (no double-load lag) and equip it during creation. Pure read,
    // no state mutation; PickRandomSpawn's random pick is consumed once by the caller (OnPlayerRespawn
    // skips its own PickRandomSpawn for this player via the bornEquipped guard).
    bool ResolveParticipantBirthSpawn(string steamId, out vector spawn, out int tier)
    {
        spawn = vector.Zero;
        tier = 0;
#ifdef SERVER
        if (!CanMutateState())
            return false;

        if (steamId == string.Empty)
            return false;

        LFGungame_PlayerEntry entry = GetEntry(steamId);
        if (!entry)
            return false;

        if (entry.role != LFGungame_Role.PARTICIPANT)
            return false;

        if (m_State != LFGungame_State.ACTIVE)
            return false;

        if (!EnsureConfig())
            return false;

        spawn = PickRandomSpawn(LFGungame_Role.PARTICIPANT);
        tier = entry.tier;
        return true;
#else
        return false;
#endif
    }

    // GG-01v5: equip a participant DURING character creation (called from MissionServer.StartingEquipSetup,
    // before the engine releases the character to the network), so observers receive it already dressed.
    // Wipes the vanilla default loadout EquipCharacter just applied, then equips the tier preset. Marks
    // the steamId so the post-birth OnPlayerRespawn skips the (desyncing) teleport+wipe+equip path.
    void EquipParticipantAtBirth(PlayerBase player, string steamId, int tier)
    {
#ifdef SERVER
        if (!player)
            return;

        WipeInventory(player);
        EquipPlayer(player, LFGungame_Role.PARTICIPANT, tier);

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
        rpc.Send(targetPlayer, LFGungame_RPC.COUNTDOWN, true, recipient);
#endif
    }

    void EndFreeze()
    {
#ifdef SERVER
        SetFreezeForEntries(false);
        string message = "round freeze ended; entering live combat";
        LogRound(message);
#endif
    }

    protected void SetFreezeForEntries(bool frozen)
    {
#ifdef SERVER
        EnsureCollections();

        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry)
                continue;

            // Only participants are ever frozen (BeginRound gates the freeze on PARTICIPANT);
            // skip public and declined-admin entries so unfreeze never touches them.
            if (entry.role != LFGungame_Role.PARTICIPANT)
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

    protected void SetTierSync(PlayerBase player, int tier)
    {
#ifdef SERVER
        if (!player)
            return;

        int safeTier = tier;
        if (safeTier < 0)
            safeTier = 0;

        int maxTier = 0;
        if (m_Config)
        {
            // R2: LFGungame_Config.GetGunPresets() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:163.
            array<string> guns = m_Config.GetGunPresets();
            if (guns)
                maxTier = guns.Count();
        }

        player.m_GG_Tier = safeTier;
        player.m_GG_TierMax = maxTier;
        // R2: EntityAI.SetSynchDirty() in scripts/3_game/entities/entityai.c:3069.
        player.SetSynchDirty();

        // H2: leaders depend on the global max tier, so any tier sync recomputes all of them.
        // The known player ref is passed through: during the respawn window the fresh character
        // can be missing from GetPlayers()/alias to the corpse, and a flag pushed to the wrong
        // object left the local crown permanently hidden (in-game test 2026-06-11).
        RecalculateLeaders(player);
#endif
    }

    protected void RecalculateLeaders(PlayerBase knownPlayer = null)
    {
#ifdef SERVER
        EnsureCollections();

        // Resolve the by-steamId bypass for the caller-supplied player (see SetTierSync).
        string knownId = string.Empty;
        if (knownPlayer)
        {
            PlayerIdentity knownIdentity = knownPlayer.GetIdentity();
            if (knownIdentity)
                knownId = knownIdentity.GetPlainId();
        }

        int maxTier = -1;
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry)
                continue;
            if (entry.role != LFGungame_Role.PARTICIPANT)
                continue;

            // F-01: a disconnected participant must not set the leader tier (suppresses live
            // crown). The known player counts as online even before GetPlayers() lists it.
            bool entryOnline = false;
            if (knownId != string.Empty && entry.steamId == knownId)
                entryOnline = true;
            else
                entryOnline = IsEntryOnline(entry);

            if (!entryOnline)
                continue;
            if (entry.tier > maxTier)
                maxTier = entry.tier;
        }

        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry leaderEntry = m_PlayerEntries.GetElement(index);
            if (!leaderEntry)
                continue;
            if (leaderEntry.role != LFGungame_Role.PARTICIPANT)
                continue;

            PlayerBase player;
            if (knownId != string.Empty && leaderEntry.steamId == knownId)
                player = knownPlayer;
            else
                player = FindPlayerBySteamId(leaderEntry.steamId);

            if (!player)
                continue;

            int leaderFlag = 0;
            if (maxTier >= 0 && leaderEntry.tier == maxTier)
                leaderFlag = 1;

            // Only push when the flag actually changed (avoids redundant SetSynchDirty churn).
            if (player.m_GG_IsLeader != leaderFlag)
            {
                player.m_GG_IsLeader = leaderFlag;
                // R2: EntityAI.SetSynchDirty() in scripts/3_game/entities/entityai.c:3069.
                player.SetSynchDirty();
            }
        }
#endif
    }

    protected void StartVotingTimer()
    {
#ifdef SERVER
        EnsureTimers();
        StopTimer(m_VotingTimer);
        float votingSeconds = 60.0;
        string callbackName = "OnVotingClosed";
        // R2: Timer.Run(float,Managed,string,Param,bool) in scripts/3_game/tools/tools.c:595.
        m_VotingTimer.Run(votingSeconds, this, callbackName);
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

        if (m_State == LFGungame_State.ENDED)
        {
            // BUG-001 (vault bug-ledger): stats survive ENDED so S7 can compute winner/scoreboard before IDLE clears.
            ResetStats();
            m_State = LFGungame_State.IDLE;
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

        if (!EnsureConfig())
            return;

        if (!m_Config.GetAutostart())
            return;

        if (m_State != LFGungame_State.IDLE)
            return;

        EnsureTimers();
        StopTimer(m_AutostartTimer);

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

        if (m_State != LFGungame_State.IDLE)
        {
            // A manually started round is in progress; its ReturnToIdle re-arms the loop.
            string busyMessage = "autostart skipped: round in progress";
            LogRound(busyMessage);
            return;
        }

        string firingMessage = "autostart firing";
        LogRound(firingMessage);
        StartGame();

        // A rejected start (config load/validate failure) leaves the state IDLE: re-arm so a
        // live config fix resumes the unsupervised loop without waiting for the next restart.
        if (m_State == LFGungame_State.IDLE)
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
        if (m_State != LFGungame_State.ACTIVE)
            return;

        string winnerId = ComputeWinnerByTiebreak();
        string detail = "round timed out";
        LogRule(LFGungame_Constants.RULE_TIMEOUT_RECORDED, detail);

        string timeoutReason = "timeout";
        EndRound(timeoutReason, winnerId);
#endif
    }

    protected string ComputeWinnerByTiebreak()
    {
#ifdef SERVER
        EnsureCollections();

        string bestId = string.Empty;
        LFGungame_PlayerEntry best = null;
        int index = 0;
        for (index = 0; index < m_PlayerEntries.Count(); index = index + 1)
        {
            LFGungame_PlayerEntry entry = m_PlayerEntries.GetElement(index);
            if (!entry)
                continue;

            if (entry.role != LFGungame_Role.PARTICIPANT)
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

    protected bool IsBetterByTiebreak(LFGungame_PlayerEntry candidate, LFGungame_PlayerEntry current)
    {
        // Tiebreak (I2): higher tier, then more kills, then more assists, then fewer deaths.
        if (candidate.tier != current.tier)
            return candidate.tier > current.tier;

        if (candidate.kills != current.kills)
            return candidate.kills > current.kills;

        if (candidate.assists != current.assists)
            return candidate.assists > current.assists;

        if (candidate.deaths != current.deaths)
            return candidate.deaths < current.deaths;

        return false;
    }

    protected int GetMaxTierIndex()
    {
#ifdef SERVER
        if (!EnsureConfig())
            return -1;

        // R2: LFGungame_Config.GetGunPresets() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:163.
        array<string> guns = m_Config.GetGunPresets();
        if (!guns)
            return -1;

        return guns.Count() - 1;
#else
        return -1;
#endif
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

    protected PlayerIdentity GetIdentityForSteamId(string steamId)
    {
#ifdef SERVER
        PlayerBase player = FindPlayerBySteamId(steamId);
        if (!player)
            return null;

        return player.GetIdentity();
#else
        return null;
#endif
    }

    protected void PromptConnectedPlayers()
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

            LFGungame_PlayerEntry entry = GetOrCreateEntry(steamId);
            entry.role = LFGungame_Role.NONE;
            entry.promptState = LFGungame_PromptState.PROMPT_PENDING;
            ApplyRoleToPlayer(player, entry.role);

            // GGDBG (REVERT) ----------------------------------------------------
            string ggdbgPcp = "server PromptConnectedPlayers prompting steamId=";
            ggdbgPcp = ggdbgPcp + steamId;
            LFGungame_Debug.Log(ggdbgPcp);
            // END GGDBG ---------------------------------------------------------

            BroadcastOpenDialog(identity, GetVotingSecondsLeft());
        }
#endif
    }

    protected void EnsurePublicEntriesForConnectedPlayers()
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

            if (IsAdminSteamId(steamId))
                continue;

            LFGungame_PlayerEntry entry = GetEntry(steamId);
            if (!entry)
            {
                entry = GetOrCreateEntry(steamId);
                entry.role = LFGungame_Role.PUBLIC;
                entry.promptState = LFGungame_PromptState.NONE;
                ApplyRoleToPlayer(player, entry.role);
            }
        }
#endif
    }

    protected LFGungame_PlayerEntry GetOrCreateEntry(string steamId)
    {
        EnsureCollections();
        LFGungame_PlayerEntry entry = GetEntry(steamId);
        if (entry)
            return entry;

        entry = new LFGungame_PlayerEntry();
        entry.steamId = steamId;
        m_PlayerEntries.Set(steamId, entry);
        return entry;
    }

    protected LFGungame_PlayerEntry GetEntry(string steamId)
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

        // R2: LFGungame_Config.Load() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:25.
        m_Config = LFGungame_Config.Load();
        if (m_Config)
            return true;
#endif
        return false;
    }

    protected int GetVotingSecondsLeft()
    {
        int secondsLeft = 60;
#ifdef SERVER
        if (m_VotingTimer && m_VotingTimer.IsRunning())
        {
            // R2: Timer.GetRemaining() in scripts/3_game/tools/tools.c:318; Math.Ceil(float) in scripts/1_core/proto/enmath.c:441.
            secondsLeft = Math.Ceil(m_VotingTimer.GetRemaining());
        }

        if (secondsLeft < 0)
            secondsLeft = 0;
#endif
        return secondsLeft;
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
            m_PlayerEntries = new map<string, ref LFGungame_PlayerEntry>;
        if (!m_WarnLog)
            m_WarnLog = new map<string, int>;
        if (!m_PendingPublicCleanup)
            m_PendingPublicCleanup = new set<string>();
    }

    protected void EnsureTimers()
    {
        if (!m_RoundTimer)
            m_RoundTimer = new Timer();
        if (!m_VotingTimer)
            m_VotingTimer = new Timer();
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
        string message = "[LFGungame] ";
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

        int last = 0;
        // R2: map.Find(TKey,out TValue) in scripts/1_core/proto/enscript.c:858.
        if (m_WarnLog.Find(ruleId, last))
        {
            int elapsed = now - last;
            if (elapsed >= 0 && elapsed < LFGungame_Constants.LOG_RULE_INTERVAL_MS)
                return;
        }

        // R2: map.Set(TKey,TValue) in scripts/1_core/proto/enscript.c:883.
        m_WarnLog.Set(ruleId, now);

        string message = "[LFGungame][RoundManager] ";
        message = message + ruleId;
        message = message + " ";
        message = message + detail;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }
}
