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

    // F5b groups (all names must exist in nexodm_hud.layout).
    protected Widget m_SelfGroup;
    protected TextWidget m_SelfRank;
    protected TextWidget m_SelfPoints;
    protected TextWidget m_SelfKD;
    protected Widget m_ClockGroup;
    protected TextWidget m_ClockTime;
    protected TextWidget m_ClockGoal;
    protected ImageWidget m_ClockFillMe;
    protected ImageWidget m_ClockMarkLead;
    protected float m_ClockTrackW;
    protected float m_ClockTrackH;
    protected Widget m_KillGroup;
    protected ref array<TextWidget> m_KillLines;
    protected Widget m_TopGroup;
    protected ref array<Widget> m_TopRows;
    protected ref array<ImageWidget> m_TopRowBgs;
    protected ref array<TextWidget> m_TopRanks;
    protected ref array<TextWidget> m_TopNames;
    protected ref array<TextWidget> m_TopKills;
    protected ref array<TextWidget> m_TopDeaths;
    protected ref array<TextWidget> m_TopPoints;
    protected bool m_Top5Held;
    protected Widget m_GlobalGroup;
    protected ref array<Widget> m_GlobalRows;
    protected ref array<ImageWidget> m_GlobalRowBgs;
    protected ref array<TextWidget> m_GlobalRanks;
    protected ref array<TextWidget> m_GlobalNames;
    protected ref array<TextWidget> m_GlobalPoints;
    protected ref array<TextWidget> m_GlobalKills;
    protected int m_GlobalSerialSeen;
    protected float m_GlobalHideAt;
    protected Widget m_WeaponGroup;
    protected ItemPreviewWidget m_WeaponPreview;
    protected TextWidget m_WeaponName;
    protected TextWidget m_WeaponAmmo;
    protected ImageWidget m_WeaponMeterFill;
    protected float m_WeaponTrackW;
    protected float m_WeaponTrackH;
    protected EntityAI m_WeaponShown;
    protected Widget m_VitalsGroup;
    protected TextWidget m_HealthPct;
    protected TextWidget m_BloodPct;
    protected TextWidget m_WaterPct;
    protected TextWidget m_FoodPct;
    protected ImageWidget m_HealthTrendUp;
    protected ImageWidget m_HealthTrendDown;
    protected ImageWidget m_BloodTrendUp;
    protected ImageWidget m_BloodTrendDown;
    protected ImageWidget m_WaterTrendUp;
    protected ImageWidget m_WaterTrendDown;
    protected ImageWidget m_FoodTrendUp;
    protected ImageWidget m_FoodTrendDown;
    protected int m_PrevHealth;
    protected int m_PrevBlood;
    protected int m_PrevWater;
    protected int m_PrevFood;
    protected int m_TrendHealth;
    protected int m_TrendBlood;
    protected int m_TrendWater;
    protected int m_TrendFood;
    protected const int TREND_HOLD_MS = 2500;
    protected int m_LastHealthChangeMs;
    protected int m_LastBloodChangeMs;
    protected int m_LastWaterChangeMs;
    protected int m_LastFoodChangeMs;
    protected int m_LastHealthValue = -1;
    protected int m_LastBloodValue = -1;
    protected int m_LastWaterValue = -1;
    protected int m_LastFoodValue = -1;
    protected float m_ClockMarkY;
    protected float m_HudTime;
    protected float m_RefreshAcc;
    protected int m_KillSeenCount;
    protected ref array<float> m_KillTimes;
    protected bool m_F5bVisible;

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
        m_WeaponTrackW = 0;
        m_WeaponTrackH = 0;
        m_Top5Held = false;
        m_GlobalSerialSeen = 0;
        m_GlobalHideAt = 0;
        m_PrevHealth = -1;
        m_PrevBlood = -1;
        m_PrevWater = -1;
        m_PrevFood = -1;
        m_TrendHealth = 0;
        m_TrendBlood = 0;
        m_TrendWater = 0;
        m_TrendFood = 0;
        m_ClockMarkY = 0.48;
        m_HudTime = 0;
        m_RefreshAcc = 0;
        m_KillSeenCount = 0;
        m_F5bVisible = false;
        m_KillLines = new array<TextWidget>;
        m_TopRows = new array<Widget>;
        m_TopRowBgs = new array<ImageWidget>;
        m_TopRanks = new array<TextWidget>;
        m_TopNames = new array<TextWidget>;
        m_TopKills = new array<TextWidget>;
        m_TopDeaths = new array<TextWidget>;
        m_TopPoints = new array<TextWidget>;
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
        HideF5bGroups();
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
        if (m_WeaponPreview)
            m_WeaponPreview.SetItem(null);
        m_WeaponPreview = null;
        m_WeaponShown = null;
        if (m_Root)
            m_Root.Unlink();
        m_Root = null;
        m_CountGroup = null;
        m_CountLabel = null;
        m_CountNumber = null;
        m_CountBar = null;
        m_RoundTag = null;
        m_SelfGroup = null;
        m_SelfRank = null;
        m_SelfPoints = null;
        m_SelfKD = null;
        m_ClockGroup = null;
        m_ClockTime = null;
        m_ClockGoal = null;
        m_ClockFillMe = null;
        m_ClockMarkLead = null;
        m_KillGroup = null;
        m_TopGroup = null;
        m_GlobalGroup = null;
        m_WeaponGroup = null;
        m_WeaponName = null;
        m_WeaponAmmo = null;
        m_WeaponMeterFill = null;
        m_VitalsGroup = null;
        m_HealthPct = null;
        m_BloodPct = null;
        m_WaterPct = null;
        m_FoodPct = null;
        m_HealthTrendUp = null;
        m_HealthTrendDown = null;
        m_BloodTrendUp = null;
        m_BloodTrendDown = null;
        m_WaterTrendUp = null;
        m_WaterTrendDown = null;
        m_FoodTrendUp = null;
        m_FoodTrendDown = null;
        if (m_KillLines)
            m_KillLines.Clear();
        m_KillLines = null;
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
        if (m_TopPoints)
            m_TopPoints.Clear();
        m_TopPoints = null;
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
        m_GlobalSerialSeen = 0;
        m_GlobalHideAt = 0;
        m_PrevHealth = -1;
        m_PrevBlood = -1;
        m_PrevWater = -1;
        m_PrevFood = -1;
        m_TrendHealth = 0;
        m_TrendBlood = 0;
        m_TrendWater = 0;
        m_TrendFood = 0;
        m_F5bVisible = false;
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
        if (held && CanShowF5b())
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
        UpdateF5b(timeslice);
        UpdateGlobalTop();
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
        m_SelfGroup = m_Root.FindAnyWidget("SelfGroup");
        m_SelfRank = TextWidget.Cast(m_Root.FindAnyWidget("SelfRank"));
        m_SelfPoints = TextWidget.Cast(m_Root.FindAnyWidget("SelfPoints"));
        m_SelfKD = TextWidget.Cast(m_Root.FindAnyWidget("SelfKD"));
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
        m_TopGroup = m_Root.FindAnyWidget("TopGroup");
        TextWidget topTitle = TextWidget.Cast(m_Root.FindAnyWidget("TopTitle"));
        if (topTitle)
            topTitle.SetText(Widget.TranslateString("#STR_NEXODM_HUD_TOP5"));
        m_WeaponGroup = m_Root.FindAnyWidget("WeaponGroup");
        m_WeaponPreview = ItemPreviewWidget.Cast(m_Root.FindAnyWidget("WeaponPreview"));
        m_WeaponName = TextWidget.Cast(m_Root.FindAnyWidget("WeaponName"));
        m_WeaponAmmo = TextWidget.Cast(m_Root.FindAnyWidget("WeaponAmmo"));
        TextWidget magLabel = TextWidget.Cast(m_Root.FindAnyWidget("WeaponMagLabel"));
        if (magLabel)
            magLabel.SetText(Widget.TranslateString("#STR_NEXODM_HUD_MAGAZINE"));
        m_WeaponMeterFill = ImageWidget.Cast(m_Root.FindAnyWidget("WeaponMeterFill"));
        Widget weaponTrack = m_Root.FindAnyWidget("WeaponMeterTrack");
        if (weaponTrack)
            weaponTrack.GetSize(m_WeaponTrackW, m_WeaponTrackH);
        m_VitalsGroup = m_Root.FindAnyWidget("VitalsGroup");
        m_HealthPct = TextWidget.Cast(m_Root.FindAnyWidget("HealthPct"));
        m_BloodPct = TextWidget.Cast(m_Root.FindAnyWidget("BloodPct"));
        m_WaterPct = TextWidget.Cast(m_Root.FindAnyWidget("WaterPct"));
        m_FoodPct = TextWidget.Cast(m_Root.FindAnyWidget("FoodPct"));
        m_HealthTrendUp = ImageWidget.Cast(m_Root.FindAnyWidget("HealthTrendUp"));
        m_HealthTrendDown = ImageWidget.Cast(m_Root.FindAnyWidget("HealthTrendDown"));
        m_BloodTrendUp = ImageWidget.Cast(m_Root.FindAnyWidget("BloodTrendUp"));
        m_BloodTrendDown = ImageWidget.Cast(m_Root.FindAnyWidget("BloodTrendDown"));
        m_WaterTrendUp = ImageWidget.Cast(m_Root.FindAnyWidget("WaterTrendUp"));
        m_WaterTrendDown = ImageWidget.Cast(m_Root.FindAnyWidget("WaterTrendDown"));
        m_FoodTrendUp = ImageWidget.Cast(m_Root.FindAnyWidget("FoodTrendUp"));
        m_FoodTrendDown = ImageWidget.Cast(m_Root.FindAnyWidget("FoodTrendDown"));
        int lineIndex = 0;
        for (lineIndex = 0; lineIndex < 4; lineIndex = lineIndex + 1)
        {
            string lineName = "KillLine" + lineIndex.ToString();
            m_KillLines.Insert(TextWidget.Cast(m_Root.FindAnyWidget(lineName)));
        }
        int rowIndex = 0;
        for (rowIndex = 0; rowIndex < 5; rowIndex = rowIndex + 1)
        {
            string suffix = rowIndex.ToString();
            m_TopRows.Insert(m_Root.FindAnyWidget("TopRow" + suffix));
            m_TopRowBgs.Insert(ImageWidget.Cast(m_Root.FindAnyWidget("TopRowBg" + suffix)));
            m_TopRanks.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopRank" + suffix)));
            m_TopNames.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopName" + suffix)));
            m_TopKills.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopKills" + suffix)));
            m_TopDeaths.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopDeaths" + suffix)));
            m_TopPoints.Insert(TextWidget.Cast(m_Root.FindAnyWidget("TopPoints" + suffix)));
        }
        m_GlobalGroup = m_Root.FindAnyWidget("GlobalTopGroup");
        TextWidget globalTitle = TextWidget.Cast(m_Root.FindAnyWidget("GlobalTitle"));
        if (globalTitle)
            globalTitle.SetText(Widget.TranslateString("#STR_NEXODM_HUD_GLOBAL_TOP"));
        int globalIndex = 0;
        for (globalIndex = 0; globalIndex < 10; globalIndex = globalIndex + 1)
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

    protected void HideF5bGroups()
    {
        if (m_SelfGroup)
            m_SelfGroup.Show(false);
        if (m_ClockGroup)
            m_ClockGroup.Show(false);
        if (m_KillGroup)
            m_KillGroup.Show(false);
        if (m_TopGroup)
            m_TopGroup.Show(false);
        if (m_WeaponGroup)
            m_WeaponGroup.Show(false);
        if (m_VitalsGroup)
            m_VitalsGroup.Show(false);
        if (m_WeaponPreview)
            m_WeaponPreview.SetItem(null);
        m_WeaponShown = null;
        m_Top5Held = false;
        m_PrevHealth = -1;
        m_PrevBlood = -1;
        m_PrevWater = -1;
        m_PrevFood = -1;
        m_TrendHealth = 0;
        m_TrendBlood = 0;
        m_TrendWater = 0;
        m_TrendFood = 0;
        m_F5bVisible = false;
    }

    protected bool CanShowF5b()
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

    protected void UpdateF5b(float timeslice)
    {
        m_HudTime = m_HudTime + timeslice;
        if (!CanShowF5b())
        {
            if (m_F5bVisible)
                HideF5bGroups();
            return;
        }
        m_RefreshAcc = m_RefreshAcc + timeslice;
        if (m_RefreshAcc < 0.25)
            return;
        m_RefreshAcc = 0;
        m_F5bVisible = true;
        if (m_SelfGroup)
            m_SelfGroup.Show(true);
        if (m_ClockGroup)
            m_ClockGroup.Show(true);
        if (m_VitalsGroup)
            m_VitalsGroup.Show(true);
        UpdateSelfAndClock();
        UpdateVitals();
        UpdateWeapon();
        UpdateKillfeed();
        UpdateTop5();
    }
    protected void UpdateSelfAndClock()
    {
        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        if (m_SelfRank && data.HasSelf())
        {
            string rankText = "#";
            if (data.GetRank() > 0)
                rankText = "#" + data.GetRank().ToString();
            m_SelfRank.SetText(rankText);
        }
        if (m_SelfPoints && data.HasSelf())
            m_SelfPoints.SetText(data.GetPoints().ToString());
        if (m_SelfKD && data.HasSelf())
        {
            string kdText = data.GetKills().ToString() + " / " + data.GetDeaths().ToString();
            m_SelfKD.SetText(kdText);
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

    protected void SetPct(TextWidget widget, int value)
    {
        if (!widget)
            return;
        widget.SetText(value.ToString() + "%");
        if (value < 25)
            widget.SetColor(ARGB(255, 220, 0, 0));
        else if (value < 50)
            widget.SetColor(ARGB(255, 220, 220, 0));
        else
            widget.SetColor(ARGB(255, 220, 220, 220));
    }

    protected int NextTrend(int storedTrend, int previous, int current)
    {
        if (previous < 0)
            return 0;
        if (current > previous)
            return 1;
        if (current < previous)
            return -1;
        return storedTrend;
    }

    protected void ApplyTrend(ImageWidget upWidget, ImageWidget downWidget, int trend)
    {
        if (upWidget)
            upWidget.Show(trend > 0);
        if (downWidget)
            downWidget.Show(trend < 0);
    }

    protected void UpdateVitals()
    {
        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return;
        if (!data.HasVitals())
            return;
        int health = data.GetHealthPercent();
        int blood = data.GetBloodPercent();
        int water = data.GetWaterPercent();
        int food = data.GetFoodPercent();
        SetPct(m_HealthPct, health);
        SetPct(m_BloodPct, blood);
        SetPct(m_WaterPct, water);
        SetPct(m_FoodPct, food);
        m_TrendHealth = NextTrend(m_TrendHealth, m_PrevHealth, health);
        m_TrendBlood = NextTrend(m_TrendBlood, m_PrevBlood, blood);
        m_TrendWater = NextTrend(m_TrendWater, m_PrevWater, water);
        m_TrendFood = NextTrend(m_TrendFood, m_PrevFood, food);
        if (m_PrevHealth < 0 || health != m_PrevHealth)
            m_PrevHealth = health;
        if (m_PrevBlood < 0 || blood != m_PrevBlood)
            m_PrevBlood = blood;
        if (m_PrevWater < 0 || water != m_PrevWater)
            m_PrevWater = water;
        if (m_PrevFood < 0 || food != m_PrevFood)
            m_PrevFood = food;
        // F5 r2: an arrow stays lit only while its vital keeps changing. Vitals arrive at most once per
        // second, so no change for TREND_HOLD_MS means the value is stable and the arrow goes off.
        // R2: CGame.GetTime() in scripts/3_game/global/game.c:1535.
        int nowMs = GetGame().GetTime();
        if (m_PrevHealth != m_LastHealthValue)
            m_LastHealthChangeMs = nowMs;
        if (m_PrevBlood != m_LastBloodValue)
            m_LastBloodChangeMs = nowMs;
        if (m_PrevWater != m_LastWaterValue)
            m_LastWaterChangeMs = nowMs;
        if (m_PrevFood != m_LastFoodValue)
            m_LastFoodChangeMs = nowMs;
        m_LastHealthValue = m_PrevHealth;
        m_LastBloodValue = m_PrevBlood;
        m_LastWaterValue = m_PrevWater;
        m_LastFoodValue = m_PrevFood;
        if (nowMs - m_LastHealthChangeMs > TREND_HOLD_MS)
            m_TrendHealth = 0;
        if (nowMs - m_LastBloodChangeMs > TREND_HOLD_MS)
            m_TrendBlood = 0;
        if (nowMs - m_LastWaterChangeMs > TREND_HOLD_MS)
            m_TrendWater = 0;
        if (nowMs - m_LastFoodChangeMs > TREND_HOLD_MS)
            m_TrendFood = 0;
        ApplyTrend(m_HealthTrendUp, m_HealthTrendDown, m_TrendHealth);
        ApplyTrend(m_BloodTrendUp, m_BloodTrendDown, m_TrendBlood);
        ApplyTrend(m_WaterTrendUp, m_WaterTrendDown, m_TrendWater);
        ApplyTrend(m_FoodTrendUp, m_FoodTrendDown, m_TrendFood);
    }

    protected void UpdateWeapon()
    {
        DayZGame game = GetGame();
        if (!game)
            return;
        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (!player)
        {
            if (m_WeaponPreview)
                m_WeaponPreview.SetItem(null);
            m_WeaponShown = null;
            if (m_WeaponGroup)
                m_WeaponGroup.Show(false);
            return;
        }
        EntityAI inHands = player.GetEntityInHands();
        Weapon_Base weapon = Weapon_Base.Cast(inHands);
        if (!weapon)
        {
            if (m_WeaponPreview)
                m_WeaponPreview.SetItem(null);
            m_WeaponShown = null;
            if (m_WeaponGroup)
                m_WeaponGroup.Show(false);
            return;
        }
        if (m_WeaponGroup)
            m_WeaponGroup.Show(true);
        if (m_WeaponPreview && m_WeaponShown != inHands)
        {
            m_WeaponPreview.SetItem(weapon);
            m_WeaponPreview.SetView(weapon.GetViewIndex());
            m_WeaponShown = inHands;
        }
        if (m_WeaponName)
            m_WeaponName.SetText(weapon.GetDisplayName());
        int current = 0;
        int maximum = 0;
        bool magCounted = false;
        int muzzleCount = weapon.GetMuzzleCount();
        if (muzzleCount < 1)
            muzzleCount = 1;
        int muzzleIndex = 0;
        for (muzzleIndex = 0; muzzleIndex < muzzleCount; muzzleIndex = muzzleIndex + 1)
        {
            if (weapon.HasInternalMagazine(muzzleIndex))
            {
                current = current + weapon.GetTotalCartridgeCount(muzzleIndex);
                maximum = maximum + weapon.GetTotalMaxCartridgeCount(muzzleIndex);
                continue;
            }
            Magazine mag = weapon.GetMagazine(muzzleIndex);
            if (mag)
            {
                if (!magCounted)
                {
                    current = current + mag.GetAmmoCount();
                    maximum = maximum + game.ConfigGetInt("CfgMagazines " + mag.GetType() + " count");
                    magCounted = true;
                }
                maximum = maximum + 1;
                if (!weapon.IsChamberEmpty(muzzleIndex) && !weapon.IsChamberFiredOut(muzzleIndex) && !weapon.IsChamberJammed(muzzleIndex))
                    current = current + 1;
            }
            else
            {
                maximum = maximum + 1;
                if (!weapon.IsChamberEmpty(muzzleIndex) && !weapon.IsChamberFiredOut(muzzleIndex) && !weapon.IsChamberJammed(muzzleIndex))
                    current = current + 1;
            }
        }
        if (m_WeaponAmmo)
        {
            string ammoText = current.ToString();
            if (maximum > 0)
                ammoText = ammoText + " / " + maximum.ToString();
            m_WeaponAmmo.SetText(ammoText);
        }
        if (m_WeaponMeterFill && m_WeaponTrackW > 0)
        {
            float fracAmmo = 0;
            if (maximum > 0)
                fracAmmo = (current * 1.0) / maximum;
            if (fracAmmo < 0)
                fracAmmo = 0;
            if (fracAmmo > 1)
                fracAmmo = 1;
            m_WeaponMeterFill.SetSize(m_WeaponTrackW * fracAmmo, m_WeaponTrackH);
        }
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
        int slot = 0;
        for (slot = 0; slot < 4; slot = slot + 1)
        {
            TextWidget line = null;
            if (slot < m_KillLines.Count())
                line = m_KillLines.Get(slot);
            if (!line)
                continue;
            int feedIndex = total - 1 - slot;
            if (feedIndex < 0)
            {
                line.SetText("");
                line.Show(false);
                continue;
            }
            float born = 0;
            if (feedIndex < m_KillTimes.Count())
                born = m_KillTimes.Get(feedIndex);
            float age = m_HudTime - born;
            if (age > 6.0)
            {
                line.SetText("");
                line.Show(false);
                continue;
            }
            string killer = data.GetKillKiller(feedIndex);
            string victim = data.GetKillVictim(feedIndex);
            string weaponClass = data.GetKillWeapon(feedIndex);
            string text = victim;
            if (killer != string.Empty && weaponClass != string.Empty)
                text = killer + "  " + weaponClass + "  " + victim;
            else if (killer != string.Empty)
                text = killer + "  " + victim;
            line.SetText(text);
            line.Show(true);
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
        for (rowIndex = 0; rowIndex < 5; rowIndex = rowIndex + 1)
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
            int dimColor = ARGB(255, 200, 200, 200);
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
            TextWidget pointsWidget = null;
            if (rowIndex < m_TopPoints.Count())
                pointsWidget = m_TopPoints.Get(rowIndex);
            if (pointsWidget)
            {
                pointsWidget.SetText(data.GetTopPoints(rowIndex).ToString());
                pointsWidget.SetColor(textColor);
            }
        }
    }

    protected void UpdateGlobalTop()
    {
        if (!m_GlobalGroup)
            return;

        NexoDM_HudData data = NexoDM_HudData.Get();
        int serial = 0;
        if (data)
            serial = data.GetGlobalSerial();

        if (serial != m_GlobalSerialSeen)
        {
            m_GlobalSerialSeen = serial;
            if (serial > 0)
                m_GlobalHideAt = m_HudTime + NexoDM_Constants.GLOBAL_TOP_SHOW_SEC;
        }

        if (data && data.IsRoundActive())
            m_GlobalHideAt = m_HudTime;

        bool showPanel = false;
        if (serial > 0 && m_HudTime < m_GlobalHideAt)
            showPanel = true;
        if (m_CountdownActive)
            showPanel = false;
        DayZGame game = GetGame();
        if (game)
        {
            UIManager ui = game.GetUIManager();
            if (ui && ui.GetMenu())
                showPanel = false;
        }
        m_GlobalGroup.Show(showPanel);
        if (!showPanel || !data)
            return;

        int rowIndex = 0;
        for (rowIndex = 0; rowIndex < 10; rowIndex = rowIndex + 1)
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
            int dimColor = ARGB(255, 200, 200, 200);
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
