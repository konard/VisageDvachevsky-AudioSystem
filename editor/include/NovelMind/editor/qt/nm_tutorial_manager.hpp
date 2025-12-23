#pragma once

/**
 * @file nm_tutorial_manager.hpp
 * @brief Tutorial Manager for Guided Onboarding System
 *
 * Central manager for the tutorial/help system. Handles:
 * - Tutorial registration and discovery
 * - Tutorial execution (start/stop/step navigation)
 * - Progress tracking and persistence
 * - Event-driven tutorial triggers
 * - Settings integration
 */

#include "nm_tutorial_types.hpp"
#include <QObject>
#include <QString>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {
class NMSettingsRegistry;
}

namespace NovelMind::editor::qt {

class NMHelpOverlay;
class NMAnchorRegistry;

/**
 * @brief Central manager for the tutorial system
 *
 * This singleton manages the lifecycle of tutorials:
 * 1. Registration: Tutorials are registered from JSON or programmatically
 * 2. Triggering: Tutorials can be triggered manually or by events
 * 3. Execution: Steps are displayed with highlights and callouts
 * 4. Tracking: Progress is persisted to settings
 *
 * Usage:
 * @code
 * auto& mgr = NMTutorialManager::instance();
 * mgr.loadTutorialsFromDirectory(":/tutorials");
 * mgr.startTutorial("getting_started");
 * @endcode
 */
class NMTutorialManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static NMTutorialManager &instance();

  /**
   * @brief Initialize the tutorial manager
   * @param overlay The help overlay widget
   * @param registry Settings registry for persistence
   */
  void initialize(NMHelpOverlay *overlay, NMSettingsRegistry *registry);

  /**
   * @brief Check if manager is initialized
   */
  [[nodiscard]] bool isInitialized() const { return m_initialized; }

  // =========================================================================
  // Tutorial Registration
  // =========================================================================

  /**
   * @brief Register a tutorial definition
   * @param tutorial The tutorial to register
   */
  void registerTutorial(const TutorialDefinition &tutorial);

  /**
   * @brief Unregister a tutorial
   * @param tutorialId The tutorial ID to remove
   */
  void unregisterTutorial(const QString &tutorialId);

  /**
   * @brief Load tutorials from a JSON file
   * @param filePath Path to the JSON file
   * @return true if loaded successfully
   */
  bool loadTutorialFromFile(const QString &filePath);

  /**
   * @brief Load all tutorials from a directory
   * @param dirPath Path to directory containing tutorial JSON files
   * @return Number of tutorials loaded
   */
  int loadTutorialsFromDirectory(const QString &dirPath);

  /**
   * @brief Get a tutorial definition by ID
   * @param tutorialId The tutorial ID
   * @return Tutorial definition if found
   */
  [[nodiscard]] std::optional<TutorialDefinition>
  getTutorial(const QString &tutorialId) const;

  /**
   * @brief Get all registered tutorials
   */
  [[nodiscard]] std::vector<TutorialDefinition> getAllTutorials() const;

  /**
   * @brief Get tutorials by category
   */
  [[nodiscard]] std::vector<TutorialDefinition>
  getTutorialsByCategory(TutorialCategory category) const;

  // =========================================================================
  // Tutorial Execution
  // =========================================================================

  /**
   * @brief Start a tutorial
   * @param tutorialId The tutorial to start
   * @param stepIndex Optional starting step (default: 0)
   * @return true if tutorial started successfully
   */
  bool startTutorial(const QString &tutorialId, int stepIndex = 0);

  /**
   * @brief Stop the current tutorial
   * @param markAsSkipped If true, marks the tutorial as skipped
   */
  void stopTutorial(bool markAsSkipped = false);

  /**
   * @brief Pause the current tutorial (hides overlay but remembers position)
   */
  void pauseTutorial();

  /**
   * @brief Resume a paused tutorial
   */
  void resumeTutorial();

  /**
   * @brief Advance to the next step
   */
  void nextStep();

  /**
   * @brief Go back to the previous step
   */
  void previousStep();

  /**
   * @brief Go to a specific step
   * @param stepIndex The step index to go to
   */
  void goToStep(int stepIndex);

  /**
   * @brief Skip the current step
   */
  void skipCurrentStep();

  /**
   * @brief Check if a tutorial is currently running
   */
  [[nodiscard]] bool isTutorialActive() const { return m_activeTutorial.has_value(); }

  /**
   * @brief Check if a tutorial is paused
   */
  [[nodiscard]] bool isTutorialPaused() const { return m_isPaused; }

  /**
   * @brief Get the active tutorial ID
   */
  [[nodiscard]] QString activeTutorialId() const;

  /**
   * @brief Get the current step index
   */
  [[nodiscard]] int currentStepIndex() const { return m_currentStepIndex; }

  /**
   * @brief Get the current step
   */
  [[nodiscard]] std::optional<TutorialStep> currentStep() const;

  /**
   * @brief Get total steps in the active tutorial
   */
  [[nodiscard]] int totalSteps() const;

  // =========================================================================
  // Progress Management
  // =========================================================================

  /**
   * @brief Get progress for a tutorial
   * @param tutorialId The tutorial ID
   * @return Progress information
   */
  [[nodiscard]] TutorialProgress getProgress(const QString &tutorialId) const;

  /**
   * @brief Check if a tutorial has been completed
   */
  [[nodiscard]] bool isTutorialCompleted(const QString &tutorialId) const;

  /**
   * @brief Check if a tutorial is disabled
   */
  [[nodiscard]] bool isTutorialDisabled(const QString &tutorialId) const;

  /**
   * @brief Disable a tutorial (don't show again)
   * @param tutorialId The tutorial ID
   * @param disabled true to disable, false to enable
   */
  void setTutorialDisabled(const QString &tutorialId, bool disabled);

  /**
   * @brief Disable a specific step within a tutorial
   * @param tutorialId The tutorial ID
   * @param stepId The step ID
   * @param disabled true to disable, false to enable
   */
  void setStepDisabled(const QString &tutorialId, const QString &stepId,
                       bool disabled);

  /**
   * @brief Reset progress for a tutorial
   * @param tutorialId The tutorial ID
   */
  void resetProgress(const QString &tutorialId);

  /**
   * @brief Reset progress for all tutorials
   */
  void resetAllProgress();

  /**
   * @brief Save progress to settings
   */
  void saveProgress();

  /**
   * @brief Load progress from settings
   */
  void loadProgress();

  // =========================================================================
  // Global Settings
  // =========================================================================

  /**
   * @brief Check if the tutorial system is enabled globally
   */
  [[nodiscard]] bool isEnabled() const { return m_globalEnabled; }

  /**
   * @brief Enable or disable the tutorial system globally
   */
  void setEnabled(bool enabled);

  /**
   * @brief Check if empty state hints are enabled
   */
  [[nodiscard]] bool areEmptyStateHintsEnabled() const {
    return m_emptyStateHintsEnabled;
  }

  /**
   * @brief Enable or disable empty state hints
   */
  void setEmptyStateHintsEnabled(bool enabled);

  // =========================================================================
  // Event Triggers
  // =========================================================================

  /**
   * @brief Notify that the editor has started (for first-run tutorials)
   */
  void onEditorStarted();

  /**
   * @brief Notify that a panel was opened
   * @param panelId The panel ID
   */
  void onPanelOpened(const QString &panelId);

  /**
   * @brief Notify that a panel entered empty state
   * @param panelId The panel ID
   * @param emptyStateType Type of empty state
   */
  void onEmptyState(const QString &panelId, const QString &emptyStateType);

  /**
   * @brief Notify that an error occurred
   * @param errorType Type of error
   * @param details Error details
   */
  void onError(const QString &errorType, const QString &details);

  /**
   * @brief Register a custom condition checker
   * @param conditionName Name of the condition
   * @param checker Function that checks the condition
   */
  void registerConditionChecker(const QString &conditionName,
                                std::function<bool()> checker);

  /**
   * @brief Register a custom action handler
   * @param actionName Name of the action
   * @param handler Function that performs the action
   */
  void registerActionHandler(const QString &actionName,
                             std::function<void(const QString &)> handler);

signals:
  /**
   * @brief Emitted when a tutorial starts
   */
  void tutorialStarted(const QString &tutorialId);

  /**
   * @brief Emitted when a tutorial stops
   */
  void tutorialStopped(const QString &tutorialId, bool completed);

  /**
   * @brief Emitted when the current step changes
   */
  void stepChanged(int stepIndex, int totalSteps);

  /**
   * @brief Emitted when a tutorial is completed
   */
  void tutorialCompleted(const QString &tutorialId);

  /**
   * @brief Emitted when tutorial progress is updated
   */
  void progressUpdated(const QString &tutorialId);

  /**
   * @brief Emitted when global enabled state changes
   */
  void enabledChanged(bool enabled);

private slots:
  void onOverlayNextClicked();
  void onOverlayBackClicked();
  void onOverlaySkipClicked();
  void onOverlayCloseClicked();
  void onOverlayDontShowAgainClicked();
  void checkAutoAdvanceConditions();

private:
  NMTutorialManager();
  ~NMTutorialManager() override;

  // Prevent copying
  NMTutorialManager(const NMTutorialManager &) = delete;
  NMTutorialManager &operator=(const NMTutorialManager &) = delete;

  void showCurrentStep();
  void hideOverlay();
  bool evaluateCondition(const TutorialCondition &condition);
  void executeAction(const TutorialAction &action);
  void updateProgress(TutorialStatus status);
  void registerDefaultSettings();
  TutorialDefinition parseTutorialJson(const QByteArray &json, bool &ok);

  // Registered tutorials
  std::unordered_map<QString, TutorialDefinition> m_tutorials;

  // Progress tracking
  std::unordered_map<QString, TutorialProgress> m_progress;

  // Active tutorial state
  std::optional<TutorialDefinition> m_activeTutorial;
  int m_currentStepIndex = 0;
  bool m_isPaused = false;

  // Global settings
  bool m_globalEnabled = true;
  bool m_emptyStateHintsEnabled = true;
  bool m_isFirstRun = true;

  // Dependencies
  NMHelpOverlay *m_overlay = nullptr;
  NMSettingsRegistry *m_settingsRegistry = nullptr;
  bool m_initialized = false;

  // Custom handlers
  std::unordered_map<QString, std::function<bool()>> m_conditionCheckers;
  std::unordered_map<QString, std::function<void(const QString &)>>
      m_actionHandlers;

  // Auto-advance timer
  QTimer *m_autoAdvanceTimer = nullptr;

  // Settings keys
  static constexpr const char *SETTING_ENABLED = "help.tutorials.enabled";
  static constexpr const char *SETTING_EMPTY_HINTS =
      "help.tutorials.empty_state_hints";
  static constexpr const char *SETTING_FIRST_RUN = "help.tutorials.first_run";
  static constexpr const char *SETTING_PROGRESS_PREFIX =
      "help.tutorials.progress.";
};

} // namespace NovelMind::editor::qt
