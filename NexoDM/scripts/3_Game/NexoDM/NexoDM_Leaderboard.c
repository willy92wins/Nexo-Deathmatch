class NexoDM_LeaderboardEntry
{
    string steamId;
    string name;
    int points;
    int kills;
    int rounds;
}

// JSON document. Kept off NexoDM_Leaderboard so SaveFile does not write the static cache.
class NexoDM_LeaderboardFile
{
    ref array<ref NexoDM_LeaderboardEntry> entries;

    void NexoDM_LeaderboardFile()
    {
        entries = new array<ref NexoDM_LeaderboardEntry>();
    }
}

// Server global board. Load on first use. One row per SteamID.
class NexoDM_Leaderboard
{
    protected static ref NexoDM_Leaderboard s_Instance;

    protected bool m_Loaded;
    protected ref array<ref NexoDM_LeaderboardEntry> m_Entries;

    void NexoDM_Leaderboard()
    {
        m_Loaded = false;
        m_Entries = new array<ref NexoDM_LeaderboardEntry>();
    }

    static NexoDM_Leaderboard Get()
    {
#ifdef SERVER
        if (!s_Instance)
            s_Instance = new NexoDM_Leaderboard();
        s_Instance.EnsureLoaded();
        return s_Instance;
#else
        return null;
#endif
    }

    void AddRound(string steamId, string playerName, int gainedPoints, int gainedKills)
    {
        if (steamId == string.Empty)
            return;

        NexoDM_LeaderboardEntry row = FindEntry(steamId);
        if (!row)
        {
            row = new NexoDM_LeaderboardEntry();
            row.steamId = steamId;
            row.name = string.Empty;
            row.points = 0;
            row.kills = 0;
            row.rounds = 0;
            m_Entries.Insert(row);
        }

        row.points = row.points + gainedPoints;
        row.kills = row.kills + gainedKills;
        row.rounds = row.rounds + 1;
        if (playerName != string.Empty)
            row.name = playerName;
        SortEntries();
    }

    int GetCount()
    {
        if (!m_Entries)
            return 0;
        return m_Entries.Count();
    }

    NexoDM_LeaderboardEntry GetEntry(int index)
    {
        if (!m_Entries)
            return null;
        if (index < 0 || index >= m_Entries.Count())
            return null;
        return m_Entries.Get(index);
    }

    bool SaveSafe()
    {
        NexoDM_LeaderboardFile document = new NexoDM_LeaderboardFile();
        int index = 0;
        for (index = 0; index < m_Entries.Count(); index = index + 1)
            document.entries.Insert(m_Entries.Get(index));

        string errorMessage;
        // R2: JsonFileLoader<T>.SaveFile(string,T,out string) in scripts/3_game/tools/jsonfileloader.c:42.
        if (!JsonFileLoader<NexoDM_LeaderboardFile>.SaveFile(NexoDM_Constants.LEADERBOARD_TMP_PATH, document, errorMessage))
        {
            LogBoard("no se pudo escribir leaderboard.json.tmp " + errorMessage);
            return false;
        }

        // R2: FileExist(string) in scripts/1_core/proto/ensystem.c:397.
        if (FileExist(NexoDM_Constants.LEADERBOARD_PATH))
        {
            if (FileExist(NexoDM_Constants.LEADERBOARD_BAK_PATH))
            {
                // R2: DeleteFile(string) in scripts/1_core/proto/ensystem.c:528.
                if (!DeleteFile(NexoDM_Constants.LEADERBOARD_BAK_PATH))
                {
                    LogBoard("no se pudo borrar leaderboard.json.bak");
                    return false;
                }
            }

            // R2: CopyFile(string,string) in scripts/1_core/proto/ensystem.c:531.
            if (!CopyFile(NexoDM_Constants.LEADERBOARD_PATH, NexoDM_Constants.LEADERBOARD_BAK_PATH))
            {
                LogBoard("no se pudo copiar leaderboard.json a leaderboard.json.bak");
                return false;
            }

            if (!DeleteFile(NexoDM_Constants.LEADERBOARD_PATH))
            {
                LogBoard("no se pudo sustituir leaderboard.json");
                return false;
            }
        }

        if (!CopyFile(NexoDM_Constants.LEADERBOARD_TMP_PATH, NexoDM_Constants.LEADERBOARD_PATH))
        {
            LogBoard("no se pudo copiar leaderboard.json.tmp a leaderboard.json");
            if (FileExist(NexoDM_Constants.LEADERBOARD_BAK_PATH))
                CopyFile(NexoDM_Constants.LEADERBOARD_BAK_PATH, NexoDM_Constants.LEADERBOARD_PATH);
            return false;
        }

        DeleteFile(NexoDM_Constants.LEADERBOARD_TMP_PATH);
        return true;
    }

    protected void EnsureLoaded()
    {
        if (m_Loaded)
            return;
        m_Loaded = true;
        m_Entries = new array<ref NexoDM_LeaderboardEntry>();

        if (!FileExist(NexoDM_Constants.PROFILE_DIR))
        {
            // R2: MakeDirectory(string) in scripts/1_core/proto/ensystem.c:525.
            MakeDirectory(NexoDM_Constants.PROFILE_DIR);
        }

        if (!FileExist(NexoDM_Constants.LEADERBOARD_PATH))
            return;

        NexoDM_LeaderboardFile document = new NexoDM_LeaderboardFile();
        string errorMessage;
        // R2: JsonFileLoader<T>.LoadFile(string,out T,out string) in scripts/3_game/tools/jsonfileloader.c:7.
        if (!JsonFileLoader<NexoDM_LeaderboardFile>.LoadFile(NexoDM_Constants.LEADERBOARD_PATH, document, errorMessage))
        {
            QuarantineBroken(errorMessage);
            return;
        }

        if (!document || !document.entries)
        {
            QuarantineBroken(errorMessage);
            return;
        }

        int index = 0;
        for (index = 0; index < document.entries.Count(); index = index + 1)
        {
            NexoDM_LeaderboardEntry row = document.entries.Get(index);
            if (!row)
                continue;
            if (row.steamId == string.Empty)
                continue;
            m_Entries.Insert(row);
        }
        SortEntries();
    }

    protected void QuarantineBroken(string errorMessage)
    {
        if (FileExist(NexoDM_Constants.LEADERBOARD_BROKEN_PATH))
            DeleteFile(NexoDM_Constants.LEADERBOARD_BROKEN_PATH);

        bool copied = CopyFile(NexoDM_Constants.LEADERBOARD_PATH, NexoDM_Constants.LEADERBOARD_BROKEN_PATH);
        if (copied)
            DeleteFile(NexoDM_Constants.LEADERBOARD_PATH);

        string detail = "leaderboard.json ilegible, se empieza vacio";
        if (copied)
            detail = "leaderboard.json ilegible, copiado a leaderboard.json.broken";
        if (errorMessage != string.Empty)
            detail = detail + " " + errorMessage;
        LogBoard(detail);
        m_Entries = new array<ref NexoDM_LeaderboardEntry>();
    }

    protected NexoDM_LeaderboardEntry FindEntry(string steamId)
    {
        int index = 0;
        for (index = 0; index < m_Entries.Count(); index = index + 1)
        {
            NexoDM_LeaderboardEntry row = m_Entries.Get(index);
            if (row && row.steamId == steamId)
                return row;
        }
        return null;
    }

    protected void SortEntries()
    {
        int a = 0;
        for (a = 0; a < m_Entries.Count(); a = a + 1)
        {
            int best = a;
            int b = a + 1;
            for (b = a + 1; b < m_Entries.Count(); b = b + 1)
            {
                if (IsBetterGlobal(m_Entries.Get(b), m_Entries.Get(best)))
                    best = b;
            }
            if (best != a)
            {
                NexoDM_LeaderboardEntry swap = m_Entries.Get(a);
                m_Entries.Set(a, m_Entries.Get(best));
                m_Entries.Set(best, swap);
            }
        }
    }

    protected bool IsBetterGlobal(NexoDM_LeaderboardEntry candidate, NexoDM_LeaderboardEntry current)
    {
        if (!candidate)
            return false;
        if (!current)
            return true;
        if (candidate.points != current.points)
            return candidate.points > current.points;
        if (candidate.kills != current.kills)
            return candidate.kills > current.kills;
        return false;
    }

    protected void LogBoard(string detail)
    {
        string message = "[NexoDM] ";
        message = message + detail;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }
}
