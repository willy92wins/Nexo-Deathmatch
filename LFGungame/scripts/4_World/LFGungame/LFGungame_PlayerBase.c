modded class PlayerBase
{
    int m_GG_Role;
    int m_GG_Tier;
    int m_GG_TierMax;
    int m_GG_IsLeader;
    protected static ref map<string, int> s_GG_RpcWarnLog;
    // Client-side: seconds carried from OPEN_DIALOG to the deferred JOIN open (join-prompt fix).
    protected int m_GG_PendingDialogSecs;
    // Client-side: deferred JOIN open retries while a foreground menu blocks EnterScriptedMenu.
    protected int m_GG_PendingDialogRetries;

    void PlayerBase()
    {
        m_GG_Role = LFGungame_Role.NONE;
        m_GG_Tier = 0;
        m_GG_TierMax = 0;
        m_GG_IsLeader = 0;

        string roleSyncName = "m_GG_Role";
        string tierSyncName = "m_GG_Tier";
        string tierMaxSyncName = "m_GG_TierMax";
        string leaderSyncName = "m_GG_IsLeader";
        // R2: EntityAI.RegisterNetSyncVariableInt(string,int,int) in scripts/3_game/entities/entityai.c:2865.
        RegisterNetSyncVariableInt(roleSyncName);
        RegisterNetSyncVariableInt(tierSyncName);
        RegisterNetSyncVariableInt(tierMaxSyncName);
        RegisterNetSyncVariableInt(leaderSyncName);
    }

    // R2: PlayerBase.OnRPC(PlayerIdentity,int,ParamsReadContext) in scripts/4_world/entities/manbase/playerbase.c:5450.
    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);

        if (rpc_type == LFGungame_RPC.VOTE)
        {
            LFGungame_HandleVoteRPC(sender, ctx);
            return;
        }

        if (rpc_type == LFGungame_RPC.OPEN_DIALOG)
        {
            LFGungame_HandleOpenDialogRPC(ctx);
            return;
        }

        if (rpc_type == LFGungame_RPC.COUNTDOWN)
        {
            LFGungame_HandleCountdownRPC(ctx);
            return;
        }

        if (rpc_type == LFGungame_RPC.CLOSE_DIALOG)
        {
            LFGungame_HandleCloseDialogRPC();
            return;
        }

        if (rpc_type == LFGungame_RPC.SCOREBOARD_REQ)
        {
            LFGungame_HandleScoreboardReqRPC(sender);
            return;
        }

        if (rpc_type == LFGungame_RPC.SCOREBOARD_DATA)
        {
            LFGungame_HandleScoreboardDataRPC(ctx);
            return;
        }
    }

    // R2: PlayerBase.OnVariablesSynchronized() in scripts/4_world/entities/manbase/playerbase.c:5886.
    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();

#ifndef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (game.IsDedicatedServer())
            return;

        // Only the local player drives its own bottom-left tier HUD (this hook also fires for
        // other players' synced PlayerBases inside the network bubble).
        // R2: CGame.GetPlayer() used for the local player in JoinMenu.c:243.
        if (PlayerBase.Cast(game.GetPlayer()) != this)
            return;

        LFGungame_HUD hud = LFGungame_HUD.Get();
        if (!hud)
            return;

        bool isLeader = (m_GG_IsLeader != 0);
        hud.UpdateTier(m_GG_Role, m_GG_Tier, m_GG_TierMax, isLeader);
#endif
    }

    // R2: PlayerBase.EEKilled(Object) in scripts/4_world/entities/manbase/playerbase.c:1183.
    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);

#ifdef SERVER
        if (!LFGungame_IsDedicatedServer())
            return;

        LFGungame_RoundManager manager = LFGungame_RoundManager.GetInstance();
        if (manager)
            manager.OnPlayerKilled(this, killer);
#endif
    }

    // R2: PlayerBase.EEHitBy(TotalDamageResult,int,EntityAI,int,string,string,vector,float) in scripts/4_world/entities/manbase/playerbase.c:1224.
    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

#ifdef SERVER
        if (!LFGungame_IsDedicatedServer())
            return;

        LFGungame_RoundManager manager = LFGungame_RoundManager.GetInstance();
        if (manager)
            manager.OnPlayerDamaged(this, source);
#endif
    }

    protected void LFGungame_HandleVoteRPC(PlayerIdentity sender, ParamsReadContext ctx)
    {
#ifdef SERVER
        if (!LFGungame_IsDedicatedServer())
            return;

        Param1<bool> vote = new Param1<bool>(false);
        // R2: Serializer.Read(void) in scripts/1_core/proto/serializer.c:58.
        if (!ctx.Read(vote))
        {
            LFGungame_WarnRpc(LFGungame_Constants.RULE_VOTE_BAD_PAYLOAD);
            return;
        }

        if (!sender)
        {
            LFGungame_WarnRpc(LFGungame_Constants.RULE_VOTE_NO_SENDER);
            return;
        }

        // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
        string steamId = sender.GetPlainId();
        if (steamId == string.Empty)
        {
            LFGungame_WarnRpc(LFGungame_Constants.RULE_VOTE_NO_STEAMID);
            return;
        }

        LFGungame_RoundManager manager = LFGungame_RoundManager.GetInstance();
        if (!manager)
        {
            LFGungame_WarnRpc(LFGungame_Constants.RULE_VOTE_NO_MANAGER);
            return;
        }

        if (manager.GetState() != LFGungame_State.VOTING)
        {
            LFGungame_WarnRpc(LFGungame_Constants.RULE_VOTE_BAD_STATE);
            return;
        }

        if (!manager.HasPromptPending(steamId))
        {
            LFGungame_WarnRpc(LFGungame_Constants.RULE_VOTE_NOT_PENDING);
            return;
        }

        manager.OnPlayerVote(steamId, vote.param1);
#endif
    }

    protected void LFGungame_HandleOpenDialogRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
        {
            return;
        }

        if (game.IsDedicatedServer())
        {
            return;
        }

        Param1<int> payload = new Param1<int>(0);
        // R2: Serializer.Read(void) in scripts/1_core/proto/serializer.c:58.
        if (!ctx.Read(payload))
        {
            return;
        }

        int secondsLeft = payload.param1;
        if (secondsLeft < 0)
            secondsLeft = 0;

        // GGDBG (REVERT) ----------------------------------------------------
        string ggdbgRecv = "client HandleOpenDialog received secs=";
        ggdbgRecv = ggdbgRecv + secondsLeft.ToString();
        LFGungame_Debug.Log(ggdbgRecv);
        // END GGDBG ---------------------------------------------------------

        // If a dialog already exists, just refresh its countdown.
        LFGungame_JoinMenu existingMenu = LFGungame_JoinMenu.GetInstance();
        if (existingMenu)
        {
            // GGDBG (REVERT)
            LFGungame_Debug.Log("client HandleOpenDialog existing menu -> SetCountdown");
            // END GGDBG
            existingMenu.SetCountdown(secondsLeft);
            return;
        }

        // Defer opening the JOIN menu. The command launcher's own !gungame makes LBmaster's
        // ChatInputMenu schedule its Close at +0.1s (LBmaster ChatInputMenu.c:67). Opening a
        // parent=NULL scripted menu while that menu is still the UIManager's current menu makes
        // the engine drop the wrong root (single current-menu; vanilla only opens a NULL-parent
        // menu when GetMenu()==NULL, missiongameplay.c:784). Opening ~200ms later lets the chat
        // input close first, so JOIN enters on a clean stack. Only the launcher hits this race.
        m_GG_PendingDialogSecs = secondsLeft;
        m_GG_PendingDialogRetries = 0;
        // GGDBG (REVERT)
        LFGungame_Debug.Log("client HandleOpenDialog scheduling deferred JOIN open (+200ms)");
        // END GGDBG
        // R2: ScriptCallQueue.CallLater(func,int,bool,...) pattern in scripts/2_gamelib/tools.c:46 (delay in ms).
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(LFGungame_OpenJoinDeferred, 200, false);
#endif
    }

    protected void LFGungame_OpenJoinDeferred()
    {
#ifndef SERVER
        DayZGame game = GetGame();
        if (!game)
            return;

        // A second OPEN_DIALOG may have created the menu while this was pending.
        LFGungame_JoinMenu existingMenu = LFGungame_JoinMenu.GetInstance();
        if (existingMenu)
        {
            existingMenu.SetCountdown(m_GG_PendingDialogSecs);
            return;
        }

        UIManager uiManager = game.GetUIManager();
        if (!uiManager)
        {
            // GGDBG (REVERT)
            LFGungame_Debug.Log("client deferred open uiManager NULL -> abort");
            // END GGDBG
            return;
        }

        // A foreground menu (inventory, map, admin tools, chat) makes the engine drop a
        // NULL-parent scripted menu: vanilla only opens one when GetMenu()==NULL
        // (missiongameplay.c:784). Retry once per second while the vote window lasts
        // instead of losing the prompt for every player who had any menu open.
        // R2: UIManager.GetMenu() in scripts/3_game/tools/uimanager.c:59.
        if (uiManager.GetMenu())
        {
            // GGDBG (REVERT)
            LFGungame_Debug.Log("client deferred open blocked by foreground menu -> retry");
            // END GGDBG
            LFGungame_ScheduleJoinRetry();
            return;
        }

        // GGDBG (REVERT)
        LFGungame_Debug.Log("client deferred open entering JOIN scripted menu");
        // END GGDBG
        // R2: UIManager.EnterScriptedMenu(int,UIMenuPanel) in scripts/3_game/tools/uimanager.c:4.
        UIScriptedMenu raw = uiManager.EnterScriptedMenu(LFGungame_MenuIDs.JOIN, NULL);
        LFGungame_JoinMenu menu = LFGungame_JoinMenu.Cast(raw);
        // GGDBG (REVERT) ----------------------------------------------------
        if (menu)
            LFGungame_Debug.Log("client deferred EnterScriptedMenu JOIN -> menu OK");
        else
            LFGungame_Debug.Log("client deferred EnterScriptedMenu JOIN -> menu NULL (cast/create failed)");
        // END GGDBG ---------------------------------------------------------
        if (menu)
            menu.SetCountdown(m_GG_PendingDialogSecs);
        else
            LFGungame_ScheduleJoinRetry();
#endif
    }

    // Reschedules the deferred JOIN open while the voting window is still running.
    // CLOSE_DIALOG cancels the chain via ScriptCallQueue.Remove (GG-05), and the
    // per-retry seconds decrement keeps a late open's countdown roughly accurate
    // while doubling as the window-expired stop condition.
    protected void LFGungame_ScheduleJoinRetry()
    {
#ifndef SERVER
        if (m_GG_PendingDialogSecs <= 1)
            return;

        m_GG_PendingDialogRetries = m_GG_PendingDialogRetries + 1;
        if (m_GG_PendingDialogRetries > 90)
            return;

        m_GG_PendingDialogSecs = m_GG_PendingDialogSecs - 1;

        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: ScriptCallQueue.CallLater(func,int,bool,...) in scripts/2_gamelib/tools.c:46 (delay in ms).
        game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(LFGungame_OpenJoinDeferred, 1000, false);
#endif
    }

    protected void LFGungame_HandleCountdownRPC(ParamsReadContext ctx)
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

        LFGungame_HUD hud = LFGungame_HUD.Get();
        if (hud)
            hud.StartCountdown(payload.param1);
#endif
    }

    protected void LFGungame_HandleCloseDialogRPC()
    {
#ifndef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (game.IsDedicatedServer())
            return;

        // GG-05: a CLOSE_DIALOG arriving inside the 200ms deferred-open window must also cancel
        // the scheduled open, or a stale JOIN menu pops after the round was stopped/closed.
        // R2: ScriptCallQueue.Remove(func) in scripts/2_gamelib/tools.c:65 (no-op when not queued).
        game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(LFGungame_OpenJoinDeferred);

        LFGungame_JoinMenu existingMenu = LFGungame_JoinMenu.GetInstance();
        if (existingMenu)
            existingMenu.ForceClose();
#endif
    }

    protected void LFGungame_HandleScoreboardReqRPC(PlayerIdentity sender)
    {
#ifdef SERVER
        if (!LFGungame_IsDedicatedServer())
            return;

        LFGungame_RoundManager manager = LFGungame_RoundManager.GetInstance();
        if (manager)
            manager.OnScoreboardRequest(sender);
#endif
    }

    protected void LFGungame_HandleScoreboardDataRPC(ParamsReadContext ctx)
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

        array<ref LFGungame_ScoreRow> rows = new array<ref LFGungame_ScoreRow>;

        int index = 0;
        for (index = 0; index < count; index = index + 1)
        {
            string name;
            int tier;
            int tierMax;
            int kills;
            int assists;
            int deaths;
            if (!ctx.Read(name))
                return;
            if (!ctx.Read(tier))
                return;
            if (!ctx.Read(tierMax))
                return;
            if (!ctx.Read(kills))
                return;
            if (!ctx.Read(assists))
                return;
            if (!ctx.Read(deaths))
                return;

            LFGungame_ScoreRow row = new LFGungame_ScoreRow();
            row.name = name;
            row.tier = tier;
            row.tierMax = tierMax;
            row.kills = kills;
            row.assists = assists;
            row.deaths = deaths;
            rows.Insert(row);
        }

        LFGungame_Scoreboard board = LFGungame_Scoreboard.GetInstance();
        if (board)
            board.SetData(rows);
#endif
    }

    protected bool LFGungame_IsDedicatedServer()
    {
        DayZGame game = GetGame();
        if (!game)
            return false;

        // R2: DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        if (!game.IsDedicatedServer())
            return false;

        return true;
    }

    protected void LFGungame_WarnRpc(string ruleId)
    {
#ifdef SERVER
        if (!s_GG_RpcWarnLog)
            s_GG_RpcWarnLog = new map<string, int>();

        int now = 0;
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.GetTime() in scripts/3_game/global/game.c:1510.
        if (GetGame())
            now = GetGame().GetTime();

        int last = 0;
        // R2: map.Find(TKey,out TValue) in scripts/1_core/proto/enscript.c:858.
        if (s_GG_RpcWarnLog.Find(ruleId, last))
        {
            int elapsed = now - last;
            if (elapsed >= 0 && elapsed < LFGungame_Constants.LOG_RULE_INTERVAL_MS)
                return;
        }

        // R2: map.Set(TKey,TValue) in scripts/1_core/proto/enscript.c:883.
        s_GG_RpcWarnLog.Set(ruleId, now);

        string message = "[LFGungame][VoteRPC] ";
        message = message + ruleId;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
#endif
    }
}
