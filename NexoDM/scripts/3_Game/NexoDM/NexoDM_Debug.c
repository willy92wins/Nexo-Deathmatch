// Tracing helper. F1 removed its call sites with the join prompt.
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
