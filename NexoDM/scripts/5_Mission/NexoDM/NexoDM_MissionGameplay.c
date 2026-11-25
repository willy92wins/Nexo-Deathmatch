modded class MissionGameplay
{
    protected ref NexoDM_HUD m_HUD;
    protected UAInput m_GG_ScoreInput;
    // No ref: UAIDWrapper has a private destructor and vanilla keeps it as a plain member.
    // R2: UAIDWrapper in scripts/3_game/inputapi/uainput.c:24-30; vanilla use in
    // scripts/4_world/classes/useractionscomponent/actioninput.c:21.
    protected UAIDWrapper m_NexoDM_Top5Wrapper;

    // R2: MissionGameplay.OnInit() in scripts/5_mission/mission/missiongameplay.c:96.
    override void OnInit()
    {
        super.OnInit();

        if (!m_HUD)
        {
            m_HUD = new NexoDM_HUD();
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

        // F5 r2 #14: the client-side HUD data is a static singleton; clear it when the mission ends
        // (disconnect, return to menu) so a later connection never shows stale round data.
        NexoDM_HudData hudData = NexoDM_HudData.Get();
        if (hudData)
            hudData.Reset();

        // F3 p3: never leak the killer camera past the mission.
        NexoDM_KillerCam killerCam = NexoDM_KillerCam.Get();
        if (killerCam)
            killerCam.Abort();
    }

    // R2: MissionGameplay.OnUpdate(float) in scripts/5_mission/mission/missiongameplay.c:278.
    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        if (m_HUD)
            m_HUD.OnUpdate(timeslice);

        // F3 p3: per-frame killer-camera tracking (re-resolves the killer each frame).
        NexoDM_KillerCam killerCamTick = NexoDM_KillerCam.Get();
        if (killerCamTick)
        {
            killerCamTick.OnUpdate(timeslice);
            // Correccion 1 (#1/#3/#4): the actual vanilla respawn lives here in
            // 5_Mission because it needs MissionGameplay. KillerCam (4_World) only
            // queues it; ConsumeRespawnIfReady fires once (still-local corpse, dead,
            // not already respawning, CanRespawnPlayer) - never in the RPC frame.
            if (killerCamTick.ConsumeRespawnIfReady())
                NexoDM_PerformKillerRespawn();
        }

        NexoDM_UpdateScoreboardToggle();
        NexoDM_UpdateScoreboardEscClose();
        NexoDM_UpdateTop5Hold();
    }

    // Correccion 1 (#1): vanilla menu respawn flow, identical to GameRespawn in
    // scripts/5_mission/gui/ingamemenu.c:345-363. Only 5_Mission may touch
    // MissionGameplay/DestroyAllMenus (scripts/5_mission/mission/missiongameplay.c:1158).
    protected void NexoDM_PerformKillerRespawn()
    {
        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: DayZGame.GetMenuDefaultCharacterData(bool) in scripts/3_game/global/game.c:1593;
        // MenuDefaultCharacterData.SetRandomCharacterForced(bool) in scripts/3_game/gameplay.c:1115.
        MenuDefaultCharacterData charData = game.GetMenuDefaultCharacterData(false);
        if (charData)
            charData.SetRandomCharacterForced(false);
        // R2: CGame.RespawnPlayer() in scripts/3_game/global/game.c:1107.
        game.RespawnPlayer();

        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:1090.
        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (player)
        {
            // R2: DayZPlayerImplement.SimulateDeath(bool) in scripts/4_world/entities/dayzplayerimplement.c:844.
            player.SimulateDeath(true);
            // R2: DayZPlayerImplement.ShowDeadScreen(bool,float) in scripts/4_world/entities/dayzplayerimplement.c:802;
            // queued as in ingamemenu.c:354.
            game.GetCallQueue(CALL_CATEGORY_GUI).Call(player.ShowDeadScreen, true, 0);
        }

        // R2: MissionGameplay.DestroyAllMenus() in scripts/5_mission/mission/missiongameplay.c:1158.
        DestroyAllMenus();
        // R2: MissionGameplay.SetPlayerRespawning(bool) in scripts/5_mission/mission/missiongameplay.c:1697
        // (Mission.SetPlayerRespawning declared in scripts/3_game/gameplay.c:825).
        SetPlayerRespawning(true);
        // R2: MissionGameplay.Continue() in scripts/5_mission/mission/missiongameplay.c:1334
        // (Mission.Continue declared in scripts/3_game/gameplay.c:773).
        Continue();
    }

    // ESC also closes the scoreboard. The board is a passive overlay (UseKeyboard false), so it
    // never receives OnKeyDown, and vanilla ignores UAUIMenu while a scripted menu is open
    // (missiongameplay.c:640-697 only handles inventory/gestures/quickbar/pause) - poll it here.
    protected void NexoDM_UpdateScoreboardEscClose()
    {
        NexoDM_Scoreboard board = NexoDM_Scoreboard.GetInstance();
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

    // G3: toggle the scoreboard with the rebindable UANexoDMScoreboard input (default L).
    protected void NexoDM_UpdateScoreboardToggle()
    {
        // R2: GetUApi() in scripts/3_game/inputapi/uainput.c:239; GetInputByName :178; LocalPress :50.
        if (!m_GG_ScoreInput)
        {
            UAInputAPI api = GetUApi();
            if (!api)
                return;

            string inputName = "UANexoDMScoreboard";
            m_GG_ScoreInput = api.GetInputByName(inputName);
        }

        if (!m_GG_ScoreInput)
            return;

        if (m_GG_ScoreInput.LocalPress())
            NexoDM_ToggleScoreboard();
    }

    // F5b: Top 5 visible only while UANexoDMTop5 is held (default T, rebindable).
    protected void NexoDM_UpdateTop5Hold()
    {
        if (!m_HUD)
            return;
        if (!m_NexoDM_Top5Wrapper)
        {
            UAInputAPI api = GetUApi();
            if (!api)
                return;
            string top5Name = "UANexoDMTop5";
            UAInput top5Input = api.GetInputByName(top5Name);
            if (!top5Input)
                return;
            m_NexoDM_Top5Wrapper = top5Input.GetPersistentWrapper();
        }
        if (!m_NexoDM_Top5Wrapper)
            return;
        UAInput input = m_NexoDM_Top5Wrapper.InputP();
        if (!input)
            return;
        bool held = input.LocalValue() > 0;
        if (input.LocalPress())
            held = true;
        if (input.LocalRelease())
            held = false;
        m_HUD.SetTop5Visible(held);
    }

    protected void NexoDM_ToggleScoreboard()
    {
        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: CGame.GetUIManager() used in NexoDM_PlayerBase.c:167.
        UIManager ui = game.GetUIManager();
        if (!ui)
            return;

        NexoDM_Scoreboard board = NexoDM_Scoreboard.GetInstance();
        if (board)
        {
            board.CloseBoard();
            return;
        }

        // Only participants open the scoreboard (the server also gates). Closing above is always allowed.
        PlayerBase localPlayer = PlayerBase.Cast(game.GetPlayer());
        if (!localPlayer || localPlayer.m_GG_Role != NexoDM_Role.PARTICIPANT)
            return;

        // R2: UIManager.EnterScriptedMenu(int,UIMenuPanel) in scripts/3_game/tools/uimanager.c:4.
        ui.EnterScriptedMenu(NexoDM_MenuIDs.SCOREBOARD, NULL);
    }

    // R2: MissionBase.CreateScriptedMenu(int) in scripts/5_mission/mission/missionbase.c:185.
    override UIScriptedMenu CreateScriptedMenu(int id)
    {
        UIScriptedMenu menu = super.CreateScriptedMenu(id);
        if (menu)
            return menu;

        if (id == NexoDM_MenuIDs.SCOREBOARD)
        {
            menu = new NexoDM_Scoreboard();
            return menu;
        }

        return null;
    }
}
