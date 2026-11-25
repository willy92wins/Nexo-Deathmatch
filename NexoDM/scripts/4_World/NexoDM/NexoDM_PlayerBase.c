modded class PlayerBase
{
    int m_GG_Role;

    void PlayerBase()
    {
        m_GG_Role = NexoDM_Role.NONE;

        string roleSyncName = "m_GG_Role";
        // R2: EntityAI.RegisterNetSyncVariableInt(string,int,int) in scripts/3_game/entities/entityai.c:2865.
        RegisterNetSyncVariableInt(roleSyncName);
    }

    // R2: PlayerBase.OnRPC(PlayerIdentity,int,ParamsReadContext) in scripts/4_world/entities/manbase/playerbase.c:5450.
    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);

        if (rpc_type == NexoDM_RPC.COUNTDOWN)
        {
            NexoDM_HandleCountdownRPC(ctx);
            return;
        }

        if (rpc_type == NexoDM_RPC.SCOREBOARD_REQ)
        {
            NexoDM_HandleScoreboardReqRPC(sender);
            return;
        }

        if (rpc_type == NexoDM_RPC.SCOREBOARD_DATA)
        {
            NexoDM_HandleScoreboardDataRPC(ctx);
            return;
        }
    }

    // R2: PlayerBase.EEKilled(Object) in scripts/4_world/entities/manbase/playerbase.c:1183.
    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);

#ifdef SERVER
        if (!NexoDM_IsDedicatedServer())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
            manager.OnPlayerKilled(this, killer);
#endif
    }

    // R2: PlayerBase.EEHitBy(TotalDamageResult,int,EntityAI,int,string,string,vector,float) in scripts/4_world/entities/manbase/playerbase.c:1224.
    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

#ifdef SERVER
        if (!NexoDM_IsDedicatedServer())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
            manager.OnPlayerDamaged(this, source);
#endif
    }

    protected void NexoDM_HandleCountdownRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (game.IsDedicatedServer())
            return;

        Param1<int> payload = new Param1<int>(0);
        // R2: Serializer.Read(void) in scripts/1_core/proto/serializer.c:58.
        if (!ctx.Read(payload))
            return;

        if (payload.param1 <= 0)
            return;

        NexoDM_HUD hud = NexoDM_HUD.Get();
        if (hud)
            hud.StartCountdown(payload.param1);
#endif
    }

    protected void NexoDM_HandleScoreboardReqRPC(PlayerIdentity sender)
    {
#ifdef SERVER
        if (!NexoDM_IsDedicatedServer())
            return;

        NexoDM_RoundManager manager = NexoDM_RoundManager.GetInstance();
        if (manager)
            manager.OnScoreboardRequest(sender);
#endif
    }

    protected void NexoDM_HandleScoreboardDataRPC(ParamsReadContext ctx)
    {
#ifndef SERVER
        // R2: GetGame() in scripts/3_game/gameplay.c:636; DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        DayZGame game = GetGame();
        if (!game)
            return;

        if (game.IsDedicatedServer())
            return;

        int count = 0;
        // R2: Serializer.Read(void) in scripts/1_core/proto/serializer.c:58.
        if (!ctx.Read(count))
            return;

        // F-21: defensive upper bound on a server-trusted count, so a buggy/forged SCOREBOARD_DATA
        // can't drive a huge alloc/loop on the client (participants are capped well below this).
        if (count < 0 || count > 256)
            return;

        array<ref NexoDM_ScoreRow> rows = new array<ref NexoDM_ScoreRow>;

        int index = 0;
        for (index = 0; index < count; index = index + 1)
        {
            string name;
            int kills;
            int assists;
            int deaths;
            if (!ctx.Read(name))
                return;
            if (!ctx.Read(kills))
                return;
            if (!ctx.Read(assists))
                return;
            if (!ctx.Read(deaths))
                return;

            NexoDM_ScoreRow row = new NexoDM_ScoreRow();
            row.name = name;
            row.kills = kills;
            row.assists = assists;
            row.deaths = deaths;
            rows.Insert(row);
        }

        NexoDM_Scoreboard board = NexoDM_Scoreboard.GetInstance();
        if (board)
            board.SetData(rows);
#endif
    }

    protected bool NexoDM_IsDedicatedServer()
    {
        DayZGame game = GetGame();
        if (!game)
            return false;

        // R2: DayZGame.IsDedicatedServer() in scripts/3_game/global/game.c:1127.
        if (!game.IsDedicatedServer())
            return false;

        return true;
    }
}
