#ifndef LB_DISABLE_CHAT
modded class LBChatHandler
{
    // R2: LBChatHandler.ChatCommandExists(string) in LBmaster_Groups/scripts/5_Mission/LBmaster_Groups/System/LBChatHandler.c:22.
    override bool ChatCommandExists(string cmd)
    {
        if (super.ChatCommandExists(cmd))
            return true;

        if (LFGungame_IsCommand(cmd))
            return true;

        return false;
    }

    // R2: LBChatHandler.HasPermission(PlayerIdentity,string) in LBmaster_Groups/scripts/5_Mission/LBmaster_Groups/System/LBChatHandler.c:36.
    override bool HasPermission(PlayerIdentity sender, string cmd)
    {
        bool superAllowed = super.HasPermission(sender, cmd);
        if (!LFGungame_IsCommand(cmd))
            return superAllowed;

        return LFGungame_IsAdmin(sender);
    }

    // R2: LBChatHandler.OnChatCommand(PlayerIdentity,string,TStringArray) in LBmaster_Groups/scripts/5_Mission/LBmaster_Groups/System/LBChatHandler.c:52.
    override void OnChatCommand(PlayerIdentity sender, string cmd, TStringArray args)
    {
        super.OnChatCommand(sender, cmd, args);

        if (!LFGungame_IsCommand(cmd))
            return;

        if (!LFGungame_IsAdmin(sender))
            return;

        LFGungame_RoundManager manager = LFGungame_RoundManager.GetInstance();
        if (!manager)
        {
            string missingMessage = "command ignored: manager unavailable";
            LFGungame_LogChat(missingMessage);
            return;
        }

        int state = manager.GetState();
        if (cmd == "gungame")
        {
            if (state == LFGungame_State.IDLE)
            {
                manager.StartGame();
                string startedAck = LFGungame_Constants.MSG_ROUND_STARTED;
                SendSimpleChatMessage(sender, startedAck, false);
                return;
            }

            string runningMessage = "start ignored: round already running";
            LFGungame_LogChat(runningMessage);
            return;
        }

        if (cmd == "gungamestop")
        {
            if (state != LFGungame_State.IDLE)
            {
                string reason = "admin_stop";
                string emptyWinner = string.Empty;
                manager.EndRound(reason, emptyWinner);
                string stoppedAck = LFGungame_Constants.MSG_ROUND_STOPPED;
                SendSimpleChatMessage(sender, stoppedAck, false);
                return;
            }

            string idleMessage = "stop ignored: round is idle";
            LFGungame_LogChat(idleMessage);
            // F-27: return so a no-op gungamestop doesn't fall through to the block(s) below.
            return;
        }

        // === LFGUNGAME DEBUG (REVERT BEFORE RELEASE): simulate gungame events for solo testing ===
        if (cmd == "ggsim")
        {
            string action = "";
            if (args && args.Count() > 0)
                action = args.Get(0);

            manager.DebugSim(sender, action);

            string simAck = "ggsim ";
            simAck = simAck + action;
            SendSimpleChatMessage(sender, simAck, false);
            return;
        }
        // === END LFGUNGAME DEBUG ===
    }

    protected bool LFGungame_IsCommand(string cmd)
    {
        if (cmd == "gungame")
            return true;

        if (cmd == "gungamestop")
            return true;

        // === LFGUNGAME DEBUG (REVERT BEFORE RELEASE): solo-test sim command ===
        if (cmd == "ggsim")
            return true;
        // === END LFGUNGAME DEBUG ===

        return false;
    }

    protected bool LFGungame_IsAdmin(PlayerIdentity sender)
    {
        if (!sender)
            return false;

        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return false;

        if (!game.IsDedicatedServer())
            return false;

        // R2: PlayerIdentity.GetPlainId() in scripts/3_game/gameplay.c:370.
        string steamId = sender.GetPlainId();
        if (steamId == string.Empty)
            return false;

        // F-09: consult the round manager's config view (cached for the round) instead of a fresh disk
        // load, so the command permission and the in-round admin logic share one source of truth and
        // can't diverge if an admin edits config.json mid-round.
        LFGungame_RoundManager manager = LFGungame_RoundManager.GetInstance();
        if (manager)
            return manager.IsAdminSteamId(steamId);

        // No manager available (non-dedicated / not yet created): fall back to a direct load.
        // R2: LFGungame_Config.Load() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:25.
        LFGungame_Config config = LFGungame_Config.Load();
        if (!config)
            return false;

        // R2: LFGungame_Config.GetAdmins() in LFGungame/scripts/3_Game/LFGungame/LFGungame_Config.c:152.
        array<string> admins = config.GetAdmins();
        if (!admins)
            return false;

        int adminIndex = 0;
        for (adminIndex = 0; adminIndex < admins.Count(); adminIndex = adminIndex + 1)
        {
            string adminId = admins.Get(adminIndex);
            if (adminId == steamId)
                return true;
        }

        return false;
    }

    protected void LFGungame_LogChat(string detail)
    {
        string message = "[LFGungame] ";
        message = message + detail;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }
}
#endif
