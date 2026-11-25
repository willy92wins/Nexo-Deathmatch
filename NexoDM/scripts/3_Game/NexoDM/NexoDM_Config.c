// JSON shape of one spawn group inside a map. Points stay strings ("x,y,z") until Validate parses them.
class NexoDM_SpawnGroupConfig
{
    string Name;
    ref array<string> Points;

    void NexoDM_SpawnGroupConfig()
    {
        Points = new array<string>();
    }
}

// JSON shape of one arena. WorldName is compared to GetGame().GetWorldName() (case-insensitive).
class NexoDM_MapConfig
{
    string Name;
    string WorldName;
    float SpawnRadius;
    ref array<ref NexoDM_SpawnGroupConfig> SpawnGroups;

    void NexoDM_MapConfig()
    {
        SpawnGroups = new array<ref NexoDM_SpawnGroupConfig>();
    }
}

// Runtime copy of a group: Points are vectors. Not a JSON field of NexoDM_Config (that class is what JsonFileLoader writes).
class NexoDM_ParsedSpawnGroup
{
    string Name;
    ref array<vector> Points;

    void NexoDM_ParsedSpawnGroup()
    {
        Points = new array<vector>();
    }
}

// Runtime copy of a map that belongs to the loaded world and passed the spawn rules.
class NexoDM_ParsedMap
{
    string Name;
    string WorldName;
    float SpawnRadius;
    ref array<ref NexoDM_ParsedSpawnGroup> SpawnGroups;

    void NexoDM_ParsedMap()
    {
        SpawnGroups = new array<ref NexoDM_ParsedSpawnGroup>();
    }
}

class NexoDM_Config
{
    int version;
    ref array<string> Admin;
    int RoundDuration;
    int KillsToWin;
    int RespawnDelay;
    int MaxPlayers;
    int MinPlayersToScore;
    string Knife;
    bool KnifeOneHit;
    bool Autostart;
    int AutostartDelaySeconds;
    string ClothingPreset;
    ref array<string> Loadouts;
    ref array<ref NexoDM_MapConfig> Maps;

    // Instance fields are written by JsonFileLoader. The parsed-map cache stays static so Save() does not emit it.
    protected static ref array<ref NexoDM_ParsedMap> s_ValidMaps;
    protected static ref map<string, int> s_LastRuleLog = new map<string, int>();

    void NexoDM_Config()
    {
        Admin = new array<string>();
        Loadouts = new array<string>();
        Maps = new array<ref NexoDM_MapConfig>();
    }

    static NexoDM_Config Load()
    {
        s_ValidMaps = new array<ref NexoDM_ParsedMap>();

        if (!EnsureProfileDirectory())
            return null;

        // R2: FileExist(string) in scripts/1_core/proto/ensystem.c:397.
        if (!FileExist(NexoDM_Constants.CONFIG_PATH))
        {
            NexoDM_Config defaultsConfig = NexoDM_Config.CreateDefaults();
            if (!defaultsConfig.Save())
                return null;
            // A fresh file is all placeholders. No round can start until Admin, ClothingPreset and Loadouts are real names.
            string createdMsg = NexoDM_Constants.LOG_PREFIX;
            createdMsg = createdMsg + "created config.json with PLACEHOLDER defaults; edit $profile:NexoDM\\config.json (Admin/ClothingPreset/Loadouts) before use";
            // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
            Print(createdMsg);
        }

        NexoDM_Config config = new NexoDM_Config();
        string errorMessage;
        // R2: JsonFileLoader<T>.LoadFile(string,out T,out string) in scripts/3_game/tools/jsonfileloader.c:7.
        if (!JsonFileLoader<NexoDM_Config>.LoadFile(NexoDM_Constants.CONFIG_PATH, config, errorMessage))
        {
            LogRule(NexoDM_Constants.RULE_LOAD_FAILED, errorMessage);
            return null;
        }

        if (!config)
        {
            LogRule(NexoDM_Constants.RULE_LOAD_FAILED, errorMessage);
            return null;
        }

        config.EnsureArrays();
        return config;
    }

    static NexoDM_Config CreateDefaults()
    {
        NexoDM_Config config = new NexoDM_Config();
        config.version = NexoDM_Constants.DEFAULT_VERSION;
        config.Admin.Insert(NexoDM_Constants.DEFAULT_ADMIN);
        config.RoundDuration = NexoDM_Constants.DEFAULT_ROUND_DURATION_VALUE;
        config.KillsToWin = NexoDM_Constants.DEFAULT_KILLS_TO_WIN;
        config.RespawnDelay = NexoDM_Constants.DEFAULT_RESPAWN_DELAY;
        config.MaxPlayers = NexoDM_Constants.DEFAULT_MAX_PLAYERS_VALUE;
        config.MinPlayersToScore = NexoDM_Constants.DEFAULT_MIN_PLAYERS_TO_SCORE;
        config.Knife = NexoDM_Constants.DEFAULT_KNIFE;
        config.KnifeOneHit = true;
        config.Autostart = false;
        config.AutostartDelaySeconds = NexoDM_Constants.DEFAULT_AUTOSTART_DELAY_VALUE;
        config.ClothingPreset = NexoDM_Constants.DEFAULT_CLOTHING_PRESET;
        config.Loadouts.Insert(NexoDM_Constants.DEFAULT_LOADOUT_1);
        config.Loadouts.Insert(NexoDM_Constants.DEFAULT_LOADOUT_2);
        // Literal Maps block from dev/F0-discovery.md section 2. No migration of the old coordinate list.
        AddDefaultMaps(config);
        return config;
    }

    bool Save()
    {
        if (!EnsureProfileDirectory())
            return false;

        string errorMessage;
        // R2: JsonFileLoader<T>.SaveFile(string,T,out string) in scripts/3_game/tools/jsonfileloader.c:42.
        if (!JsonFileLoader<NexoDM_Config>.SaveFile(NexoDM_Constants.CONFIG_PATH, this, errorMessage))
        {
            LogRule(NexoDM_Constants.RULE_SAVE_FAILED, errorMessage);
            return false;
        }

        return true;
    }

    bool Validate()
    {
        EnsureArrays();
        s_ValidMaps = new array<ref NexoDM_ParsedMap>();

        bool valid = true;

        if (version != NexoDM_Constants.DEFAULT_VERSION)
        {
            LogRule(NexoDM_Constants.RULE_VERSION, NexoDM_Constants.KEY_VERSION);
            valid = false;
        }

        string roundDetail = string.Empty;
        if (RoundDuration <= 0)
            roundDetail = NexoDM_Constants.KEY_ROUND_DURATION;
        if (KillsToWin <= 0)
        {
            if (roundDetail != string.Empty)
                roundDetail = roundDetail + " ";
            roundDetail = roundDetail + NexoDM_Constants.KEY_KILLS_TO_WIN;
        }
        if (roundDetail != string.Empty)
        {
            LogRule(NexoDM_Constants.RULE_ROUND, roundDetail);
            valid = false;
        }

        if (RespawnDelay < 0 || RespawnDelay > NexoDM_Constants.RESPAWN_DELAY_MAX)
        {
            LogRule(NexoDM_Constants.RULE_RESPAWN_DELAY, NexoDM_Constants.KEY_RESPAWN_DELAY);
            valid = false;
        }

        if (MaxPlayers < 2)
        {
            LogRule(NexoDM_Constants.RULE_MAX_PLAYERS_INVALID, NexoDM_Constants.KEY_MAX_PLAYERS);
            valid = false;
        }

        if (MinPlayersToScore < 1)
        {
            LogRule(NexoDM_Constants.RULE_MIN_PLAYERS, NexoDM_Constants.KEY_MIN_PLAYERS);
            valid = false;
        }
        // No comparison with MaxPlayers: it no longer caps the round (dev/memory/bug-ledger.md NDM-01).

        if (!ValidateKnife())
            valid = false;

        if (!ValidatePresetField(ClothingPreset, NexoDM_Constants.KEY_CLOTHING_PRESET))
            valid = false;

        if (!Loadouts || Loadouts.Count() == 0)
        {
            LogRule(NexoDM_Constants.RULE_PRESET, NexoDM_Constants.KEY_LOADOUTS);
            valid = false;
        }
        else
        {
            int loadoutIndex = 0;
            for (loadoutIndex = 0; loadoutIndex < Loadouts.Count(); loadoutIndex = loadoutIndex + 1)
            {
                string loadoutName = Loadouts.Get(loadoutIndex);
                if (!ValidatePresetField(loadoutName, NexoDM_Constants.KEY_LOADOUTS))
                    valid = false;
            }
        }

        if (!ValidateMaps())
            valid = false;

        return valid;
    }

    // Placeholder names (CAMBIAR_*) are logged at round start. Non-blocking: the 4_World preset resolve is what rejects the round.
    void LogPlaceholderWarnings()
    {
        EnsureArrays();

        int i = 0;
        for (i = 0; i < Admin.Count(); i = i + 1)
        {
            string adminId = Admin.Get(i);
            if (adminId == NexoDM_Constants.DEFAULT_ADMIN)
                LogPlaceholder("admin id", adminId);
        }

        if (ClothingPreset == NexoDM_Constants.DEFAULT_CLOTHING_PRESET)
            LogPlaceholder("ClothingPreset", ClothingPreset);

        for (i = 0; i < Loadouts.Count(); i = i + 1)
        {
            string loadoutName = Loadouts.Get(i);
            if (loadoutName == NexoDM_Constants.DEFAULT_LOADOUT_1 || loadoutName == NexoDM_Constants.DEFAULT_LOADOUT_2)
                LogPlaceholder("Loadout", loadoutName);
        }
    }

    protected void LogPlaceholder(string fieldName, string value)
    {
        string message = NexoDM_Constants.LOG_PREFIX;
        message = message + "PLACEHOLDER ";
        message = message + fieldName;
        message = message + " (edit config.json before use): ";
        message = message + value;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }

    array<string> GetAdmins()
    {
        EnsureArrays();
        return Admin;
    }

    string GetClothingPreset()
    {
        return ClothingPreset;
    }

    array<string> GetLoadouts()
    {
        EnsureArrays();
        return Loadouts;
    }

    string GetKnife()
    {
        return Knife;
    }

    bool GetKnifeOneHit()
    {
        return KnifeOneHit;
    }

    int GetRoundDuration()
    {
        if (RoundDuration > 0)
            return RoundDuration;
        return NexoDM_Constants.DEFAULT_ROUND_DURATION_VALUE;
    }

    int GetKillsToWin()
    {
        return KillsToWin;
    }

    int GetRespawnDelay()
    {
        return RespawnDelay;
    }

    int GetMaxPlayers()
    {
        if (MaxPlayers >= 2)
            return MaxPlayers;
        return NexoDM_Constants.DEFAULT_MAX_PLAYERS_VALUE;
    }

    int GetMinPlayersToScore()
    {
        if (MinPlayersToScore >= 1)
            return MinPlayersToScore;
        return NexoDM_Constants.DEFAULT_MIN_PLAYERS_TO_SCORE;
    }

    bool GetAutostart()
    {
        return Autostart;
    }

    // Soft-fallback: a non-positive delay logs and uses the default. It does not fail Validate().
    // Only consulted while GetAutostart() is true. F3 owns the timer.
    int GetAutostartDelaySeconds()
    {
        if (AutostartDelaySeconds > 0)
            return AutostartDelaySeconds;

        LogRule(NexoDM_Constants.RULE_AUTOSTART_DELAY_INVALID, AutostartDelaySeconds.ToString());
        return NexoDM_Constants.DEFAULT_AUTOSTART_DELAY_VALUE;
    }

    // Maps of the loaded world, filled by Validate(). Groups carry array<vector>, not the raw strings.
    array<ref NexoDM_ParsedMap> GetValidMaps()
    {
        if (!s_ValidMaps)
            s_ValidMaps = new array<ref NexoDM_ParsedMap>();
        return s_ValidMaps;
    }

    protected void EnsureArrays()
    {
        if (!Admin)
            Admin = new array<string>();
        if (!Loadouts)
            Loadouts = new array<string>();
        if (!Maps)
            Maps = new array<ref NexoDM_MapConfig>();

        int mapIndex = 0;
        for (mapIndex = 0; mapIndex < Maps.Count(); mapIndex = mapIndex + 1)
        {
            NexoDM_MapConfig mapCfg = Maps.Get(mapIndex);
            if (!mapCfg)
                continue;
            if (!mapCfg.SpawnGroups)
                mapCfg.SpawnGroups = new array<ref NexoDM_SpawnGroupConfig>();

            int groupIndex = 0;
            for (groupIndex = 0; groupIndex < mapCfg.SpawnGroups.Count(); groupIndex = groupIndex + 1)
            {
                NexoDM_SpawnGroupConfig group = mapCfg.SpawnGroups.Get(groupIndex);
                if (!group)
                    continue;
                if (!group.Points)
                    group.Points = new array<string>();
            }
        }
    }

    protected static bool EnsureProfileDirectory()
    {
        // R2: FileExist(string) in scripts/1_core/proto/ensystem.c:397; local mods use it for profile dirs before MakeDirectory.
        if (FileExist(NexoDM_Constants.PROFILE_DIR))
            return true;

        // R2: MakeDirectory(string) in scripts/1_core/proto/ensystem.c:525.
        if (!MakeDirectory(NexoDM_Constants.PROFILE_DIR))
        {
            LogRule(NexoDM_Constants.RULE_DIR_CREATE_FAILED, NexoDM_Constants.PROFILE_DIR);
            return false;
        }

        return true;
    }

    protected bool ValidatePresetField(string presetName, string fieldName)
    {
        // R2: string.Trim() in scripts/1_core/proto/enstring.c:304.
        string trimmedName = presetName.Trim();
        if (trimmedName == string.Empty)
        {
            LogRule(NexoDM_Constants.RULE_PRESET, fieldName);
            return false;
        }

        // R2: LB_PresetLoader lives in 4_World and is invisible from this Game module.
        // StartGame (NexoDM_RoundManager) resolves ClothingPreset and every Loadouts entry
        // with LB_PresetLoader.Get.GetPreset after this method returns.
        return true;
    }

    protected bool ValidateKnife()
    {
        // R2: string.Trim() in scripts/1_core/proto/enstring.c:304.
        string knifeName = Knife.Trim();
        if (knifeName == string.Empty)
        {
            LogRule(NexoDM_Constants.RULE_KNIFE_INVALID, Knife);
            return false;
        }

        string cfgPath = NexoDM_Constants.CFG_VEHICLES;
        cfgPath = cfgPath + knifeName;
        // R2: GetGame() in scripts/3_game/gameplay.c:636.
        DayZGame game = GetGame();
        if (!game)
        {
            LogRule(NexoDM_Constants.RULE_KNIFE_INVALID, knifeName);
            return false;
        }

        // R2: DayZGame.ConfigIsExisting(string) in scripts/3_game/global/game.c:611.
        if (!game.ConfigIsExisting(cfgPath))
        {
            LogRule(NexoDM_Constants.RULE_KNIFE_INVALID, knifeName);
            return false;
        }

        return true;
    }

    protected bool ValidateMaps()
    {
        if (!Maps || Maps.Count() == 0)
        {
            LogRule(NexoDM_Constants.RULE_MAPS, NexoDM_Constants.KEY_MAPS);
            return false;
        }

        bool spawnOk = true;
        int mapIndex = 0;
        for (mapIndex = 0; mapIndex < Maps.Count(); mapIndex = mapIndex + 1)
        {
            NexoDM_MapConfig mapCfg = Maps.Get(mapIndex);
            if (!mapCfg)
            {
                LogRule(NexoDM_Constants.RULE_SPAWN, NexoDM_Constants.KEY_MAPS);
                spawnOk = false;
                continue;
            }

            if (!WorldMatches(mapCfg.WorldName))
            {
                LogRule(NexoDM_Constants.RULE_MAP_OTHER_WORLD, mapCfg.Name);
                continue;
            }

            NexoDM_ParsedMap parsed = ParseMap(mapCfg);
            if (!parsed)
                spawnOk = false;
            else
                s_ValidMaps.Insert(parsed);
        }

        if (!spawnOk)
            return false;

        if (!s_ValidMaps || s_ValidMaps.Count() == 0)
        {
            LogRule(NexoDM_Constants.RULE_MAPS, NexoDM_Constants.KEY_MAPS);
            return false;
        }

        return true;
    }

    protected bool WorldMatches(string mapWorld)
    {
        // R2: GetGame() in scripts/3_game/gameplay.c:636.
        DayZGame game = GetGame();
        if (!game)
            return false;

        // R2: CGame.GetWorldName() in scripts/3_game/global/game.c:934.
        string loaded = game.GetWorldName();
        string wanted = mapWorld;
        // R2: string.ToLower() in scripts/1_core/proto/enstring.c:171. Mutates the receiver, so both are copies.
        loaded.ToLower();
        wanted.ToLower();
        if (loaded == string.Empty)
            return false;
        return loaded == wanted;
    }

    protected NexoDM_ParsedMap ParseMap(NexoDM_MapConfig mapCfg)
    {
        if (!mapCfg.SpawnGroups || mapCfg.SpawnGroups.Count() == 0)
        {
            string emptyGroups = NexoDM_Constants.KEY_SPAWN_GROUPS;
            emptyGroups = emptyGroups + " ";
            emptyGroups = emptyGroups + mapCfg.Name;
            LogRule(NexoDM_Constants.RULE_SPAWN, emptyGroups);
            return null;
        }

        NexoDM_ParsedMap parsed = new NexoDM_ParsedMap();
        parsed.Name = mapCfg.Name;
        parsed.WorldName = mapCfg.WorldName;
        parsed.SpawnRadius = mapCfg.SpawnRadius;

        bool pointsOk = true;
        int groupIndex = 0;
        for (groupIndex = 0; groupIndex < mapCfg.SpawnGroups.Count(); groupIndex = groupIndex + 1)
        {
            NexoDM_SpawnGroupConfig group = mapCfg.SpawnGroups.Get(groupIndex);
            if (!group || !group.Points || group.Points.Count() == 0)
            {
                string emptyPoints = NexoDM_Constants.KEY_POINTS;
                emptyPoints = emptyPoints + " ";
                emptyPoints = emptyPoints + mapCfg.Name;
                LogRule(NexoDM_Constants.RULE_SPAWN, emptyPoints);
                pointsOk = false;
                continue;
            }

            NexoDM_ParsedSpawnGroup parsedGroup = new NexoDM_ParsedSpawnGroup();
            parsedGroup.Name = group.Name;

            int pointIndex = 0;
            for (pointIndex = 0; pointIndex < group.Points.Count(); pointIndex = pointIndex + 1)
            {
                vector parsedPoint = vector.Zero;
                string rawPoint = group.Points.Get(pointIndex);
                if (!TryParseCoord(rawPoint, parsedPoint))
                {
                    string badPoint = NexoDM_Constants.KEY_POINTS;
                    badPoint = badPoint + " ";
                    badPoint = badPoint + rawPoint;
                    LogRule(NexoDM_Constants.RULE_SPAWN, badPoint);
                    pointsOk = false;
                }
                else
                {
                    parsedGroup.Points.Insert(parsedPoint);
                }
            }

            if (parsedGroup.Points.Count() > 0)
                parsed.SpawnGroups.Insert(parsedGroup);
        }

        if (!pointsOk)
            return null;
        if (parsed.SpawnGroups.Count() == 0)
            return null;
        return parsed;
    }

    protected bool TryParseCoord(string raw, out vector parsed)
    {
        parsed = vector.Zero;
        // R2: string.Trim() in scripts/1_core/proto/enstring.c:304.
        string value = raw.Trim();
        TStringArray parts = new TStringArray();
        // R2: string.Split(string,out array<string>) in scripts/1_core/proto/enstring.c:396.
        value.Split(NexoDM_Constants.COORD_SEPARATOR, parts);

        if (parts.Count() != 3)
            return false;

        float x = 0.0;
        float y = 0.0;
        float z = 0.0;
        // R2: string.Trim() in scripts/1_core/proto/enstring.c:304.
        string partX = parts.Get(0).Trim();
        string partY = parts.Get(1).Trim();
        string partZ = parts.Get(2).Trim();

        if (!TryParseFloat(partX, x))
            return false;
        if (!TryParseFloat(partY, y))
            return false;
        if (!TryParseFloat(partZ, z))
            return false;

        // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
        parsed = Vector(x, y, z);
        return true;
    }

    protected bool TryParseFloat(string raw, out float parsed)
    {
        parsed = 0.0;
        // R2: string.Trim() in scripts/1_core/proto/enstring.c:304.
        string value = raw.Trim();
        if (!IsFloatString(value))
            return false;

        // R2: string.ToFloat() in scripts/1_core/proto/enstring.c:46.
        parsed = value.ToFloat();
        return true;
    }

    protected bool IsFloatString(string value)
    {
        if (value == string.Empty)
            return false;

        int dotCount = 0;
        int digitCount = 0;
        int valueIndex = 0;
        // R2: string.Length()/Substring()/IndexOf() in scripts/1_core/proto/enstring.c:199/:113/:240.
        for (valueIndex = 0; valueIndex < value.Length(); valueIndex = valueIndex + 1)
        {
            string current = value.Substring(valueIndex, 1);
            if (current == NexoDM_Constants.MINUS)
            {
                if (valueIndex != 0)
                    return false;
            }
            else if (current == NexoDM_Constants.DOT)
            {
                dotCount = dotCount + 1;
                if (dotCount > 1)
                    return false;
            }
            else if (NexoDM_Constants.DIGITS.IndexOf(current) == -1)
            {
                return false;
            }
            else
            {
                digitCount = digitCount + 1;
            }
        }

        return digitCount > 0;
    }

    protected static void AddDefaultMaps(NexoDM_Config config)
    {
        NexoDM_MapConfig kamyshovo = AddMap(config, "Kamyshovo", "ChernarusPlus");
        NexoDM_SpawnGroupConfig kamyshovoEste = AddGroup(kamyshovo, "Este");
        kamyshovoEste.Points.Insert("12252.5,5.7,3513.9");
        kamyshovoEste.Points.Insert("12239.5,5.4,3539.2");
        kamyshovoEste.Points.Insert("12227.7,5.7,3510.7");
        kamyshovoEste.Points.Insert("12247.7,5.4,3529.5");
        kamyshovoEste.Points.Insert("12210.9,5.5,3545.2");
        NexoDM_SpawnGroupConfig kamyshovoCentro = AddGroup(kamyshovo, "Centro");
        kamyshovoCentro.Points.Insert("12198.4,5.5,3544.4");
        kamyshovoCentro.Points.Insert("12153.4,6.1,3501.2");
        kamyshovoCentro.Points.Insert("12178.2,6.1,3504.4");
        kamyshovoCentro.Points.Insert("12127.8,6.0,3548.5");
        kamyshovoCentro.Points.Insert("12156.1,5.8,3545.5");

        NexoDM_MapConfig vybor = AddMap(config, "Vybor", "ChernarusPlus");
        NexoDM_SpawnGroupConfig vyborSureste = AddGroup(vybor, "Sureste");
        vyborSureste.Points.Insert("3980.4,321.2,8938.5");
        vyborSureste.Points.Insert("3968.8,320.8,8935.0");
        vyborSureste.Points.Insert("3946.4,317.3,8943.9");
        vyborSureste.Points.Insert("3935.6,315.8,8947.4");
        vyborSureste.Points.Insert("3919.2,313.5,8960.0");
        NexoDM_SpawnGroupConfig vyborNoroeste = AddGroup(vybor, "Noroeste");
        vyborNoroeste.Points.Insert("3829.8,313.4,9084.7");
        vyborNoroeste.Points.Insert("3826.7,313.0,9067.5");
        vyborNoroeste.Points.Insert("3854.1,311.9,9027.2");
        vyborNoroeste.Points.Insert("3844.1,312.7,9049.2");
        vyborNoroeste.Points.Insert("3819.2,312.9,9060.7");
        NexoDM_SpawnGroupConfig vyborSuroeste = AddGroup(vybor, "Suroeste");
        vyborSuroeste.Points.Insert("3907.7,312.2,8965.3");
        vyborSuroeste.Points.Insert("3855.0,311.1,8980.3");
        vyborSuroeste.Points.Insert("3850.3,311.0,8932.0");
        vyborSuroeste.Points.Insert("3852.7,311.1,8969.0");
        vyborSuroeste.Points.Insert("3871.9,310.8,8974.5");

        NexoDM_MapConfig pavlovo = AddMap(config, "Pavlovo_Militar", "ChernarusPlus");
        NexoDM_SpawnGroupConfig pavlovoSur = AddGroup(pavlovo, "Sur");
        pavlovoSur.Points.Insert("2004.8,107.4,3320.4");
        pavlovoSur.Points.Insert("1994.2,110.0,3333.1");
        pavlovoSur.Points.Insert("2014.9,104.7,3312.1");
        pavlovoSur.Points.Insert("1978.5,113.6,3352.5");
        pavlovoSur.Points.Insert("2030.7,99.8,3302.9");
        NexoDM_SpawnGroupConfig pavlovoEste = AddGroup(pavlovo, "Este");
        pavlovoEste.Points.Insert("2102.5,103.3,3446.8");
        pavlovoEste.Points.Insert("2109.8,99.6,3428.9");
        pavlovoEste.Points.Insert("2112.8,96.6,3416.2");
        pavlovoEste.Points.Insert("2092.5,107.5,3469.6");
        pavlovoEste.Points.Insert("2112.8,92.8,3390.7");

        NexoDM_MapConfig grishino = AddMap(config, "Grishino", "ChernarusPlus");
        NexoDM_SpawnGroupConfig grishinoNoreste = AddGroup(grishino, "Noreste");
        grishinoNoreste.Points.Insert("6068.6,271.9,10314.3");
        grishinoNoreste.Points.Insert("6048.4,270.2,10318.6");
        grishinoNoreste.Points.Insert("6036.3,269.2,10321.4");
        grishinoNoreste.Points.Insert("6021.5,268.0,10326.4");
        NexoDM_SpawnGroupConfig grishinoSuroeste = AddGroup(grishino, "Suroeste");
        grishinoSuroeste.Points.Insert("5907.7,270.6,10239.9");
        grishinoSuroeste.Points.Insert("5915.6,274.2,10209.9");
        grishinoSuroeste.Points.Insert("5926.5,276.2,10202.8");
        grishinoSuroeste.Points.Insert("5896.9,271.7,10224.5");
        grishinoSuroeste.Points.Insert("5905.6,272.6,10217.5");
        NexoDM_SpawnGroupConfig grishinoNorte = AddGroup(grishino, "Norte");
        grishinoNorte.Points.Insert("5923.4,269.1,10259.4");
        grishinoNorte.Points.Insert("5982.7,266.9,10345.0");
        grishinoNorte.Points.Insert("5934.9,268.4,10274.2");
        grishinoNorte.Points.Insert("6000.4,267.0,10334.7");
        grishinoNorte.Points.Insert("5974.7,267.0,10335.3");
    }

    protected static NexoDM_MapConfig AddMap(NexoDM_Config config, string name, string world)
    {
        NexoDM_MapConfig mapCfg = new NexoDM_MapConfig();
        mapCfg.Name = name;
        mapCfg.WorldName = world;
        mapCfg.SpawnRadius = NexoDM_Constants.DEFAULT_SPAWN_RADIUS;
        config.Maps.Insert(mapCfg);
        return mapCfg;
    }

    protected static NexoDM_SpawnGroupConfig AddGroup(NexoDM_MapConfig mapCfg, string name)
    {
        NexoDM_SpawnGroupConfig group = new NexoDM_SpawnGroupConfig();
        group.Name = name;
        mapCfg.SpawnGroups.Insert(group);
        return group;
    }

    protected static bool IsUnguardedConfigRule(string ruleId)
    {
        if (ruleId == NexoDM_Constants.RULE_MAP_OTHER_WORLD)
            return true;
        // R2: string.IndexOf(string) in scripts/1_core/proto/enstring.c:240.
        if (ruleId.IndexOf("E_CFG_") == 0)
            return true;
        return false;
    }

    protected static void LogRule(string ruleId, string detail)
    {
        if (!s_LastRuleLog)
            s_LastRuleLog = new map<string, int>();

        int now = 0;
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.GetTime() in scripts/3_game/global/game.c:1510.
        if (GetGame())
            now = GetGame().GetTime();

        // E_CFG_* and W_CFG_MAP_OTHER_WORLD always print. The 30 s gate hid a second other-world map
        // and a retry of StartGame inside that window.
        if (!IsUnguardedConfigRule(ruleId))
        {
            int last = 0;
            // R2: map.Find(TKey,out TValue) in scripts/1_core/proto/enscript.c:858.
            if (s_LastRuleLog.Find(ruleId, last))
            {
                int elapsed = now - last;
                if (elapsed >= 0 && elapsed < NexoDM_Constants.LOG_RULE_INTERVAL_MS)
                    return;
            }

            // R2: map.Set(TKey,TValue) in scripts/1_core/proto/enscript.c:883.
            s_LastRuleLog.Set(ruleId, now);
        }

        string message = NexoDM_Constants.LOG_PREFIX;
        message = message + ruleId;
        message = message + " ";
        message = message + detail;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }
}
