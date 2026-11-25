// Scoreboard (G3-G5): toggled with the UALFGungameScoreboard input (default L, rebindable).
// Participants only. Data is server-authoritative: the menu asks via SCOREBOARD_REQ and the
// server answers with SCOREBOARD_DATA (parsed in LFGungame_PlayerBase.OnRPC, which calls
// SetData), refreshed every 1.5s while the board is open.

class LFGungame_ScoreRow : Managed
{
    string name;
    int tier;
    int tierMax;
    int kills;
    int assists;
    int deaths;
}

class LFGungame_Scoreboard extends UIScriptedMenu
{
    protected static ref LFGungame_Scoreboard s_Instance;

    protected Widget m_RowsContainer;
    protected ref array<Widget> m_Rows;
    protected ref Timer m_RefreshTimer;

    void LFGungame_Scoreboard()
    {
        m_Rows = new array<Widget>;
        m_RefreshTimer = new Timer();
    }

    void ~LFGungame_Scoreboard()
    {
        StopRefresh();
        if (s_Instance == this)
            s_Instance = null;
    }

    static LFGungame_Scoreboard GetInstance()
    {
        return s_Instance;
    }

    // External close (scoreboard toggle key) — wraps the inherited UIScriptedMenu.Close().
    void CloseBoard()
    {
        Close();
    }

    // Passive overlay [verify in-game]: don't capture mouse/keyboard so the player keeps moving and
    // the rebindable toggle key (handled in MissionGameplay) closes it. A captured menu would freeze
    // the player while the board is up and eat the toggle key. R2: UIScriptedMenu.UseMouse/UseKeyboard
    // in scripts/3_game/tools/uiscriptedmenu.c:33/41 (overridden in vanilla sceneeditormenu.c:345/353).
    override bool UseMouse()
    {
        return false;
    }

    override bool UseKeyboard()
    {
        return false;
    }

    override Widget Init()
    {
        DayZGame game = GetGame();
        if (!game)
            return null;

        // R2: DayZGame.GetWorkspace() in scripts/3_game/global/game.c:84.
        WorkspaceWidget workspace = game.GetWorkspace();
        if (!workspace)
            return null;

        string layoutPath = "LFGungame/gui/layouts/lfgungame_scoreboard.layout";
        // R2: WorkspaceWidget.CreateWidgets(string,Widget,bool) in scripts/1_core/proto/enwidgets.c:181.
        layoutRoot = workspace.CreateWidgets(layoutPath);
        if (!layoutRoot)
            return null;

        s_Instance = this;

        string rowsContainerName = "RowsContainer";
        // R2: Widget.FindAnyWidget(string) in scripts/1_core/proto/enwidgets.c:169.
        m_RowsContainer = layoutRoot.FindAnyWidget(rowsContainerName);

        RequestData();
        StartRefresh();
        return layoutRoot;
    }

    override void OnHide()
    {
        StopRefresh();
        if (s_Instance == this)
            s_Instance = null;

        super.OnHide();
    }

    override bool OnKeyDown(Widget w, int x, int y, int key)
    {
        if (key == KeyCode.KC_ESCAPE)
        {
            Close();
            return true;
        }

        return super.OnKeyDown(w, x, y, key);
    }

    // Called from LFGungame_PlayerBase.OnRPC on the client when SCOREBOARD_DATA arrives.
    void SetData(array<ref LFGungame_ScoreRow> rows)
    {
        if (!m_RowsContainer)
            return;

        ClearRows();

        if (!rows)
            return;

        string rowLayout = "LFGungame/gui/layouts/lfgungame_scoreboard_row.layout";
        string rankName = "RankText";
        string nameName = "NameText";
        string tierName = "TierText";
        string kName = "KText";
        string aName = "AText";
        string dName = "DText";
        string barName = "TierBar";
        string crownName = "CrownIcon";

        // Leader rows = rows sharing the top tier. Rows arrive ranked best-first by the server
        // tiebreak (tier>kills>assists>deaths, OnScoreboardRequest), so row 0 carries the max tier.
        int leaderTier = -1;
        if (rows.Count() > 0)
        {
            LFGungame_ScoreRow first = rows.Get(0);
            if (first)
                leaderTier = first.tier;
        }

        int index = 0;
        for (index = 0; index < rows.Count(); index = index + 1)
        {
            LFGungame_ScoreRow data = rows.Get(index);
            if (!data)
                continue;

            Widget row = GetGame().GetWorkspace().CreateWidgets(rowLayout, m_RowsContainer);
            if (!row)
                continue;

            int rank = index + 1;
            string rankStr = rank.ToString();
            SetRowText(row, rankName, rankStr);
            SetRowText(row, nameName, data.name);

            int shown = data.tier + 1;
            int total = data.tierMax;
            if (total < 1)
                total = 1;
            if (shown < 1)
                shown = 1;
            if (shown > total)
                shown = total;

            string tierLabel = "Arma ";
            tierLabel = tierLabel + shown.ToString();
            tierLabel = tierLabel + "/";
            tierLabel = tierLabel + total.ToString();
            SetRowText(row, tierName, tierLabel);

            string kStr = data.kills.ToString();
            SetRowText(row, kName, kStr);
            string aStr = data.assists.ToString();
            SetRowText(row, aName, aStr);
            string dStr = data.deaths.ToString();
            SetRowText(row, dName, dStr);

            // Leader marker: gold-tint rank + name while crown.edds is pending (the CrownIcon
            // image stays hidden below - an untextured ImageWidget renders a solid block).
            if (leaderTier >= 0 && data.tier == leaderTier)
            {
                SetRowColor(row, rankName, LFGungame_UIConst.COL_GOLD);
                SetRowColor(row, nameName, LFGungame_UIConst.COL_GOLD);
            }

            // Progress bar + crown image are kit polish, not wired here; keep them hidden.
            Widget bar = row.FindAnyWidget(barName);
            if (bar)
                bar.Show(false);
            Widget crown = row.FindAnyWidget(crownName);
            if (crown)
                crown.Show(false);

            m_Rows.Insert(row);
        }
    }

    protected void ClearRows()
    {
        if (!m_Rows)
        {
            m_Rows = new array<Widget>;
            return;
        }

        int index = 0;
        for (index = 0; index < m_Rows.Count(); index = index + 1)
        {
            Widget old = m_Rows.Get(index);
            if (old)
            {
                // R2: Widget.Unlink() in scripts/1_core/proto/enwidgets.c:173.
                old.Unlink();
            }
        }
        m_Rows.Clear();
    }

    protected void SetRowText(Widget root, string widgetName, string value)
    {
        if (!root)
            return;

        // R2: Class.Cast in scripts/1_core/proto/enscript.c:94; TextWidget.SetText in enwidgets.c:195.
        TextWidget t = TextWidget.Cast(root.FindAnyWidget(widgetName));
        if (t)
            t.SetText(value);
    }

    protected void SetRowColor(Widget root, string widgetName, int color)
    {
        if (!root)
            return;

        // R2: Widget.SetColor(int) in scripts/1_core/proto/enwidgets.c:144.
        Widget t = root.FindAnyWidget(widgetName);
        if (t)
            t.SetColor(color);
    }

    void RequestData()
    {
        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        if (game.IsDedicatedServer())
            return;

        // R2: CGame.GetPlayer() used for the local player in JoinMenu.c:243.
        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (!player)
            return;

        // F-22: stop pinging once the local player is no longer a participant (round ended / became
        // public). The server already rejects the request; this also closes the now-stale board and
        // stops the 1.5s refresh instead of spinning RPCs forever. m_GG_Role is synced from the server.
        if (player.m_GG_Role != LFGungame_Role.PARTICIPANT)
        {
            StopRefresh();
            Close();
            return;
        }

        ScriptRPC rpc = new ScriptRPC();
        // R2: ScriptRPC.Send(Object,int,bool,PlayerIdentity) in scripts/3_game/gameplay.c:117.
        rpc.Send(player, LFGungame_RPC.SCOREBOARD_REQ, true, null);
    }

    protected void StartRefresh()
    {
        if (!m_RefreshTimer)
            m_RefreshTimer = new Timer();

        StopRefresh();

        float refreshSeconds = 1.5;
        string callbackName = "RequestData";
        // R2: Timer.Run(float,Managed,string,Param,bool) in scripts/3_game/tools/tools.c:595.
        m_RefreshTimer.Run(refreshSeconds, this, callbackName, null, true);
    }

    protected void StopRefresh()
    {
        if (!m_RefreshTimer)
            return;

        // R2: TimerBase.IsRunning() in scripts/3_game/tools/tools.c:264; Timer.Stop() in scripts/3_game/tools/tools.c:617.
        if (m_RefreshTimer.IsRunning())
            m_RefreshTimer.Stop();
    }
}
