class NexoDM_HudTopRow
{
    string name;
    int points;
    int kills;
    int deaths;
}

class NexoDM_HudKillLine
{
    string killerName;
    string victimName;
    string weaponClass;
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
    protected int m_Rank;

    protected ref array<ref NexoDM_HudTopRow> m_Top;
    protected ref array<ref NexoDM_HudKillLine> m_KillsFeed;

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
        m_Rank = 0;
        m_Top = new array<ref NexoDM_HudTopRow>;
        m_KillsFeed = new array<ref NexoDM_HudKillLine>;
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

    void SetRound(int secondsLeft, int killsToWin, bool active)
    {
        bool wasActive = m_RoundActive;
        if (active && !wasActive)
            m_KillsFeed.Clear();

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

    void SetSelf(int points, int kills, int deaths, int rank)
    {
        m_HasSelf = true;
        m_Points = points;
        m_Kills = kills;
        m_Deaths = deaths;
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
