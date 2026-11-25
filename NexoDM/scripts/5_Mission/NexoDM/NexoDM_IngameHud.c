// F8b point 4: bottom right keeps only the vanilla blood and health notifiers.
// Water, food and temperature panels are hidden once, right after the vanilla
// HUD builds them. Vanilla only re-shows the icons and arrows inside each
// panel (DisplayNotifier in P:\scripts\5_mission\gui\ingamehud.c:447), never
// the panels themselves, so hiding the panel hides its icon and arrows too.
// Panel names and geometry: P:\gui\layouts\day_z_hud.layout:1144 (Temperature),
// :1387 (Hungry) and :1553 (Thirsty). Blood (:938) and Health (:768) stay.
modded class IngameHud
{
    // R2: IngameHud.Init(Widget) in scripts/5_mission/gui/ingamehud.c:143.
    override void Init(Widget hud_panel_widget)
    {
        super.Init(hud_panel_widget);

        if (!hud_panel_widget)
            return;

        Widget thirsty = hud_panel_widget.FindAnyWidget("Thirsty");
        if (thirsty)
            thirsty.Show(false);

        Widget hungry = hud_panel_widget.FindAnyWidget("Hungry");
        if (hungry)
            hungry.Show(false);

        Widget temperature = hud_panel_widget.FindAnyWidget("Temperature");
        if (temperature)
            temperature.Show(false);
    }
}
