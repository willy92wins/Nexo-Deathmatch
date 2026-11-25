// F8b points 4/2, corrected R1 #1/#2: vanilla water/food/temperature (plus
// their NotifierDivider) and the left PresencePanel/ItemActions are hidden
// ONLY while the local player is a participant of an active round. Outside
// the round the vanilla HUD is restored as-is.
// Vanilla never re-shows those panels itself: DisplayNotifier
// (P:\scripts\5_mission\gui\ingamehud.c:447-484), DisplayTendencyTemp
// (:520-582) and IngameHudHeatBuffer.Update only touch children (Icon*,
// *Arrow*, TemperatureValue*, HeatBuffer1..3), and IngameHudVisibility
// (P:\scripts\5_mission\gui\ingamehudvisibility.c:63) only toggles the
// parent NotifiersPanel. So Show(false) on the panel hides icon+arrows and
// Show(true) restores them.
// Panel names/geometry: Temperature P:\gui\layouts\day_z_hud.layout:1144,
// Hungry :1387, Thirsty :1553, NotifierDivider :1132, Blood :938, Health
// :768 (both kept). Left side: PresencePanel :3513, ItemActions :3862;
// StancePanel :3614 and StaminaBar :3832 already sit bottom-left in
// vanilla, so they are not moved and never hidden here.
modded class IngameHud
{
    protected Widget m_NexoThirsty;
    protected Widget m_NexoHungry;
    protected Widget m_NexoTemperature;
    protected Widget m_NexoNotifierDivider;
    protected Widget m_NexoPresence;
    protected Widget m_NexoItemActions;
    protected bool m_NexoRoundHudHidden;

    // R2: IngameHud.Init(Widget) in P:\scripts\5_mission\gui\ingamehud.c:143.
    override void Init(Widget hud_panel_widget)
    {
        super.Init(hud_panel_widget);

        m_NexoRoundHudHidden = false;
        if (!hud_panel_widget)
            return;

        m_NexoThirsty = hud_panel_widget.FindAnyWidget("Thirsty");
        m_NexoHungry = hud_panel_widget.FindAnyWidget("Hungry");
        m_NexoTemperature = hud_panel_widget.FindAnyWidget("Temperature");
        m_NexoNotifierDivider = hud_panel_widget.FindAnyWidget("NotifierDivider");
        m_NexoPresence = hud_panel_widget.FindAnyWidget("PresencePanel");
        m_NexoItemActions = hud_panel_widget.FindAnyWidget("ItemActions");
        NexoDM_ApplyRoundHudVisibility();
    }

    // R2: IngameHud.Update(float) in P:\scripts\5_mission\gui\ingamehud.c:1141;
    // super.Update calls DisplayPresence (:1146, :776-810) which only shows
    // Presence0..4 children, never PresencePanel itself.
    // ItemActionsWidget.Update (P:\scripts\5_mission\gui\itemactionswidget.c:189-224)
    // does m_Root.Show(true) from the GUI update queue
    // (P:\scripts\3_game\dayzgame.c:3044-3046), which runs before
    // hud.Update in OnPostUpdate (dayzgame.c:3120-3123); hiding after
    // super.Update therefore wins the frame, and re-hiding every frame
    // while in round keeps ItemActions off.
    override void Update(float timeslice)
    {
        super.Update(timeslice);
        NexoDM_ApplyRoundHudVisibility();
    }

    protected bool NexoDM_IsLocalRoundParticipant()
    {
        NexoDM_HudData data = NexoDM_HudData.Get();
        if (!data)
            return false;
        if (!data.IsRoundActive())
            return false;

        // Pattern: PlayerBase.Cast(g_Game.GetPlayer()) in ingamehud.c:780.
        if (!g_Game)
            return false;
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return false;
        if (player.m_GG_Role != NexoDM_Role.PARTICIPANT)
            return false;
        return true;
    }

    protected void NexoDM_ApplyRoundHudVisibility()
    {
        if (NexoDM_IsLocalRoundParticipant())
        {
            // Every frame while in round (see Update note on ItemActions).
            if (m_NexoThirsty)
                m_NexoThirsty.Show(false);
            if (m_NexoHungry)
                m_NexoHungry.Show(false);
            if (m_NexoTemperature)
                m_NexoTemperature.Show(false);
            if (m_NexoNotifierDivider)
                m_NexoNotifierDivider.Show(false);
            if (m_NexoPresence)
                m_NexoPresence.Show(false);
            if (m_NexoItemActions)
                m_NexoItemActions.Show(false);
            m_NexoRoundHudHidden = true;
            return;
        }

        if (!m_NexoRoundHudHidden)
            return;

        // Restore once on leaving the round; afterwards vanilla/widget
        // scripts own these widgets again. ItemActions is NOT forced visible:
        // ItemActionsWidget.Update (itemactionswidget.c:189-221) decides it
        // per frame (HUD flags, hands, open menu). PresencePanel follows the
        // vanilla LHUD rule (ingamehudvisibility.c:135-141), so it stays
        // hidden with the HUD disabled/hidden, a menu open, unconscious or in
        // a vehicle, unless the inventory is open.
        if (m_NexoThirsty)
            m_NexoThirsty.Show(true);
        if (m_NexoHungry)
            m_NexoHungry.Show(true);
        if (m_NexoTemperature)
            m_NexoTemperature.Show(true);
        if (m_NexoNotifierDivider)
            m_NexoNotifierDivider.Show(true);
        if (m_NexoPresence)
            m_NexoPresence.Show(NexoDM_VanillaPresenceVisible());
        m_NexoRoundHudHidden = false;
    }

    // Mirrors IngameHudVisibility.UpdateElement for LHUD_PRESENCE
    // (P:\scripts\5_mission\gui\ingamehudvisibility.c:135-141) through the
    // public IsContextFlagActive (:120) and HUD_HIDE_FLAGS (:43).
    protected bool NexoDM_VanillaPresenceVisible()
    {
        IngameHudVisibility vis = GetHudVisibility();
        if (!vis)
            return true;
        if (vis.IsContextFlagActive(EHudContextFlags.INVENTORY_OPEN))
            return true;
        if (vis.IsContextFlagActive(IngameHudVisibility.HUD_HIDE_FLAGS | EHudContextFlags.VEHICLE))
            return false;
        return true;
    }
}
