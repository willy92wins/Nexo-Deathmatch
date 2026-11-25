// ============================================================================
// LFGungame - config.cpp
// Mod de GunGame para DayZ. Activado por comando de chat LBmaster "!gungame".
// Contrato de producto: ../LFGungame_dev/product-spec.md
// ----------------------------------------------------------------------------
// Declara el addon (CfgPatches::requiredAddons) y carga los 3 modulos de
// script (CfgMods: 3_Game / 4_World / 5_Mission). La logica de juego vive en
// scripts/; este archivo solo declara el addon y engancha los modulos.
// ============================================================================

class CfgPatches
{
    class LFGungame
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        // Verificados contra el source montado (R2):
        //   LBmaster_Core/config/config.cpp:3   -> class LBmaster_Core
        //   LBmaster_Groups/config.cpp:2        -> class LBmaster_Groups
        //   SimpleGroup/config.cpp:79-88        -> requiredAddons incluye JM_CF_Scripts
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "LBmaster_Core",
            "LBmaster_Groups",
            "JM_CF_Scripts"
        };
    };
};

// ----------------------------------------------------------------------------
// CfgMods - carga de los 3 modulos de script (patron estandar mod de scripts)
// ----------------------------------------------------------------------------
class CfgMods
{
    class LFGungame
    {
        type = "mod";
        dir = "LFGungame";
        inputs = "LFGungame/inputs.xml";
        dependencies[] = { "Game", "World", "Mission" };

        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = { "LFGungame/scripts/3_Game" };
            };
            class worldScriptModule
            {
                value = "";
                files[] = { "LFGungame/scripts/4_World" };
            };
            class missionScriptModule
            {
                value = "";
                files[] = { "LFGungame/scripts/5_Mission" };
            };
        };
    };
};
