// F3 p3: killer camera (client only). After a participant death the server sends KILLER_CAM
// with the killer's NetworkID and RespawnDelay. A local staticcamera films the killer in
// third person (or the victim's own corpse when there is no killer) for RespawnDelay
// seconds, then the client respawns through the vanilla menu flow. Body possession is
// never touched (no SelectPlayer/SelectSpectator).
class NexoDM_KillerCam : Managed
{
    protected static ref NexoDM_KillerCam s_Instance;

    protected Camera m_Camera;
    protected int m_KillerLo;
    protected int m_KillerHi;
    protected vector m_CorpsePos;
    protected float m_TimeLeft;
    protected bool m_Active;

    void NexoDM_KillerCam()
    {
        m_KillerLo = 0;
        m_KillerHi = 0;
        m_CorpsePos = vector.Zero;
        m_TimeLeft = 0;
        m_Active = false;
    }

    static NexoDM_KillerCam Get()
    {
#ifndef SERVER
        if (!s_Instance)
            s_Instance = new NexoDM_KillerCam();
        return s_Instance;
#else
        return null;
#endif
    }

    void Start(int lo, int hi, int delaySeconds, vector corpsePos)
    {
#ifndef SERVER
        CleanupCamera();

        m_KillerLo = lo;
        m_KillerHi = hi;
        m_CorpsePos = corpsePos;

        // F3 p3: RespawnDelay 0 means no camera and an immediate respawn.
        if (delaySeconds <= 0)
        {
            RespawnNow();
            return;
        }

        DayZGame game = GetGame();
        if (!game)
        {
            RespawnNow();
            return;
        }

        vector startPos = ComputeCameraPos(game);
        // R2: CGame.CreateObject(string,vector,bool) in scripts/3_game/global/game.c:691;
        // pattern in scripts/5_mission/dayzintroscene.c:62.
        Object camObject = game.CreateObject("staticcamera", startPos, true);
        m_Camera = Camera.Cast(camObject);
        if (!m_Camera)
        {
            // Never leave the client without an active camera: respawn instead.
            if (camObject)
            {
                // R2: CGame.ObjectDelete(Object) in scripts/3_game/global/game.c:705.
                game.ObjectDelete(camObject);
            }
            RespawnNow();
            return;
        }

        // R2: Object.SetPosition(vector) in scripts/3_game/entities/object.c:294.
        m_Camera.SetPosition(startPos);
        // R2: Camera.SetFOV(float) in scripts/3_game/entities/camera.c:67.
        m_Camera.SetFOV(1.0);
        // R2: Camera.LookAt(vector) in scripts/3_game/entities/camera.c:80.
        m_Camera.LookAt(ComputeTargetPos(game));
        // R2: Camera.SetActive(bool) in scripts/3_game/entities/camera.c:45.
        m_Camera.SetActive(true);

        // Keep the death-screen fade from covering the camera.
        ClearDeadScreen();

        m_TimeLeft = delaySeconds;
        m_Active = true;
#endif
    }

    void OnUpdate(float timeslice)
    {
#ifndef SERVER
        if (!m_Active)
            return;

        DayZGame game = GetGame();
        if (!game)
        {
            FinishAndRespawn();
            return;
        }

        if (!m_Camera)
        {
            FinishAndRespawn();
            return;
        }

        // Re-resolve the killer every frame; if it is gone, film the corpse instead.
        vector camPos = ComputeCameraPos(game);
        // R2: Object.SetPosition(vector) in scripts/3_game/entities/object.c:294.
        m_Camera.SetPosition(camPos);
        // R2: Camera.LookAt(vector) in scripts/3_game/entities/camera.c:80.
        m_Camera.LookAt(ComputeTargetPos(game));

        m_TimeLeft = m_TimeLeft - timeslice;
        if (m_TimeLeft <= 0)
            FinishAndRespawn();
#endif
    }

    protected Object ResolveTarget(DayZGame game)
    {
#ifndef SERVER
        if (m_KillerLo == 0 && m_KillerHi == 0)
            return null;
        // R2: CGame.GetObjectByNetworkId(int,int) in scripts/3_game/global/game.c:356.
        return game.GetObjectByNetworkId(m_KillerLo, m_KillerHi);
#else
        return null;
#endif
    }

    protected vector ComputeTargetPos(DayZGame game)
    {
        Object target = ResolveTarget(game);
        if (target)
        {
            // R2: Object.GetPosition() in scripts/3_game/entities/object.c:287.
            vector targetPos = target.GetPosition();
            // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
            return Vector(targetPos[0], targetPos[1] + 1.6, targetPos[2]);
        }
        // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
        return Vector(m_CorpsePos[0], m_CorpsePos[1] + 1.6, m_CorpsePos[2]);
    }

    protected vector ComputeCameraPos(DayZGame game)
    {
        vector targetPos = ComputeTargetPos(game);
        Object target = ResolveTarget(game);
        if (target)
        {
            // Third person: behind and above, along the target's facing.
            // R2: Object.GetDirection() in scripts/3_game/entities/object.c:314.
            vector dir = target.GetDirection();
            float cx = targetPos[0] - (dir[0] * 4.5);
            float cz = targetPos[2] - (dir[2] * 4.5);
            // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
            return Vector(cx, targetPos[1] + 2.0, cz);
        }
        // Corpse fallback: fixed offset encuadre over the body.
        // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
        return Vector(targetPos[0] + 3.0, targetPos[1] + 2.0, targetPos[2] + 3.0);
    }

    protected void ClearDeadScreen()
    {
#ifndef SERVER
        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:1090.
        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (!player)
            return;

        // R2: DayZPlayerImplement.ShowDeadScreen(bool,float) in
        // scripts/4_world/entities/dayzplayerimplement.c:802 (fade-out branch :825).
        player.ShowDeadScreen(false, 0);
#endif
    }

    protected void CleanupCamera()
    {
#ifndef SERVER
        m_Active = false;
        m_TimeLeft = 0;

        if (!m_Camera)
            return;

        // R2: Camera.SetActive(bool) in scripts/3_game/entities/camera.c:45.
        m_Camera.SetActive(false);

        DayZGame game = GetGame();
        if (game)
        {
            // R2: CGame.ObjectDelete(Object) in scripts/3_game/global/game.c:705.
            game.ObjectDelete(m_Camera);
        }
        m_Camera = null;
#endif
    }

    // Mission teardown: drop the camera without respawning.
    void Abort()
    {
#ifndef SERVER
        CleanupCamera();
#endif
    }

    protected void FinishAndRespawn()
    {
#ifndef SERVER
        CleanupCamera();
        RespawnNow();
#endif
    }

    // Reproduces the vanilla menu respawn flow (GameRespawn), it does not invent another.
    protected void RespawnNow()
    {
#ifndef SERVER
        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: DayZGame.GetMenuDefaultCharacterData(bool) in scripts/3_game/global/game.c:1593;
        // MenuDefaultCharacterData.SetRandomCharacterForced(bool) in scripts/3_game/gameplay.c:1115.
        // Flow copied from GameRespawn in scripts/5_mission/gui/ingamemenu.c:345-363.
        MenuDefaultCharacterData charData = game.GetMenuDefaultCharacterData(false);
        if (charData)
            charData.SetRandomCharacterForced(false);
        // R2: CGame.RespawnPlayer() in scripts/3_game/global/game.c:1107.
        game.RespawnPlayer();

        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:1090.
        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (player)
        {
            // R2: DayZPlayerImplement.SimulateDeath(bool) in scripts/4_world/entities/dayzplayerimplement.c:844.
            player.SimulateDeath(true);
            // R2: DayZPlayerImplement.ShowDeadScreen(bool,float) in scripts/4_world/entities/dayzplayerimplement.c:802;
            // queued as in ingamemenu.c:354.
            game.GetCallQueue(CALL_CATEGORY_GUI).Call(player.ShowDeadScreen, true, 0);
        }

        MissionGameplay missionGP = MissionGameplay.Cast(game.GetMission());
        if (missionGP)
        {
            // R2: MissionGameplay.DestroyAllMenus() in scripts/5_mission/mission/missiongameplay.c:1158.
            missionGP.DestroyAllMenus();
            // R2: MissionGameplay.SetPlayerRespawning(bool) in scripts/5_mission/mission/missiongameplay.c:1697.
            missionGP.SetPlayerRespawning(true);
            // R2: MissionGameplay.Continue() in scripts/5_mission/mission/missiongameplay.c:1334.
            missionGP.Continue();
        }
#endif
    }
}
