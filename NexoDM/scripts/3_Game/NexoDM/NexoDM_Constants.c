enum NexoDM_State
{
    IDLE = 0,
    VOTING = 1,
    ACTIVE = 2,
    ENDED = 3
}

enum NexoDM_Role
{
    NONE = 0,
    PARTICIPANT = 1,
    PUBLIC = 2
}

enum NexoDM_PromptState
{
    NONE = 0,
    PROMPT_PENDING = 1,
    ANSWERED = 2
}

class NexoDM_Constants
{
    static const string PROFILE_DIR = "$profile:NexoDM";
    static const string CONFIG_PATH = "$profile:NexoDM\\config.json";

    static const string DEFAULT_ADMIN_1 = "7656XXXXXXXXXX";
    static const string DEFAULT_ADMIN_2 = "7656XXXXXXXXXX";
    static const string DEFAULT_CLOTHING_PRESET = "Preset1";
    static const string DEFAULT_GUN_PRESET_1 = "GunPreset1";
    static const string DEFAULT_GUN_PRESET_2 = "GunPreset2";
    static const string DEFAULT_GUN_PRESET_3 = "GunPreset3";
    static const string DEFAULT_KNIFE = "HuntingKnife";
    static const string DEFAULT_KNIFE_ONE_HIT = "true";
    static const string DEFAULT_ROUND_DURATION = "1200";
    static const int DEFAULT_ROUND_DURATION_VALUE = 1200;
    static const string DEFAULT_MAX_PLAYERS = "15";
    static const int DEFAULT_MAX_PLAYERS_VALUE = 15;
    static const string DEFAULT_RESPAWN_COORD = "18939,3828,3993";
    static const string DEFAULT_RESPAWN_RADIUS = "10";
    static const float DEFAULT_RESPAWN_RADIUS_VALUE = 10.0;
    static const string DEFAULT_PUBLIC_COORD = "18235,876,8567";
    static const string DEFAULT_PUBLIC_RESPAWN_RADIUS = "10";
    static const float DEFAULT_PUBLIC_RESPAWN_RADIUS_VALUE = 10.0;
    static const string DEFAULT_AUTOSTART = "0";
    static const string DEFAULT_AUTOSTART_DELAY = "300";
    static const int DEFAULT_AUTOSTART_DELAY_VALUE = 300;
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

    // F-18: KEY_ADMIN/KNIFE/KNIFE_ONE_HIT/ROUND_DURATION/MAX_PLAYERS removed (declared, never used).
    static const string KEY_CLOTHING_PRESET = "ClothingPreset";
    static const string KEY_GUN_PRESETS = "GunPresets";
    static const string KEY_RESPAWN_COORDINATES = "RespawnCoordinates";
    static const string KEY_RESPAWN_RADIUS = "RespawnRadius";
    static const string KEY_PUBLIC_COORDINATES = "PublicCoordinates";
    static const string KEY_PUBLIC_RESPAWN_RADIUS = "PublicRespawnRadius";

    static const string CFG_VEHICLES = "CfgVehicles ";
    static const string COORD_SEPARATOR = ",";
    static const string DIGITS = "0123456789";
    static const string MINUS = "-";
    static const string DOT = ".";
    static const string BOOL_TRUE = "true";
    static const string BOOL_ONE = "1";
    static const string LOG_PREFIX = "[NexoDM][Config] ";
    static const int LOG_RULE_INTERVAL_MS = 30000;

    static const string RULE_LOAD_FAILED = "E_CFG_LOAD_FAILED";
    static const string RULE_SAVE_FAILED = "E_CFG_SAVE_FAILED";
    static const string RULE_DIR_CREATE_FAILED = "E_CFG_DIR_CREATE_FAILED";
    static const string RULE_GUN_PRESETS_EMPTY = "E_CFG_GUN_PRESETS_EMPTY";
    static const string RULE_PRESET_MISSING = "E_CFG_PRESET_MISSING";
    static const string RULE_KNIFE_INVALID = "E_CFG_KNIFE_INVALID";
    static const string RULE_ROUND_DURATION_INVALID = "E_CFG_ROUND_DURATION_INVALID";
    static const string RULE_MAX_PLAYERS_INVALID = "E_CFG_MAX_PLAYERS_INVALID";
    static const string RULE_COORD_INVALID = "E_CFG_COORD_INVALID";
    static const string RULE_RADIUS_INVALID = "E_CFG_RADIUS_INVALID";
    static const string RULE_AUTOSTART_DELAY_INVALID = "E_CFG_AUTOSTART_DELAY_INVALID";
    static const string RULE_B4_GAME_FULL_CHAT_DEFERRED = "B4_GAME_FULL_CHAT_DEFERRED";
    static const string RULE_OPEN_DIALOG_NO_TARGET = "OPEN_DIALOG_NO_TARGET";
    static const string RULE_VOTE_BAD_PAYLOAD = "VOTE_BAD_PAYLOAD";
    static const string RULE_VOTE_NO_SENDER = "VOTE_NO_SENDER";
    static const string RULE_VOTE_NO_STEAMID = "VOTE_NO_STEAMID";
    static const string RULE_VOTE_NO_MANAGER = "VOTE_NO_MANAGER";
    // F-18: RULE_VOTE_ADMIN removed (dead since the "admin can participate" amendment; 0 uses).
    static const string RULE_VOTE_BAD_STATE = "VOTE_BAD_STATE";
    static const string RULE_VOTE_NOT_PENDING = "VOTE_NOT_PENDING";
    static const string RULE_KILL_RECORDED = "KILL_RECORDED";
    static const string RULE_DEATH_RECORDED = "DEATH_RECORDED";
    static const string RULE_KNIFE_DEATH_RECORDED = "KNIFE_DEATH_RECORDED";
    static const string RULE_ASSIST_RECORDED = "ASSIST_RECORDED";
    static const string RULE_RESPAWN_EQUIPPED = "RESPAWN_EQUIPPED";
    static const string RULE_VICTORY_RECORDED = "VICTORY_RECORDED";
    static const string RULE_TIMEOUT_RECORDED = "TIMEOUT_RECORDED";
    static const string RULE_ROUND_CLEANUP = "ROUND_CLEANUP";

    static const string MSG_WINNER_PREFIX = "Ganador del Juego de Armas: ";
    static const string MSG_NO_WINNER = "Juego de Armas terminado sin ganador.";
    static const string MSG_GAME_FULL = "La partida esta llena.";
    static const string MSG_ROUND_STARTED = "Juego de Armas iniciado.";
    static const string MSG_ROUND_STOPPED = "Juego de Armas detenido.";
}
