/**
 * @file nm_main_window_tutorials.cpp
 * @brief Tutorial system integration for NMMainWindow
 *
 * This file contains the tutorial-related functionality that extends
 * NMMainWindow. It sets up:
 * - Help menu with tutorial entries
 * - Tutorial system initialization
 * - Panel anchor registration
 */

#include "NovelMind/editor/qt/nm_anchor_registry.hpp"
#include "NovelMind/editor/qt/nm_help_overlay.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_main_window.hpp"
#include "NovelMind/editor/qt/nm_tutorial_manager.hpp"
#include "NovelMind/editor/qt/nm_tutorial_types.hpp"
#include "NovelMind/editor/settings_registry.hpp"

#include <QAction>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QUrl>

namespace NovelMind::editor::qt {

/**
 * @brief Helper class for tutorial system initialization
 *
 * This class encapsulates the tutorial system setup to keep
 * the main window code clean. It's used during NMMainWindow::initialize().
 */
class TutorialSystemHelper {
public:
  /**
   * @brief Initialize the tutorial system for a main window
   * @param mainWindow The main window instance
   * @param settingsRegistry Settings registry for persistence
   */
  static void initialize(NMMainWindow *mainWindow,
                         NMSettingsRegistry *settingsRegistry) {
    if (!mainWindow) {
      return;
    }

    // Create the help overlay
    auto *overlay = new NMHelpOverlay(mainWindow);
    overlay->hide();

    // Initialize the tutorial manager
    auto &tutorialMgr = NMTutorialManager::instance();
    tutorialMgr.initialize(overlay, settingsRegistry);

    // Load built-in tutorials
    QString tutorialsPath = ":/tutorials";
    tutorialMgr.loadTutorialsFromDirectory(tutorialsPath);

    // Register panel anchors
    registerPanelAnchors(mainWindow);

    // Register action handlers for tutorial system
    registerActionHandlers(mainWindow);

    // Setup Help menu tutorials
    setupHelpMenu(mainWindow);

    // Trigger first-run check after a short delay
    // (gives the UI time to fully initialize)
    QTimer::singleShot(500, []() {
      NMTutorialManager::instance().onEditorStarted();
    });
  }

private:
  /**
   * @brief Register anchor points for all panels
   */
  static void registerPanelAnchors(NMMainWindow *mainWindow) {
    auto &registry = NMAnchorRegistry::instance();

    // Scene View
    if (auto *panel = mainWindow->sceneViewPanel()) {
      registry.registerAnchor("scene_view.root", panel, "Scene View", "scene_view");
    }

    // Story Graph
    if (auto *panel = mainWindow->storyGraphPanel()) {
      registry.registerAnchor("story_graph.root", panel, "Story Graph", "story_graph");
    }

    // Inspector
    if (auto *panel = mainWindow->inspectorPanel()) {
      registry.registerAnchor("inspector.root", panel, "Inspector", "inspector");
    }

    // Console
    if (auto *panel = mainWindow->consolePanel()) {
      registry.registerAnchor("console.root", panel, "Console", "console");
    }

    // Asset Browser
    if (auto *panel = mainWindow->assetBrowserPanel()) {
      registry.registerAnchor("asset_browser.root", panel, "Asset Browser", "asset_browser");
    }

    // Scene Palette
    if (auto *panel = mainWindow->scenePalettePanel()) {
      registry.registerAnchor("scene_palette.root", panel, "Scene Palette", "scene_palette");
    }

    // Hierarchy
    if (auto *panel = mainWindow->hierarchyPanel()) {
      registry.registerAnchor("hierarchy.root", panel, "Hierarchy", "hierarchy");
    }

    // Script Editor
    if (auto *panel = mainWindow->scriptEditorPanel()) {
      registry.registerAnchor("script_editor.root", panel, "Script Editor", "script_editor");
    }

    // Script Docs
    if (auto *panel = mainWindow->scriptDocPanel()) {
      registry.registerAnchor("script_docs.root", panel, "Script Docs", "script_docs");
    }

    // Voice Manager
    if (auto *panel = mainWindow->voiceManagerPanel()) {
      registry.registerAnchor("voice_manager.root", panel, "Voice Manager", "voice_manager");
    }

    // Voice Studio
    if (auto *panel = mainWindow->voiceStudioPanel()) {
      registry.registerAnchor("voice_studio.root", panel, "Voice Studio", "voice_studio");
    }

    // Audio Mixer
    if (auto *panel = mainWindow->audioMixerPanel()) {
      registry.registerAnchor("audio_mixer.root", panel, "Audio Mixer", "audio_mixer");
    }

    // Localization
    if (auto *panel = mainWindow->localizationPanel()) {
      registry.registerAnchor("localization.root", panel, "Localization", "localization");
    }

    // Timeline
    if (auto *panel = mainWindow->timelinePanel()) {
      registry.registerAnchor("timeline.root", panel, "Timeline", "timeline");
    }

    // Curve Editor
    if (auto *panel = mainWindow->curveEditorPanel()) {
      registry.registerAnchor("curve_editor.root", panel, "Curve Editor", "curve_editor");
    }

    // Build Settings
    if (auto *panel = mainWindow->buildSettingsPanel()) {
      registry.registerAnchor("build_settings.root", panel, "Build Settings", "build_settings");
    }

    // Debug Overlay
    if (auto *panel = mainWindow->debugOverlayPanel()) {
      registry.registerAnchor("debug_overlay.root", panel, "Debug Overlay", "debug_overlay");
    }

    // Issues
    if (auto *panel = mainWindow->issuesPanel()) {
      registry.registerAnchor("issues.root", panel, "Issues", "issues");
    }

    // Diagnostics
    if (auto *panel = mainWindow->diagnosticsPanel()) {
      registry.registerAnchor("diagnostics.root", panel, "Diagnostics", "diagnostics");
    }
  }

  /**
   * @brief Register action handlers for tutorial actions
   */
  static void registerActionHandlers(NMMainWindow *mainWindow) {
    auto &tutorialMgr = NMTutorialManager::instance();

    // Panel focus action
    tutorialMgr.registerActionHandler(
        "panel", [mainWindow](const QString &panelId) {
          // Focus the specified panel
          if (panelId == "scene_view" && mainWindow->sceneViewPanel()) {
            mainWindow->sceneViewPanel()->show();
            mainWindow->sceneViewPanel()->raise();
          } else if (panelId == "story_graph" && mainWindow->storyGraphPanel()) {
            mainWindow->storyGraphPanel()->show();
            mainWindow->storyGraphPanel()->raise();
          } else if (panelId == "inspector" && mainWindow->inspectorPanel()) {
            mainWindow->inspectorPanel()->show();
            mainWindow->inspectorPanel()->raise();
          }
          // Add more panels as needed
        });

    // Navigation action
    tutorialMgr.registerActionHandler(
        "navigate", [mainWindow](const QString &location) {
          // Navigate to a specific location in the editor
          // This could open specific views, select objects, etc.
          Q_UNUSED(mainWindow)
          Q_UNUSED(location)
        });
  }

  /**
   * @brief Setup the Help menu with tutorial entries
   */
  static void setupHelpMenu(NMMainWindow *mainWindow) {
    QMenuBar *menuBar = mainWindow->menuBar();
    if (!menuBar) {
      return;
    }

    // Find the Help menu
    QMenu *helpMenu = nullptr;
    for (QAction *action : menuBar->actions()) {
      if (action->text().contains("Help", Qt::CaseInsensitive)) {
        helpMenu = action->menu();
        break;
      }
    }

    if (!helpMenu) {
      return;
    }

    auto &iconMgr = NMIconManager::instance();
    auto &tutorialMgr = NMTutorialManager::instance();

    // Add separator before tutorials
    helpMenu->addSeparator();

    // Tutorials submenu
    QMenu *tutorialsMenu = helpMenu->addMenu(
        iconMgr.getIcon("help", 16), QObject::tr("&Tutorials"));

    // Getting Started section
    QAction *gettingStartedHeader = tutorialsMenu->addAction(
        QObject::tr("Getting Started"));
    gettingStartedHeader->setEnabled(false);

    auto gettingStartedTutorials =
        tutorialMgr.getTutorialsByCategory(TutorialCategory::GettingStarted);
    for (const auto &tutorial : gettingStartedTutorials) {
      QAction *action = tutorialsMenu->addAction(
          iconMgr.getIcon("help", 16), tutorial.title);
      action->setToolTip(tutorial.description);

      QString tutorialId = tutorial.id;
      QObject::connect(action, &QAction::triggered, [tutorialId]() {
        NMTutorialManager::instance().startTutorial(tutorialId);
      });

      // Show checkmark if completed
      if (tutorialMgr.isTutorialCompleted(tutorial.id)) {
        action->setIcon(iconMgr.getIcon("check", 16));
      }
    }

    tutorialsMenu->addSeparator();

    // Workflow section
    QAction *workflowHeader = tutorialsMenu->addAction(
        QObject::tr("Workflow"));
    workflowHeader->setEnabled(false);

    auto workflowTutorials =
        tutorialMgr.getTutorialsByCategory(TutorialCategory::Workflow);
    for (const auto &tutorial : workflowTutorials) {
      QAction *action = tutorialsMenu->addAction(
          iconMgr.getIcon("help", 16), tutorial.title);
      action->setToolTip(tutorial.description);

      QString tutorialId = tutorial.id;
      QObject::connect(action, &QAction::triggered, [tutorialId]() {
        NMTutorialManager::instance().startTutorial(tutorialId);
      });

      if (tutorialMgr.isTutorialCompleted(tutorial.id)) {
        action->setIcon(iconMgr.getIcon("check", 16));
      }
    }

    tutorialsMenu->addSeparator();

    // Reset progress action
    QAction *resetAction = tutorialsMenu->addAction(
        iconMgr.getIcon("refresh", 16), QObject::tr("Reset Tutorial Progress"));
    resetAction->setToolTip(
        QObject::tr("Reset all tutorial progress to start fresh"));
    QObject::connect(resetAction, &QAction::triggered, []() {
      QMessageBox::StandardButton reply = QMessageBox::question(
          nullptr, QObject::tr("Reset Tutorials"),
          QObject::tr("This will reset all tutorial progress. Continue?"),
          QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::Yes) {
        NMTutorialManager::instance().resetAllProgress();
      }
    });

    // Tutorial settings action
    helpMenu->addSeparator();

    QAction *tutorialSettingsAction = helpMenu->addAction(
        iconMgr.getIcon("settings", 16), QObject::tr("Tutorial Settings..."));
    tutorialSettingsAction->setToolTip(
        QObject::tr("Configure tutorial and help settings"));
    QObject::connect(tutorialSettingsAction, &QAction::triggered,
                     [mainWindow]() {
                       // Open settings dialog to the Help section
                       mainWindow->showSettingsDialog();
                     });
  }
};

} // namespace NovelMind::editor::qt
