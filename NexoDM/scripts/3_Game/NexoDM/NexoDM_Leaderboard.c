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
            LogBoard("guardado fallido: no se pudo escribir leaderboard.json.tmp " + errorMessage);
            DeleteFile(NexoDM_Constants.LEADERBOARD_TMP_PATH);
            return false;
        }

        NexoDM_LeaderboardFile reread = new NexoDM_LeaderboardFile();
        string rereadError;
        // R2: JsonFileLoader<T>.LoadFile(string,out T,out string) in scripts/3_game/tools/jsonfileloader.c:7.
        if (!JsonFileLoader<NexoDM_LeaderboardFile>.LoadFile(NexoDM_Constants.LEADERBOARD_TMP_PATH, reread, rereadError))
        {
            LogBoard("guardado fallido: leaderboard.json.tmp no se relee " + rereadError);
            DeleteFile(NexoDM_Constants.LEADERBOARD_TMP_PATH);
            return false;
        }
        if (!reread || !reread.entries)
        {
            LogBoard("guardado fallido: leaderboard.json.tmp sin entradas");
            DeleteFile(NexoDM_Constants.LEADERBOARD_TMP_PATH);
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
                    LogBoard("guardado fallido: no se pudo renovar leaderboard.json.bak; leaderboard.json queda intacto");
                    DeleteFile(NexoDM_Constants.LEADERBOARD_TMP_PATH);
                    return false;
                }
            }

            // R2: CopyFile(string,string) in scripts/1_core/proto/ensystem.c:531.
            if (!CopyFile(NexoDM_Constants.LEADERBOARD_PATH, NexoDM_Constants.LEADERBOARD_BAK_PATH))
            {
                LogBoard("guardado fallido: no se pudo copiar leaderboard.json a leaderboard.json.bak; leaderboard.json queda intacto");
                DeleteFile(NexoDM_Constants.LEADERBOARD_TMP_PATH);
                return false;
            }

            if (!DeleteFile(NexoDM_Constants.LEADERBOARD_PATH))
            {
                LogBoard("guardado fallido: no se pudo quitar leaderboard.json; el principal queda intacto");
                DeleteFile(NexoDM_Constants.LEADERBOARD_TMP_PATH);
                return false;
            }
        }

        if (!CopyFile(NexoDM_Constants.LEADERBOARD_TMP_PATH, NexoDM_Constants.LEADERBOARD_PATH))
        {
            LogBoard("guardado fallido: no se pudo copiar leaderboard.json.tmp al principal; se borra el destino parcial");
            if (FileExist(NexoDM_Constants.LEADERBOARD_PATH))
                DeleteFile(NexoDM_Constants.LEADERBOARD_PATH);
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

        bool liveOk = false;
        if (FileExist(NexoDM_Constants.LEADERBOARD_PATH))
            liveOk = TryTakeFile(NexoDM_Constants.LEADERBOARD_PATH, true);

        if (liveOk)
            return;

        bool liveMissing = true;
        if (FileExist(NexoDM_Constants.LEADERBOARD_PATH))
            liveMissing = false;

        if (!liveMissing)
            QuarantineBroken("leaderboard.json ilegible");

        if (FileExist(NexoDM_Constants.LEADERBOARD_TMP_PATH))
        {
            if (TryTakeFile(NexoDM_Constants.LEADERBOARD_TMP_PATH, true))
            {
                LogBoard("leaderboard recuperado desde leaderboard.json.tmp");
                InstallRecovered(NexoDM_Constants.LEADERBOARD_TMP_PATH);
                return;
            }
        }

        if (FileExist(NexoDM_Constants.LEADERBOARD_BAK_PATH))
        {
            if (TryTakeFile(NexoDM_Constants.LEADERBOARD_BAK_PATH, true))
            {
                LogBoard("leaderboard recuperado desde leaderboard.json.bak");
                InstallRecovered(NexoDM_Constants.LEADERBOARD_BAK_PATH);
                return;
            }
        }

        LogBoard("leaderboard vacio: ni principal ni .tmp ni .bak legibles");
    }

    protected bool TryTakeFile(string path, bool logRejects)
    {
        NexoDM_LeaderboardFile document = new NexoDM_LeaderboardFile();
        string errorMessage;
        if (!JsonFileLoader<NexoDM_LeaderboardFile>.LoadFile(path, document, errorMessage))
            return false;
        if (!document || !document.entries)
            return false;
        AcceptEntries(document, logRejects);
        return true;
    }

    protected void AcceptEntries(NexoDM_LeaderboardFile document, bool logRejects)
    {
        m_Entries = new array<ref NexoDM_LeaderboardEntry>();
        int index = 0;
        for (index = 0; index < document.entries.Count(); index = index + 1)
        {
            NexoDM_LeaderboardEntry row = document.entries.Get(index);
            if (!row)
                continue;
            if (row.steamId == string.Empty)
            {
                if (logRejects)
                    LogBoard("entrada descartada: steamId vacio");
                continue;
            }
            if (row.points < 0 || row.kills < 0 || row.rounds < 0)
            {
                if (logRejects)
                    LogBoard("entrada descartada: numeros negativos " + row.steamId);
                continue;
            }
            if (FindEntry(row.steamId))
            {
                if (logRejects)
                    LogBoard("entrada descartada: steamId duplicado " + row.steamId);
                continue;
            }
            m_Entries.Insert(row);
        }
        SortEntries();
    }

    // Copies a recovered file onto the live path. Never deletes or overwrites .bak.
    protected void InstallRecovered(string sourcePath)
    {
        if (sourcePath == NexoDM_Constants.LEADERBOARD_PATH)
            return;
        if (FileExist(NexoDM_Constants.LEADERBOARD_PATH))
            return;
        if (!CopyFile(sourcePath, NexoDM_Constants.LEADERBOARD_PATH))
            LogBoard("recuperado en memoria, no se pudo copiar al principal");
        if (sourcePath == NexoDM_Constants.LEADERBOARD_TMP_PATH)
            DeleteFile(NexoDM_Constants.LEADERBOARD_TMP_PATH);
    }

    protected void QuarantineBroken(string errorMessage)
    {
        bool copied = false;
        if (FileExist(NexoDM_Constants.LEADERBOARD_PATH))
        {
            if (FileExist(NexoDM_Constants.LEADERBOARD_BROKEN_PATH))
                DeleteFile(NexoDM_Constants.LEADERBOARD_BROKEN_PATH);

            copied = CopyFile(NexoDM_Constants.LEADERBOARD_PATH, NexoDM_Constants.LEADERBOARD_BROKEN_PATH);
            if (copied)
                DeleteFile(NexoDM_Constants.LEADERBOARD_PATH);
            else
                DeleteFile(NexoDM_Constants.LEADERBOARD_PATH);
        }

        string detail = "leaderboard.json ilegible, apartado sin tocar leaderboard.json.bak";
        if (!copied)
            detail = "leaderboard.json ilegible, no se pudo copiar a .broken; principal retirado; .bak intacto";
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
