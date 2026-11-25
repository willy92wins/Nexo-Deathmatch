// F3 p3: killer camera (client only). After a participant death the server sends KILLER_CAM
// with the killer's NetworkID and RespawnDelay. A local staticcamera films the killer in
// third person (or the victim's own corpse when there is no killer) for RespawnDelay
// seconds, then the client respawns through the vanilla menu flow. Body possession is
// never touched (no SelectPlayer/SelectSpectator).
// Correccion 1 (#1): this file lives in 4_World and must not reference MissionGameplay
// (5_Mission: scripts/5_mission/mission/missiongameplay.c:1, DestroyAllMenus :1158).
// It only signals a pending respawn; NexoDM_MissionGameplay (5_Mission) performs the
// vanilla GameRespawn flow (incl. DestroyAllMenus) when ConsumeRespawnIfReady() fires.
class NexoDM_KillerCam : Managed
{
    protected static ref NexoDM_KillerCam s_Instance;

    protected Camera m_Camera;
    protected int m_KillerLo;
    protected int m_KillerHi;
    protected vector m_CorpsePos;
    protected float m_TimeLeft;
    protected bool m_Active;
    protected bool m_PendingRespawn;
    protected PlayerBase m_CorpsePlayer;

    void NexoDM_KillerCam()
    {
        m_KillerLo = 0;
        m_KillerHi = 0;
        m_CorpsePos = vector.Zero;
        m_TimeLeft = 0;
        m_Active = false;
        m_PendingRespawn = false;
        m_CorpsePlayer = null;
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

    // True while the death screen must stay neutralised (camera running or respawn queued).
    bool IsSuppressingDeathScreen()
    {
        return m_Active || m_PendingRespawn;
    }

    bool IsActive()
    {
        return m_Active;
    }

    void Start(int lo, int hi, int delaySeconds, vector corpsePos)
    {
#ifndef SERVER
        CleanupCamera();
        m_PendingRespawn = false;

        m_KillerLo = lo;
        m_KillerHi = hi;
        m_CorpsePos = corpsePos;

        DayZGame gameStart = GetGame();
        if (gameStart)
        {
            // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:944.
            m_CorpsePlayer = PlayerBase.Cast(gameStart.GetPlayer());
        }

        // #4: delay 0 never respawns in the RPC frame; queue it and let
        // ConsumeRespawnIfReady() fire once the client knows it is dead.
        if (delaySeconds <= 0)
        {
            m_PendingRespawn = true;
            return;
        }

        DayZGame game = GetGame();
        if (!game)
        {
            m_PendingRespawn = true;
            return;
        }

        vector startPos = ComputeCameraPos(game);
        // R2: CGame.CreateObject(string,vector,bool) in scripts/3_game/global/game.c:691;
        // pattern in scripts/5_mission/dayzintroscene.c:62.
        Object camObject = game.CreateObject("staticcamera", startPos, true);
        m_Camera = Camera.Cast(camObject);
        if (!m_Camera)
        {
            // Never leave the client without an active camera: queue respawn instead.
            if (camObject)
            {
                // R2: CGame.ObjectDelete(Object) in scripts/3_game/global/game.c:705.
                game.ObjectDelete(camObject);
            }
            m_PendingRespawn = true;
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
        if (m_PendingRespawn && !m_Active)
            return;

        if (!m_Active)
            return;

        DayZGame game = GetGame();
        if (!game)
        {
            FinishAndRespawn();
            return;
        }

        // Correccion 3 (P1 nuevo 1/2): abort (no respawn) only if the local player
        // became a DIFFERENT non-null object, or a (manual) respawn is in progress.
        // Same object still alive (death not yet replicated) and GetPlayer()==null
        // (corpse deleted at 10s) both keep the camera running.
        if (ShouldAbortCamera(game))
        {
            Abort();
            return;
        }

        if (!m_Camera)
        {
            FinishAndRespawn();
            return;
        }

        // #2: vanilla CheckDeath (0.1s timer) may have fired before/around Start;
        // keep fade + death darkening cleared every frame while the camera runs.
        ClearDeadScreen();

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

    // #3/#4 + Correccion 3: called by NexoDM_MissionGameplay (5_Mission) every frame
    // after OnUpdate. Returns true exactly once when the respawn may fire. Aborts the
    // pending respawn (returns false forever after) only if the local player became a
    // DIFFERENT non-null object or a respawn is already in progress. Waits (returns
    // false, stays pending) while the same corpse object is still alive (death not yet
    // replicated: Object.IsAlive() in scripts/3_game/entities/object.c:517-520) or
    // CanRespawnPlayer() is false. GetPlayer()==null (corpse deleted) respawns anyway.
    bool ConsumeRespawnIfReady()
    {
#ifndef SERVER
        if (!m_PendingRespawn)
            return false;

        DayZGame game = GetGame();
        if (!game)
            return false;

        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:944.
        PlayerBase localPlayer = PlayerBase.Cast(game.GetPlayer());
        if (localPlayer && localPlayer != m_CorpsePlayer)
        {
            m_PendingRespawn = false;
            m_CorpsePlayer = null;
            return false;
        }

        // R2: CGame.GetMission() in scripts/3_game/global/game.c:1093;
        // Mission.IsPlayerRespawning() in scripts/3_game/gameplay.c:827
        // (implemented by MissionGameplay in scripts/5_mission/mission/missiongameplay.c:1702).
        Mission mission = game.GetMission();
        if (mission && mission.IsPlayerRespawning())
        {
            m_PendingRespawn = false;
            return false;
        }

        if (!localPlayer)
        {
            // Corpse object deleted (server cleanup at 10s): not another player.
            m_PendingRespawn = false;
            return true;
        }

        // R2: Object.IsAlive() in scripts/3_game/entities/object.c:517-520;
        // PlayerBase.CheckDeath uses !IsAlive() in scripts/4_world/entities/manbase/playerbase.c:7306.
        // Still alive on this same object = death not replicated yet: wait, do not cancel.
        if (localPlayer.IsAlive())
            return false;

        // R2: CGame.CanRespawnPlayer() in scripts/3_game/global/game.c:1108.
        if (!game.CanRespawnPlayer())
            return false;

        m_PendingRespawn = false;
        return true;
#else
        return false;
#endif
    }

    // Correccion 3: true only for a DIFFERENT non-null local player or a respawn
    // already in progress. Null (deleted corpse) and same-object-still-alive wait.
    protected bool ShouldAbortCamera(DayZGame game)
    {
#ifndef SERVER
        if (!game)
            return false;
        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:944.
        PlayerBase localPlayer = PlayerBase.Cast(game.GetPlayer());
        if (localPlayer && localPlayer != m_CorpsePlayer)
            return true;
        // R2: CGame.GetMission() in scripts/3_game/global/game.c:1093;
        // Mission.IsPlayerRespawning() in scripts/3_game/gameplay.c:827.
        Mission mission = game.GetMission();
        if (mission && mission.IsPlayerRespawning())
            return true;
        return false;
#else
        return false;
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
        Object target = ResolveTarget(game);
        if (target)
        {
            // #10: camera ~2m above the target's FEET (not head+2m) and 4.5m behind.
            // R2: Object.GetPosition() in scripts/3_game/entities/object.c:287.
            vector feetPos = target.GetPosition();
            // R2: Object.GetDirection() in scripts/3_game/entities/object.c:314.
            vector dir = target.GetDirection();
            float cx = feetPos[0] - (dir[0] * 4.5);
            float cz = feetPos[2] - (dir[2] * 4.5);
            // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
            return Vector(cx, feetPos[1] + 2.0, cz);
        }
        // Corpse fallback: 2m above the corpse's feet, fixed diagonal offset.
        // R2: Vector(float,float,float) in scripts/1_core/proto/enmath3d.c:18.
        return Vector(m_CorpsePos[0] + 3.0, m_CorpsePos[1] + 2.0, m_CorpsePos[2] + 3.0);
    }

    protected void ClearDeadScreen()
    {
#ifndef SERVER
        // Correccion 3 (P1 #2 resto): death while unconscious leaves the uncon vignette
        // running (OnUnconsciousUpdate Start in scripts/4_world/entities/manbase/playerbase.c:3906-3908;
        // OnUnconsciousStop skips it on COMMANDID_DEATH in playerbase.c:3830-3834), so stop
        // it while the camera is active, even if GetPlayer() is already null.
        // R2: PPERequesterBank.GetRequester(typename) in scripts/3_game/ppemanager/pperequesterbank.c:78;
        // PPERequester_UnconEffects in scripts/3_game/ppemanager/requesters/pperunconeffects.c:1;
        // PPERequesterBase.Stop(Param) in scripts/3_game/ppemanager/requesters/pperequestplatformsbase.c:44.
        PPERequester_UnconEffects unconRequester = PPERequester_UnconEffects.Cast(PPERequesterBank.GetRequester(PPERequester_UnconEffects));
        if (unconRequester)
            unconRequester.Stop();

        DayZGame game = GetGame();
        if (!game)
            return;

        // R2: CGame.GetPlayer() in scripts/3_game/global/game.c:944.
        PlayerBase player = PlayerBase.Cast(game.GetPlayer());
        if (!player)
            return;

        // R2: DayZPlayerImplement.ShowDeadScreen(bool,float) in
        // scripts/4_world/entities/dayzplayerimplement.c:802 (fade-out branch :823-830,
        // which also queues StopDeathDarkeningEffect).
        player.ShowDeadScreen(false, 0);
        // R2: DayZPlayerImplement.StopDeathDarkeningEffect() in
        // scripts/4_world/entities/dayzplayerimplement.c:835-842 (stops m_DeathEffectTimer;
        // DeathEffectTimer.Stop stops PPERequester_DeathDarkening at :38-46, whose exposure
        // -10 is in scripts/3_game/ppemanager/requesters/pperdeathdarkening.c:12-15).
        player.StopDeathDarkeningEffect();
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
        m_PendingRespawn = false;
        m_CorpsePlayer = null;
        CleanupCamera();
#endif
    }

    protected void FinishAndRespawn()
    {
#ifndef SERVER
        CleanupCamera();
        m_PendingRespawn = true;
#endif
    }
}
