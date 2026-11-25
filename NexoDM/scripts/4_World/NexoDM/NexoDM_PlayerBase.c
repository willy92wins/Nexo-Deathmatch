modded class PlayerBase
{
    int m_GG_Role;
    // Server-only cache: EEKilled after logout can run with GetIdentity() == null.
    string m_GG_SteamId;
    int m_NexoDM_VitalsNextMs;
    int m_NexoDM_SentHealth;
    int m_NexoDM_SentBlood;
    int m_NexoDM_SentWater;
    int m_NexoDM_SentFood;
    int m_NexoDM_KoStartMs;

    void PlayerBase()
    {
        m_GG_Role = NexoDM_Role.NONE;
        m_GG_SteamId = string.Empty;

        string roleSyncName = "m_GG_Role";
        // R2: EntityAI.RegisterNetSyncVariableInt(string,int,int) in scripts/3_game/entities/entityai.c:2865.
        RegisterNetSyncVariableInt(roleSyncName);

        m_NexoDM_VitalsNextMs = 0;
        m_NexoDM_SentHealth = -1;
        m_NexoDM_SentBlood = -1;
        m_NexoDM_SentWater = -1;
        m_NexoDM_SentFood = -1;
        m_NexoDM_KoStartMs = 0;
    }

    // R2: PlayerBase.OnScheduledTick(float) in scripts/4_world/entities/manbase/playerbase.c:3007.
    // Client MissionGameplay.TickScheduler also calls it (dev/F5-vitals.md:19); the body is server-only.
    override void OnScheduledTick(float deltaTime)
    {
        super.OnScheduledTick(deltaTime);
#ifdef SERVER
        NexoDM_TickVitals();
#endif
    }

    // R2: PlayerBase.OnRPC(PlayerIdentity,int,ParamsReadContext) in scripts/4_world/entities/manbase/playerbase.c:5450.
    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);

        if (rpc_type == NexoDM_RPC.COUNTDOWN)
        {
            NexoDM_HandleCountdownRPC(ctx);
            return;
        }

        if (rpc_type == NexoDM_RPC.SCOREBOARD_REQ)
        {
            NexoDM_HandleScoreboardReqRPC(sender);
            return;
        }

        if (rpc_type == NexoDM_RPC.SCOREBOARD_DATA)
        {
            NexoDM_HandleScoreboardDataRPC(ctx);
            return;
        }

        if (rpc_type == NexoDM_RPC.KILLER_CAM)
        {
            NexoDM_HandleKillerCamRPC(ctx);
            return;
        }

        if (rpc_type == NexoDM_RPC.HUD_VITALS || rpc_type == NexoDM_RPC.HUD_ROUND || rpc_type == NexoDM_RPC.HUD_SELF || rpc_type == NexoDM_RPC.HUD_TOP5 || rpc_type == NexoDM_RPC.HUD_KILL || rpc_type == NexoDM_RPC.HUD_GLOBAL_TOP10)
        {
#ifdef SERVER
            // Client must not drive HUD state. Drop the payload.
            return;
#else
            if (rpc_type == NexoDM_RPC.HUD_VITALS)
                NexoDM_HandleHudVitalsRPC(ctx);
            if (rpc_type == NexoDM_RPC.HUD_ROUND)
                NexoDM_HandleHudRoundRPC(ctx);
            if (rpc_type == NexoDM_RPC.HUD_SELF)
                NexoDM_HandleHudSelfRPC(ctx);
            if (rpc_type == NexoDM_RPC.HUD_TOP5)
                NexoDM_HandleHudTop5RPC(ctx);
            if (rpc_type == NexoDM_RPC.HUD_KILL)
                NexoDM_HandleHudKillRPC(ctx);
            if (rpc_type == NexoDM_RPC.HUD_GLOBAL_TOP10)
                NexoDM_HandleHudGlobalTop10RPC(ctx);
            return;
#endif
        }
    }

    // R2: PlayerBase.EEDelete(EntityAI) in scripts/4_world/entities/manbase/playerbase.c:1393.
    override void EEDelete(EntityAI parent)
    {
#ifdef SERVER
        // F7: cancel a pending short-KO wake so it never fires on a deleted body.
        // R2: GetGame() in scripts/3_game/gameplay.c:636.
        DayZGame koGame = GetGame();
        if (koGame)
        {
            // R2: CGame.GetCallQueue(int) in scripts/3_game/global/game.c:1550;
            // ScriptCallQueue.Remove(func) in scripts/2_gamelib/tools.c:65.
            koGame.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(NexoDM_KoWake);
        }
#endif
#ifndef SERVER
        DayZGame game = GetGame();
        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:1090.
        if (game && this == PlayerBase.Cast(game.GetPlayer()))
        {
            NexoDM_HudData data = NexoDM_HudData.Get();
            if (data)
                data.ResetExceptGlobal();
        }
#endif
        super.EEDelete(parent);
    }

    // F7 short KO (server only, round or not): 1 s after unconsciousness starts, raise shock to
    // max, then re-raise every 500 ms while still unconscious (a later hit can drop shock again
    // before vanilla wakes the player), capped at 10 s from the start; vanilla then wakes the
    // player under its own rules (incl. its ~2 s minimum).
    // R2: PlayerBase.OnUnconsciousStart() in scripts/4_world/entities/manbase/playerbase.c:3756.
    override void OnUnconsciousStart()
    {
        super.OnUnconsciousStart();

#ifdef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636.
        DayZGame koStartGame = GetGame();
        if (!koStartGame)
            return;

        // R2: CGame.IsServer() in scripts/3_game/global/game.c:1120.
        if (!koStartGame.IsServer())
            return;

        // R2: PlayerBase.IsPlayerDisconnected() in scripts/4_world/entities/manbase/playerbase.c:2638
        // (set true by PlayerBase.OnDisconnect() in scripts/4_world/entities/manbase/playerbase.c:7665).
        if (IsPlayerDisconnected())
            return;

        // R2: CGame.GetTime() in scripts/3_game/global/game.c:1535.
        m_NexoDM_KoStartMs = koStartGame.GetTime();

        string koStartMsg = "[NexoDM][KO] unconscious start t=";
        koStartMsg = koStartMsg + m_NexoDM_KoStartMs.ToString();
        koStartMsg = koStartMsg + " ms";
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(koStartMsg);

        // R2: CGame.GetCallQueue(int) in scripts/3_game/global/game.c:1550;
        // ScriptCallQueue.Remove(func) in scripts/2_gamelib/tools.c:65;
        // ScriptCallQueue.CallLater(func,int,bool,...) in scripts/2_gamelib/tools.c:61.
        koStartGame.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(NexoDM_KoWake);
        koStartGame.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(NexoDM_KoWake, NexoDM_Constants.KO_WAKE_DELAY_MS, false);
#endif
    }

    protected void NexoDM_KoWake()
    {
#ifdef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636.
        DayZGame koWakeGame = GetGame();
        if (!koWakeGame)
            return;

        // R2: CGame.IsServer() in scripts/3_game/global/game.c:1120.
        if (!koWakeGame.IsServer())
            return;

        // R2: PlayerBase.IsPlayerDisconnected() in scripts/4_world/entities/manbase/playerbase.c:2638
        // (set true by PlayerBase.OnDisconnect() in scripts/4_world/entities/manbase/playerbase.c:7665).
        // No raise and no re-schedule: vanilla combat-log death of a disconnected body is respected.
        if (IsPlayerDisconnected())
            return;

        // F7 r2 #2: during the logout countdown OnDisconnect has not run yet, so also stop while the
        // mission has this player in its logout queue. R2: Mission.IsPlayerDisconnecting(Man) in
        // scripts/3_game/gameplay.c:730 (MissionServer implementation in scripts/5_mission/mission/missionserver.c:240).
        Mission koMission = koWakeGame.GetMission();
        if (koMission && koMission.IsPlayerDisconnecting(this))
            return;

        // R2: Object.IsAlive() in scripts/3_game/entities/object.c:517.
        if (!IsAlive())
            return;

        // R2: EntityAI.IsSetForDeletion() in scripts/3_game/entities/entityai.c:812.
        if (IsSetForDeletion())
            return;

        // R2: Man.IsUnconscious() in scripts/3_game/entities/man.c:63
        // (PlayerBase impl in scripts/4_world/entities/manbase/playerbase.c:3925).
        if (!IsUnconscious())
            return;

        // R2: CGame.GetTime() in scripts/3_game/global/game.c:1535.
        int koWakeMs = koWakeGame.GetTime();
        int koElapsedMs = koWakeMs - m_NexoDM_KoStartMs;
        if (koElapsedMs >= NexoDM_Constants.KO_WAKE_MAX_MS)
            return;

        // R2: Object.SetHealth(string,string,float) in scripts/3_game/entities/object.c:1015
        // (vanilla shock write in scripts/4_world/classes/playermodifiers/modifiers/diseases/heatstroke.c:383);
        // Object.GetMaxHealth(string,string) in scripts/3_game/entities/object.c:1008
        // (vanilla shock max read in scripts/4_world/classes/shockhandler.c:35).
        SetHealth("", "Shock", GetMaxHealth("", "Shock"));

        string koWakeMsg = "[NexoDM][KO] shock raised to max t=";
        koWakeMsg = koWakeMsg + koWakeMs.ToString();
        koWakeMsg = koWakeMsg + " ms start=";
        koWakeMsg = koWakeMsg + m_NexoDM_KoStartMs.ToString();
        koWakeMsg = koWakeMsg + " ms";
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(koWakeMsg);

        // Re-raise every KO_WAKE_REPEAT_MS while the guards above still pass, but schedule
        // nothing at or past the KO_WAKE_MAX_MS cap from the start, so the loop terminates.
        if (koElapsedMs + NexoDM_Constants.KO_WAKE_REPEAT_MS < NexoDM_Constants.KO_WAKE_MAX_MS)
        {
            // R2: CGame.GetCallQueue(int) in scripts/3_game/global/game.c:1550;
            // ScriptCallQueue.CallLater(func,int,bool,...) in scripts/2_gamelib/tools.c:61.
            koWakeGame.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(NexoDM_KoWake, NexoDM_Constants.KO_WAKE_REPEAT_MS, false);
        }
#endif
    }

    // R2: PlayerBase.OnDisconnect() in scripts/4_world/entities/manbase/playerbase.c:7647.
    override void OnDisconnect()
    {
        super.OnDisconnect();

#ifdef SERVER
        if (!NexoDM_IsDedicatedServer())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (!manager)
            return;

        string steamId = m_GG_SteamId;
        PlayerIdentity identity = GetIdentity();
        if (identity)
        {
            // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
            string plainId = identity.GetPlainId();
            if (plainId != string.Empty)
                steamId = plainId;
        }
        manager.OnParticipantDisconnected(steamId);
#else
        DayZGame game = GetGame();
        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:1090.
        if (game && this == PlayerBase.Cast(game.GetPlayer()))
        {
            NexoDM_HudData data = NexoDM_HudData.Get();
            if (data)
                data.Reset();
        }
#endif
    }

    // R2: PlayerBase.EEKilled(Object) in scripts/4_world/entities/manbase/playerbase.c:1183.
    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);

#ifdef SERVER
        if (!NexoDM_IsDedicatedServer())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
            manager.OnPlayerKilled(this, killer);
#endif
    }

    // R2: PlayerBase.EEHitBy(TotalDamageResult,int,EntityAI,int,string,string,vector,float) in scripts/4_world/entities/manbase/playerbase.c:1224.
    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

#ifdef SERVER
        if (!NexoDM_IsDedicatedServer())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
            manager.OnPlayerDamaged(this, source);
#endif
    }

    protected void NexoDM_HandleCountdownRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (game.IsDedicatedServer())
            return;

        Param1<int> payload = new Param1<int>(0);
        // R2: Serializer.Read(void) in scripts/1_core/proto/serializer.c:58.
        if (!ctx.Read(payload))
            return;

        if (payload.param1 <= 0)
            return;

        NexoDM_HUD hud = NexoDM_HUD.Get();
        if (hud)
            hud.StartCountdown(payload.param1);
#endif
    }

    protected void NexoDM_HandleScoreboardReqRPC(PlayerIdentity sender)
    {
#ifdef SERVER
        if (!NexoDM_IsDedicatedServer())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
            manager.OnScoreboardRequest(sender);
#endif
    }

    protected void NexoDM_HandleScoreboardDataRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (game.IsDedicatedServer())
            return;

        int count = 0;
        // R2: Serializer.Read(void) in scripts/1_core/proto/serializer.c:58.
        if (!ctx.Read(count))
            return;

        // F-21: defensive upper bound on a server-trusted count, so a buggy/forged SCOREBOARD_DATA
        // can't drive a huge alloc/loop on the client (participants are capped well below this).
        if (count < 0 || count > 256)
            return;

        array<ref NexoDM_ScoreRow> rows = new array<ref NexoDM_ScoreRow>;

        int index = 0;
        for (index = 0; index < count; index = index + 1)
        {
            string name;
            int kills;
            int assists;
            int deaths;
            int points;
            if (!ctx.Read(name))
                return;
            if (!ctx.Read(kills))
                return;
            if (!ctx.Read(assists))
                return;
            if (!ctx.Read(deaths))
                return;
            if (!ctx.Read(points))
                return;

            NexoDM_ScoreRow row = new NexoDM_ScoreRow();
            row.name = name;
            row.kills = kills;
            row.assists = assists;
            row.deaths = deaths;
            row.points = points;
            rows.Insert(row);
        }

        NexoDM_Scoreboard board = NexoDM_Scoreboard.GetInstance();
        if (board)
            board.SetData(rows);
#endif
    }

    // #2: while the killer camera (or its queued respawn) is active, the vanilla
    // 0.1s death check must not cover the screen. Vanilla: OnSelectPlayer starts
    // m_DeathCheckTimer every 0.1s in scripts/4_world/entities/manbase/playerbase.c:6393-6394,
    // CheckDeath() at :7304-7311 calls SimulateDeath(true) at
    // scripts/4_world/entities/dayzplayerimplement.c:844-880 (LockControls :849,
    // ShowDeadScreen fade :868, DeathEffectTimer -> PPERequester_DeathDarkening
    // exposure -10 in scripts/3_game/ppemanager/requesters/pperdeathdarkening.c:12-15).
    // When the camera is not active, super.CheckDeath() keeps vanilla death intact.
    // R2: PlayerBase.CheckDeath() in scripts/4_world/entities/manbase/playerbase.c:7304.
    override void CheckDeath()
    {
#ifndef SERVER
        NexoDM_KillerCam deathCam = NexoDM_KillerCam.Get();
        if (deathCam && deathCam.IsSuppressingDeathScreen())
        {
            DayZGame deathGame = GetGame();
            // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:1090.
            if (deathGame && this == PlayerBase.Cast(deathGame.GetPlayer()))
            {
                // R2: DayZPlayerImplement.ShowDeadScreen(bool,float) in
                // scripts/4_world/entities/dayzplayerimplement.c:802 (fade-out :823-830).
                ShowDeadScreen(false, 0);
                // R2: DayZPlayerImplement.StopDeathDarkeningEffect() in
                // scripts/4_world/entities/dayzplayerimplement.c:835-842.
                StopDeathDarkeningEffect();
                return;
            }
        }
#endif
        super.CheckDeath();
    }

    protected void NexoDM_HandleKillerCamRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (game.IsDedicatedServer())
            return;

        // #11: ignore a late RPC on a body that is not the current local player,
        // or while a (manual) respawn is already in progress.
        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:1090.
        if (this != PlayerBase.Cast(game.GetPlayer()))
            return;

        // R2: CGame.GetMission() in scripts/3_game/global/game.c:1093;
        // Mission.IsPlayerRespawning() in scripts/3_game/gameplay.c:827
        // (MissionGameplay impl in scripts/5_mission/mission/missiongameplay.c:1702).
        Mission killerMission = game.GetMission();
        if (killerMission && killerMission.IsPlayerRespawning())
            return;

        Param3<int, int, int> payload = new Param3<int, int, int>(0, 0, 0);
        // R2: Serializer.Read(void) in scripts/1_core/proto/serializer.c:58.
        // Fail-closed: an incomplete payload is discarded whole (no camera, no respawn).
        if (!ctx.Read(payload))
            return;

        if (payload.param3 < 0)
            return;

        vector corpsePos = GetPosition();
        NexoDM_KillerCam cam = NexoDM_KillerCam.Get();
        if (!cam)
            return;

        cam.Start(payload.param1, payload.param2, payload.param3, corpsePos);
#endif
    }

    protected void NexoDM_TickVitals()
    {
#ifdef SERVER
        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: CGame.IsServer() in scripts/3_game/global/game.c:1120.
        if (!game.IsServer())
            return;

        // R2: DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        if (!game.IsDedicatedServer())
            return;

        if (m_GG_Role != NexoDM_Role.PARTICIPANT)
            return;

        // R2: Object.IsAlive() in scripts/3_game/entities/object.c:523.
        if (!IsAlive())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (!manager)
            return;
        if (manager.GetState() != NexoDM_State.ACTIVE)
            return;

        // R2: DayZGame.GetTime() in scripts/3_game/global/game.c:1510.
        int nowMs = game.GetTime();
        if (nowMs < m_NexoDM_VitalsNextMs)
            return;
        m_NexoDM_VitalsNextMs = nowMs + 1000;

        int health = NexoDM_Percent(GetHealth("", "Health"), GetMaxHealth("", "Health"));
        // Blood max is 5000 (dev/F5-vitals.md:14). GetBlood() from TransferValues is not this ratio.
        int blood = NexoDM_Percent(GetHealth("", "Blood"), 5000.0);
        int water = 0;
        int food = 0;
        // R2: PlayerBase.GetStatWater() in scripts/4_world/entities/manbase/playerbase.c:8197.
        // R2: PlayerStatBase.Get() in scripts/4_world/classes/playerstats/playerstatbase.c:134.
        // R2: PlayerConstants.SL_WATER_MAX in scripts/3_game/playerconstants.c:51.
        PlayerStat<float> waterStat = GetStatWater();
        if (waterStat)
            water = NexoDM_Percent(waterStat.Get(), PlayerConstants.SL_WATER_MAX);
        // R2: PlayerBase.GetStatEnergy() in scripts/4_world/entities/manbase/playerbase.c:8215.
        // R2: PlayerConstants.SL_ENERGY_MAX in scripts/3_game/playerconstants.c:45.
        PlayerStat<float> foodStat = GetStatEnergy();
        if (foodStat)
            food = NexoDM_Percent(foodStat.Get(), PlayerConstants.SL_ENERGY_MAX);

        // R2: Object.GetHealth(string,string) in scripts/3_game/entities/object.c:994.
        // R2: Object.GetMaxHealth(string,string) in scripts/3_game/entities/object.c:1008.
        if (health == m_NexoDM_SentHealth && blood == m_NexoDM_SentBlood && water == m_NexoDM_SentWater && food == m_NexoDM_SentFood)
            return;

        PlayerIdentity identity = GetIdentity();
        if (!identity)
            return;

        m_NexoDM_SentHealth = health;
        m_NexoDM_SentBlood = blood;
        m_NexoDM_SentWater = water;
        m_NexoDM_SentFood = food;

        ScriptRPC rpc = new ScriptRPC();
        Param4<int, int, int, int> payload = new Param4<int, int, int, int>(health, blood, water, food);
        // R2: Serializer.Write(void) in scripts/1_core/proto/serializer.c:57.
        rpc.Write(payload);
        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        // Delta-only: a lost packet is not repeated, so delivery is guaranteed.
        rpc.Send(this, NexoDM_RPC.HUD_VITALS, true, identity);
#endif
    }

    void NexoDM_ResetVitalsSent()
    {
        m_NexoDM_SentHealth = -1;
        m_NexoDM_SentBlood = -1;
        m_NexoDM_SentWater = -1;
        m_NexoDM_SentFood = -1;
    }

    protected int NexoDM_Percent(float value, float maxValue)
    {
        if (maxValue <= 0)
            return 0;
        float scaled = (value / maxValue) * 100.0;
        if (scaled <= 0)
            return 0;
        int whole = scaled;
        float frac = scaled - whole;
        if (frac >= 0.5)
            whole = whole + 1;
        if (whole > 100)
            return 100;
        if (whole < 0)
            return 0;
        return whole;
    }

    protected bool NexoDM_PercentInRange(int value)
    {
        if (value < 0)
            return false;
        if (value > 100)
            return false;
        return true;
    }

    protected void NexoDM_HandleHudVitalsRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        DayZGame vitalsGame = GetGame();
        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:1090.
        // A late packet on the corpse must not overwrite the new body's store.
        if (!vitalsGame || this != PlayerBase.Cast(vitalsGame.GetPlayer()))
            return;

        Param4<int, int, int, int> payload = new Param4<int, int, int, int>(0, 0, 0, 0);
        // R2: Serializer.Read(void) in scripts/1_core/proto/serializer.c:58.
        if (!ctx.Read(payload))
            return;
        if (!NexoDM_PercentInRange(payload.param1))
            return;
        if (!NexoDM_PercentInRange(payload.param2))
            return;
        if (!NexoDM_PercentInRange(payload.param3))
            return;
        if (!NexoDM_PercentInRange(payload.param4))
            return;

        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        data.SetVitals(payload.param1, payload.param2, payload.param3, payload.param4);
#endif
    }

    protected void NexoDM_HandleHudRoundRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        Param3<int, int, int> payload = new Param3<int, int, int>(0, 0, 0);
        if (!ctx.Read(payload))
            return;
        if (payload.param1 < 0)
            return;
        if (payload.param2 < 0)
            return;
        if (payload.param3 != 0 && payload.param3 != 1)
            return;

        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        bool active = false;
        if (payload.param3 == 1)
            active = true;
        data.SetRound(payload.param1, payload.param2, active);
#endif
    }

    protected void NexoDM_HandleHudSelfRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        Param4<int, int, int, int> payload = new Param4<int, int, int, int>(0, 0, 0, 0);
        if (!ctx.Read(payload))
            return;
        if (payload.param1 < 0)
            return;
        if (payload.param2 < 0)
            return;
        if (payload.param3 < 0)
            return;
        if (payload.param4 < 0)
            return;

        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        data.SetSelf(payload.param1, payload.param2, payload.param3, payload.param4);
#endif
    }

    protected void NexoDM_HandleHudTop5RPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        int count = 0;
        if (!ctx.Read(count))
            return;
        if (count < 0 || count > 5)
            return;

        array<ref NexoDM_HudTopRow> rows = new array<ref NexoDM_HudTopRow>;
        int index = 0;
        for (index = 0; index < count; index = index + 1)
        {
            string name;
            int points;
            int kills;
            int deaths;
            if (!ctx.Read(name))
                return;
            if (name == string.Empty)
                return;
            if (!ctx.Read(points))
                return;
            if (!ctx.Read(kills))
                return;
            if (!ctx.Read(deaths))
                return;

            NexoDM_HudTopRow row = new NexoDM_HudTopRow();
            row.name = name;
            row.points = points;
            row.kills = kills;
            row.deaths = deaths;
            rows.Insert(row);
        }

        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        data.SetTop(rows);
#endif
    }

    protected void NexoDM_HandleHudGlobalTop10RPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        int count = 0;
        if (!ctx.Read(count))
            return;
        if (count < 0 || count > 10)
            return;

        array<ref NexoDM_HudGlobalRow> rows = new array<ref NexoDM_HudGlobalRow>;
        int index = 0;
        for (index = 0; index < count; index = index + 1)
        {
            string name;
            int points;
            int kills;
            int isReceiver;
            if (!ctx.Read(name))
                return;
            if (name == string.Empty)
                return;
            if (!ctx.Read(points))
                return;
            if (!ctx.Read(kills))
                return;
            if (!ctx.Read(isReceiver))
                return;
            if (points < 0)
                return;
            if (kills < 0)
                return;
            if (isReceiver < 0)
                return;

            NexoDM_HudGlobalRow row = new NexoDM_HudGlobalRow();
            row.name = name;
            row.points = points;
            row.kills = kills;
            row.isReceiver = isReceiver;
            rows.Insert(row);
        }

        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        data.SetGlobal(rows);
#endif
    }

    protected void NexoDM_HandleHudKillRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        string killerName;
        string victimName;
        string weaponClass;
        if (!ctx.Read(killerName))
            return;
        if (!ctx.Read(victimName))
            return;
        if (!ctx.Read(weaponClass))
            return;

        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        data.AddKill(killerName, victimName, weaponClass);
#endif
    }

    protected bool NexoDM_IsDedicatedServer()
    {
        DayZGame game = GetGame();
        if (!game)
            return false;

        // R2: DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        if (!game.IsDedicatedServer())
            return false;

        return true;
    }
}
