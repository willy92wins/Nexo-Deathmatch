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
        m_PrevHealth = -1;
        m_PrevBlood = -1;
        m_PrevWater = -1;
        m_PrevFood = -1;
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
        if (m_Root)
            m_Root.Unlink();
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
        m_Root.Show(show);
    }

    void SetTop5Visible(bool held)
    {
        m_Top5Held = held;
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
        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (!player)
            return false;
        if (player.m_GG_Role != NexoDM_Role.PARTICIPANT)
            return false;
        NexoDM_KillerCam cam = NexoDM_KillerCam.Get();
        if (cam && cam.IsActive())
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
        if (m_ClockFillMe && killsToWin > 0 && m_ClockTrackW > 0)
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
        if (m_ClockMarkLead && killsToWin > 0 && m_ClockTrackW > 0)
        {
            int leadKills = 0;
            if (data.GetTopCount() > 0)
                leadKills = data.GetTopKills(0);
            float fracLead = (leadKills * 1.0) / killsToWin;
            if (fracLead < 0)
                fracLead = 0;
            if (fracLead > 1)
                fracLead = 1;
            m_ClockMarkLead.SetPos(m_ClockTrackW * fracLead, 0, true);
            m_ClockMarkLead.Show(true);
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

    protected void SetTrend(ImageWidget upWidget, ImageWidget downWidget, int previous, int current)
    {
        if (upWidget)
            upWidget.Show(false);
        if (downWidget)
            downWidget.Show(false);
        if (previous < 0)
            return;
        if (current > previous)
        {
            if (upWidget)
                upWidget.Show(true);
        }
        if (current < previous)
        {
            if (downWidget)
                downWidget.Show(true);
        }
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
        SetTrend(m_HealthTrendUp, m_HealthTrendDown, m_PrevHealth, health);
        SetTrend(m_BloodTrendUp, m_BloodTrendDown, m_PrevBlood, blood);
        SetTrend(m_WaterTrendUp, m_WaterTrendDown, m_PrevWater, water);
        SetTrend(m_FoodTrendUp, m_FoodTrendDown, m_PrevFood, food);
        m_PrevHealth = health;
        m_PrevBlood = blood;
        m_PrevWater = water;
        m_PrevFood = food;
    }

    protected void UpdateWeapon()
    {
        DayZGame game = GetGame();
        if (!game)
            return;
        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (!player)
        {
            if (m_WeaponGroup)
                m_WeaponGroup.Show(false);
            return;
        }
        EntityAI inHands = player.GetEntityInHands();
        Weapon_Base weapon = Weapon_Base.Cast(inHands);
        if (!weapon)
        {
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
            m_WeaponShown = inHands;
        }
        if (m_WeaponName)
            m_WeaponName.SetText(weapon.GetDisplayName());
        int muzzle = weapon.GetCurrentMuzzle();
        int current = 0;
        int maximum = 0;
        if (weapon.HasInternalMagazine(muzzle))
        {
            current = weapon.GetTotalCartridgeCount(muzzle);
            maximum = weapon.GetTotalMaxCartridgeCount(muzzle);
        }
        else
        {
            Magazine mag = weapon.GetMagazine(muzzle);
            if (mag)
            {
                current = mag.GetAmmoCount();
                if (!weapon.IsChamberEmpty(muzzle))
                    current = current + 1;
                maximum = game.ConfigGetInt("CfgMagazines " + mag.GetType() + " count");
            }
            else
            {
                if (!weapon.IsChamberEmpty(muzzle))
                    current = 1;
                maximum = 0;
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
