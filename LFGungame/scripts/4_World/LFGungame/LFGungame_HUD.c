class LFGungame_HUD : Managed
{
    protected static LFGungame_HUD s_Instance;

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
    protected Widget m_WeaponGroup;
    protected TextWidget m_WeaponTier;
    protected TextWidget m_WeaponName;
    protected Widget m_WeaponPips;
    protected ImageWidget m_CrownIcon;
    protected TextWidget m_CrownText;
    protected TextWidget m_CrownTextEN;
    protected float m_TimeLeft;
    protected int m_LastShown;
    protected float m_CountTotal;
    protected float m_BarBaseW;
    protected float m_BarBaseH;
    // Freeze-phase gate: the server syncs role/tier at freeze start (BeginRound), 10s before
    // combat. The card/crown must not show during the countdown, so the last synced values are
    // stored here and applied when the countdown reaches zero.
    protected bool m_CountdownActive;
    protected int m_PendRole;
    protected int m_PendTier;
    protected int m_PendTierMax;
    protected bool m_PendLeader;

    void LFGungame_HUD()
    {
        m_TimeLeft = -1;
        m_LastShown = -1;
        m_CountTotal = 0;
        m_BarBaseW = 0;
        m_BarBaseH = 0;
        m_CountdownActive = false;
        m_PendRole = LFGungame_Role.NONE;
        m_PendTier = 0;
        m_PendTierMax = 0;
        m_PendLeader = false;

        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: DayZGame.GetWorkspace() in scripts/3_game/global/game.c:84.
        WorkspaceWidget workspace = game.GetWorkspace();
        if (!workspace)
            return;

        string layoutPath = "LFGungame/gui/layouts/lfgungame_hud.layout";
        // R2: WorkspaceWidget.CreateWidgets(string,Widget,bool) in scripts/1_core/proto/enwidgets.c:181.
        m_Root = workspace.CreateWidgets(layoutPath);
        if (!m_Root)
            return;

        CacheWidgets();
        HideCountdown();
        if (m_WeaponGroup)
            m_WeaponGroup.Show(false);

        s_Instance = this;
    }

    void ~LFGungame_HUD()
    {
        Destroy();
    }

    static LFGungame_HUD Get()
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
        m_WeaponGroup = null;
        m_WeaponTier = null;
        m_WeaponName = null;
        m_WeaponPips = null;
        m_CrownIcon = null;
        m_CrownText = null;
        m_CrownTextEN = null;
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

    // H1/H2: bottom-left tier counter + leader crown. Driven by PlayerBase.OnVariablesSynchronized
    // from the synced m_GG_Role/Tier/TierMax/IsLeader. tier is 0-based; shown 1-based as "Arma X/N".
    void UpdateTier(int role, int tier, int tierMax, bool isLeader)
    {
        if (!m_Root)
            return;

        m_PendRole = role;
        m_PendTier = tier;
        m_PendTierMax = tierMax;
        m_PendLeader = isLeader;

        // Freeze gate: while the start countdown runs the card stays hidden; the stored values
        // are applied from HideCountdown when the countdown reaches zero.
        if (m_CountdownActive)
        {
            if (m_WeaponGroup)
                m_WeaponGroup.Show(false);
            return;
        }

        ApplyTierPending();
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
        if (m_WeaponGroup)
            m_WeaponGroup.Show(false);
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
        string weaponGroupName = "WeaponGroup";

        // R2: Widget.FindAnyWidget(string) in scripts/1_core/proto/enwidgets.c:169.
        m_CountGroup = m_Root.FindAnyWidget(countGroupName);
        m_CountNumber = TextWidget.Cast(m_Root.FindAnyWidget(countNumberName));
        m_CountBar = ImageWidget.Cast(m_Root.FindAnyWidget(countBarName));
        m_RoundTag = TextWidget.Cast(m_Root.FindAnyWidget(roundTagName));
        m_WeaponGroup = m_Root.FindAnyWidget(weaponGroupName);

        string countLabelName = "CountLabel";
        m_CountLabel = TextWidget.Cast(m_Root.FindAnyWidget(countLabelName));

        // Workspace-created HUD: #STR keys baked in the .layout render the raw key here (a UIScriptedMenu
        // resolves them, a raw workspace widget does not). Resolve the round tag + countdown label
        // explicitly to the client language. R2: Widget.TranslateString(string) in scripts/1_core/proto/enwidgets.c:112.
        if (m_RoundTag)
            m_RoundTag.SetText(Widget.TranslateString("#STR_LFGUNGAME_HUD_ROUND_TAG"));
        if (m_CountLabel)
            m_CountLabel.SetText(Widget.TranslateString("#STR_LFGUNGAME_HUD_STARTING"));

        if (m_CountBar)
        {
            // Base size feeds the width animation in OnUpdate (relative units, exact-size off).
            // R2: Widget.GetSize(out float,out float) in scripts/1_core/proto/enwidgets.c:152.
            m_CountBar.GetSize(m_BarBaseW, m_BarBaseH);
        }

        string weaponTierName = "WeaponTier";
        string weaponNameName = "WeaponName";
        string weaponPipsName = "WeaponPips";
        string crownIconName = "CrownIcon";
        string crownTextName = "CrownText";
        string crownTextENName = "CrownTextEN";
        m_WeaponTier = TextWidget.Cast(m_Root.FindAnyWidget(weaponTierName));
        m_WeaponName = TextWidget.Cast(m_Root.FindAnyWidget(weaponNameName));
        m_WeaponPips = m_Root.FindAnyWidget(weaponPipsName);
        // R2: Class.Cast in scripts/1_core/proto/enscript.c:94; ImageWidget in enwidgets.c.
        m_CrownIcon = ImageWidget.Cast(m_Root.FindAnyWidget(crownIconName));
        m_CrownText = TextWidget.Cast(m_Root.FindAnyWidget(crownTextName));
        m_CrownTextEN = TextWidget.Cast(m_Root.FindAnyWidget(crownTextENName));

        // Same workspace-HUD #STR caveat as RoundTag/CountLabel: the .layout-baked #STR renders the raw
        // key here, so resolve the rank/crown label to the client language explicitly.
        // R2: Widget.TranslateString(string) in scripts/1_core/proto/enwidgets.c:112.
        if (m_CrownText)
            m_CrownText.SetText(Widget.TranslateString("#STR_LFGUNGAME_RANK_FIRST"));

        // No friendly weapon name (D-K2 deferred) and no pips wired: keep both hidden so the
        // card shows only the "Arma X/N" counter when WeaponGroup becomes visible.
        if (m_WeaponName)
            m_WeaponName.Show(false);
        if (m_WeaponPips)
            m_WeaponPips.Show(false);
    }

    // Applies the last synced role/tier/crown to the bottom-left widgets. Split from UpdateTier
    // so HideCountdown can re-apply the values deferred by the freeze gate.
    protected void ApplyTierPending()
    {
        if (!m_Root)
            return;

        bool isParticipant = (m_PendRole == LFGungame_Role.PARTICIPANT);

        if (m_WeaponGroup)
            m_WeaponGroup.Show(isParticipant);

        if (!isParticipant)
            return;

        if (m_WeaponTier)
        {
            // Wait window: a participant whose tier has not been synced yet (m_PendTierMax == 0, i.e.
            // VOTING accepted, before BeginRound runs SetTierSync) shows the role label, not the weapon
            // counter. Once the round arms combat, m_PendTierMax becomes the gun-preset count (>= 1) and
            // the "Arma X/N" counter is shown. "Participante" is hardcoded to match the hardcoded "Arma".
            if (m_PendTierMax < 1)
            {
                // Localized role label, resolved to the client language (workspace HUD, see CacheWidgets).
                // R2: Widget.TranslateString(string) in scripts/1_core/proto/enwidgets.c:112.
                m_WeaponTier.SetText(Widget.TranslateString("#STR_LFGUNGAME_ROLE_PARTICIPANT"));
            }
            else
            {
                int shown = m_PendTier + 1;
                int total = m_PendTierMax;
                if (shown < 1)
                    shown = 1;
                if (shown > total)
                    shown = total;

                // Localized weapon word + dynamic tier counter (e.g. "Weapon 3/7" / "Arma 3/7").
                // R2: Widget.TranslateString(string) in scripts/1_core/proto/enwidgets.c:112.
                string label = Widget.TranslateString("#STR_LFGUNGAME_HUD_WEAPON");
                label = label + " ";
                label = label + shown.ToString();
                label = label + "/";
                label = label + total.ToString();
                // R2: TextWidget.SetText(string,bool) in scripts/1_core/proto/enwidgets.c:195.
                m_WeaponTier.SetText(label);
            }
        }

        // Crown: text fallback ("1.º"/"1st") for the leader(s) until crown.edds is wired.
        if (m_CrownText)
            m_CrownText.Show(m_PendLeader);
        if (m_CrownTextEN)
            m_CrownTextEN.Show(m_PendLeader);
        if (m_CrownIcon)
            m_CrownIcon.Show(false);
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
            m_CountNumber.SetColor(LFGungame_UIConst.COL_DANGER);
        }
        else
        {
            // R2: Widget.SetColor(int) in scripts/1_core/proto/enwidgets.c:144.
            m_CountNumber.SetColor(LFGungame_UIConst.COL_TEXT);
        }
    }

    protected void HideCountdown()
    {
        if (m_CountGroup)
            m_CountGroup.Show(false);

        if (m_RoundTag)
            m_RoundTag.Show(false);

        // Freeze gate OFF: restore the vanilla hud and apply the tier sync deferred during the
        // countdown. Constructor-time calls skip this branch (gate never armed).
        if (m_CountdownActive)
        {
            m_CountdownActive = false;
            SetVanillaHudVisible(true);
            ApplyTierPending();
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
