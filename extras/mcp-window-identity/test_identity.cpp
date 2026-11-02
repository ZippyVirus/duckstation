#include "mcp_window_identity.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

int main()
{
  MCPServer::WindowIdentity identity;
  assert(identity.Get().Suffix().empty());
  identity.Start(2346, "Codex", "widescreen");
  assert(identity.Get().Suffix() == " [MCP :2346 | Owner: Codex / widescreen]");
  identity.SetClient("claude-code");
  assert(identity.Get().owner == "Codex"); // Transport identity never claims ownership.
  assert(identity.Get().Suffix().find("Client: claude-code") != std::string::npos);
  identity.SetClient({});
  assert(identity.Get().owner == "Codex"); // Session end does not relinquish task assignment.
  assert(identity.Get().client.empty());
  identity.SetOwner("Claude", "Heat port");
  assert(identity.Get().Suffix() == " [MCP :2346 | Owner: Claude / Heat port]");
  identity.SetOwner({}, {});
  assert(identity.Get().Suffix() == " [MCP :2346 | Owner: unassigned]");
  assert(MCPServer::WindowIdentity::Clean(" \n Codex | [task]\t  ") == "Codex task");
  assert(MCPServer::WindowIdentity::Clean(std::string(200, 'x')).size() == 96);
  identity.Stop();
  assert(identity.Get().port == 0 && identity.Get().owner.empty() && identity.Get().Suffix().empty());
  identity.Start(2351, "Claude", "Heat");
  assert(identity.Get().Suffix().find(":2351") != std::string::npos);
  assert(identity.Get().client.empty());

  // UI snapshot must never combine port/owner/task from different generations.
  std::atomic<bool> done{false};
  std::thread writer([&]() {
    for (int i = 0; i < 100000; i++)
    {
      identity.Start(2346, "Codex", "widescreen");
      identity.Start(2351, "Claude", "Heat");
      identity.Stop();
    }
    done = true;
  });
  do
  {
    const auto snapshot = identity.Get();
    assert((snapshot.port == 2346 && snapshot.owner == "Codex" && snapshot.task == "widescreen") ||
           (snapshot.port == 2351 && snapshot.owner == "Claude" && snapshot.task == "Heat") ||
           (snapshot.port == 0 && snapshot.owner.empty() && snapshot.task.empty()));
  } while (!done);
  writer.join();
  std::cout << "Native identity lifecycle, sanitization, assignment and concurrent snapshots passed\n";
}
