class NexoDM_HudTopRow
{
    string name;
    int points;
    int kills;
    int deaths;
    int assists;
}

class NexoDM_HudGlobalRow
{
    string name;
    int points;
    int kills;
    int isReceiver;
}

class NexoDM_HudKillLine
{
    string killerName;
    string victimName;
    string weaponClass;
}

class NexoDM_HudRoundResultRow
{
    string name;
    int kills;
    int deaths;
    int assists;
    int isReceiver;
}

// Client store for the F5a HUD RPCs. No widgets.
class NexoDM_HudData
{
    protected static ref NexoDM_HudData s_Instance;

    protected bool m_HasVitals;
    protected int m_Health;
    protected int m_Blood;
    protected int m_Water;
    protected int m_Food;

    protected bool m_HasRound;
    protected bool m_RoundActive;
    protected int m_RoundSecondsAtRecv;
    protected int m_RoundRecvMs;
    protected int m_KillsToWin;

    protected bool m_HasSelf;
    protected int m_Points;
    protected int m_Kills;
    protected int m_Deaths;
    protected int m_Assists;
    protected int m_Rank;

    protected ref array<ref NexoDM_HudTopRow> m_Top;
    protected ref array<ref NexoDM_HudGlobalRow> m_Global;
    protected int m_GlobalSerial;
    protected ref array<ref NexoDM_HudKillLine> m_KillsFeed;

    protected string m_RoundResultWinner;
    protected string m_RoundResultMap;
    protected int m_RoundResultKillsToWin;
    protected ref array<ref NexoDM_HudRoundResultRow> m_RoundResult;
    protected int m_RoundResultSerial;

    void NexoDM_HudData()
    {
        m_HasVitals = false;
        m_Health = 0;
        m_Blood = 0;
        m_Water = 0;
        m_Food = 0;
        m_HasRound = false;
        m_RoundActive = false;
        m_RoundSecondsAtRecv = 0;
        m_RoundRecvMs = 0;
        m_KillsToWin = 0;
        m_HasSelf = false;
        m_Points = 0;
        m_Kills = 0;
        m_Deaths = 0;
        m_Assists = 0;
        m_Rank = 0;
        m_Top = new array<ref NexoDM_HudTopRow>;
        m_Global = new array<ref NexoDM_HudGlobalRow>;
        m_GlobalSerial = 0;
        m_KillsFeed = new array<ref NexoDM_HudKillLine>;
        m_RoundResultWinner = string.Empty;
        m_RoundResultMap = string.Empty;
        m_RoundResultKillsToWin = 0;
        m_RoundResult = new array<ref NexoDM_HudRoundResultRow>;
        m_RoundResultSerial = 0;
    }

    static NexoDM_HudData Get()
    {
#ifdef SERVER
        return null;
#else
        if (!s_Instance)
            s_Instance = new NexoDM_HudData();
        return s_Instance;
#endif
    }

    void SetVitals(int health, int blood, int water, int food)
    {
        m_HasVitals = true;
        m_Health = health;
        m_Blood = blood;
        m_Water = water;
        m_Food = food;
    }

    bool HasVitals()
    {
        return m_HasVitals;
    }

    int GetHealthPercent()
    {
        return m_Health;
    }

    int GetBloodPercent()
    {
        return m_Blood;
    }

    int GetWaterPercent()
    {
        return m_Water;
    }

    int GetFoodPercent()
    {
        return m_Food;
    }

    void Reset()
    {
        m_HasVitals = false;
        m_Health = 0;
        m_Blood = 0;
        m_Water = 0;
        m_Food = 0;
        m_HasRound = false;
        m_RoundActive = false;
        m_RoundSecondsAtRecv = 0;
        m_RoundRecvMs = 0;
        m_KillsToWin = 0;
        m_HasSelf = false;
        m_Points = 0;
        m_Kills = 0;
        m_Deaths = 0;
        m_Assists = 0;
        m_Rank = 0;
        if (m_Top)
            m_Top.Clear();
        if (m_Global)
            m_Global.Clear();
        m_GlobalSerial = 0;
        if (m_KillsFeed)
            m_KillsFeed.Clear();
        m_RoundResultWinner = string.Empty;
        m_RoundResultMap = string.Empty;
        m_RoundResultKillsToWin = 0;
        if (m_RoundResult)
            m_RoundResult.Clear();
        m_RoundResultSerial = 0;
    }

    // Body removal after the round must not drop the global top before its 10 s.
    void ResetExceptGlobal()
    {
        int keptSerial = m_GlobalSerial;
        array<ref NexoDM_HudGlobalRow> keptRows = new array<ref NexoDM_HudGlobalRow>;
        if (m_Global)
        {
            int index = 0;
            for (index = 0; index < m_Global.Count(); index = index + 1)
                keptRows.Insert(m_Global.Get(index));
        }
        int keptResultSerial = m_RoundResultSerial;
        string keptWinner = m_RoundResultWinner;
        string keptMap = m_RoundResultMap;
        int keptKillsToWin = m_RoundResultKillsToWin;
        array<ref NexoDM_HudRoundResultRow> keptResult = new array<ref NexoDM_HudRoundResultRow>;
        if (m_RoundResult)
        {
            int resultIndex = 0;
            for (resultIndex = 0; resultIndex < m_RoundResult.Count(); resultIndex = resultIndex + 1)
                keptResult.Insert(m_RoundResult.Get(resultIndex));
        }
        Reset();
        m_GlobalSerial = keptSerial;
        if (!m_Global)
            m_Global = new array<ref NexoDM_HudGlobalRow>;
        int put = 0;
        for (put = 0; put < keptRows.Count(); put = put + 1)
            m_Global.Insert(keptRows.Get(put));
        m_RoundResultSerial = keptResultSerial;
        m_RoundResultWinner = keptWinner;
        m_RoundResultMap = keptMap;
        m_RoundResultKillsToWin = keptKillsToWin;
        if (!m_RoundResult)
            m_RoundResult = new array<ref NexoDM_HudRoundResultRow>;
        int putResult = 0;
        for (putResult = 0; putResult < keptResult.Count(); putResult = putResult + 1)
            m_RoundResult.Insert(keptResult.Get(putResult));
    }

    protected void ClearMatchStats()
    {
        m_HasVitals = false;
        m_Health = 0;
        m_Blood = 0;
        m_Water = 0;
        m_Food = 0;
        m_HasSelf = false;
        m_Points = 0;
        m_Kills = 0;
        m_Deaths = 0;
        m_Assists = 0;
        m_Rank = 0;
        if (m_Top)
            m_Top.Clear();
        if (m_KillsFeed)
            m_KillsFeed.Clear();
    }

    void SetRound(int secondsLeft, int killsToWin, bool active)
    {
        bool wasActive = m_RoundActive;
        int localLeft = GetRoundSecondsLeft();
        bool newRound = false;
        if (active && !wasActive)
            newRound = true;
        // Missed the inactive packet: a jump upward in the clock is a new round, not a resync.
        if (active && wasActive && secondsLeft > localLeft + 2)
            newRound = true;
        if (!active || newRound)
            ClearMatchStats();

        m_HasRound = true;
        m_RoundActive = active;
        m_RoundSecondsAtRecv = secondsLeft;
        m_KillsToWin = killsToWin;
        m_RoundRecvMs = 0;
        DayZGame game = GetGame();
        if (game)
        {
            // R2: DayZGame.GetTime() in scripts/3_game/global/game.c:1510.
            m_RoundRecvMs = game.GetTime();
        }
        if (!active)
            m_RoundSecondsAtRecv = 0;
    }

    bool HasRound()
    {
        return m_HasRound;
    }

    bool IsRoundActive()
    {
        return m_RoundActive;
    }

    int GetKillsToWin()
    {
        return m_KillsToWin;
    }

    // Local countdown from the last HUD_ROUND. The server is not asked again each second.
    int GetRoundSecondsLeft()
    {
        if (!m_HasRound)
            return 0;
        if (!m_RoundActive)
            return 0;

        DayZGame game = GetGame();
        if (!game)
            return m_RoundSecondsAtRecv;

        int elapsedMs = game.GetTime() - m_RoundRecvMs;
        if (elapsedMs < 0)
            elapsedMs = 0;
        int left = m_RoundSecondsAtRecv - (elapsedMs / 1000);
        if (left < 0)
            return 0;
        return left;
    }

    void SetSelf(int points, int kills, int deaths, int assists, int rank)
    {
        m_HasSelf = true;
        m_Points = points;
        m_Kills = kills;
        m_Deaths = deaths;
        m_Assists = assists;
        m_Rank = rank;
    }

    bool HasSelf()
    {
        return m_HasSelf;
    }

    int GetPoints()
    {
        return m_Points;
    }

    int GetKills()
    {
        return m_Kills;
    }

    int GetDeaths()
    {
        return m_Deaths;
    }

    int GetAssists()
    {
        return m_Assists;
    }

    int GetRank()
    {
        return m_Rank;
    }

    void SetTop(array<ref NexoDM_HudTopRow> rows)
    {
        m_Top.Clear();
        if (!rows)
            return;
        int index = 0;
        for (index = 0; index < rows.Count(); index = index + 1)
            m_Top.Insert(rows.Get(index));
    }

    int GetTopCount()
    {
        if (!m_Top)
            return 0;
        return m_Top.Count();
    }

    string GetTopName(int index)
    {
        NexoDM_HudTopRow row = TopRow(index);
        if (!row)
            return string.Empty;
        return row.name;
    }

    int GetTopPoints(int index)
    {
        NexoDM_HudTopRow row = TopRow(index);
        if (!row)
            return 0;
        return row.points;
    }

    int GetTopKills(int index)
    {
        NexoDM_HudTopRow row = TopRow(index);
        if (!row)
            return 0;
        return row.kills;
    }

    int GetTopDeaths(int index)
    {
        NexoDM_HudTopRow row = TopRow(index);
        if (!row)
            return 0;
        return row.deaths;
    }

    int GetTopAssists(int index)
    {
        NexoDM_HudTopRow row = TopRow(index);
        if (!row)
            return 0;
        return row.assists;
    }

    void SetGlobal(array<ref NexoDM_HudGlobalRow> rows)
    {
        m_Global.Clear();
        if (rows)
        {
            int index = 0;
            for (index = 0; index < rows.Count(); index = index + 1)
                m_Global.Insert(rows.Get(index));
        }
        m_GlobalSerial = m_GlobalSerial + 1;
    }

    int GetGlobalSerial()
    {
        return m_GlobalSerial;
    }

    int GetGlobalCount()
    {
        if (!m_Global)
            return 0;
        return m_Global.Count();
    }

    string GetGlobalName(int index)
    {
        NexoDM_HudGlobalRow row = GlobalRow(index);
        if (!row)
            return string.Empty;
        return row.name;
    }

    int GetGlobalPoints(int index)
    {
        NexoDM_HudGlobalRow row = GlobalRow(index);
        if (!row)
            return 0;
        return row.points;
    }

    int GetGlobalKills(int index)
    {
        NexoDM_HudGlobalRow row = GlobalRow(index);
        if (!row)
            return 0;
        return row.kills;
    }

    int GetGlobalIsReceiver(int index)
    {
        NexoDM_HudGlobalRow row = GlobalRow(index);
        if (!row)
            return 0;
        return row.isReceiver;
    }

    void SetRoundResult(string winnerName, string mapName, int killsToWin, array<ref NexoDM_HudRoundResultRow> rows)
    {
        m_RoundResultWinner = winnerName;
        m_RoundResultMap = mapName;
        m_RoundResultKillsToWin = killsToWin;
        if (!m_RoundResult)
            m_RoundResult = new array<ref NexoDM_HudRoundResultRow>;
        m_RoundResult.Clear();
        if (rows)
        {
            int index = 0;
            for (index = 0; index < rows.Count(); index = index + 1)
                m_RoundResult.Insert(rows.Get(index));
        }
        m_RoundResultSerial = m_RoundResultSerial + 1;
    }

    int GetRoundResultSerial()
    {
        return m_RoundResultSerial;
    }

    string GetRoundResultWinner()
    {
        return m_RoundResultWinner;
    }

    string GetRoundResultMap()
    {
        return m_RoundResultMap;
    }

    int GetRoundResultKillsToWin()
    {
        return m_RoundResultKillsToWin;
    }

    int GetRoundResultCount()
    {
        if (!m_RoundResult)
            return 0;
        return m_RoundResult.Count();
    }

    string GetRoundResultName(int index)
    {
        NexoDM_HudRoundResultRow row = RoundResultRow(index);
        if (!row)
            return string.Empty;
        return row.name;
    }

    int GetRoundResultKills(int index)
    {
        NexoDM_HudRoundResultRow row = RoundResultRow(index);
        if (!row)
            return 0;
        return row.kills;
    }

    int GetRoundResultDeaths(int index)
    {
        NexoDM_HudRoundResultRow row = RoundResultRow(index);
        if (!row)
            return 0;
        return row.deaths;
    }

    int GetRoundResultAssists(int index)
    {
        NexoDM_HudRoundResultRow row = RoundResultRow(index);
        if (!row)
            return 0;
        return row.assists;
    }

    int GetRoundResultIsReceiver(int index)
    {
        NexoDM_HudRoundResultRow row = RoundResultRow(index);
        if (!row)
            return 0;
        return row.isReceiver;
    }

    void AddKill(string killerName, string victimName, string weaponClass)
    {
        NexoDM_HudKillLine line = new NexoDM_HudKillLine();
        line.killerName = killerName;
        line.victimName = victimName;
        line.weaponClass = weaponClass;
        m_KillsFeed.Insert(line);
    }

    int GetKillFeedCount()
    {
        if (!m_KillsFeed)
            return 0;
        return m_KillsFeed.Count();
    }

    string GetKillKiller(int index)
    {
        NexoDM_HudKillLine line = KillLine(index);
        if (!line)
            return string.Empty;
        return line.killerName;
    }

    string GetKillVictim(int index)
    {
        NexoDM_HudKillLine line = KillLine(index);
        if (!line)
            return string.Empty;
        return line.victimName;
    }

    string GetKillWeapon(int index)
    {
        NexoDM_HudKillLine line = KillLine(index);
        if (!line)
            return string.Empty;
        return line.weaponClass;
    }

    protected NexoDM_HudRoundResultRow RoundResultRow(int index)
    {
        if (!m_RoundResult)
            return null;
        if (index < 0 || index >= m_RoundResult.Count())
            return null;
        return m_RoundResult.Get(index);
    }

    protected NexoDM_HudGlobalRow GlobalRow(int index)
    {
        if (!m_Global)
            return null;
        if (index < 0 || index >= m_Global.Count())
            return null;
        return m_Global.Get(index);
    }

    protected NexoDM_HudTopRow TopRow(int index)
    {
        if (!m_Top)
            return null;
        if (index < 0 || index >= m_Top.Count())
            return null;
        return m_Top.Get(index);
    }

    protected NexoDM_HudKillLine KillLine(int index)
    {
        if (!m_KillsFeed)
            return null;
        if (index < 0 || index >= m_KillsFeed.Count())
            return null;
        return m_KillsFeed.Get(index);
    }
}
