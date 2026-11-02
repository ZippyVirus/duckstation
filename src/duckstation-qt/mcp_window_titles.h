// Local native DuckStation MCP extension. Runs only on the Qt UI thread.
#pragma once

#include "core/mcp_server.h"

#ifdef ENABLE_MCP_SERVER
#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

namespace MCPWindowTitles {
class Filter final : public QObject
{
public:
  explicit Filter(QObject* parent) : QObject(parent)
  {
    qApp->installEventFilter(this);
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() { Refresh(); });
    timer->start(500);
    Refresh();
  }

protected:
  bool eventFilter(QObject* object, QEvent* event) override
  {
    if (!m_updating && (event->type() == QEvent::Show || event->type() == QEvent::WindowTitleChange))
    {
      if (auto* widget = qobject_cast<QWidget*>(object); widget && widget->isWindow())
        Decorate(widget);
    }
    return false;
  }

private:
  void Decorate(QWidget* widget)
  {
    const QString current = widget->windowTitle();
    const QString last = widget->property("_mcpDecoratedTitle").toString();
    const QString base = (current == last) ? widget->property("_mcpBaseTitle").toString() : current;
    const std::string suffix = MCPServer::GetWindowIdentity().Suffix();
    const QString decorated = base + QString::fromUtf8(suffix.data(), static_cast<qsizetype>(suffix.size()));
    widget->setProperty("_mcpBaseTitle", base);
    widget->setProperty("_mcpDecoratedTitle", decorated);
    if (current != decorated)
    {
      m_updating = true;
      widget->setWindowTitle(decorated);
      m_updating = false;
    }
  }

  void Refresh()
  {
    for (QWidget* widget : QApplication::topLevelWidgets())
      Decorate(widget);
  }

  bool m_updating = false;
};

inline void Install()
{
  // Application lifetime: survives MainWindow recreation and covers newly opened dialogs,
  // detached render windows, debugger windows, and every instance using this executable.
  static auto* filter = new Filter(qApp);
  (void)filter;
}
} // namespace MCPWindowTitles
#endif
