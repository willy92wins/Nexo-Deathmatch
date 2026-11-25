enum NexoDM_State
{
    IDLE = 0,
    ACTIVE = 2,
    ENDED = 3
}

enum NexoDM_Role
{
    NONE = 0,
    PARTICIPANT = 1
}

class NexoDM_Constants
{
    static const string PROFILE_DIR = "$profile:NexoDM";
    static const string CONFIG_PATH = "$profile:NexoDM\\config.json";

    static const int DEFAULT_VERSION = 1;
    static const string DEFAULT_ADMIN = "CAMBIAR_STEAMID64";
    static const string DEFAULT_CLOTHING_PRESET = "CAMBIAR_Ropa";
    static const string DEFAULT_LOADOUT_1 = "CAMBIAR_Loadout1";
    static const string DEFAULT_LOADOUT_2 = "CAMBIAR_Loadout2";
    static const string DEFAULT_KNIFE = "HuntingKnife";
    static const int DEFAULT_ROUND_DURATION_VALUE = 1200;
    static const int DEFAULT_KILLS_TO_WIN = 30;
    static const int DEFAULT_RESPAWN_DELAY = 5;
    static const int RESPAWN_DELAY_MAX = 30;
    static const int DEFAULT_MAX_PLAYERS_VALUE = 15;
    static const int DEFAULT_AUTOSTART_DELAY_VALUE = 300;
    static const float DEFAULT_SPAWN_RADIUS = 3.0;
    // Fresh-character routing (connect/respawn) runs one tick after creation, off the creation tick,
    // with no perceptible wait. GG-01v5: a participant respawn character is born at the gungame spawn
    // (pos override in OnClientNewEvent) and equipped during creation (StartingEquipSetup), so this
    // routing only does post-birth stat reset + HUD sync, not teleport/equip. ms.
    static const int FRESH_CHAR_ROUTE_DELAY_MS = 0;

    // Option A downed-kill credit: when a participant dies with NO killer reported (scripted SetHealth(0)
    // of an unconscious player choosing Respawn via OnClientRespawnEvent, or a bleed-out), OnPlayerKilled
    // credits the last participant who damaged them IF the hit was within this window - so a stale hit
    // before an unrelated death is not credited. 120 s covers a quick respawn-click and a long bleed-out.
    static const int ATTACKER_CREDIT_WINDOW_MS = 120000;

    static const string KEY_VERSION = "version";
    static const string KEY_CLOTHING_PRESET = "ClothingPreset";
    static const string KEY_LOADOUTS = "Loadouts";
    static const string KEY_ROUND_DURATION = "RoundDuration";
    static const string KEY_KILLS_TO_WIN = "KillsToWin";
    static const string KEY_RESPAWN_DELAY = "RespawnDelay";
    static const string KEY_MAX_PLAYERS = "MaxPlayers";
    static const string KEY_MAPS = "Maps";
    static const string KEY_SPAWN_GROUPS = "SpawnGroups";
    static const string KEY_POINTS = "Points";

    static const string CFG_VEHICLES = "CfgVehicles ";
    static const string COORD_SEPARATOR = ",";
    static const string DIGITS = "0123456789";
    static const string MINUS = "-";
    static const string DOT = ".";
    static const string LOG_PREFIX = "[NexoDM][Config] ";
    static const int LOG_RULE_INTERVAL_MS = 30000;

    static const string RULE_LOAD_FAILED = "E_CFG_LOAD_FAILED";
    static const string RULE_SAVE_FAILED = "E_CFG_SAVE_FAILED";
    static const string RULE_DIR_CREATE_FAILED = "E_CFG_DIR_CREATE_FAILED";
    static const string RULE_VERSION = "E_CFG_VERSION";
    static const string RULE_ROUND = "E_CFG_ROUND";
    static const string RULE_RESPAWN_DELAY = "E_CFG_RESPAWN_DELAY";
    static const string RULE_MAPS = "E_CFG_MAPS";
    static const string RULE_MAP_OTHER_WORLD = "W_CFG_MAP_OTHER_WORLD";
    static const string RULE_SPAWN = "E_CFG_SPAWN";
    static const string RULE_PRESET = "E_CFG_PRESET";
    static const string RULE_KNIFE_INVALID = "E_CFG_KNIFE_INVALID";
    static const string RULE_MAX_PLAYERS_INVALID = "E_CFG_MAX_PLAYERS_INVALID";
    static const string RULE_AUTOSTART_DELAY_INVALID = "E_CFG_AUTOSTART_DELAY_INVALID";
    static const string RULE_KILL_RECORDED = "KILL_RECORDED";
    static const string RULE_DEATH_RECORDED = "DEATH_RECORDED";
    static const string RULE_ASSIST_RECORDED = "ASSIST_RECORDED";
    static const string RULE_RESPAWN_EQUIPPED = "RESPAWN_EQUIPPED";
    static const string RULE_TIMEOUT_RECORDED = "TIMEOUT_RECORDED";

    static const string MSG_WINNER_PREFIX = "Ganador del Juego de Armas: ";
    static const string MSG_NO_WINNER = "Juego de Armas terminado sin ganador.";
    static const string MSG_ROUND_STARTED = "Juego de Armas iniciado.";
    static const string MSG_ROUND_STOPPED = "Juego de Armas detenido.";
}
