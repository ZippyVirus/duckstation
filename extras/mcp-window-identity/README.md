# Native MCP window identity

All top-level Qt windows show the listening MCP port, declared owner/task, and connected client label. The suffix appears only after a successful bind. Owner assignments are advisory, not exclusive locks.

Set DUCKSTATION_MCP_OWNER and DUCKSTATION_MCP_TASK at launch, or call mcp_identity with {"action":"set","owner":"Codex","task":"your task"} on the explicitly owned endpoint. Actions get and clear read or release the assignment. Client metadata never replaces the declared owner.

Windows x64 CI compiles and runs identity lifecycle/concurrency tests before building the native application. Runtime UI acceptance is still required before treating a binary as validated. The original patch is retained for reference; source changes are already applied and the build must not reapply it.
