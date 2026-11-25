class NexoDM_JoinMenu extends UIScriptedMenu
{
    protected static ref NexoDM_JoinMenu s_Instance;

    protected ButtonWidget m_BtnYes;
    protected ButtonWidget m_BtnNo;
    protected TextWidget m_CountdownText;
    protected ref Timer m_Timer;
    protected int m_SecondsLeft;
    protected bool m_Closed;

    void NexoDM_JoinMenu()
    {
        m_SecondsLeft = 0;
        m_Closed = false;
        m_Timer = new Timer();
    }

    void ~NexoDM_JoinMenu()
    {
        StopTimer();
        if (s_Instance == this)
            s_Instance = null;
    }

    override Widget Init()
    {
        DayZGame game = GetGame();
        if (!game)
            return null;

        WorkspaceWidget workspace = game.GetWorkspace();
        if (!workspace)
            return null;

        string layoutPath = "NexoDM/gui/layouts/nexodm_join.layout";
        // R2: WorkspaceWidget.CreateWidgets(string,Widget,bool) in scripts/1_core/proto/enwidgets.c:181.
        layoutRoot = workspace.CreateWidgets(layoutPath);
        // GGDBG (REVERT) ----------------------------------------------------
        if (!layoutRoot)
            NexoDM_Debug.Log("JoinMenu.Init CreateWidgets returned NULL for nexodm_join.layout");
        else
            NexoDM_Debug.Log("JoinMenu.Init layout loaded OK");
        // END GGDBG ---------------------------------------------------------
        if (!layoutRoot)
            return null;

        s_Instance = this;

        string btnYesName = "BtnYes";
        string btnNoName = "BtnNo";
        string countdownName = "CountdownText";
        string yesBgName = "BtnYesBg";

        // R2: Widget.FindAnyWidget(string) in scripts/1_core/proto/enwidgets.c:169; Class.Cast(Class) in scripts/1_core/proto/enscript.c:94.
        m_BtnYes = ButtonWidget.Cast(layoutRoot.FindAnyWidget(btnYesName));
        m_BtnNo = ButtonWidget.Cast(layoutRoot.FindAnyWidget(btnNoName));
        m_CountdownText = TextWidget.Cast(layoutRoot.FindAnyWidget(countdownName));

        Widget yesBg = layoutRoot.FindAnyWidget(yesBgName);
        if (yesBg)
            yesBg.SetColor(NexoDM_UIConst.COL_ACCENT);

        if (m_CountdownText)
            m_CountdownText.SetColor(NexoDM_UIConst.COL_MUTED);

        InitText();
        UpdateCountdownText();
        return layoutRoot;
    }

    override void OnHide()
    {
        StopTimer();
        if (s_Instance == this)
            s_Instance = null;

        super.OnHide();
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        super.OnClick(w, x, y, button);

        if (w == m_BtnYes)
        {
            SendJoin(true);
            CloseLocal();
            return true;
        }

        if (w == m_BtnNo)
        {
            SendJoin(false);
            CloseLocal();
            return true;
        }

        return false;
    }

    override bool OnKeyDown(Widget w, int x, int y, int key)
    {
        if (key == KeyCode.KC_ESCAPE)
        {
            CloseLocal();
            return true;
        }

        return super.OnKeyDown(w, x, y, key);
    }

    static NexoDM_JoinMenu GetInstance()
    {
        return s_Instance;
    }

    // Server-driven close (CLOSE_DIALOG RPC) for a join dialog orphaned by an admin stop
    // during VOTING. CloseLocal is idempotent via the m_Closed guard.
    void ForceClose()
    {
        CloseLocal();
    }

    void SetCountdown(int seconds)
    {
        if (seconds <= 0)
        {
            CloseLocal();
            return;
        }

        if (seconds > 60)
            seconds = 60;

        if (seconds < 1)
            seconds = 1;

        m_SecondsLeft = seconds;
        UpdateCountdownText();
        // F-18: the unreachable `m_SecondsLeft <= 0` block was removed (seconds is clamped >= 1 above).
        StartTimer();
    }

    void OnCountdownTick()
    {
        if (m_Closed)
            return;

        if (m_SecondsLeft > 0)
            m_SecondsLeft = m_SecondsLeft - 1;

        UpdateCountdownText();

        if (m_SecondsLeft <= 0)
        {
            StopTimer();
            CloseLocal();
        }
    }

    protected void InitText()
    {
        string kickerName = "KickerText";
        string kickerText = "GUNGAME";
        SetText(kickerName, kickerText);

        string titleName = "TitleText";
        string titleText = "#STR_NEXODM_JOIN_TITLE";
        SetText(titleName, titleText);

        string titleEnName = "TitleTextEN";
        string titleEnText = "Join Gungame?";
        SetText(titleEnName, titleEnText);

        string descName = "DescText";
        string descText = "#STR_NEXODM_JOIN_DESC";
        SetText(descName, descText);

        string yesName = "BtnYesLabel";
        string yesText = "#STR_NEXODM_YES";
        SetText(yesName, yesText);

        string yesEnName = "BtnYesLabelEN";
        string yesEnText = "YES";
        SetText(yesEnName, yesEnText);

        string noName = "BtnNoLabel";
        string noText = "#STR_NEXODM_NO";
        SetText(noName, noText);

        string noEnName = "BtnNoLabelEN";
        string noEnText = "NO";
        SetText(noEnName, noEnText);

        string hintName = "HintText";
        string hintText = "#STR_NEXODM_JOIN_HINT";
        SetText(hintName, hintText);

        string langName = "LangTag";
        string langText = "ES / EN";
        SetText(langName, langText);

        // Title/desc font size AND word-wrap now come from the layout (MultilineTextWidget +
        // explicit font, the vanilla pattern - e.g. day_z_connection_dialogue.layout). Do NOT pin
        // the size in code with SetTextExactSize: it overrides the layout font and defeats the
        // engine word-wrap, which left the ES title clipped to a single line (join popup 2026-06-11/13).
    }

    protected void SetText(string widgetName, string value)
    {
        if (!layoutRoot)
            return;

        TextWidget textWidget = TextWidget.Cast(layoutRoot.FindAnyWidget(widgetName));
        if (textWidget)
            textWidget.SetText(value);
    }

    protected void UpdateCountdownText()
    {
        if (m_CountdownText)
            m_CountdownText.SetText(m_SecondsLeft.ToString());
    }

    protected void StartTimer()
    {
        if (!m_Timer)
            m_Timer = new Timer();

        StopTimer();

        float tickSeconds = 1.0;
        string callbackName = "OnCountdownTick";
        // R2: Timer.Run(float,Managed,string,Param,bool) in scripts/3_game/tools/tools.c:595.
        m_Timer.Run(tickSeconds, this, callbackName, null, true);
    }

    protected void StopTimer()
    {
        if (!m_Timer)
            return;

        // R2: TimerBase.IsRunning() in scripts/3_game/tools/tools.c:264; Timer.Stop() in scripts/3_game/tools/tools.c:617.
        if (m_Timer.IsRunning())
            m_Timer.Stop();
    }

    protected void SendJoin(bool join)
    {
        DayZGame game = GetGame();
        if (!game)
            return;

        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        Param1<bool> payload = new Param1<bool>(join);
        // R2: Serializer.Write(void) in scripts/1_core/proto/serializer.c:57.
        rpc.Write(payload);
        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        rpc.Send(player, NexoDM_RPC.VOTE, true, null);
    }

    protected void CloseLocal()
    {
        if (m_Closed)
            return;

        m_Closed = true;
        StopTimer();
        Close();
    }
}
