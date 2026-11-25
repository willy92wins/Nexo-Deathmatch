// Chat announce bridge (4_World -> 5_Mission).
// RoundManager (4_World) decides what to announce but LBChatHandler.SendSimpleChatMessage
// lives in 5_Mission. This bus lives in 3_Game so both layers can reference it: RoundManager
// raises an announcement, a 5_Mission listener (registered under #ifndef LB_DISABLE_CHAT)
// forwards it to LBmaster. recipient null + global true = broadcast to everyone; a concrete
// recipient + global false = that player only (B4 "match full").

class LFGungame_AnnouncePayload : Managed
{
    PlayerIdentity recipient;
    string message;
    bool global;

    void LFGungame_AnnouncePayload(PlayerIdentity recipientIdentity, string text, bool isGlobal)
    {
        recipient = recipientIdentity;
        message = text;
        global = isGlobal;
    }
}

class LFGungame_AnnounceBus
{
    protected static ref ScriptInvoker s_Event;

    // R2: ScriptInvoker in scripts/2_gamelib/tools.c:115 (Invoke :118, Insert :120, Remove :122).
    static ScriptInvoker GetEvent()
    {
        if (!s_Event)
            s_Event = new ScriptInvoker();
        return s_Event;
    }

    // Single payload param keeps the string off ScriptInvoker's void params.
    static void Raise(PlayerIdentity recipient, string message, bool global)
    {
        LFGungame_AnnouncePayload payload = new LFGungame_AnnouncePayload(recipient, message, global);
        GetEvent().Invoke(payload);
    }
}
