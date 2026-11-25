// === GGDBG (REVERT BEFORE RELEASE) ==========================================
// Temporary tracing for the join-prompt path: server PromptConnectedPlayers /
// BroadcastOpenDialog and client OPEN_DIALOG handler / JoinMenu.Init.
// Remove this whole file AND every NexoDM_Debug.Log(...) call before release.
// Grep marker: [GGDBG]
// ============================================================================
class NexoDM_Debug
{
    static void Log(string detail)
    {
        string message = "[GGDBG] ";
        message = message + detail;
        // R2: Print(void) in scripts/1_core/proto/endebug.c:96.
        Print(message);
    }
}
