#pragma once

/**
 * @file nm_context_help.hpp
 * @brief Context-sensitive Help System
 *
 * Provides contextual help features that work independently of tutorials:
 * - Empty state hints for panels
 * - Inline help icons with tooltips
 * - "Learn more" links
 * - Quick tips
 */

#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <functional>

class QVBoxLayout;

namespace NovelMind::editor::qt {

/**
 * @brief Empty state widget with helpful hints
 *
 * Display this widget when a panel has no content to show.
 * Provides a friendly message and optional action buttons.
 */
class NMEmptyStateWidget : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Construct an empty state widget
   * @param parent Parent widget
   */
  explicit NMEmptyStateWidget(QWidget *parent = nullptr);
  ~NMEmptyStateWidget() override = default;

  /**
   * @brief Set the icon (emoji or icon name)
   */
  void setIcon(const QString &icon);

  /**
   * @brief Set the main title/message
   */
  void setTitle(const QString &title);

  /**
   * @brief Set the description text
   */
  void setDescription(const QString &description);

  /**
   * @brief Set the primary action button
   * @param text Button text
   * @param callback Function to call when clicked
   */
  void setPrimaryAction(const QString &text, std::function<void()> callback);

  /**
   * @brief Set the secondary action (usually a link)
   * @param text Link text
   * @param callback Function to call when clicked
   */
  void setSecondaryAction(const QString &text, std::function<void()> callback);

  /**
   * @brief Set a "Learn more" link
   * @param url URL to open
   * @param label Optional custom label
   */
  void setLearnMoreLink(const QString &url, const QString &label = QString());

  /**
   * @brief Set a related tutorial ID
   * @param tutorialId Tutorial to start when "Start tutorial" is clicked
   */
  void setRelatedTutorial(const QString &tutorialId);

signals:
  void primaryActionClicked();
  void secondaryActionClicked();
  void learnMoreClicked(const QString &url);
  void startTutorialClicked(const QString &tutorialId);

private:
  void setupUI();

  QVBoxLayout *m_layout = nullptr;
  QLabel *m_iconLabel = nullptr;
  QLabel *m_titleLabel = nullptr;
  QLabel *m_descriptionLabel = nullptr;
  QPushButton *m_primaryButton = nullptr;
  QPushButton *m_secondaryButton = nullptr;
  QPushButton *m_learnMoreButton = nullptr;
  QPushButton *m_tutorialButton = nullptr;

  QString m_learnMoreUrl;
  QString m_tutorialId;
  std::function<void()> m_primaryCallback;
  std::function<void()> m_secondaryCallback;
};

/**
 * @brief Inline help icon with tooltip
 *
 * A small "?" icon that shows help text on hover.
 */
class NMHelpIcon : public QLabel {
  Q_OBJECT

public:
  /**
   * @brief Construct a help icon
   * @param helpText Text to show in tooltip
   * @param parent Parent widget
   */
  explicit NMHelpIcon(const QString &helpText, QWidget *parent = nullptr);
  ~NMHelpIcon() override = default;

  /**
   * @brief Set the help text
   */
  void setHelpText(const QString &text);

  /**
   * @brief Set a "Learn more" URL
   */
  void setLearnMoreUrl(const QString &url);

signals:
  void clicked();

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  QString m_helpText;
  QString m_learnMoreUrl;
};

/**
 * @brief Quick tip notification
 *
 * A non-modal tip that appears briefly and fades out.
 */
class NMQuickTip : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Show a quick tip near a widget
   * @param anchor Widget to show the tip near
   * @param message Tip message
   * @param durationMs How long to show (default 3000ms)
   */
  static void show(QWidget *anchor, const QString &message,
                   int durationMs = 3000);

  /**
   * @brief Show a quick tip at a screen position
   * @param pos Global screen position
   * @param message Tip message
   * @param durationMs How long to show (default 3000ms)
   */
  static void showAt(const QPoint &pos, const QString &message,
                     int durationMs = 3000);

private:
  explicit NMQuickTip(QWidget *parent = nullptr);
  void showTip(const QString &message, int durationMs);

  QLabel *m_label = nullptr;
};

/**
 * @brief Static helper class for common empty state messages
 */
class NMEmptyStateMessages {
public:
  // Scene/Hierarchy
  static constexpr const char *EMPTY_SCENE =
      "Your scene is empty. Add characters, backgrounds, and dialogue to bring it to life.";

  static constexpr const char *EMPTY_HIERARCHY =
      "No objects in the scene. Use the + button or drag assets from the Asset Browser.";

  // Story Graph
  static constexpr const char *EMPTY_STORY_GRAPH =
      "Start building your story. Right-click to add dialogue nodes, then connect them to create flow.";

  // Timeline
  static constexpr const char *EMPTY_TIMELINE =
      "No keyframes yet. Select an object and double-click on the timeline to add animation keyframes.";

  // Localization
  static constexpr const char *EMPTY_LOCALIZATION =
      "No translatable strings found. Add dialogue to your story to populate this table.";

  static constexpr const char *MISSING_TRANSLATIONS =
      "Some strings are missing translations. Click to filter and see what needs to be translated.";

  // Voice
  static constexpr const char *EMPTY_VOICE_MANAGER =
      "No voice files linked. Add dialogue with voice tags, then link audio files here.";

  // Asset Browser
  static constexpr const char *EMPTY_ASSET_BROWSER =
      "No assets in project. Drag files here or use File > Import to add images, audio, and scripts.";

  // Console
  static constexpr const char *EMPTY_CONSOLE =
      "Console is empty. Messages, warnings, and errors will appear here during development.";

  // Build
  static constexpr const char *BUILD_READY =
      "Ready to build. Configure your target platform and click Build to export your project.";

  // Script Editor
  static constexpr const char *EMPTY_SCRIPT =
      "Create or open a script to start coding. Scripts use NMScript for custom game logic.";

  // Inspector
  static constexpr const char *NO_SELECTION =
      "Select an object to view and edit its properties.";

  NMEmptyStateMessages() = delete;
};

} // namespace NovelMind::editor::qt
