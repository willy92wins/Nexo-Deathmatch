class LFGungame_Config
{
    ref array<string> Admin;
    string ClothingPreset;
    ref array<string> GunPresets;
    string Knife;
    string KnifeOneHit;
    string RoundDuration;
    string MaxPlayers;
    ref array<string> RespawnCoordinates;
    string RespawnRadius;
    ref array<string> PublicCoordinates;
    string PublicRespawnRadius;
    // Unsupervised 24/7 mode: when enabled ("1"/"true"), the round manager arms a StartGame
    // attempt AutostartDelaySeconds after server boot and after each round returns to idle.
    // The delay field is only consulted while Autostart is enabled.
    string Autostart;
    string AutostartDelaySeconds;

    protected static ref map<string, int> s_LastRuleLog = new map<string, int>();

    void LFGungame_Config()
    {
        Admin = new array<string>();
        GunPresets = new array<string>();
        RespawnCoordinates = new array<string>();
        PublicCoordinates = new array<string>();
    }

    static LFGungame_Config Load()
    {
        if (!EnsureProfileDirectory())
            return null;

        // R2: FileExist(string) in scripts/1_core/proto/ensystem.c:397.
        if (!FileExist(LFGungame_Constants.CONFIG_PATH))
        {
            LFGungame_Config defaultsConfig = LFGungame_Config.CreateDefaults();
            if (!defaultsConfig.Save())
                return null;
            // F-13/F-14 (WARN-3): a freshly created config is ALL placeholders, and no real admin can
            // start a round until it's edited. Warn here (first boot) so a fully-default install is
            // diagnosable even though the StartGame placeholder check is never reached in that case.
            string createdMsg = LFGungame_Constants.LOG_PREFIX;
            createdMsg = createdMsg + "created config.json with PLACEHOLDER defaults; edit $profile:LFGungame\\config.json (Admin/GunPresets/coords) before use";
            // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
            Print(createdMsg);
        }

        LFGungame_Config config = new LFGungame_Config();
        string errorMessage;
        // R2: JsonFileLoader<T>.LoadFile(string,out T,out string) in scripts/3_game/tools/jsonfileloader.c:7.
        if (!JsonFileLoader<LFGungame_Config>.LoadFile(LFGungame_Constants.CONFIG_PATH, config, errorMessage))
        {
            LogRule(LFGungame_Constants.RULE_LOAD_FAILED, errorMessage);
            return null;
        }

        config.EnsureArrays();
        return config;
    }

    static LFGungame_Config CreateDefaults()
    {
        LFGungame_Config config = new LFGungame_Config();
        config.Admin.Insert(LFGungame_Constants.DEFAULT_ADMIN_1);
        config.Admin.Insert(LFGungame_Constants.DEFAULT_ADMIN_2);
        config.ClothingPreset = LFGungame_Constants.DEFAULT_CLOTHING_PRESET;
        config.GunPresets.Insert(LFGungame_Constants.DEFAULT_GUN_PRESET_1);
        config.GunPresets.Insert(LFGungame_Constants.DEFAULT_GUN_PRESET_2);
        config.GunPresets.Insert(LFGungame_Constants.DEFAULT_GUN_PRESET_3);
        config.Knife = LFGungame_Constants.DEFAULT_KNIFE;
        config.KnifeOneHit = LFGungame_Constants.DEFAULT_KNIFE_ONE_HIT;
        config.RoundDuration = LFGungame_Constants.DEFAULT_ROUND_DURATION;
        config.MaxPlayers = LFGungame_Constants.DEFAULT_MAX_PLAYERS;
        config.RespawnCoordinates.Insert(LFGungame_Constants.DEFAULT_RESPAWN_COORD);
        config.RespawnRadius = LFGungame_Constants.DEFAULT_RESPAWN_RADIUS;
        config.PublicCoordinates.Insert(LFGungame_Constants.DEFAULT_PUBLIC_COORD);
        config.PublicRespawnRadius = LFGungame_Constants.DEFAULT_PUBLIC_RESPAWN_RADIUS;
        config.Autostart = LFGungame_Constants.DEFAULT_AUTOSTART;
        config.AutostartDelaySeconds = LFGungame_Constants.DEFAULT_AUTOSTART_DELAY;
        return config;
    }

    bool Save()
    {
        if (!EnsureProfileDirectory())
            return false;

        string errorMessage;
        // R2: JsonFileLoader<T>.SaveFile(string,T,out string) in scripts/3_game/tools/jsonfileloader.c:42.
        if (!JsonFileLoader<LFGungame_Config>.SaveFile(LFGungame_Constants.CONFIG_PATH, this, errorMessage))
        {
            LogRule(LFGungame_Constants.RULE_SAVE_FAILED, errorMessage);
            return false;
        }

        return true;
    }

    bool Validate()
    {
        EnsureArrays();

        bool valid = true;

        if (!ValidatePresetField(ClothingPreset, LFGungame_Constants.KEY_CLOTHING_PRESET))
            valid = false;

        if (!GunPresets || GunPresets.Count() == 0)
        {
            LogRule(LFGungame_Constants.RULE_GUN_PRESETS_EMPTY, LFGungame_Constants.KEY_GUN_PRESETS);
            valid = false;
        }
        else
        {
            int gunIndex = 0;
            for (gunIndex = 0; gunIndex < GunPresets.Count(); gunIndex = gunIndex + 1)
            {
                string gunPreset = GunPresets.Get(gunIndex);
                if (!ValidatePresetField(gunPreset, LFGungame_Constants.KEY_GUN_PRESETS))
                    valid = false;
            }
        }

        if (!ValidateKnife())
            valid = false;

        int roundDuration = 0;
        if (!TryParsePositiveInt(RoundDuration, roundDuration))
        {
            LogRule(LFGungame_Constants.RULE_ROUND_DURATION_INVALID, RoundDuration);
            valid = false;
        }

        int maxPlayers = 0;
        if (!TryParseInt(MaxPlayers, maxPlayers) || maxPlayers < 2)
        {
            LogRule(LFGungame_Constants.RULE_MAX_PLAYERS_INVALID, MaxPlayers);
            valid = false;
        }

        float respawnRadius = 0.0;
        if (!TryParsePositiveFloat(RespawnRadius, respawnRadius))
        {
            LogRule(LFGungame_Constants.RULE_RADIUS_INVALID, LFGungame_Constants.KEY_RESPAWN_RADIUS);
            valid = false;
        }

        float publicRadius = 0.0;
        if (!TryParsePositiveFloat(PublicRespawnRadius, publicRadius))
        {
            LogRule(LFGungame_Constants.RULE_RADIUS_INVALID, LFGungame_Constants.KEY_PUBLIC_RESPAWN_RADIUS);
            valid = false;
        }

        if (!ValidateCoordList(RespawnCoordinates, LFGungame_Constants.KEY_RESPAWN_COORDINATES))
            valid = false;

        if (!ValidateCoordList(PublicCoordinates, LFGungame_Constants.KEY_PUBLIC_COORDINATES))
            valid = false;

        return valid;
    }

    // F-13/F-14: log a warning for each config field still at its placeholder default, so an operator
    // who forgot to edit $profile:LFGungame\config.json sees why a round spawns players unarmed
    // (placeholder presets) or in the sky (placeholder coords). Non-blocking; logs each field directly
    // via LogPlaceholder (not the rate-limited LogRule, which would dedup all but the first per category).
    void LogPlaceholderWarnings()
    {
        EnsureArrays();

        int i = 0;
        for (i = 0; i < Admin.Count(); i = i + 1)
        {
            string adminId = Admin.Get(i);
            if (adminId == LFGungame_Constants.DEFAULT_ADMIN_1 || adminId == LFGungame_Constants.DEFAULT_ADMIN_2)
                LogPlaceholder("admin id", adminId);
        }

        if (ClothingPreset == LFGungame_Constants.DEFAULT_CLOTHING_PRESET)
            LogPlaceholder("ClothingPreset", ClothingPreset);

        for (i = 0; i < GunPresets.Count(); i = i + 1)
        {
            string gunName = GunPresets.Get(i);
            if (gunName == LFGungame_Constants.DEFAULT_GUN_PRESET_1 || gunName == LFGungame_Constants.DEFAULT_GUN_PRESET_2 || gunName == LFGungame_Constants.DEFAULT_GUN_PRESET_3)
                LogPlaceholder("GunPreset", gunName);
        }

        for (i = 0; i < RespawnCoordinates.Count(); i = i + 1)
        {
            if (RespawnCoordinates.Get(i) == LFGungame_Constants.DEFAULT_RESPAWN_COORD)
                LogPlaceholder("RespawnCoordinate", RespawnCoordinates.Get(i));
        }

        for (i = 0; i < PublicCoordinates.Count(); i = i + 1)
        {
            if (PublicCoordinates.Get(i) == LFGungame_Constants.DEFAULT_PUBLIC_COORD)
                LogPlaceholder("PublicCoordinate", PublicCoordinates.Get(i));
        }
    }

    // F-13/F-14 (WARN-1): direct, non-rate-limited log so EVERY placeholder field is reported at round
    // start. LogRule dedups by ruleId, which would hide all but the first placeholder per category.
    protected void LogPlaceholder(string fieldName, string value)
    {
        string message = LFGungame_Constants.LOG_PREFIX;
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

    array<string> GetGunPresets()
    {
        EnsureArrays();
        return GunPresets;
    }

    string GetKnife()
    {
        return Knife;
    }

    bool GetKnifeOneHit()
    {
        string value = KnifeOneHit;
        // R2: string.ToLower() in scripts/1_core/proto/enstring.c:171.
        value.ToLower();
        if (value == LFGungame_Constants.BOOL_TRUE)
            return true;
        if (value == LFGungame_Constants.BOOL_ONE)
            return true;
        return false;
    }

    int GetRoundDuration()
    {
        int parsed = 0;
        if (TryParsePositiveInt(RoundDuration, parsed))
            return parsed;
        return LFGungame_Constants.DEFAULT_ROUND_DURATION_VALUE;
    }

    int GetMaxPlayers()
    {
        int parsed = 0;
        if (TryParseInt(MaxPlayers, parsed) && parsed >= 2)
            return parsed;
        return LFGungame_Constants.DEFAULT_MAX_PLAYERS_VALUE;
    }

    bool GetAutostart()
    {
        string value = Autostart;
        // R2: string.ToLower() in scripts/1_core/proto/enstring.c:171.
        value.ToLower();
        if (value == LFGungame_Constants.BOOL_TRUE)
            return true;
        if (value == LFGungame_Constants.BOOL_ONE)
            return true;
        return false;
    }

    // Soft-fallback by design: an unparseable/non-positive delay logs (rate-limited) and uses the
    // default instead of failing Validate(), so a typo here can never block manual rounds or kill
    // the unsupervised loop. Only consulted while GetAutostart() is true.
    int GetAutostartDelaySeconds()
    {
        int parsed = 0;
        if (TryParsePositiveInt(AutostartDelaySeconds, parsed))
            return parsed;

        LogRule(LFGungame_Constants.RULE_AUTOSTART_DELAY_INVALID, AutostartDelaySeconds);
        return LFGungame_Constants.DEFAULT_AUTOSTART_DELAY_VALUE;
    }

    float GetRespawnRadius()
    {
        float parsed = 0.0;
        if (TryParsePositiveFloat(RespawnRadius, parsed))
            return parsed;
        return LFGungame_Constants.DEFAULT_RESPAWN_RADIUS_VALUE;
    }

    float GetPublicRespawnRadius()
    {
        float parsed = 0.0;
        if (TryParsePositiveFloat(PublicRespawnRadius, parsed))
            return parsed;
        return LFGungame_Constants.DEFAULT_PUBLIC_RESPAWN_RADIUS_VALUE;
    }

    array<vector> GetRespawnCoords()
    {
        EnsureArrays();
        return ParseCoordListWithFallback(RespawnCoordinates, LFGungame_Constants.DEFAULT_RESPAWN_COORD);
    }

    array<vector> GetPublicRespawnCoords()
    {
        EnsureArrays();
        return ParseCoordListWithFallback(PublicCoordinates, LFGungame_Constants.DEFAULT_PUBLIC_COORD);
    }

    protected void EnsureArrays()
    {
        if (!Admin)
            Admin = new array<string>();
        if (!GunPresets)
            GunPresets = new array<string>();
        if (!RespawnCoordinates)
            RespawnCoordinates = new array<string>();
        if (!PublicCoordinates)
            PublicCoordinates = new array<string>();
    }

    protected static bool EnsureProfileDirectory()
    {
        // R2: FileExist(string) in scripts/1_core/proto/ensystem.c:397; local mods use it for profile dirs before MakeDirectory.
        if (FileExist(LFGungame_Constants.PROFILE_DIR))
            return true;

        // R2: MakeDirectory(string) in scripts/1_core/proto/ensystem.c:525.
        if (!MakeDirectory(LFGungame_Constants.PROFILE_DIR))
        {
            LogRule(LFGungame_Constants.RULE_DIR_CREATE_FAILED, LFGungame_Constants.PROFILE_DIR);
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
            LogRule(LFGungame_Constants.RULE_PRESET_MISSING, fieldName);
            return false;
        }

        // Layer constraint (R20): LB_PresetLoader vive en 4_World y NO es visible en 3_Game
        // (esta clase compila en el modulo Game). La existencia del preset en LBmaster se
        // valida/guarda en el spawn (4_World): RoundManager null-checkea LB_PresetLoader.Get y
        // GetPreset() antes de cada SpawnPreset (LFGungame_RoundManager.c:751-760 / 1212-1237).
        // Aqui (3_Game) solo se valida que el nombre del preset no este vacio.
        return true;
    }

    protected bool ValidateKnife()
    {
        // R2: string.Trim() in scripts/1_core/proto/enstring.c:304.
        string knifeName = Knife.Trim();
        if (knifeName == string.Empty)
        {
            LogRule(LFGungame_Constants.RULE_KNIFE_INVALID, Knife);
            return false;
        }

        string cfgPath = LFGungame_Constants.CFG_VEHICLES;
        cfgPath = cfgPath + knifeName;
        // R2: GetGame() in scripts/3_game/gameplay.c:636.
        DayZGame game = GetGame();
        if (!game)
        {
            LogRule(LFGungame_Constants.RULE_KNIFE_INVALID, knifeName);
            return false;
        }

        // R2: DayZGame.ConfigIsExisting(string) in scripts/3_game/global/game.c:611.
        if (!game.ConfigIsExisting(cfgPath))
        {
            LogRule(LFGungame_Constants.RULE_KNIFE_INVALID, knifeName);
            return false;
        }

        return true;
    }

    protected bool ValidateCoordList(array<string> rawCoords, string fieldName)
    {
        if (!rawCoords || rawCoords.Count() == 0)
        {
            LogRule(LFGungame_Constants.RULE_COORD_INVALID, fieldName);
            return false;
        }

        bool valid = true;
        int coordIndex = 0;
        for (coordIndex = 0; coordIndex < rawCoords.Count(); coordIndex = coordIndex + 1)
        {
            vector parsed = vector.Zero;
            string rawCoord = rawCoords.Get(coordIndex);
            if (!TryParseCoord(rawCoord, parsed))
            {
                string message = fieldName;
                message = message + ":";
                message = message + rawCoord;
                LogRule(LFGungame_Constants.RULE_COORD_INVALID, message);
                valid = false;
            }
        }

        return valid;
    }

    protected array<vector> ParseCoordListWithFallback(array<string> rawCoords, string fallbackRaw)
    {
        array<vector> coords = new array<vector>();
        vector parsed = vector.Zero;

        int coordIndex = 0;
        if (rawCoords)
        {
            for (coordIndex = 0; coordIndex < rawCoords.Count(); coordIndex = coordIndex + 1)
            {
                string rawCoord = rawCoords.Get(coordIndex);
                if (TryParseCoord(rawCoord, parsed))
                    coords.Insert(parsed);
            }
        }

        if (coords.Count() == 0)
        {
            if (TryParseCoord(fallbackRaw, parsed))
                coords.Insert(parsed);
        }

        return coords;
    }

    protected bool TryParseCoord(string raw, out vector parsed)
    {
        parsed = vector.Zero;
        // R2: string.Trim() in scripts/1_core/proto/enstring.c:304.
        string value = raw.Trim();
        TStringArray parts = new TStringArray();
        // R2: string.Split(string,out array<string>) in scripts/1_core/proto/enstring.c:396.
        value.Split(LFGungame_Constants.COORD_SEPARATOR, parts);

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

    protected bool TryParsePositiveInt(string raw, out int parsed)
    {
        if (!TryParseInt(raw, parsed))
            return false;
        if (parsed <= 0)
            return false;
        return true;
    }

    protected bool TryParseInt(string raw, out int parsed)
    {
        parsed = 0;
        // R2: string.Trim() in scripts/1_core/proto/enstring.c:304.
        string value = raw.Trim();
        if (!IsIntString(value))
            return false;

        // R2: string.ToInt() in scripts/1_core/proto/enstring.c:20.
        parsed = value.ToInt();
        return true;
    }

    protected bool TryParsePositiveFloat(string raw, out float parsed)
    {
        if (!TryParseFloat(raw, parsed))
            return false;
        if (parsed <= 0.0)
            return false;
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

    protected bool IsIntString(string value)
    {
        if (value == string.Empty)
            return false;

        int digitCount = 0;
        int valueIndex = 0;
        // R2: string.Length()/Substring()/IndexOf() in scripts/1_core/proto/enstring.c:199/:113/:240.
        for (valueIndex = 0; valueIndex < value.Length(); valueIndex = valueIndex + 1)
        {
            string current = value.Substring(valueIndex, 1);
            if (current == LFGungame_Constants.MINUS)
            {
                if (valueIndex != 0)
                    return false;
            }
            else if (LFGungame_Constants.DIGITS.IndexOf(current) == -1)
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
            if (current == LFGungame_Constants.MINUS)
            {
                if (valueIndex != 0)
                    return false;
            }
            else if (current == LFGungame_Constants.DOT)
            {
                dotCount = dotCount + 1;
                if (dotCount > 1)
                    return false;
            }
            else if (LFGungame_Constants.DIGITS.IndexOf(current) == -1)
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

    protected static void LogRule(string ruleId, string detail)
    {
        if (!s_LastRuleLog)
            s_LastRuleLog = new map<string, int>();

        int now = 0;
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.GetTime() in scripts/3_game/global/game.c:1510.
        if (GetGame())
            now = GetGame().GetTime();

        int last = 0;
        // R2: map.Find(TKey,out TValue) in scripts/1_core/proto/enscript.c:858.
        if (s_LastRuleLog.Find(ruleId, last))
        {
            int elapsed = now - last;
            if (elapsed >= 0 && elapsed < LFGungame_Constants.LOG_RULE_INTERVAL_MS)
                return;
        }

        // R2: map.Set(TKey,TValue) in scripts/1_core/proto/enscript.c:883.
        s_LastRuleLog.Set(ruleId, now);

        string message = LFGungame_Constants.LOG_PREFIX;
        message = message + ruleId;
        message = message + " ";
        message = message + detail;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }
}



