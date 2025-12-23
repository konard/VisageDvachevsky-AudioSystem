/**
 * @file nm_tutorial_manager.cpp
 * @brief Implementation of the Tutorial Manager
 */

#include "NovelMind/editor/qt/nm_tutorial_manager.hpp"
#include "NovelMind/editor/qt/nm_anchor_registry.hpp"
#include "NovelMind/editor/qt/nm_help_overlay.hpp"
#include "NovelMind/editor/settings_registry.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace NovelMind::editor::qt {

NMTutorialManager &NMTutorialManager::instance() {
  static NMTutorialManager instance;
  return instance;
}

NMTutorialManager::NMTutorialManager() : QObject(nullptr) {
  m_autoAdvanceTimer = new QTimer(this);
  m_autoAdvanceTimer->setSingleShot(true);
  connect(m_autoAdvanceTimer, &QTimer::timeout, this,
          &NMTutorialManager::checkAutoAdvanceConditions);
}

NMTutorialManager::~NMTutorialManager() {
  if (m_initialized) {
    saveProgress();
  }
}

void NMTutorialManager::initialize(NMHelpOverlay *overlay,
                                   NMSettingsRegistry *registry) {
  if (m_initialized) {
    return;
  }

  m_overlay = overlay;
  m_settingsRegistry = registry;

  // Connect overlay signals
  if (m_overlay) {
    connect(m_overlay, &NMHelpOverlay::nextClicked, this,
            &NMTutorialManager::onOverlayNextClicked);
    connect(m_overlay, &NMHelpOverlay::backClicked, this,
            &NMTutorialManager::onOverlayBackClicked);
    connect(m_overlay, &NMHelpOverlay::skipClicked, this,
            &NMTutorialManager::onOverlaySkipClicked);
    connect(m_overlay, &NMHelpOverlay::closeClicked, this,
            &NMTutorialManager::onOverlayCloseClicked);
    connect(m_overlay, &NMHelpOverlay::dontShowAgainClicked, this,
            &NMTutorialManager::onOverlayDontShowAgainClicked);
  }

  // Register default settings
  registerDefaultSettings();

  // Load settings
  if (m_settingsRegistry) {
    m_globalEnabled =
        m_settingsRegistry->getBool(SETTING_ENABLED, true);
    m_emptyStateHintsEnabled =
        m_settingsRegistry->getBool(SETTING_EMPTY_HINTS, true);
    m_isFirstRun =
        m_settingsRegistry->getBool(SETTING_FIRST_RUN, true);
  }

  // Load progress
  loadProgress();

  m_initialized = true;
}

void NMTutorialManager::registerDefaultSettings() {
  if (!m_settingsRegistry) {
    return;
  }

  // Enable Guided Help (global toggle)
  SettingDefinition enabledDef;
  enabledDef.key = SETTING_ENABLED;
  enabledDef.displayName = "Enable Guided Help";
  enabledDef.description = "Show tutorial guides and contextual help";
  enabledDef.category = "Editor/Help";
  enabledDef.type = SettingType::Bool;
  enabledDef.scope = SettingScope::User;
  enabledDef.defaultValue = true;
  m_settingsRegistry->registerSetting(enabledDef);

  // Show Empty State Hints
  SettingDefinition hintsDef;
  hintsDef.key = SETTING_EMPTY_HINTS;
  hintsDef.displayName = "Show Tips on Empty States";
  hintsDef.description = "Display helpful hints when panels are empty";
  hintsDef.category = "Editor/Help";
  hintsDef.type = SettingType::Bool;
  hintsDef.scope = SettingScope::User;
  hintsDef.defaultValue = true;
  m_settingsRegistry->registerSetting(hintsDef);

  // First run flag (hidden)
  SettingDefinition firstRunDef;
  firstRunDef.key = SETTING_FIRST_RUN;
  firstRunDef.displayName = "First Run";
  firstRunDef.description = "Internal flag for first run detection";
  firstRunDef.category = "Editor/Help";
  firstRunDef.type = SettingType::Bool;
  firstRunDef.scope = SettingScope::User;
  firstRunDef.defaultValue = true;
  firstRunDef.isAdvanced = true;
  m_settingsRegistry->registerSetting(firstRunDef);

  // Register change callbacks
  m_settingsRegistry->registerChangeCallback(
      SETTING_ENABLED,
      [this](const std::string &, const SettingValue &value) {
        if (auto *enabled = std::get_if<bool>(&value)) {
          m_globalEnabled = *enabled;
          emit enabledChanged(*enabled);
          if (!*enabled && isTutorialActive()) {
            stopTutorial(false);
          }
        }
      });

  m_settingsRegistry->registerChangeCallback(
      SETTING_EMPTY_HINTS,
      [this](const std::string &, const SettingValue &value) {
        if (auto *enabled = std::get_if<bool>(&value)) {
          m_emptyStateHintsEnabled = *enabled;
        }
      });
}

void NMTutorialManager::registerTutorial(const TutorialDefinition &tutorial) {
  m_tutorials[tutorial.id] = tutorial;

  // Initialize progress if not exists
  if (!m_progress.contains(tutorial.id)) {
    TutorialProgress progress;
    progress.tutorialId = tutorial.id;
    m_progress[tutorial.id] = progress;
  }
}

void NMTutorialManager::unregisterTutorial(const QString &tutorialId) {
  m_tutorials.erase(tutorialId);
}

bool NMTutorialManager::loadTutorialFromFile(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open tutorial file:" << filePath;
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  bool ok = false;
  TutorialDefinition tutorial = parseTutorialJson(data, ok);
  if (ok) {
    registerTutorial(tutorial);
    return true;
  }

  qWarning() << "Failed to parse tutorial file:" << filePath;
  return false;
}

int NMTutorialManager::loadTutorialsFromDirectory(const QString &dirPath) {
  QDir dir(dirPath);
  if (!dir.exists()) {
    return 0;
  }

  int count = 0;
  QStringList filters;
  filters << "*.json";

  for (const QString &fileName : dir.entryList(filters, QDir::Files)) {
    QString filePath = dir.absoluteFilePath(fileName);
    if (loadTutorialFromFile(filePath)) {
      count++;
    }
  }

  return count;
}

std::optional<TutorialDefinition>
NMTutorialManager::getTutorial(const QString &tutorialId) const {
  auto it = m_tutorials.find(tutorialId);
  if (it != m_tutorials.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<TutorialDefinition> NMTutorialManager::getAllTutorials() const {
  std::vector<TutorialDefinition> result;
  result.reserve(m_tutorials.size());
  for (const auto &[id, tutorial] : m_tutorials) {
    result.push_back(tutorial);
  }
  return result;
}

std::vector<TutorialDefinition>
NMTutorialManager::getTutorialsByCategory(TutorialCategory category) const {
  std::vector<TutorialDefinition> result;
  for (const auto &[id, tutorial] : m_tutorials) {
    if (tutorial.category == category) {
      result.push_back(tutorial);
    }
  }
  return result;
}

bool NMTutorialManager::startTutorial(const QString &tutorialId,
                                      int stepIndex) {
  if (!m_initialized || !m_globalEnabled) {
    return false;
  }

  auto it = m_tutorials.find(tutorialId);
  if (it == m_tutorials.end()) {
    qWarning() << "Tutorial not found:" << tutorialId;
    return false;
  }

  const TutorialDefinition &tutorial = it->second;

  // Check if tutorial is disabled
  if (isTutorialDisabled(tutorialId)) {
    return false;
  }

  // Stop any active tutorial
  if (m_activeTutorial.has_value()) {
    stopTutorial(false);
  }

  m_activeTutorial = tutorial;
  m_currentStepIndex = qBound(0, stepIndex, static_cast<int>(tutorial.steps.size()) - 1);
  m_isPaused = false;

  // Update progress
  auto &progress = m_progress[tutorialId];
  if (progress.status == TutorialStatus::NotStarted) {
    progress.status = TutorialStatus::InProgress;
    progress.startedTimestamp = QDateTime::currentMSecsSinceEpoch();
  }

  emit tutorialStarted(tutorialId);

  showCurrentStep();
  return true;
}

void NMTutorialManager::stopTutorial(bool markAsSkipped) {
  if (!m_activeTutorial.has_value()) {
    return;
  }

  QString tutorialId = m_activeTutorial->id;
  bool completed = (m_currentStepIndex >= static_cast<int>(m_activeTutorial->steps.size()) - 1);

  // Update progress
  if (markAsSkipped) {
    updateProgress(TutorialStatus::Skipped);
  } else if (completed) {
    updateProgress(TutorialStatus::Completed);
    emit tutorialCompleted(tutorialId);
  }

  m_autoAdvanceTimer->stop();
  hideOverlay();

  m_activeTutorial.reset();
  m_currentStepIndex = 0;
  m_isPaused = false;

  emit tutorialStopped(tutorialId, completed);

  saveProgress();
}

void NMTutorialManager::pauseTutorial() {
  if (m_activeTutorial.has_value() && !m_isPaused) {
    m_isPaused = true;
    m_autoAdvanceTimer->stop();
    hideOverlay();
  }
}

void NMTutorialManager::resumeTutorial() {
  if (m_activeTutorial.has_value() && m_isPaused) {
    m_isPaused = false;
    showCurrentStep();
  }
}

void NMTutorialManager::nextStep() {
  if (!m_activeTutorial.has_value()) {
    return;
  }

  int totalSteps = static_cast<int>(m_activeTutorial->steps.size());

  if (m_currentStepIndex < totalSteps - 1) {
    // Execute on-complete action for current step
    if (m_currentStepIndex < totalSteps) {
      executeAction(m_activeTutorial->steps[m_currentStepIndex].onComplete);
    }

    m_currentStepIndex++;

    // Update progress
    auto &progress = m_progress[m_activeTutorial->id];
    progress.currentStepIndex = m_currentStepIndex;
    progress.completedStepCount =
        qMax(progress.completedStepCount, m_currentStepIndex);

    emit stepChanged(m_currentStepIndex, totalSteps);
    showCurrentStep();
  } else {
    // Last step completed
    stopTutorial(false);
  }
}

void NMTutorialManager::previousStep() {
  if (!m_activeTutorial.has_value() || m_currentStepIndex <= 0) {
    return;
  }

  m_currentStepIndex--;

  int totalSteps = static_cast<int>(m_activeTutorial->steps.size());
  emit stepChanged(m_currentStepIndex, totalSteps);
  showCurrentStep();
}

void NMTutorialManager::goToStep(int stepIndex) {
  if (!m_activeTutorial.has_value()) {
    return;
  }

  int totalSteps = static_cast<int>(m_activeTutorial->steps.size());
  stepIndex = qBound(0, stepIndex, totalSteps - 1);

  if (stepIndex != m_currentStepIndex) {
    m_currentStepIndex = stepIndex;
    emit stepChanged(m_currentStepIndex, totalSteps);
    showCurrentStep();
  }
}

void NMTutorialManager::skipCurrentStep() {
  if (!m_activeTutorial.has_value()) {
    return;
  }

  const TutorialStep &step = m_activeTutorial->steps[m_currentStepIndex];

  // Mark step as disabled if user explicitly skips
  // (This is a "don't show this step again" behavior)

  nextStep();
}

QString NMTutorialManager::activeTutorialId() const {
  if (m_activeTutorial.has_value()) {
    return m_activeTutorial->id;
  }
  return QString();
}

std::optional<TutorialStep> NMTutorialManager::currentStep() const {
  if (!m_activeTutorial.has_value()) {
    return std::nullopt;
  }

  if (m_currentStepIndex >= 0 &&
      m_currentStepIndex < static_cast<int>(m_activeTutorial->steps.size())) {
    return m_activeTutorial->steps[m_currentStepIndex];
  }

  return std::nullopt;
}

int NMTutorialManager::totalSteps() const {
  if (m_activeTutorial.has_value()) {
    return static_cast<int>(m_activeTutorial->steps.size());
  }
  return 0;
}

TutorialProgress
NMTutorialManager::getProgress(const QString &tutorialId) const {
  auto it = m_progress.find(tutorialId);
  if (it != m_progress.end()) {
    return it->second;
  }

  TutorialProgress defaultProgress;
  defaultProgress.tutorialId = tutorialId;
  return defaultProgress;
}

bool NMTutorialManager::isTutorialCompleted(const QString &tutorialId) const {
  auto it = m_progress.find(tutorialId);
  if (it != m_progress.end()) {
    return it->second.status == TutorialStatus::Completed;
  }
  return false;
}

bool NMTutorialManager::isTutorialDisabled(const QString &tutorialId) const {
  auto it = m_progress.find(tutorialId);
  if (it != m_progress.end()) {
    return it->second.status == TutorialStatus::Disabled;
  }
  return false;
}

void NMTutorialManager::setTutorialDisabled(const QString &tutorialId,
                                            bool disabled) {
  auto &progress = m_progress[tutorialId];
  progress.tutorialId = tutorialId;
  progress.status =
      disabled ? TutorialStatus::Disabled : TutorialStatus::NotStarted;

  emit progressUpdated(tutorialId);
  saveProgress();
}

void NMTutorialManager::setStepDisabled(const QString &tutorialId,
                                        const QString &stepId, bool disabled) {
  auto &progress = m_progress[tutorialId];
  progress.tutorialId = tutorialId;

  if (disabled) {
    if (!progress.disabledStepIds.contains(stepId)) {
      progress.disabledStepIds.append(stepId);
    }
  } else {
    progress.disabledStepIds.removeAll(stepId);
  }

  emit progressUpdated(tutorialId);
  saveProgress();
}

void NMTutorialManager::resetProgress(const QString &tutorialId) {
  auto it = m_progress.find(tutorialId);
  if (it != m_progress.end()) {
    it->second = TutorialProgress();
    it->second.tutorialId = tutorialId;
    emit progressUpdated(tutorialId);
    saveProgress();
  }
}

void NMTutorialManager::resetAllProgress() {
  for (auto &[id, progress] : m_progress) {
    progress = TutorialProgress();
    progress.tutorialId = id;
    emit progressUpdated(id);
  }

  // Reset first run flag
  m_isFirstRun = true;
  if (m_settingsRegistry) {
    m_settingsRegistry->setValue(SETTING_FIRST_RUN, true);
  }

  saveProgress();
}

void NMTutorialManager::saveProgress() {
  if (!m_settingsRegistry) {
    return;
  }

  // Serialize progress to JSON and store in settings
  QJsonObject progressObj;
  for (const auto &[id, progress] : m_progress) {
    QJsonObject entry;
    entry["status"] = static_cast<int>(progress.status);
    entry["currentStep"] = progress.currentStepIndex;
    entry["completedSteps"] = progress.completedStepCount;
    entry["startedAt"] = progress.startedTimestamp;
    entry["completedAt"] = progress.completedTimestamp;

    QJsonArray disabledSteps;
    for (const QString &stepId : progress.disabledStepIds) {
      disabledSteps.append(stepId);
    }
    entry["disabledSteps"] = disabledSteps;

    progressObj[id] = entry;
  }

  QJsonDocument doc(progressObj);
  QString progressJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

  m_settingsRegistry->setValue(
      std::string(SETTING_PROGRESS_PREFIX) + "data",
      progressJson.toStdString());
}

void NMTutorialManager::loadProgress() {
  if (!m_settingsRegistry) {
    return;
  }

  std::string progressJson = m_settingsRegistry->getString(
      std::string(SETTING_PROGRESS_PREFIX) + "data", "");

  if (progressJson.empty()) {
    return;
  }

  QJsonDocument doc =
      QJsonDocument::fromJson(QByteArray::fromStdString(progressJson));
  if (!doc.isObject()) {
    return;
  }

  QJsonObject progressObj = doc.object();
  for (auto it = progressObj.begin(); it != progressObj.end(); ++it) {
    QString tutorialId = it.key();
    QJsonObject entry = it.value().toObject();

    TutorialProgress progress;
    progress.tutorialId = tutorialId;
    progress.status =
        static_cast<TutorialStatus>(entry["status"].toInt());
    progress.currentStepIndex = entry["currentStep"].toInt();
    progress.completedStepCount = entry["completedSteps"].toInt();
    progress.startedTimestamp = entry["startedAt"].toInteger();
    progress.completedTimestamp = entry["completedAt"].toInteger();

    QJsonArray disabledSteps = entry["disabledSteps"].toArray();
    for (const QJsonValue &v : disabledSteps) {
      progress.disabledStepIds.append(v.toString());
    }

    m_progress[tutorialId] = progress;
  }
}

void NMTutorialManager::setEnabled(bool enabled) {
  if (m_globalEnabled != enabled) {
    m_globalEnabled = enabled;

    if (m_settingsRegistry) {
      m_settingsRegistry->setValue(SETTING_ENABLED, enabled);
    }

    emit enabledChanged(enabled);

    if (!enabled && isTutorialActive()) {
      stopTutorial(false);
    }
  }
}

void NMTutorialManager::setEmptyStateHintsEnabled(bool enabled) {
  if (m_emptyStateHintsEnabled != enabled) {
    m_emptyStateHintsEnabled = enabled;

    if (m_settingsRegistry) {
      m_settingsRegistry->setValue(SETTING_EMPTY_HINTS, enabled);
    }
  }
}

void NMTutorialManager::onEditorStarted() {
  if (!m_initialized || !m_globalEnabled) {
    return;
  }

  if (m_isFirstRun) {
    // Find first-run tutorials
    for (const auto &[id, tutorial] : m_tutorials) {
      if (tutorial.showOnFirstRun && !isTutorialCompleted(id) &&
          !isTutorialDisabled(id)) {
        // Start the first first-run tutorial
        startTutorial(id);
        break;
      }
    }

    // Mark first run as complete
    m_isFirstRun = false;
    if (m_settingsRegistry) {
      m_settingsRegistry->setValue(SETTING_FIRST_RUN, false);
    }
  }
}

void NMTutorialManager::onPanelOpened(const QString &panelId) {
  if (!m_initialized || !m_globalEnabled || isTutorialActive()) {
    return;
  }

  // Find tutorials triggered by this panel
  for (const auto &[id, tutorial] : m_tutorials) {
    if (tutorial.showOnPanelOpen && tutorial.triggerPanelId == panelId &&
        !isTutorialCompleted(id) && !isTutorialDisabled(id)) {
      startTutorial(id);
      break;
    }
  }
}

void NMTutorialManager::onEmptyState(const QString &panelId,
                                     const QString &emptyStateType) {
  if (!m_initialized || !m_emptyStateHintsEnabled) {
    return;
  }

  // Empty state hints can be shown even during tutorials
  // This is handled by a separate context help system
  // For now, just emit an event that the UI can listen to
}

void NMTutorialManager::onError(const QString &errorType,
                                const QString &details) {
  if (!m_initialized || !m_globalEnabled) {
    return;
  }

  // Find tutorials that help with this error type
  // This is a placeholder for future error-triggered help
}

void NMTutorialManager::registerConditionChecker(const QString &conditionName,
                                                 std::function<bool()> checker) {
  m_conditionCheckers[conditionName] = checker;
}

void NMTutorialManager::registerActionHandler(
    const QString &actionName, std::function<void(const QString &)> handler) {
  m_actionHandlers[actionName] = handler;
}

void NMTutorialManager::onOverlayNextClicked() {
  nextStep();
}

void NMTutorialManager::onOverlayBackClicked() {
  previousStep();
}

void NMTutorialManager::onOverlaySkipClicked() {
  skipCurrentStep();
}

void NMTutorialManager::onOverlayCloseClicked() {
  stopTutorial(true);
}

void NMTutorialManager::onOverlayDontShowAgainClicked() {
  if (m_activeTutorial.has_value()) {
    QString tutorialId = m_activeTutorial->id;
    stopTutorial(false);
    setTutorialDisabled(tutorialId, true);
  }
}

void NMTutorialManager::checkAutoAdvanceConditions() {
  if (!m_activeTutorial.has_value() || m_isPaused) {
    return;
  }

  const TutorialStep &step = m_activeTutorial->steps[m_currentStepIndex];

  if (step.autoAdvance && evaluateCondition(step.completeCondition)) {
    nextStep();
  } else if (step.autoAdvance) {
    // Check again later
    m_autoAdvanceTimer->start(100);
  }
}

void NMTutorialManager::showCurrentStep() {
  if (!m_activeTutorial.has_value() || !m_overlay) {
    return;
  }

  const TutorialStep &step = m_activeTutorial->steps[m_currentStepIndex];

  // Check show condition
  if (!evaluateCondition(step.showCondition)) {
    // Skip to next step if condition not met
    if (m_currentStepIndex < static_cast<int>(m_activeTutorial->steps.size()) - 1) {
      m_currentStepIndex++;
      showCurrentStep();
    } else {
      stopTutorial(false);
    }
    return;
  }

  // Execute on-show action
  executeAction(step.onShow);

  // Show the overlay
  int totalSteps = static_cast<int>(m_activeTutorial->steps.size());
  m_overlay->showStep(step, m_currentStepIndex, totalSteps);

  // Setup auto-advance if needed
  if (step.autoAdvance) {
    m_autoAdvanceTimer->start(step.autoAdvanceDelayMs);
  }
}

void NMTutorialManager::hideOverlay() {
  if (m_overlay) {
    m_overlay->hideOverlay();
  }
}

bool NMTutorialManager::evaluateCondition(const TutorialCondition &condition) {
  bool result = false;

  switch (condition.type) {
  case ConditionType::Always:
    result = true;
    break;

  case ConditionType::PanelVisible:
    // Check if panel is visible using anchor registry
    result = NMAnchorRegistry::instance().isAnchorVisible(
        condition.parameter + ".root");
    break;

  case ConditionType::PanelFocused:
    // This would need integration with panel focus tracking
    result = true; // Placeholder
    break;

  case ConditionType::ObjectSelected:
    // This would need integration with selection manager
    result = true; // Placeholder
    break;

  case ConditionType::EmptyState:
    // This would need integration with panel state
    result = true; // Placeholder
    break;

  case ConditionType::SettingValue:
    if (m_settingsRegistry) {
      std::string value = m_settingsRegistry->getString(
          condition.parameter.toStdString(), "");
      result = QString::fromStdString(value) == condition.value;
    }
    break;

  case ConditionType::Custom:
    if (condition.customCheck) {
      result = condition.customCheck();
    } else {
      // Check registered custom checkers
      auto it = m_conditionCheckers.find(condition.parameter);
      if (it != m_conditionCheckers.end()) {
        result = it->second();
      }
    }
    break;
  }

  return condition.invert ? !result : result;
}

void NMTutorialManager::executeAction(const TutorialAction &action) {
  switch (action.type) {
  case StepActionType::None:
    break;

  case StepActionType::HighlightMenu:
    // This would highlight a menu item
    break;

  case StepActionType::OpenPanel:
  case StepActionType::FocusPanel:
    // Check registered action handlers
    {
      auto it = m_actionHandlers.find("panel");
      if (it != m_actionHandlers.end()) {
        it->second(action.target);
      }
    }
    break;

  case StepActionType::SelectObject:
    {
      auto it = m_actionHandlers.find("select");
      if (it != m_actionHandlers.end()) {
        it->second(action.target);
      }
    }
    break;

  case StepActionType::NavigateTo:
    {
      auto it = m_actionHandlers.find("navigate");
      if (it != m_actionHandlers.end()) {
        it->second(action.target);
      }
    }
    break;

  case StepActionType::ShowHint:
    // Show an inline hint
    break;

  case StepActionType::Custom:
    if (action.customAction) {
      action.customAction();
    } else {
      auto it = m_actionHandlers.find(action.target);
      if (it != m_actionHandlers.end()) {
        it->second(action.parameter);
      }
    }
    break;
  }
}

void NMTutorialManager::updateProgress(TutorialStatus status) {
  if (!m_activeTutorial.has_value()) {
    return;
  }

  auto &progress = m_progress[m_activeTutorial->id];
  progress.status = status;

  if (status == TutorialStatus::Completed) {
    progress.completedTimestamp = QDateTime::currentMSecsSinceEpoch();
    progress.completedStepCount = static_cast<int>(m_activeTutorial->steps.size());
  }

  emit progressUpdated(m_activeTutorial->id);
}

TutorialDefinition NMTutorialManager::parseTutorialJson(const QByteArray &json,
                                                        bool &ok) {
  TutorialDefinition tutorial;
  ok = false;

  QJsonDocument doc = QJsonDocument::fromJson(json);
  if (!doc.isObject()) {
    return tutorial;
  }

  QJsonObject root = doc.object();

  tutorial.id = root["id"].toString();
  tutorial.title = root["title"].toString();
  tutorial.description = root["description"].toString();
  tutorial.iconName = root["icon"].toString();
  tutorial.category = categoryFromString(root["category"].toString());
  tutorial.showOnFirstRun = root["showOnFirstRun"].toBool(false);
  tutorial.showOnPanelOpen = root["showOnPanelOpen"].toBool(false);
  tutorial.triggerPanelId = root["triggerPanel"].toString();
  tutorial.estimatedDurationSeconds = root["duration"].toInt(60);

  // Parse tags
  QJsonArray tagsArray = root["tags"].toArray();
  for (const QJsonValue &v : tagsArray) {
    tutorial.tags.append(v.toString());
  }

  // Parse prerequisites
  QJsonArray prereqArray = root["prerequisites"].toArray();
  for (const QJsonValue &v : prereqArray) {
    tutorial.prerequisites.append(v.toString());
  }

  // Parse steps
  QJsonArray stepsArray = root["steps"].toArray();
  for (const QJsonValue &stepVal : stepsArray) {
    QJsonObject stepObj = stepVal.toObject();

    TutorialStep step;
    step.id = stepObj["id"].toString();
    step.anchorId = stepObj["anchor"].toString();
    step.title = stepObj["title"].toString();
    step.content = stepObj["content"].toString();
    step.detailText = stepObj["details"].toString();
    step.placement = placementFromString(stepObj["placement"].toString());
    step.highlightStyle =
        highlightStyleFromString(stepObj["highlight"].toString());
    step.allowSkip = stepObj["allowSkip"].toBool(true);
    step.autoAdvance = stepObj["autoAdvance"].toBool(false);
    step.autoAdvanceDelayMs = stepObj["autoAdvanceDelay"].toInt(500);
    step.learnMoreUrl = stepObj["learnMoreUrl"].toString();
    step.learnMoreLabel = stepObj["learnMoreLabel"].toString();

    // Parse conditions (simplified)
    if (stepObj.contains("showWhen")) {
      QJsonObject condObj = stepObj["showWhen"].toObject();
      step.showCondition.parameter = condObj["panel"].toString();
      if (!step.showCondition.parameter.isEmpty()) {
        step.showCondition.type = ConditionType::PanelVisible;
      }
    }

    tutorial.steps.push_back(step);
  }

  ok = !tutorial.id.isEmpty() && !tutorial.steps.empty();
  return tutorial;
}

} // namespace NovelMind::editor::qt
