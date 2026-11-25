modded class MissionGameplay
{
    protected ref LFGungame_HUD m_HUD;
    protected UAInput m_GG_ScoreInput;

    // R2: MissionGameplay.OnInit() in scripts/5_mission/mission/missiongameplay.c:96.
    override void OnInit()
    {
        super.OnInit();

        if (!m_HUD)
        {
            m_HUD = new LFGungame_HUD();
            if (m_HUD)
                m_HUD.Show(true);
        }
    }

    // R2: MissionGameplay.OnMissionFinish() in scripts/5_mission/mission/missiongameplay.c:257.
    override void OnMissionFinish()
    {
        super.OnMissionFinish();

        if (m_HUD)
            m_HUD.Destroy();

        m_HUD = null;
    }

    // R2: MissionGameplay.OnUpdate(float) in scripts/5_mission/mission/missiongameplay.c:278.
    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        if (m_HUD)
            m_HUD.OnUpdate(timeslice);

        LFGungame_UpdateScoreboardToggle();
        LFGungame_UpdateScoreboardEscClose();
    }

    // ESC also closes the scoreboard. The board is a passive overlay (UseKeyboard false), so it
    // never receives OnKeyDown, and vanilla ignores UAUIMenu while a scripted menu is open
    // (missiongameplay.c:640-697 only handles inventory/gestures/quickbar/pause) - poll it here.
    protected void LFGungame_UpdateScoreboardEscClose()
    {
        LFGungame_Scoreboard board = LFGungame_Scoreboard.GetInstance();
        if (!board)
            return;

        // R2: GetUApi() in scripts/3_game/inputapi/uainput.c:239; GetInputByID pattern in
        // scripts/5_mission/mission/missiongameplay.c:691 (UAUIMenu) / :677 (UAUIBack).
        UAInputAPI api = GetUApi();
        if (!api)
            return;

        if (api.GetInputByID(UAUIMenu).LocalPress() || api.GetInputByID(UAUIBack).LocalPress())
            board.CloseBoard();
    }

    // G3: toggle the scoreboard with the rebindable UALFGungameScoreboard input (default L).
    protected void LFGungame_UpdateScoreboardToggle()
    {
        // R2: GetUApi() in scripts/3_game/inputapi/uainput.c:239; GetInputByName :178; LocalPress :50.
        if (!m_GG_ScoreInput)
        {
            UAInputAPI api = GetUApi();
            if (!api)
                return;

            string inputName = "UALFGungameScoreboard";
            m_GG_ScoreInput = api.GetInputByName(inputName);
        }

        if (!m_GG_ScoreInput)
            return;

        if (m_GG_ScoreInput.LocalPress())
            LFGungame_ToggleScoreboard();
    }

    protected void LFGungame_ToggleScoreboard()
    {
        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: CGame.GetUIManager() used in LFGungame_PlayerBase.c:167.
        UIManager ui = game.GetUIManager();
        if (!ui)
            return;

        LFGungame_Scoreboard board = LFGungame_Scoreboard.GetInstance();
        if (board)
        {
            board.CloseBoard();
            return;
        }

        // Only participants open the scoreboard (the server also gates). Closing above is always allowed.
        PlayerBase localPlayer = PlayerBase.Cast(game.GetPlayer());
        if (!localPlayer || localPlayer.m_GG_Role != LFGungame_Role.PARTICIPANT)
            return;

        // R2: UIManager.EnterScriptedMenu(int,UIMenuPanel) in scripts/3_game/tools/uimanager.c:4.
        ui.EnterScriptedMenu(LFGungame_MenuIDs.SCOREBOARD, NULL);
    }

    // R2: MissionBase.CreateScriptedMenu(int) in scripts/5_mission/mission/missionbase.c:185.
    override UIScriptedMenu CreateScriptedMenu(int id)
    {
        UIScriptedMenu menu = super.CreateScriptedMenu(id);
        if (menu)
            return menu;

        if (id == LFGungame_MenuIDs.JOIN)
        {
            menu = new LFGungame_JoinMenu();
            return menu;
        }

        if (id == LFGungame_MenuIDs.SCOREBOARD)
        {
            menu = new LFGungame_Scoreboard();
            return menu;
        }

        return null;
    }
}
