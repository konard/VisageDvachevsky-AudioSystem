/**
 * @file nm_anchor_registry.cpp
 * @brief Implementation of the UI Anchor Registry
 */

#include "NovelMind/editor/qt/nm_anchor_registry.hpp"
#include <QRegularExpression>

namespace NovelMind::editor::qt {

NMAnchorRegistry &NMAnchorRegistry::instance() {
  static NMAnchorRegistry instance;
  return instance;
}

NMAnchorRegistry::NMAnchorRegistry() : QObject(nullptr) {}

void NMAnchorRegistry::registerAnchor(const QString &id, QWidget *widget,
                                      const QString &displayName,
                                      const QString &panelId) {
  registerAnchor(id, widget, nullptr, displayName, panelId);
}

void NMAnchorRegistry::registerAnchor(const QString &id, QWidget *widget,
                                      std::function<bool()> isVisible,
                                      const QString &displayName,
                                      const QString &panelId) {
  if (!widget || id.isEmpty()) {
    return;
  }

  // If this widget was previously registered, clean up
  if (m_widgetToId.contains(widget)) {
    QString oldId = m_widgetToId[widget];
    m_anchors.erase(oldId);
    m_widgetToId.erase(widget);
  }

  // If this ID was previously registered, clean up
  if (m_anchors.contains(id)) {
    QWidget *oldWidget = m_anchors[id].widget;
    if (oldWidget) {
      disconnect(oldWidget, &QObject::destroyed, this,
                 &NMAnchorRegistry::onWidgetDestroyed);
      m_widgetToId.erase(oldWidget);
    }
  }

  // Create anchor info
  AnchorInfo info;
  info.id = id;
  info.displayName = displayName.isEmpty() ? id : displayName;
  info.panelId = panelId;
  info.widget = widget;
  info.isVisible = isVisible;

  m_anchors[id] = info;
  m_widgetToId[widget] = id;

  // Connect to widget destruction
  connect(widget, &QObject::destroyed, this,
          &NMAnchorRegistry::onWidgetDestroyed);

  emit anchorRegistered(id);
}

void NMAnchorRegistry::registerAnchorWithBounds(
    const QString &id, QWidget *widget, std::function<QRect()> customBounds,
    const QString &displayName, const QString &panelId) {
  if (!widget || id.isEmpty()) {
    return;
  }

  registerAnchor(id, widget, nullptr, displayName, panelId);

  if (m_anchors.contains(id)) {
    m_anchors[id].customBounds = customBounds;
  }
}

void NMAnchorRegistry::unregisterAnchor(const QString &id) {
  auto it = m_anchors.find(id);
  if (it == m_anchors.end()) {
    return;
  }

  QWidget *widget = it->second.widget;
  if (widget) {
    disconnect(widget, &QObject::destroyed, this,
               &NMAnchorRegistry::onWidgetDestroyed);
    m_widgetToId.erase(widget);
  }

  m_anchors.erase(it);
  emit anchorUnregistered(id);
}

void NMAnchorRegistry::unregisterPanelAnchors(const QString &panelId) {
  QStringList toRemove;
  for (const auto &[id, info] : m_anchors) {
    if (info.panelId == panelId) {
      toRemove.append(id);
    }
  }

  for (const QString &id : toRemove) {
    unregisterAnchor(id);
  }
}

std::optional<AnchorInfo>
NMAnchorRegistry::getAnchor(const QString &id) const {
  auto it = m_anchors.find(id);
  if (it != m_anchors.end()) {
    return it->second;
  }
  return std::nullopt;
}

QWidget *NMAnchorRegistry::getWidget(const QString &id) const {
  auto it = m_anchors.find(id);
  if (it != m_anchors.end()) {
    return it->second.widget;
  }
  return nullptr;
}

QRect NMAnchorRegistry::getGlobalRect(const QString &id) const {
  auto it = m_anchors.find(id);
  if (it == m_anchors.end()) {
    return QRect();
  }

  const AnchorInfo &info = it->second;
  if (!info.widget) {
    return QRect();
  }

  // Use custom bounds if provided
  if (info.customBounds) {
    QRect localRect = info.customBounds();
    QPoint globalPos = info.widget->mapToGlobal(localRect.topLeft());
    return QRect(globalPos, localRect.size());
  }

  // Otherwise use the widget's full rect
  QPoint globalPos = info.widget->mapToGlobal(QPoint(0, 0));
  return QRect(globalPos, info.widget->size());
}

bool NMAnchorRegistry::isAnchorValid(const QString &id) const {
  auto it = m_anchors.find(id);
  if (it == m_anchors.end()) {
    return false;
  }
  return it->second.widget != nullptr;
}

bool NMAnchorRegistry::isAnchorVisible(const QString &id) const {
  auto it = m_anchors.find(id);
  if (it == m_anchors.end()) {
    return false;
  }

  const AnchorInfo &info = it->second;
  if (!info.widget) {
    return false;
  }

  // Check widget visibility
  if (!info.widget->isVisible()) {
    return false;
  }

  // Check custom visibility function
  if (info.isVisible && !info.isVisible()) {
    return false;
  }

  return true;
}

QStringList NMAnchorRegistry::getAnchorsForPanel(const QString &panelId) const {
  QStringList result;
  for (const auto &[id, info] : m_anchors) {
    if (info.panelId == panelId) {
      result.append(id);
    }
  }
  return result;
}

QStringList NMAnchorRegistry::getAllAnchorIds() const {
  QStringList result;
  for (const auto &[id, info] : m_anchors) {
    result.append(id);
  }
  return result;
}

QStringList NMAnchorRegistry::findAnchors(const QString &pattern) const {
  QStringList result;

  // Convert wildcard pattern to regex
  QString regexPattern = QRegularExpression::escape(pattern);
  regexPattern.replace("\\*", ".*");
  QRegularExpression regex("^" + regexPattern + "$");

  for (const auto &[id, info] : m_anchors) {
    if (regex.match(id).hasMatch()) {
      result.append(id);
    }
  }

  return result;
}

void NMAnchorRegistry::clear() {
  // Disconnect all widgets
  for (const auto &[id, info] : m_anchors) {
    if (info.widget) {
      disconnect(info.widget, &QObject::destroyed, this,
                 &NMAnchorRegistry::onWidgetDestroyed);
    }
  }

  m_anchors.clear();
  m_widgetToId.clear();
}

void NMAnchorRegistry::onWidgetDestroyed(QObject *obj) {
  auto *widget = static_cast<QWidget *>(obj);
  auto it = m_widgetToId.find(widget);
  if (it != m_widgetToId.end()) {
    QString id = it->second;
    m_widgetToId.erase(it);

    // Clear the widget pointer but keep the anchor info
    // (allows for re-registration)
    if (m_anchors.contains(id)) {
      m_anchors[id].widget = nullptr;
    }

    emit anchorInvalidated(id);
  }
}

} // namespace NovelMind::editor::qt
