#pragma once

/**
 * @file nm_tutorial_types.hpp
 * @brief Type definitions for the Tutorial System
 *
 * Defines the data structures for tutorials, steps, and conditions.
 * These types are designed to be data-driven, loaded from JSON files.
 */

#include <QString>
#include <QStringList>
#include <functional>
#include <optional>
#include <vector>

namespace NovelMind::editor::qt {

/**
 * @brief Placement of the callout bubble relative to the anchor
 */
enum class CalloutPlacement {
  Auto,        // Automatically choose best placement
  Top,         // Above the anchor
  Bottom,      // Below the anchor
  Left,        // To the left of anchor
  Right,       // To the right of anchor
  TopLeft,     // Above and to the left
  TopRight,    // Above and to the right
  BottomLeft,  // Below and to the left
  BottomRight, // Below and to the right
  Center       // Centered over the anchor (for full-screen messages)
};

/**
 * @brief Type of highlight effect for the anchor
 */
enum class HighlightStyle {
  None,      // No highlight (just callout)
  Spotlight, // Dim everything except the anchor (spotlight effect)
  Outline,   // Animated outline around the anchor
  Pulse,     // Gentle pulsing highlight
  Arrow      // Arrow pointing to the anchor
};

/**
 * @brief Condition type for step visibility/progression
 */
enum class ConditionType {
  Always,        // Always true
  PanelVisible,  // A specific panel is visible
  PanelFocused,  // A specific panel has focus
  ObjectSelected,// An object of specific type is selected
  EmptyState,    // A panel is in empty state
  SettingValue,  // A setting has a specific value
  Custom         // Custom callback condition
};

/**
 * @brief A condition that must be met for a step to be shown
 */
struct TutorialCondition {
  ConditionType type = ConditionType::Always;
  QString parameter;  // e.g., panel ID, setting key
  QString value;      // Expected value (for comparison conditions)
  bool invert = false; // Invert the condition result

  // For custom conditions (set programmatically, not from JSON)
  std::function<bool()> customCheck;
};

/**
 * @brief Action to perform when a step is shown or completed
 */
enum class StepActionType {
  None,           // No action
  HighlightMenu,  // Highlight a menu item
  OpenPanel,      // Open/show a panel
  FocusPanel,     // Focus a panel
  SelectObject,   // Select an object
  NavigateTo,     // Navigate to a location in the editor
  ShowHint,       // Show an inline hint
  Custom          // Custom callback action
};

/**
 * @brief An action associated with a tutorial step
 */
struct TutorialAction {
  StepActionType type = StepActionType::None;
  QString target;     // Target of the action (panel ID, object ID, etc.)
  QString parameter;  // Additional parameter

  // For custom actions (set programmatically)
  std::function<void()> customAction;
};

/**
 * @brief A single step in a tutorial
 */
struct TutorialStep {
  QString id;              // Unique step ID within the tutorial
  QString anchorId;        // Anchor point to highlight (from NMAnchorRegistry)
  QString title;           // Short title for the step
  QString content;         // Main instructional text (supports basic markdown)
  QString detailText;      // Optional expanded details

  CalloutPlacement placement = CalloutPlacement::Auto;
  HighlightStyle highlightStyle = HighlightStyle::Spotlight;

  // Conditions
  TutorialCondition showCondition;  // When to show this step
  TutorialCondition completeCondition; // When step is considered complete

  // Actions
  TutorialAction onShow;     // Action when step becomes visible
  TutorialAction onComplete; // Action when step is completed

  // Navigation
  bool allowSkip = true;     // User can skip this step
  bool autoAdvance = false;  // Auto-advance when completeCondition is met
  int autoAdvanceDelayMs = 500; // Delay before auto-advancing

  // Optional link to documentation
  QString learnMoreUrl;
  QString learnMoreLabel;
};

/**
 * @brief Tutorial category for organization in the Help menu
 */
enum class TutorialCategory {
  GettingStarted, // First-run tutorials
  Workflow,       // Workflow-specific tutorials
  Advanced,       // Advanced feature tutorials
  Tips            // Quick tips and tricks
};

/**
 * @brief Status of a tutorial
 */
enum class TutorialStatus {
  NotStarted,  // Tutorial has never been started
  InProgress,  // Tutorial is currently active
  Completed,   // Tutorial was completed
  Skipped,     // Tutorial was skipped
  Disabled     // Tutorial is disabled by user
};

/**
 * @brief Complete tutorial definition
 */
struct TutorialDefinition {
  QString id;               // Unique tutorial ID
  QString title;            // Display title
  QString description;      // Brief description
  QString iconName;         // Icon for Help menu
  TutorialCategory category = TutorialCategory::GettingStarted;

  std::vector<TutorialStep> steps;

  // Trigger conditions
  bool showOnFirstRun = false;      // Show automatically on first run
  bool showOnPanelOpen = false;     // Show when relevant panel is opened
  QString triggerPanelId;           // Panel that triggers this tutorial

  // Tags for search and filtering
  QStringList tags;

  // Estimated duration in seconds (for display)
  int estimatedDurationSeconds = 60;

  // Prerequisites (other tutorial IDs that should be completed first)
  QStringList prerequisites;
};

/**
 * @brief Progress information for a tutorial
 */
struct TutorialProgress {
  QString tutorialId;
  TutorialStatus status = TutorialStatus::NotStarted;
  int currentStepIndex = 0;
  int completedStepCount = 0;
  qint64 startedTimestamp = 0;
  qint64 completedTimestamp = 0;
  QStringList disabledStepIds; // Individual steps disabled by user
};

/**
 * @brief Convert CalloutPlacement to string
 */
inline QString placementToString(CalloutPlacement p) {
  switch (p) {
    case CalloutPlacement::Auto: return "auto";
    case CalloutPlacement::Top: return "top";
    case CalloutPlacement::Bottom: return "bottom";
    case CalloutPlacement::Left: return "left";
    case CalloutPlacement::Right: return "right";
    case CalloutPlacement::TopLeft: return "top-left";
    case CalloutPlacement::TopRight: return "top-right";
    case CalloutPlacement::BottomLeft: return "bottom-left";
    case CalloutPlacement::BottomRight: return "bottom-right";
    case CalloutPlacement::Center: return "center";
  }
  return "auto";
}

/**
 * @brief Parse CalloutPlacement from string
 */
inline CalloutPlacement placementFromString(const QString &s) {
  if (s == "top") return CalloutPlacement::Top;
  if (s == "bottom") return CalloutPlacement::Bottom;
  if (s == "left") return CalloutPlacement::Left;
  if (s == "right") return CalloutPlacement::Right;
  if (s == "top-left") return CalloutPlacement::TopLeft;
  if (s == "top-right") return CalloutPlacement::TopRight;
  if (s == "bottom-left") return CalloutPlacement::BottomLeft;
  if (s == "bottom-right") return CalloutPlacement::BottomRight;
  if (s == "center") return CalloutPlacement::Center;
  return CalloutPlacement::Auto;
}

/**
 * @brief Convert HighlightStyle to string
 */
inline QString highlightStyleToString(HighlightStyle h) {
  switch (h) {
    case HighlightStyle::None: return "none";
    case HighlightStyle::Spotlight: return "spotlight";
    case HighlightStyle::Outline: return "outline";
    case HighlightStyle::Pulse: return "pulse";
    case HighlightStyle::Arrow: return "arrow";
  }
  return "spotlight";
}

/**
 * @brief Parse HighlightStyle from string
 */
inline HighlightStyle highlightStyleFromString(const QString &s) {
  if (s == "none") return HighlightStyle::None;
  if (s == "outline") return HighlightStyle::Outline;
  if (s == "pulse") return HighlightStyle::Pulse;
  if (s == "arrow") return HighlightStyle::Arrow;
  return HighlightStyle::Spotlight;
}

/**
 * @brief Convert TutorialCategory to string
 */
inline QString categoryToString(TutorialCategory c) {
  switch (c) {
    case TutorialCategory::GettingStarted: return "getting_started";
    case TutorialCategory::Workflow: return "workflow";
    case TutorialCategory::Advanced: return "advanced";
    case TutorialCategory::Tips: return "tips";
  }
  return "getting_started";
}

/**
 * @brief Parse TutorialCategory from string
 */
inline TutorialCategory categoryFromString(const QString &s) {
  if (s == "workflow") return TutorialCategory::Workflow;
  if (s == "advanced") return TutorialCategory::Advanced;
  if (s == "tips") return TutorialCategory::Tips;
  return TutorialCategory::GettingStarted;
}

} // namespace NovelMind::editor::qt
