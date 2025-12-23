#pragma once

/**
 * @file nm_anchor_registry.hpp
 * @brief UI Anchor Registry for Tutorial System
 *
 * Provides a centralized registry for UI anchor points that tutorials
 * can reference. Panels register stable anchor IDs with their widgets,
 * allowing the tutorial system to:
 * - Highlight specific UI elements
 * - Position callout bubbles
 * - Check visibility conditions
 */

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QWidget>
#include <functional>
#include <optional>
#include <unordered_map>

namespace NovelMind::editor::qt {

/**
 * @brief Information about a registered anchor point
 */
struct AnchorInfo {
  QString id;                          // Unique anchor ID (e.g., "timeline.playhead")
  QString displayName;                 // Human-readable name for the anchor
  QString panelId;                     // ID of the panel containing this anchor
  QPointer<QWidget> widget;            // Weak reference to the widget
  std::function<bool()> isVisible;     // Optional visibility check
  std::function<QRect()> customBounds; // Optional custom bounds (for partial highlights)
};

/**
 * @brief Central registry for UI anchor points
 *
 * This singleton manages anchor points across all editor panels.
 * Anchors are identified by stable string IDs that follow a
 * hierarchical naming convention:
 *
 * Format: <panel_name>.<component>.<subcomponent>
 * Examples:
 *   - "voice_manager.play_button"
 *   - "timeline.keyframe_area"
 *   - "inspector.property_grid"
 *
 * Thread Safety: This class is designed for use on the main Qt thread only.
 */
class NMAnchorRegistry : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static NMAnchorRegistry &instance();

  /**
   * @brief Register an anchor point
   * @param id Unique anchor ID
   * @param widget The widget to anchor to
   * @param displayName Human-readable name
   * @param panelId ID of the containing panel
   */
  void registerAnchor(const QString &id, QWidget *widget,
                      const QString &displayName = QString(),
                      const QString &panelId = QString());

  /**
   * @brief Register an anchor with custom visibility check
   * @param id Unique anchor ID
   * @param widget The widget to anchor to
   * @param isVisible Callback to check if anchor is currently valid
   * @param displayName Human-readable name
   * @param panelId ID of the containing panel
   */
  void registerAnchor(const QString &id, QWidget *widget,
                      std::function<bool()> isVisible,
                      const QString &displayName = QString(),
                      const QString &panelId = QString());

  /**
   * @brief Register an anchor with custom bounds
   * @param id Unique anchor ID
   * @param widget The widget to anchor to
   * @param customBounds Callback to get custom highlight bounds
   * @param displayName Human-readable name
   * @param panelId ID of the containing panel
   */
  void registerAnchorWithBounds(const QString &id, QWidget *widget,
                                std::function<QRect()> customBounds,
                                const QString &displayName = QString(),
                                const QString &panelId = QString());

  /**
   * @brief Unregister an anchor point
   * @param id The anchor ID to remove
   */
  void unregisterAnchor(const QString &id);

  /**
   * @brief Unregister all anchors for a specific panel
   * @param panelId The panel ID
   */
  void unregisterPanelAnchors(const QString &panelId);

  /**
   * @brief Get anchor info by ID
   * @param id The anchor ID
   * @return Anchor info if found
   */
  [[nodiscard]] std::optional<AnchorInfo> getAnchor(const QString &id) const;

  /**
   * @brief Get the widget for an anchor
   * @param id The anchor ID
   * @return The widget pointer (may be null if widget was deleted)
   */
  [[nodiscard]] QWidget *getWidget(const QString &id) const;

  /**
   * @brief Get the global screen rect for an anchor
   * @param id The anchor ID
   * @return The rect in global coordinates, or empty rect if not found/visible
   */
  [[nodiscard]] QRect getGlobalRect(const QString &id) const;

  /**
   * @brief Check if an anchor exists and its widget is valid
   * @param id The anchor ID
   * @return true if anchor is valid and widget exists
   */
  [[nodiscard]] bool isAnchorValid(const QString &id) const;

  /**
   * @brief Check if an anchor is currently visible
   * @param id The anchor ID
   * @return true if anchor is visible (widget visible + custom check passes)
   */
  [[nodiscard]] bool isAnchorVisible(const QString &id) const;

  /**
   * @brief Get all anchors for a specific panel
   * @param panelId The panel ID
   * @return List of anchor IDs
   */
  [[nodiscard]] QStringList getAnchorsForPanel(const QString &panelId) const;

  /**
   * @brief Get all registered anchor IDs
   * @return List of all anchor IDs
   */
  [[nodiscard]] QStringList getAllAnchorIds() const;

  /**
   * @brief Search for anchors matching a pattern
   * @param pattern Pattern to match (supports * wildcard)
   * @return List of matching anchor IDs
   */
  [[nodiscard]] QStringList findAnchors(const QString &pattern) const;

  /**
   * @brief Clear all registered anchors
   */
  void clear();

signals:
  /**
   * @brief Emitted when an anchor is registered
   */
  void anchorRegistered(const QString &id);

  /**
   * @brief Emitted when an anchor is unregistered
   */
  void anchorUnregistered(const QString &id);

  /**
   * @brief Emitted when an anchor's widget is destroyed
   */
  void anchorInvalidated(const QString &id);

private:
  NMAnchorRegistry();
  ~NMAnchorRegistry() override = default;

  // Prevent copying
  NMAnchorRegistry(const NMAnchorRegistry &) = delete;
  NMAnchorRegistry &operator=(const NMAnchorRegistry &) = delete;

  void onWidgetDestroyed(QObject *obj);

  std::unordered_map<QString, AnchorInfo> m_anchors;
  std::unordered_map<QWidget *, QString> m_widgetToId;
};

} // namespace NovelMind::editor::qt
