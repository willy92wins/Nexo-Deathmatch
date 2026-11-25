class NexoDM_HUD : Managed
{
    protected static NexoDM_HUD s_Instance;

    protected Widget m_Root;
    protected Widget m_CountGroup;
    protected TextWidget m_CountLabel;
    protected TextWidget m_CountNumber;
    protected ImageWidget m_CountBar;
    protected TextWidget m_RoundTag;
    protected float m_TimeLeft;
    protected int m_LastShown;
    protected float m_CountTotal;
    protected float m_BarBaseW;
    protected float m_BarBaseH;
    protected bool m_CountdownActive;

    // F8b groups (all names must exist in nexodm_hud.layout).
    protected Widget m_KdaGroup;
    protected TextWidget m_KdaKills;
    protected TextWidget m_KdaDeaths;
    protected TextWidget m_KdaAssists;
    protected Widget m_ClockGroup;
    protected TextWidget m_ClockTime;
    protected TextWidget m_ClockGoal;
    protected ImageWidget m_ClockFillMe;
    protected ImageWidget m_ClockMarkLead;
    protected float m_ClockTrackW;
    protected float m_ClockTrackH;
    protected float m_ClockMarkY;
    protected Widget m_KillGroup;
    protected ref array<Widget> m_KillRows;
    protected ref array<TextWidget> m_KillKillers;
    protected ref array<ItemPreviewWidget> m_KillPreviews;
    protected ref array<ImageWidget> m_KillFallbacks;
    protected ref array<TextWidget> m_KillWeaponTexts;
    protected ref array<TextWidget> m_KillVictims;
    protected ref map<string, EntityAI> m_KillIconCache;
    protected ref array<string> m_KillIconOrder;
    protected ref array<string> m_KillIconFailed;
    protected Widget m_TopGroup;
    protected ref array<Widget> m_TopRows;
    protected ref array<ImageWidget> m_TopRowBgs;
    protected ref array<TextWidget> m_TopRanks;
    protected ref array<TextWidget> m_TopNames;
    protected ref array<TextWidget> m_TopKills;
    protected ref array<TextWidget> m_TopDeaths;
    protected ref array<TextWidget> m_TopAssists;
    protected bool m_Top5Held;
    protected Widget m_RoundGroup;
    protected TextWidget m_RoundTitle;
    protected TextWidget m_RoundSub;
    protected ref array<Widget> m_RoundRows;
    protected ref array<ImageWidget> m_RoundRowBgs;
    protected ref array<TextWidget> m_RoundRanks;
    protected ref array<TextWidget> m_RoundNames;
    protected ref array<TextWidget> m_RoundKills;
    protected ref array<TextWidget> m_RoundDeaths;
    protected ref array<TextWidget> m_RoundAssists;
    protected int m_RoundSerialSeen;
    protected float m_RoundHideAt;
    protected Widget m_GlobalGroup;
    protected ref array<Widget> m_GlobalRows;
    protected ref array<ImageWidget> m_GlobalRowBgs;
    protected ref array<TextWidget> m_GlobalRanks;
    protected ref array<TextWidget> m_GlobalNames;
    protected ref array<TextWidget> m_GlobalPoints;
    protected ref array<TextWidget> m_GlobalKills;
    protected int m_GlobalSerialSeen;
    protected float m_GlobalHideAt;
    protected bool m_GlobalPending;
    protected float m_HudTime;
    protected float m_RefreshAcc;
    protected int m_KillSeenCount;
    protected ref array<float> m_KillTimes;
    protected bool m_HudVisible;

    protected const int KILL_ROWS = 4;
    protected const int TOP_ROWS = 5;
    protected const int ROUND_ROWS = 26;
    protected const int GLOBAL_ROWS = 10;
    protected const int KILL_ICON_CACHE_MAX = 16;
    protected const float END_PANEL_SECONDS = 10.0;
    protected const float KILL_FEED_SECONDS = 6.0;

    void NexoDM_HUD()
    {
        m_TimeLeft = -1;
        m_LastShown = -1;
        m_CountTotal = 0;
        m_BarBaseW = 0;
        m_BarBaseH = 0;
        m_CountdownActive = false;
        m_ClockTrackW = 0;
        m_ClockTrackH = 0;
        m_ClockMarkY = 0.48;
        m_Top5Held = false;
        m_RoundSerialSeen = 0;
        m_RoundHideAt = 0;
        m_GlobalSerialSeen = 0;
        m_GlobalHideAt = 0;
        m_GlobalPending = false;
        m_HudTime = 0;
        m_RefreshAcc = 0;
        m_KillSeenCount = 0;
        m_HudVisible = false;
        m_KillRows = new array<Widget>;
        m_KillKillers = new array<TextWidget>;
        m_KillPreviews = new array<ItemPreviewWidget>;
        m_KillFallbacks = new array<ImageWidget>;
        m_KillWeaponTexts = new array<TextWidget>;
        m_KillVictims = new array<TextWidget>;
        m_KillIconCache = new map<string, EntityAI>;
        m_KillIconOrder = new array<string>;
        m_KillIconFailed = new array<string>;
        m_TopRows = new array<Widget>;
        m_TopRowBgs = new array<ImageWidget>;
        m_TopRanks = new array<TextWidget>;
        m_TopNames = new array<TextWidget>;
        m_TopKills = new array<TextWidget>;
        m_TopDeaths = new array<TextWidget>;
        m_TopAssists = new array<TextWidget>;
        m_RoundRows = new array<Widget>;
        m_RoundRowBgs = new array<ImageWidget>;
        m_RoundRanks = new array<TextWidget>;
        m_RoundNames = new array<TextWidget>;
        m_RoundKills = new array<TextWidget>;
        m_RoundDeaths = new array<TextWidget>;
        m_RoundAssists = new array<TextWidget>;
        m_GlobalRows = new array<Widget>;
        m_GlobalRowBgs = new array<ImageWidget>;
        m_GlobalRanks = new array<TextWidget>;
        m_GlobalNames = new array<TextWidget>;
        m_GlobalPoints = new array<TextWidget>;
        m_GlobalKills = new array<TextWidget>;
        m_KillTimes = new array<float>;

        DayZGame game = GetGame();
        if (!game)
            return;
        WorkspaceWidget workspace = game.GetWorkspace();
        if (!workspace)
            return;
        string layoutPath = "NexoDM/gui/layouts/nexodm_hud.layout";
        m_Root = workspace.CreateWidgets(layoutPath);
        if (!m_Root)
            return;
        CacheWidgets();
        HideCountdown();
        HideHudGroups();
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
        if (m_CountdownActive)
        {
            m_CountdownActive = false;
            SetVanillaHudVisible(true);
        }
        ClearKillPreviews();
        ClearKillIconCache();
        if (m_Root)
            m_Root.Unlink();
        m_Root = null;
        m_CountGroup = null;
        m_CountLabel = null;
        m_CountNumber = null;
        m_CountBar = null;
        m_RoundTag = null;
        m_KdaGroup = null;
        m_KdaKills = null;
        m_KdaDeaths = null;
        m_KdaAssists = null;
        m_ClockGroup = null;
        m_ClockTime = null;
        m_ClockGoal = null;
        m_ClockFillMe = null;
        m_ClockMarkLead = null;
        m_KillGroup = null;
        m_TopGroup = null;
        m_RoundGroup = null;
        m_RoundTitle = null;
        m_RoundSub = null;
        m_GlobalGroup = null;
        if (m_KillRows)
            m_KillRows.Clear();
        m_KillRows = null;
        if (m_KillKillers)
            m_KillKillers.Clear();
        m_KillKillers = null;
        if (m_KillPreviews)
            m_KillPreviews.Clear();
        m_KillPreviews = null;
        if (m_KillFallbacks)
            m_KillFallbacks.Clear();
        m_KillFallbacks = null;
        if (m_KillWeaponTexts)
            m_KillWeaponTexts.Clear();
        m_KillWeaponTexts = null;
        if (m_KillVictims)
            m_KillVictims.Clear();
        m_KillVictims = null;
        if (m_KillIconCache)
            m_KillIconCache.Clear();
        m_KillIconCache = null;
        if (m_KillIconOrder)
            m_KillIconOrder.Clear();
        m_KillIconOrder = null;
        if (m_KillIconFailed)
            m_KillIconFailed.Clear();
        m_KillIconFailed = null;
        if (m_TopRows)
            m_TopRows.Clear();
        m_TopRows = null;
        if (m_TopRowBgs)
            m_TopRowBgs.Clear();
        m_TopRowBgs = null;
        if (m_TopRanks)
            m_TopRanks.Clear();
        m_TopRanks = null;
        if (m_TopNames)
            m_TopNames.Clear();
        m_TopNames = null;
        if (m_TopKills)
            m_TopKills.Clear();
        m_TopKills = null;
        if (m_TopDeaths)
            m_TopDeaths.Clear();
        m_TopDeaths = null;
        if (m_TopAssists)
            m_TopAssists.Clear();
        m_TopAssists = null;
        if (m_RoundRows)
            m_RoundRows.Clear();
        m_RoundRows = null;
        if (m_RoundRowBgs)
            m_RoundRowBgs.Clear();
        m_RoundRowBgs = null;
        if (m_RoundRanks)
            m_RoundRanks.Clear();
        m_RoundRanks = null;
        if (m_RoundNames)
            m_RoundNames.Clear();
        m_RoundNames = null;
        if (m_RoundKills)
            m_RoundKills.Clear();
        m_RoundKills = null;
        if (m_RoundDeaths)
            m_RoundDeaths.Clear();
        m_RoundDeaths = null;
        if (m_RoundAssists)
            m_RoundAssists.Clear();
        m_RoundAssists = null;
        if (m_GlobalRows)
            m_GlobalRows.Clear();
        m_GlobalRows = null;
        if (m_GlobalRowBgs)
            m_GlobalRowBgs.Clear();
        m_GlobalRowBgs = null;
        if (m_GlobalRanks)
            m_GlobalRanks.Clear();
        m_GlobalRanks = null;
        if (m_GlobalNames)
            m_GlobalNames.Clear();
        m_GlobalNames = null;
        if (m_GlobalPoints)
            m_GlobalPoints.Clear();
        m_GlobalPoints = null;
        if (m_GlobalKills)
            m_GlobalKills.Clear();
        m_GlobalKills = null;
        if (m_KillTimes)
            m_KillTimes.Clear();
        m_KillTimes = null;
        m_Top5Held = false;
        m_RoundSerialSeen = 0;
        m_RoundHideAt = 0;
        m_GlobalSerialSeen = 0;
        m_GlobalHideAt = 0;
        m_GlobalPending = false;
        m_HudVisible = false;
        m_TimeLeft = -1;
        m_CountTotal = 0;
    }

    void Show(bool show)
    {
        if (!m_Root)
            return;
        m_Root.Show(show);
    }

    void SetTop5Visible(bool held)
    {
        m_Top5Held = held;
        if (!m_TopGroup)
            return;
        if (held && CanShowHud())
        {
            m_TopGroup.Show(true);
            UpdateTop5();
        }
        else
        {
            m_TopGroup.Show(false);
        }
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
        m_CountdownActive = true;
        SetVanillaHudVisible(false);
        if (m_CountGroup)
            m_CountGroup.Show(true);
        if (m_RoundTag)
            m_RoundTag.Show(true);
        if (m_CountBar)
            m_CountBar.SetSize(m_BarBaseW, m_BarBaseH);
        UpdateCountdownNumber(seconds);
    }

    void OnUpdate(float timeslice)
    {
        UpdateCountdown(timeslice);
        UpdateHud(timeslice);
        UpdateEndPanels();
    }

    protected void UpdateCountdown(float timeslice)
    {
        if (m_TimeLeft < 0)
            return;
        m_TimeLeft = m_TimeLeft - timeslice;
        if (m_TimeLeft < 0)
            m_TimeLeft = 0;
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
        m_CountGroup = m_Root.FindAnyWidget("CountGroup");
        m_CountNumber = TextWidget.Cast(m_Root.FindAnyWidget("CountNumber"));
        m_CountBar = ImageWidget.Cast(m_Root.FindAnyWidget("CountBar"));
        m_RoundTag = TextWidget.Cast(m_Root.FindAnyWidget("RoundTag"));
        m_CountLabel = TextWidget.Cast(m_Root.FindAnyWidget("CountLabel"));
        if (m_RoundTag)
            m_RoundTag.SetText(Widget.TranslateString("#STR_NEXODM_HUD_ROUND_TAG"));
        if (m_CountLabel)
            m_CountLabel.SetText(Widget.TranslateString("#STR_NEXODM_HUD_STARTING"));
        if (m_CountBar)
            m_CountBar.GetSize(m_BarBaseW, m_BarBaseH);
        m_KdaGroup = m_Root.FindAnyWidget("KdaGroup");
        m_KdaKills = TextWidget.Cast(m_Root.FindAnyWidget("KdaKills"));
        m_KdaDeaths = TextWidget.Cast(m_Root.FindAnyWidget("KdaDeaths"));
        m_KdaAssists = TextWidget.Cast(m_Root.FindAnyWidget("KdaAssists"));
        TextWidget kdaKillsLabel = TextWidget.Cast(m_Root.FindAnyWidget("KdaKillsLabel"));
        if (kdaKillsLabel)
            kdaKillsLabel.SetText(Widget.TranslateString("#STR_NEXODM_HUD_KILLS"));
        TextWidget kdaDeathsLabel = TextWidget.Cast(m_Root.FindAnyWidget("KdaDeathsLabel"));
        if (kdaDeathsLabel)
            kdaDeathsLabel.SetText(Widget.TranslateString("#STR_NEXODM_HUD_DEATHS"));
        TextWidget kdaAssistsLabel = TextWidget.Cast(m_Root.FindAnyWidget("KdaAssistsLabel"));
        if (kdaAssistsLabel)
            kdaAssistsLabel.SetText(Widget.TranslateString("#STR_NEXODM_HUD_ASSISTS"));
        m_ClockGroup = m_Root.FindAnyWidget("ClockGroup");
        m_ClockTime = TextWidget.Cast(m_Root.FindAnyWidget("ClockTime"));
        m_ClockGoal = TextWidget.Cast(m_Root.FindAnyWidget("ClockGoal"));
        m_ClockFillMe = ImageWidget.Cast(m_Root.FindAnyWidget("ClockFillMe"));
        m_ClockMarkLead = ImageWidget.Cast(m_Root.FindAnyWidget("ClockMarkLead"));
        Widget clockTrack = m_Root.FindAnyWidget("ClockTrack");
        if (clockTrack)
            clockTrack.GetSize(m_ClockTrackW, m_ClockTrackH);
        if (m_ClockMarkLead)
        {
            float markX = 0;
            float markY = 0;
            m_ClockMarkLead.GetPos(markX, markY);
            m_ClockMarkY = markY;
        }
        m_KillGroup = m_Root.FindAnyWidget("KillGroup");
        int killIndex = 0;
        for (killIndex = 0; killIndex < KILL_ROWS; killIndex = killIndex + 1)
        {
            string killSuffix = killIndex.ToString();
            m_KillRows.Insert(m_Root.FindAnyWidget("KillRow" + killSuffix));
            m_KillKillers.Insert(TextWidget.Cast(m_Root.FindAnyWidget("KillKiller" + killSuffix)));
            m_KillPreviews.Insert(ItemPreviewWidget.Cast(m_Root.FindAnyWidget("KillPreview" + killSuffix)));
            m_KillFallbacks.Insert(ImageWidget.Cast(m_Root.FindAnyWidget("KillFallback" + killSuffix)));
            m_KillWeaponTexts.Insert(TextWidget.Cast(m_Root.FindAnyWidget("KillWeaponText" + killSuffix)));
            m_KillVictims.Insert(TextWidget.Cast(m_Root.FindAnyWidget("KillVictim" + killSuffix)));
        }
        m_TopGroup = m_Root.FindAnyWidget("TopGroup");
        TextWidget topTitle = TextWidget.Cast(m_Root.FindAnyWidget("TopTitle"));
        if (topTitle)
            topTitle.SetText(Widget.TranslateString("#STR_NEXODM_HUD_TOP5"));
        TextWidget topKillsHead = TextWidget.Cast(m_Root.FindAnyWidget("TopKillsHead"));
        if (topKillsHead)
            topKillsHead.SetText(Widget.TranslateString("#STR_NEXODM_HUD_KILLS"));
        TextWidget topDeathsHead = TextWidget.Cast(m_Root.FindAnyWidget("TopDeathsHead"));
        if (topDeathsHead)
            topDeathsHead.SetText(Widget.TranslateString("#STR_NEXODM_HUD_DEATHS"));
        TextWidget topAssistsHead = TextWidget.Cast(m_Root.FindAnyWidget("TopAssistsHead"));
        if (topAssistsHead)
            topAssistsHead.SetText(Widget.TranslateString("#STR_NEXODM_HUD_ASSISTS"));
        int rowIndex = 0;
        for (rowIndex = 0; rowIndex < TOP_ROWS; rowIndex = rowIndex + 1)
        {
            string suffix = rowIndex.ToString();
            m_TopRows.Insert(m_Root.FindAnyWidget("TopRow" + suffix));
            m_TopRowBgs.Insert(ImageWidget.Cast(m_Root.FindAnyWidget("TopRowBg" + suffix)));
            m_TopRanks.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopRank" + suffix)));
            m_TopNames.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopName" + suffix)));
            m_TopKills.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopKills" + suffix)));
            m_TopDeaths.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopDeaths" + suffix)));
            m_TopAssists.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopAssists" + suffix)));
        }
        m_RoundGroup = m_Root.FindAnyWidget("RoundGroup");
        m_RoundTitle = TextWidget.Cast(m_Root.FindAnyWidget("RoundTitle"));
        m_RoundSub = TextWidget.Cast(m_Root.FindAnyWidget("RoundSub"));
        TextWidget roundHeadTitle = TextWidget.Cast(m_Root.FindAnyWidget("RoundHeadTitle"));
        if (roundHeadTitle)
            roundHeadTitle.SetText(Widget.TranslateString("#STR_NEXODM_SCOREBOARD_TITLE"));
        TextWidget roundKillsHead = TextWidget.Cast(m_Root.FindAnyWidget("RoundKillsHead"));
        if (roundKillsHead)
            roundKillsHead.SetText(Widget.TranslateString("#STR_NEXODM_HUD_KILLS"));
        TextWidget roundDeathsHead = TextWidget.Cast(m_Root.FindAnyWidget("RoundDeathsHead"));
        if (roundDeathsHead)
            roundDeathsHead.SetText(Widget.TranslateString("#STR_NEXODM_HUD_DEATHS"));
        TextWidget roundAssistsHead = TextWidget.Cast(m_Root.FindAnyWidget("RoundAssistsHead"));
        if (roundAssistsHead)
            roundAssistsHead.SetText(Widget.TranslateString("#STR_NEXODM_HUD_ASSISTS"));
        int roundIndex = 0;
        for (roundIndex = 0; roundIndex < ROUND_ROWS; roundIndex = roundIndex + 1)
        {
            string roundSuffix = roundIndex.ToString();
            m_RoundRows.Insert(m_Root.FindAnyWidget("RoundRow" + roundSuffix));
            m_RoundRowBgs.Insert(ImageWidget.Cast(m_Root.FindAnyWidget("RoundRowBg" + roundSuffix)));
            m_RoundRanks.Insert(TextWidget.Cast(m_Root.FindAnyWidget("RoundRank" + roundSuffix)));
            m_RoundNames.Insert(TextWidget.Cast(m_Root.FindAnyWidget("RoundName" + roundSuffix)));
            m_RoundKills.Insert(TextWidget.Cast(m_Root.FindAnyWidget("RoundKills" + roundSuffix)));
            m_RoundDeaths.Insert(TextWidget.Cast(m_Root.FindAnyWidget("RoundDeaths" + roundSuffix)));
            m_RoundAssists.Insert(TextWidget.Cast(m_Root.FindAnyWidget("RoundAssists" + roundSuffix)));
        }
        if (m_RoundGroup)
            m_RoundGroup.Show(false);
        m_GlobalGroup = m_Root.FindAnyWidget("GlobalTopGroup");
        TextWidget globalTitle = TextWidget.Cast(m_Root.FindAnyWidget("GlobalTitle"));
        if (globalTitle)
            globalTitle.SetText(Widget.TranslateString("#STR_NEXODM_HUD_GLOBAL_TOP"));
        TextWidget globalPointsHead = TextWidget.Cast(m_Root.FindAnyWidget("GlobalPointsHead"));
        if (globalPointsHead)
            globalPointsHead.SetText(Widget.TranslateString("#STR_NEXODM_HUD_POINTS"));
        TextWidget globalKillsHead = TextWidget.Cast(m_Root.FindAnyWidget("GlobalKillsHead"));
        if (globalKillsHead)
            globalKillsHead.SetText(Widget.TranslateString("#STR_NEXODM_HUD_KILLS"));
        int globalIndex = 0;
        for (globalIndex = 0; globalIndex < GLOBAL_ROWS; globalIndex = globalIndex + 1)
        {
            string globalSuffix = globalIndex.ToString();
            m_GlobalRows.Insert(m_Root.FindAnyWidget("GlobalRow" + globalSuffix));
            m_GlobalRowBgs.Insert(ImageWidget.Cast(m_Root.FindAnyWidget("GlobalRowBg" + globalSuffix)));
            m_GlobalRanks.Insert(TextWidget.Cast(m_Root.FindAnyWidget("GlobalRank" + globalSuffix)));
            m_GlobalNames.Insert(TextWidget.Cast(m_Root.FindAnyWidget("GlobalName" + globalSuffix)));
            m_GlobalPoints.Insert(TextWidget.Cast(m_Root.FindAnyWidget("GlobalPoints" + globalSuffix)));
            m_GlobalKills.Insert(TextWidget.Cast(m_Root.FindAnyWidget("GlobalKills" + globalSuffix)));
        }
        if (m_GlobalGroup)
            m_GlobalGroup.Show(false);
    }

    protected void HideHudGroups()
    {
        if (m_KdaGroup)
            m_KdaGroup.Show(false);
        if (m_ClockGroup)
            m_ClockGroup.Show(false);
        if (m_KillGroup)
            m_KillGroup.Show(false);
        if (m_TopGroup)
            m_TopGroup.Show(false);
        ClearKillPreviews();
        m_Top5Held = false;
        m_HudVisible = false;
    }

    protected bool CanShowHud()
    {
        DayZGame game = GetGame();
        if (!game)
            return false;
        if (game.IsDedicatedServer())
            return false;
        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return false;
        if (!data.IsRoundActive())
            return false;
        if (m_CountdownActive)
            return false;
        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (!player)
            return false;
        if (player.m_GG_Role != NexoDM_Role.PARTICIPANT)
            return false;
        if (!player.IsAlive())
            return false;
        if (player.IsUnconscious())
            return false;
        NexoDM_KillerCam cam = NexoDM_KillerCam.Get();
        if (cam && cam.IsActive())
            return false;
        if (cam && cam.IsSuppressingDeathScreen())
            return false;
        Mission mission = game.GetMission();
        if (mission && mission.IsPlayerRespawning())
            return false;
        UIManager ui = game.GetUIManager();
        if (ui && ui.GetMenu())
            return false;
        return true;
    }

    protected void UpdateHud(float timeslice)
    {
        m_HudTime = m_HudTime + timeslice;
        if (!CanShowHud())
        {
            if (m_HudVisible)
                HideHudGroups();
            return;
        }
        m_RefreshAcc = m_RefreshAcc + timeslice;
        if (m_RefreshAcc < 0.25)
            return;
        m_RefreshAcc = 0;
        m_HudVisible = true;
        if (m_KdaGroup)
            m_KdaGroup.Show(true);
        if (m_ClockGroup)
            m_ClockGroup.Show(true);
        UpdateKdaAndClock();
        UpdateKillfeed();
        UpdateTop5();
    }

    protected void UpdateKdaAndClock()
    {
        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        if (data.HasSelf())
        {
            if (m_KdaKills)
                m_KdaKills.SetText(data.GetKills().ToString());
            if (m_KdaDeaths)
                m_KdaDeaths.SetText(data.GetDeaths().ToString());
            if (m_KdaAssists)
                m_KdaAssists.SetText(data.GetAssists().ToString());
        }
        int secondsLeft = data.GetRoundSecondsLeft();
        int minutes = secondsLeft / 60;
        int seconds = secondsLeft - (minutes * 60);
        string secText = seconds.ToString();
        if (seconds < 10)
            secText = "0" + secText;
        string minText = minutes.ToString();
        if (minutes < 10)
            minText = "0" + minText;
        if (m_ClockTime)
            m_ClockTime.SetText(minText + ":" + secText);
        int killsToWin = data.GetKillsToWin();
        if (m_ClockGoal)
        {
            string goalText = "";
            if (killsToWin > 0)
            {
                string label = Widget.TranslateString("#STR_NEXODM_HUD_KILLS_LABEL");
                goalText = data.GetKills().ToString() + " / " + killsToWin.ToString() + " " + label;
            }
            m_ClockGoal.SetText(goalText);
        }
        if (m_ClockFillMe)
        {
            if (killsToWin > 0 && m_ClockTrackW > 0)
            {
                float fracMe = 0;
                if (data.HasSelf())
                    fracMe = (data.GetKills() * 1.0) / killsToWin;
                if (fracMe < 0)
                    fracMe = 0;
                if (fracMe > 1)
                    fracMe = 1;
                m_ClockFillMe.SetSize(m_ClockTrackW * fracMe, m_ClockTrackH);
                m_ClockFillMe.Show(true);
            }
            else
            {
                m_ClockFillMe.Show(false);
            }
        }
        if (m_ClockMarkLead)
        {
            if (killsToWin > 0 && m_ClockTrackW > 0)
            {
                int leadKills = 0;
                if (data.GetTopCount() > 0)
                    leadKills = data.GetTopKills(0);
                float fracLead = (leadKills * 1.0) / killsToWin;
                if (fracLead < 0)
                    fracLead = 0;
                if (fracLead > 1)
                    fracLead = 1;
                m_ClockMarkLead.SetPos(m_ClockTrackW * fracLead, m_ClockMarkY, true);
                m_ClockMarkLead.Show(true);
            }
            else
            {
                m_ClockMarkLead.Show(false);
            }
        }
    }

    protected void ClearKillPreviews()
    {
        if (!m_KillPreviews)
            return;
        int index = 0;
        for (index = 0; index < m_KillPreviews.Count(); index = index + 1)
        {
            ItemPreviewWidget preview = m_KillPreviews.Get(index);
            if (preview)
                preview.SetItem(null);
        }
    }

    protected void ClearKillIconCache()
    {
        DayZGame game = GetGame();
        if (m_KillIconOrder && m_KillIconCache)
        {
            int index = 0;
            for (index = 0; index < m_KillIconOrder.Count(); index = index + 1)
            {
                string className = m_KillIconOrder.Get(index);
                EntityAI entity = null;
                if (m_KillIconCache.Contains(className))
                    entity = m_KillIconCache.Get(className);
                if (entity && game)
                    game.ObjectDelete(entity);
            }
        }
        if (m_KillIconCache)
            m_KillIconCache.Clear();
        if (m_KillIconOrder)
            m_KillIconOrder.Clear();
        if (m_KillIconFailed)
            m_KillIconFailed.Clear();
    }

    protected void EvictKillIcons()
    {
        DayZGame game = GetGame();
        while (m_KillIconOrder && m_KillIconOrder.Count() > KILL_ICON_CACHE_MAX)
        {
            string oldest = m_KillIconOrder.Get(0);
            m_KillIconOrder.Remove(0);
            EntityAI entity = null;
            if (m_KillIconCache && m_KillIconCache.Contains(oldest))
                entity = m_KillIconCache.Get(oldest);
            if (m_KillIconCache)
                m_KillIconCache.Remove(oldest);
            if (!entity)
                continue;
            if (m_KillPreviews)
            {
                int index = 0;
                for (index = 0; index < m_KillPreviews.Count(); index = index + 1)
                {
                    ItemPreviewWidget preview = m_KillPreviews.Get(index);
                    if (preview && preview.GetItem() == entity)
                        preview.SetItem(null);
                }
            }
            if (game)
                game.ObjectDelete(entity);
        }
    }

    // Local preview entity per weapon classname (F8-weapon-icon.md). Only Weapon_Base
    // classes spawn; anything else returns null and the caller uses the fallback.
    protected EntityAI GetKillIcon(string weaponClass)
    {
        if (weaponClass == string.Empty)
            return null;
        if (!m_KillIconCache || !m_KillIconOrder || !m_KillIconFailed)
            return null;
        if (m_KillIconFailed.Find(weaponClass) >= 0)
            return null;
        if (m_KillIconCache.Contains(weaponClass))
        {
            EntityAI cached = m_KillIconCache.Get(weaponClass);
            if (cached)
            {
                int orderIndex = m_KillIconOrder.Find(weaponClass);
                if (orderIndex >= 0)
                    m_KillIconOrder.Remove(orderIndex);
                m_KillIconOrder.Insert(weaponClass);
                return cached;
            }
            m_KillIconCache.Remove(weaponClass);
        }
        DayZGame game = GetGame();
        if (!game)
            return null;
        if (!game.IsKindOf(weaponClass, "Weapon_Base"))
            return null;
        if (!game.ConfigIsExisting("CfgWeapons " + weaponClass))
        {
            m_KillIconFailed.Insert(weaponClass);
            return null;
        }
        EntityAI created = EntityAI.Cast(game.CreateObjectEx(weaponClass, "0 0 0", ECE_LOCAL | ECE_CREATEPHYSICS | ECE_TRACE));
        if (!created)
        {
            m_KillIconFailed.Insert(weaponClass);
            return null;
        }
        dBodyDestroy(created);
        created.DisableSimulation(true);
        created.SetAllowDamage(false);
        m_KillIconCache.Set(weaponClass, created);
        m_KillIconOrder.Insert(weaponClass);
        EvictKillIcons();
        return created;
    }

    protected string GetKillWeaponName(string weaponClass)
    {
        DayZGame game = GetGame();
        if (!game)
            return weaponClass;
        string displayName = "";
        if (game.ConfigGetText("CfgWeapons " + weaponClass + " displayName", displayName) && displayName != string.Empty)
            return displayName;
        displayName = "";
        if (game.ConfigGetText("CfgVehicles " + weaponClass + " displayName", displayName) && displayName != string.Empty)
            return displayName;
        displayName = "";
        if (game.ConfigGetText("CfgMagazines " + weaponClass + " displayName", displayName) && displayName != string.Empty)
            return displayName;
        return weaponClass;
    }

    protected void UpdateKillfeed()
    {
        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        int total = data.GetKillFeedCount();
        if (total < m_KillSeenCount)
        {
            m_KillSeenCount = 0;
            m_KillTimes.Clear();
        }
        int added = total - m_KillSeenCount;
        int addIndex = 0;
        for (addIndex = 0; addIndex < added; addIndex = addIndex + 1)
            m_KillTimes.Insert(m_HudTime);
        m_KillSeenCount = total;
        if (m_KillGroup)
        {
            if (total == 0)
                m_KillGroup.Show(false);
            else
                m_KillGroup.Show(true);
        }
        DayZGame game = GetGame();
        int slot = 0;
        for (slot = 0; slot < KILL_ROWS; slot = slot + 1)
        {
            Widget row = null;
            if (slot < m_KillRows.Count())
                row = m_KillRows.Get(slot);
            ItemPreviewWidget preview = null;
            if (slot < m_KillPreviews.Count())
                preview = m_KillPreviews.Get(slot);
            ImageWidget fallback = null;
            if (slot < m_KillFallbacks.Count())
                fallback = m_KillFallbacks.Get(slot);
            TextWidget weaponText = null;
            if (slot < m_KillWeaponTexts.Count())
                weaponText = m_KillWeaponTexts.Get(slot);
            int feedIndex = total - 1 - slot;
            bool showRow = true;
            if (feedIndex < 0)
                showRow = false;
            float born = 0;
            if (showRow && feedIndex < m_KillTimes.Count())
                born = m_KillTimes.Get(feedIndex);
            if (showRow && m_HudTime - born > KILL_FEED_SECONDS)
                showRow = false;
            if (!showRow)
            {
                if (row)
                    row.Show(false);
                if (preview)
                    preview.SetItem(null);
                continue;
            }
            if (row)
                row.Show(true);
            TextWidget killerWidget = null;
            if (slot < m_KillKillers.Count())
                killerWidget = m_KillKillers.Get(slot);
            if (killerWidget)
                killerWidget.SetText(data.GetKillKiller(feedIndex));
            TextWidget victimWidget = null;
            if (slot < m_KillVictims.Count())
                victimWidget = m_KillVictims.Get(slot);
            if (victimWidget)
                victimWidget.SetText(data.GetKillVictim(feedIndex));
            string weaponClass = data.GetKillWeapon(feedIndex);
            if (weaponClass == string.Empty)
            {
                if (preview)
                {
                    preview.SetItem(null);
                    preview.Show(false);
                }
                if (fallback)
                    fallback.Show(false);
                if (weaponText)
                    weaponText.Show(false);
                continue;
            }
            EntityAI icon = GetKillIcon(weaponClass);
            if (icon)
            {
                if (preview)
                {
                    if (preview.GetItem() != icon)
                    {
                        preview.SetItem(icon);
                        preview.SetModelOrientation("0 0 0");
                        preview.SetView(icon.GetViewIndex());
                    }
                    preview.Show(true);
                }
                if (fallback)
                    fallback.Show(false);
                if (weaponText)
                    weaponText.Show(false);
                continue;
            }
            if (preview)
            {
                preview.SetItem(null);
                preview.Show(false);
            }
            bool isWeapon = false;
            if (game && game.IsKindOf(weaponClass, "Weapon_Base"))
                isWeapon = true;
            if (isWeapon)
            {
                if (fallback)
                {
                    string category = "shoulderright";
                    if (game.IsKindOf(weaponClass, "Pistol_Base"))
                        category = "pistol";
                    fallback.LoadImageFile(0, "set:dayz_inventory image:" + category);
                    fallback.Show(true);
                }
                if (weaponText)
                    weaponText.Show(false);
            }
            else
            {
                if (fallback)
                    fallback.Show(false);
                if (weaponText)
                {
                    weaponText.SetText(GetKillWeaponName(weaponClass));
                    weaponText.Show(true);
                }
            }
        }
    }

    protected void UpdateTop5()
    {
        if (m_TopGroup)
        {
            if (m_Top5Held)
                m_TopGroup.Show(true);
            else
                m_TopGroup.Show(false);
        }
        if (!m_Top5Held)
            return;
        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        string myName = "";
        DayZGame game = GetGame();
        if (game)
        {
            PlayerBase player = PlayerBase.Cast(game.GetPlayer());
            if (player && player.GetIdentity())
                myName = player.GetIdentity().GetName();
        }
        int rowIndex = 0;
        for (rowIndex = 0; rowIndex < TOP_ROWS; rowIndex = rowIndex + 1)
        {
            Widget row = null;
            if (rowIndex < m_TopRows.Count())
                row = m_TopRows.Get(rowIndex);
            if (rowIndex >= data.GetTopCount())
            {
                if (row)
                    row.Show(false);
                continue;
            }
            if (row)
                row.Show(true);
            string name = data.GetTopName(rowIndex);
            bool isMe = false;
            if (myName != string.Empty && name == myName)
                isMe = true;
            if (data.HasSelf() && data.GetRank() == rowIndex + 1)
            {
                if (myName == string.Empty)
                    isMe = true;
            }
            ImageWidget bg = null;
            if (rowIndex < m_TopRowBgs.Count())
                bg = m_TopRowBgs.Get(rowIndex);
            if (bg)
            {
                if (isMe)
                    bg.SetColor(ARGB(255, 0, 0, 0));
                else
                    bg.SetColor(ARGB(0, 0, 0, 0));
            }
            int textColor = ARGB(255, 255, 255, 255);
            int dimColor = ARGB(200, 255, 255, 255);
            if (isMe)
            {
                textColor = ARGB(255, 255, 0, 0);
                dimColor = ARGB(255, 255, 0, 0);
            }
            TextWidget rankWidget = null;
            if (rowIndex < m_TopRanks.Count())
                rankWidget = m_TopRanks.Get(rowIndex);
            if (rankWidget)
            {
                rankWidget.SetText((rowIndex + 1).ToString());
                rankWidget.SetColor(dimColor);
            }
            TextWidget nameWidget = null;
            if (rowIndex < m_TopNames.Count())
                nameWidget = m_TopNames.Get(rowIndex);
            if (nameWidget)
            {
                nameWidget.SetText(name);
                nameWidget.SetColor(textColor);
            }
            TextWidget killsWidget = null;
            if (rowIndex < m_TopKills.Count())
                killsWidget = m_TopKills.Get(rowIndex);
            if (killsWidget)
            {
                killsWidget.SetText(data.GetTopKills(rowIndex).ToString());
                killsWidget.SetColor(textColor);
            }
            TextWidget deathsWidget = null;
            if (rowIndex < m_TopDeaths.Count())
                deathsWidget = m_TopDeaths.Get(rowIndex);
            if (deathsWidget)
            {
                deathsWidget.SetText(data.GetTopDeaths(rowIndex).ToString());
                deathsWidget.SetColor(textColor);
            }
            TextWidget assistsWidget = null;
            if (rowIndex < m_TopAssists.Count())
                assistsWidget = m_TopAssists.Get(rowIndex);
            if (assistsWidget)
            {
                assistsWidget.SetText(data.GetTopAssists(rowIndex).ToString());
                assistsWidget.SetColor(textColor);
            }
        }
    }

    // End of round, in order: round classification (10 s), then global top 10 (10 s).
    // The global window only starts when the panel first shows, so its 10 s are whole.
    protected void UpdateEndPanels()
    {
        if (!m_RoundGroup || !m_GlobalGroup)
            return;
        NexoDM_HudData data = NexoDM_HudData.Get();
        bool blocked = false;
        if (m_CountdownActive)
            blocked = true;
        DayZGame game = GetGame();
        if (game)
        {
            UIManager ui = game.GetUIManager();
            if (ui && ui.GetMenu())
                blocked = true;
        }
        int roundSerial = 0;
        if (data)
            roundSerial = data.GetRoundResultSerial();
        if (roundSerial != m_RoundSerialSeen)
        {
            m_RoundSerialSeen = roundSerial;
            if (roundSerial > 0)
                m_RoundHideAt = m_HudTime + END_PANEL_SECONDS;
        }
        if (data && data.IsRoundActive())
            m_RoundHideAt = m_HudTime;
        bool showRound = false;
        if (roundSerial > 0 && m_HudTime < m_RoundHideAt && !blocked)
            showRound = true;
        m_RoundGroup.Show(showRound);
        if (showRound && data)
            FillRoundPanel(data);
        int globalSerial = 0;
        if (data)
            globalSerial = data.GetGlobalSerial();
        if (globalSerial != m_GlobalSerialSeen)
        {
            m_GlobalSerialSeen = globalSerial;
            if (globalSerial > 0)
            {
                m_GlobalPending = true;
                m_GlobalHideAt = 0;
            }
        }
        if (data && data.IsRoundActive())
        {
            m_GlobalHideAt = m_HudTime;
            m_GlobalPending = false;
        }
        bool showGlobal = false;
        if (m_GlobalPending && !showRound && !blocked)
        {
            if (m_GlobalHideAt <= 0)
                m_GlobalHideAt = m_HudTime + NexoDM_Constants.GLOBAL_TOP_SHOW_SEC;
            if (m_HudTime < m_GlobalHideAt)
                showGlobal = true;
            else
            {
                m_GlobalPending = false;
                m_GlobalHideAt = 0;
            }
        }
        m_GlobalGroup.Show(showGlobal);
        if (showGlobal && data)
            FillGlobalPanel(data);
    }

    protected void FillRoundPanel(NexoDM_HudData data)
    {
        if (m_RoundTitle)
        {
            string winner = data.GetRoundResultWinner();
            string titleText = "";
            if (winner != string.Empty)
                titleText = Widget.TranslateString("#STR_NEXODM_HUD_END_TITLE") + " " + winner;
            else
                titleText = Widget.TranslateString("#STR_NEXODM_HUD_END_NO_WINNER");
            m_RoundTitle.SetText(titleText);
        }
        if (m_RoundSub)
        {
            string mapName = data.GetRoundResultMap();
            int killsToWin = data.GetRoundResultKillsToWin();
            string subText = "";
            if (mapName != string.Empty && killsToWin > 0)
                subText = mapName + " · " + killsToWin.ToString() + " " + Widget.TranslateString("#STR_NEXODM_HUD_KILLS_LABEL");
            else if (mapName != string.Empty)
                subText = mapName;
            else if (killsToWin > 0)
                subText = killsToWin.ToString() + " " + Widget.TranslateString("#STR_NEXODM_HUD_KILLS_LABEL");
            m_RoundSub.SetText(subText);
        }
        int count = data.GetRoundResultCount();
        if (count > ROUND_ROWS)
            count = ROUND_ROWS;
        int rowIndex = 0;
        for (rowIndex = 0; rowIndex < ROUND_ROWS; rowIndex = rowIndex + 1)
        {
            Widget row = null;
            if (rowIndex < m_RoundRows.Count())
                row = m_RoundRows.Get(rowIndex);
            if (rowIndex >= count)
            {
                if (row)
                    row.Show(false);
                continue;
            }
            if (row)
                row.Show(true);
            bool isMe = false;
            if (data.GetRoundResultIsReceiver(rowIndex) != 0)
                isMe = true;
            ImageWidget bg = null;
            if (rowIndex < m_RoundRowBgs.Count())
                bg = m_RoundRowBgs.Get(rowIndex);
            if (bg)
            {
                if (isMe)
                    bg.SetColor(ARGB(255, 0, 0, 0));
                else
                    bg.SetColor(ARGB(0, 0, 0, 0));
            }
            int textColor = ARGB(255, 255, 255, 255);
            int dimColor = ARGB(200, 255, 255, 255);
            if (isMe)
            {
                textColor = ARGB(255, 255, 0, 0);
                dimColor = ARGB(255, 255, 0, 0);
            }
            TextWidget rankWidget = null;
            if (rowIndex < m_RoundRanks.Count())
                rankWidget = m_RoundRanks.Get(rowIndex);
            if (rankWidget)
            {
                rankWidget.SetText((rowIndex + 1).ToString());
                rankWidget.SetColor(dimColor);
            }
            TextWidget nameWidget = null;
            if (rowIndex < m_RoundNames.Count())
                nameWidget = m_RoundNames.Get(rowIndex);
            if (nameWidget)
            {
                nameWidget.SetText(data.GetRoundResultName(rowIndex));
                nameWidget.SetColor(textColor);
            }
            TextWidget killsWidget = null;
            if (rowIndex < m_RoundKills.Count())
                killsWidget = m_RoundKills.Get(rowIndex);
            if (killsWidget)
            {
                killsWidget.SetText(data.GetRoundResultKills(rowIndex).ToString());
                killsWidget.SetColor(textColor);
            }
            TextWidget deathsWidget = null;
            if (rowIndex < m_RoundDeaths.Count())
                deathsWidget = m_RoundDeaths.Get(rowIndex);
            if (deathsWidget)
            {
                deathsWidget.SetText(data.GetRoundResultDeaths(rowIndex).ToString());
                deathsWidget.SetColor(textColor);
            }
            TextWidget assistsWidget = null;
            if (rowIndex < m_RoundAssists.Count())
                assistsWidget = m_RoundAssists.Get(rowIndex);
            if (assistsWidget)
            {
                assistsWidget.SetText(data.GetRoundResultAssists(rowIndex).ToString());
                assistsWidget.SetColor(textColor);
            }
        }
    }

    protected void FillGlobalPanel(NexoDM_HudData data)
    {
        int rowIndex = 0;
        for (rowIndex = 0; rowIndex < GLOBAL_ROWS; rowIndex = rowIndex + 1)
        {
            Widget row = null;
            if (rowIndex < m_GlobalRows.Count())
                row = m_GlobalRows.Get(rowIndex);
            if (rowIndex >= data.GetGlobalCount())
            {
                if (row)
                    row.Show(false);
                continue;
            }
            if (row)
                row.Show(true);
            bool isMe = false;
            if (data.GetGlobalIsReceiver(rowIndex) != 0)
                isMe = true;
            ImageWidget bg = null;
            if (rowIndex < m_GlobalRowBgs.Count())
                bg = m_GlobalRowBgs.Get(rowIndex);
            if (bg)
            {
                if (isMe)
                    bg.SetColor(ARGB(255, 0, 0, 0));
                else
                    bg.SetColor(ARGB(0, 0, 0, 0));
            }
            int textColor = ARGB(255, 255, 255, 255);
            int dimColor = ARGB(200, 255, 255, 255);
            if (isMe)
            {
                textColor = ARGB(255, 255, 0, 0);
                dimColor = ARGB(255, 255, 0, 0);
            }
            TextWidget rankWidget = null;
            if (rowIndex < m_GlobalRanks.Count())
                rankWidget = m_GlobalRanks.Get(rowIndex);
            if (rankWidget)
            {
                rankWidget.SetText((rowIndex + 1).ToString());
                rankWidget.SetColor(dimColor);
            }
            TextWidget nameWidget = null;
            if (rowIndex < m_GlobalNames.Count())
                nameWidget = m_GlobalNames.Get(rowIndex);
            if (nameWidget)
            {
                nameWidget.SetText(data.GetGlobalName(rowIndex));
                nameWidget.SetColor(textColor);
            }
            TextWidget pointsWidget = null;
            if (rowIndex < m_GlobalPoints.Count())
                pointsWidget = m_GlobalPoints.Get(rowIndex);
            if (pointsWidget)
            {
                pointsWidget.SetText(data.GetGlobalPoints(rowIndex).ToString());
                pointsWidget.SetColor(textColor);
            }
            TextWidget killsWidget = null;
            if (rowIndex < m_GlobalKills.Count())
                killsWidget = m_GlobalKills.Get(rowIndex);
            if (killsWidget)
            {
                killsWidget.SetText(data.GetGlobalKills(rowIndex).ToString());
                killsWidget.SetColor(textColor);
            }
        }
    }

    protected void UpdateCountdownNumber(int secs)
    {
        m_LastShown = secs;
        if (!m_CountNumber)
            return;
        string text = secs.ToString();
        m_CountNumber.SetText(text);
        if (secs <= 3)
            m_CountNumber.SetColor(NexoDM_UIConst.COL_DANGER);
        else
            m_CountNumber.SetColor(NexoDM_UIConst.COL_TEXT);
    }

    protected void HideCountdown()
    {
        if (m_CountGroup)
            m_CountGroup.Show(false);
        if (m_RoundTag)
            m_RoundTag.Show(false);
        if (m_CountdownActive)
        {
            m_CountdownActive = false;
            SetVanillaHudVisible(true);
        }
    }

    protected void SetVanillaHudVisible(bool show)
    {
        DayZGame game = GetGame();
        if (!game)
            return;
        Mission mission = game.GetMission();
        if (!mission)
            return;
        Hud hud = mission.GetHud();
        if (!hud)
            return;
        hud.ShowQuickbarPlayer(show);
        hud.ShowHudPlayer(show);
    }
}
