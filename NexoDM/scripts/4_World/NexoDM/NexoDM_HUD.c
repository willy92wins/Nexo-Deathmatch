class NexoDM_HUD : Managed
{
    protected static NexoDM_HUD s_Instance;

    protected Widget m_Root;
    protected Widget m_CountGroup;
    // m_CountLabel re-added (was removed in F-18 on the wrong assumption "the layout shows their static
    // text"): HUD widgets are workspace-created and #STR keys baked in the .layout render the RAW key
    // here, so the localized round tag + countdown label are resolved by script in CacheWidgets.
    // m_CountLabelEN stays static ("Starting in", the bilingual gray sub-line).
    protected TextWidget m_CountLabel;
    protected TextWidget m_CountNumber;
    protected ImageWidget m_CountBar;
    protected TextWidget m_RoundTag;
    protected float m_TimeLeft;
    protected int m_LastShown;
    protected float m_CountTotal;
    protected float m_BarBaseW;
    protected float m_BarBaseH;
    // Freeze-phase gate: hide the vanilla HUD while the round-start countdown runs.
    protected bool m_CountdownActive;

    void NexoDM_HUD()
    {
        m_TimeLeft = -1;
        m_LastShown = -1;
        m_CountTotal = 0;
        m_BarBaseW = 0;
        m_BarBaseH = 0;
        m_CountdownActive = false;

        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: DayZGame.GetWorkspace() in scripts/3_game/global/game.c:84.
        WorkspaceWidget workspace = game.GetWorkspace();
        if (!workspace)
            return;

        string layoutPath = "NexoDM/gui/layouts/nexodm_hud.layout";
        // R2: WorkspaceWidget.CreateWidgets(string,Widget,bool) in scripts/1_core/proto/enwidgets.c:181.
        m_Root = workspace.CreateWidgets(layoutPath);
        if (!m_Root)
            return;

        CacheWidgets();
        HideCountdown();

        s_Instance = this;
    }

    void ~NexoDM_HUD()
    {
        Destroy();
    }

    static NexoDM_HUD Get()
    {
        return s_Instance;
    }

    void Destroy()
    {
        if (s_Instance == this)
            s_Instance = null;

        // Mid-countdown teardown must not leave the vanilla hud hidden.
        if (m_CountdownActive)
        {
            m_CountdownActive = false;
            SetVanillaHudVisible(true);
        }

        if (m_Root)
        {
            // R2: Widget.Unlink() in scripts/1_core/proto/enwidgets.c:173.
            m_Root.Unlink();
        }

        m_Root = null;
        m_CountGroup = null;
        m_CountNumber = null;
        m_CountBar = null;
        m_RoundTag = null;
        m_TimeLeft = -1;
        m_CountTotal = 0;
    }

    void Show(bool show)
    {
        if (!m_Root)
            return;

        // R2: Widget.Show(bool,bool) in scripts/1_core/proto/enwidgets.c:125.
        m_Root.Show(show);
    }

    void StartCountdown(int seconds)
    {
        if (seconds <= 0)
        {
            HideCountdown();
            return;
        }

        m_TimeLeft = seconds;
        m_CountTotal = seconds;
        m_LastShown = -1;

        // Freeze gate ON: only the countdown is visible until it reaches zero.
        m_CountdownActive = true;
        SetVanillaHudVisible(false);

        if (m_CountGroup)
            m_CountGroup.Show(true);

        if (m_RoundTag)
            m_RoundTag.Show(true);

        if (m_CountBar)
        {
            // R2: Widget.SetSize(float,float,bool) in scripts/1_core/proto/enwidgets.c:141.
            m_CountBar.SetSize(m_BarBaseW, m_BarBaseH);
        }

        UpdateCountdownNumber(seconds);
    }

    void OnUpdate(float timeslice)
    {
        if (m_TimeLeft < 0)
            return;

        m_TimeLeft = m_TimeLeft - timeslice;
        if (m_TimeLeft < 0)
            m_TimeLeft = 0;

        // R2: Math.Ceil(float) in scripts/1_core/proto/enmath.c:441.
        int secs = Math.Ceil(m_TimeLeft);
        if (secs != m_LastShown)
            UpdateCountdownNumber(secs);

        if (m_CountBar && m_CountTotal > 0)
        {
            float frac = m_TimeLeft / m_CountTotal;
            if (frac < 0)
                frac = 0;
            if (frac > 1)
                frac = 1;
            // R2: Widget.SetSize(float,float,bool) in scripts/1_core/proto/enwidgets.c:141.
            m_CountBar.SetSize(m_BarBaseW * frac, m_BarBaseH);
        }

        if (m_TimeLeft <= 0)
        {
            HideCountdown();
            m_TimeLeft = -1;
        }
    }

    protected void CacheWidgets()
    {
        if (!m_Root)
            return;

        string countGroupName = "CountGroup";
        string countNumberName = "CountNumber";
        string countBarName = "CountBar";
        string roundTagName = "RoundTag";

        // R2: Widget.FindAnyWidget(string) in scripts/1_core/proto/enwidgets.c:169.
        m_CountGroup = m_Root.FindAnyWidget(countGroupName);
        m_CountNumber = TextWidget.Cast(m_Root.FindAnyWidget(countNumberName));
        m_CountBar = ImageWidget.Cast(m_Root.FindAnyWidget(countBarName));
        m_RoundTag = TextWidget.Cast(m_Root.FindAnyWidget(roundTagName));

        string countLabelName = "CountLabel";
        m_CountLabel = TextWidget.Cast(m_Root.FindAnyWidget(countLabelName));

        // Workspace-created HUD: #STR keys baked in the .layout render the raw key here (a UIScriptedMenu
        // resolves them, a raw workspace widget does not). Resolve the round tag + countdown label
        // explicitly to the client language. R2: Widget.TranslateString(string) in scripts/1_core/proto/enwidgets.c:112.
        if (m_RoundTag)
            m_RoundTag.SetText(Widget.TranslateString("#STR_NEXODM_HUD_ROUND_TAG"));
        if (m_CountLabel)
            m_CountLabel.SetText(Widget.TranslateString("#STR_NEXODM_HUD_STARTING"));

        if (m_CountBar)
        {
            // Base size feeds the width animation in OnUpdate (relative units, exact-size off).
            // R2: Widget.GetSize(out float,out float) in scripts/1_core/proto/enwidgets.c:152.
            m_CountBar.GetSize(m_BarBaseW, m_BarBaseH);
        }
    }

    protected void UpdateCountdownNumber(int secs)
    {
        m_LastShown = secs;
        if (!m_CountNumber)
            return;

        // R2: int.ToString() in scripts/1_core/proto/enconvert.c:30.
        string text = secs.ToString();
        // R2: TextWidget.SetText(string,bool) in scripts/1_core/proto/enwidgets.c:195.
        m_CountNumber.SetText(text);

        if (secs <= 3)
        {
            // R2: Widget.SetColor(int) in scripts/1_core/proto/enwidgets.c:144.
            m_CountNumber.SetColor(NexoDM_UIConst.COL_DANGER);
        }
        else
        {
            // R2: Widget.SetColor(int) in scripts/1_core/proto/enwidgets.c:144.
            m_CountNumber.SetColor(NexoDM_UIConst.COL_TEXT);
        }
    }

    protected void HideCountdown()
    {
        if (m_CountGroup)
            m_CountGroup.Show(false);

        if (m_RoundTag)
            m_RoundTag.Show(false);

        // Freeze gate OFF: restore the vanilla hud. Constructor-time calls skip this branch.
        if (m_CountdownActive)
        {
            m_CountdownActive = false;
            SetVanillaHudVisible(true);
        }
    }

    // Temporarily hides the vanilla quickbar + left hud during the freeze countdown. Uses the
    // *_HIDE context flags (ShowQuickbarPlayer/ShowHudPlayer) so the player's option-menu flags
    // (QUICKBAR_DISABLE/HUD_DISABLE, ingamehud.c:888/924) are never clobbered.
    protected void SetVanillaHudVisible(bool show)
    {
        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: CGame.GetMission() in scripts/3_game/global/game.c:1095.
        Mission mission = game.GetMission();
        if (!mission)
            return;

        // R2: Mission.GetHud() in scripts/3_game/gameplay.c:720; class Hud :637.
        Hud hud = mission.GetHud();
        if (!hud)
            return;

        // R2: Hud.ShowQuickbarPlayer(bool) in scripts/3_game/gameplay.c:666; ShowHudPlayer :667.
        hud.ShowQuickbarPlayer(show);
        hud.ShowHudPlayer(show);
    }
}
