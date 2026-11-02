// Local native DuckStation MCP extension. New code, not original emulator logic.
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace MCPServer {
// Core thread writes, Qt UI thread reads. Never expose the socket or session state.
class WindowIdentity
{
public:
  struct Snapshot
  {
    std::uint16_t port = 0;
    std::string owner;
    std::string task;
    std::string client;

    std::string Suffix() const
    {
      if (!port)
        return {};
      std::string result = " [MCP :" + std::to_string(port) + " | Owner: " +
                           (owner.empty() ? "unassigned" : owner);
      if (!task.empty())
        result += " / " + task;
      if (!client.empty())
        result += " | Client: " + client;
      return result + "]";
    }
  };

  // Labels are short, printable ASCII, without title delimiters or control codes.
  static std::string Clean(std::string_view value)
  {
    std::string result;
    for (const unsigned char ch : value)
    {
      if (result.size() == 96)
        break;
      const char c = (ch >= 32 && ch < 127 && ch != '|' && ch != '[' && ch != ']') ? ch : ' ';
      if (c != ' ' || (!result.empty() && result.back() != ' '))
        result += c;
    }
    while (!result.empty() && result.back() == ' ')
      result.pop_back();
    return result;
  }

  void Start(std::uint16_t port, std::string_view owner, std::string_view task)
  {
    std::lock_guard lock(m_mutex);
    m_state = {port, Clean(owner), Clean(task), {}};
  }

  void Stop()
  {
    std::lock_guard lock(m_mutex);
    m_state = {};
  }

  void SetOwner(std::string_view owner, std::string_view task)
  {
    std::lock_guard lock(m_mutex);
    m_state.owner = Clean(owner);
    m_state.task = Clean(task);
  }

  void SetClient(std::string_view client)
  {
    std::lock_guard lock(m_mutex);
    m_state.client = Clean(client);
  }

  Snapshot Get() const
  {
    std::lock_guard lock(m_mutex);
    return m_state;
  }

private:
  mutable std::mutex m_mutex;
  Snapshot m_state;
};
} // namespace MCPServer
