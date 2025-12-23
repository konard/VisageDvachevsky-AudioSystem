# GUI ↔ Core Feature Parity Matrix

> **Canonical Document** - NovelMind Editor Feature Coverage Analysis

**Version**: 1.0
**Date**: 2025-12-23
**Status**: Active

---

## Contents

1. [Overview](#overview)
2. [Audio System](#1-audio-system)
3. [Localization System](#2-localization-system)
4. [Animation/Timeline System](#3-animationtimeline-system)
5. [SceneGraph/Inspector System](#4-scenegraphinspector-system)
6. [Script Runtime System](#5-script-runtime-system)
7. [Project/Settings System](#6-projectsettings-system)
8. [Diagnostics System](#7-diagnostics-system)
9. [Property System](#8-property-system)
10. [Gap Summary & Action Plan](#gap-summary--action-plan)
11. [Regression Prevention Checklist](#regression-prevention-checklist)

---

## Overview

This document provides a systematic analysis of feature parity between `engine_core` and `editor` GUI components. Each subsystem is analyzed with:

- **Core API entrypoints** - Files/classes in engine_core
- **UI Exposure** - Panels/Windows in editor
- **Current Status** - ✅ Full | 🟡 Partial | 🔴 Missing
- **Gap Description** - What functionality is missing
- **Fix Plan** - Actions to close the gap

---

## 1. Audio System

### Core API Location
- `engine_core/include/NovelMind/audio/audio_manager.hpp`
- `engine_core/include/NovelMind/audio/audio_recorder.hpp`
- `engine_core/include/NovelMind/audio/voice_manifest.hpp`

### UI Exposure
- `editor/src/qt/panels/nm_voice_manager_panel.cpp`
- `editor/src/qt/panels/nm_voice_studio_panel.cpp`
- `editor/src/qt/panels/nm_recording_studio_panel.cpp`
- `editor/src/voice_manager.cpp`

### Feature Parity Table

| Core Feature | Core API | UI Panel | Actions | Status | Gap Description |
|-------------|----------|----------|---------|--------|-----------------|
| **AudioManager** | | | | | |
| Sound Playback | `playSound()`, `stopSound()` | VoiceManagerPanel | Play/Stop | ✅ Full | - |
| Music Playback | `playMusic()`, `stopMusic()`, `pauseMusic()`, `resumeMusic()` | - | - | 🔴 Missing | No music preview/control in editor |
| Voice Playback | `playVoice()`, `stopVoice()`, `skipVoice()` | VoiceManagerPanel | Preview | ✅ Full | - |
| Volume Control (Per-Channel) | `setChannelVolume()`, 6 channels | - | - | 🔴 Missing | No per-channel volume UI |
| Master Volume | `setMasterVolume()` | VoiceStudioPanel | Volume slider | 🟡 Partial | Only in VoiceStudio |
| Music Seek | `seekMusic()`, `getMusicPosition()` | - | - | 🔴 Missing | No seek UI |
| Crossfade | `crossfadeMusic()` | - | - | 🔴 Missing | No crossfade UI |
| Auto-Ducking | `setAutoDuckingEnabled()`, `setDuckingParams()` | - | - | 🔴 Missing | No ducking controls |
| **AudioRecorder** | | | | | |
| Device Selection | `getInputDevices()`, `setInputDevice()` | RecordingStudioPanel | Dropdown | ✅ Full | - |
| Level Metering | `getCurrentLevel()`, `startMetering()` | RecordingStudioPanel | VU Meter | ✅ Full | - |
| Recording | `startRecording()`, `stopRecording()` | RecordingStudioPanel | Record/Stop | ✅ Full | - |
| Recording Format | `setRecordingFormat()` (WAV/FLAC/OGG) | - | - | 🔴 Missing | No format selection UI |
| Auto-Trim Silence | `RecordingFormat.autoTrimSilence` | - | - | 🔴 Missing | No auto-trim UI |
| Normalize | `RecordingFormat.normalize` | VoiceStudioPanel | Effect | ✅ Full | Available as effect |
| Input Monitoring | `setMonitoringEnabled()` | - | - | 🔴 Missing | No live monitoring toggle |
| **VoiceManifest** | | | | | |
| Basic Voice Binding | `addLine()`, `getLine()` | VoiceManagerPanel | Table | ✅ Full | - |
| Multi-Locale Support | `addLocale()`, `getLocales()`, per-locale files | - | - | 🔴 Missing | Single-locale only in UI |
| Take Management | `addTake()`, `setActiveTake()`, `getTakes()` | - | - | 🔴 Missing | No take selection UI |
| Production Status | `VoiceLineStatus` (Missing/Recorded/Imported/NeedsReview/Approved) | - | - | 🔴 Missing | Simple binding status only |
| Audio Metadata | `sampleRate`, `channels`, `loudnessLUFS` | - | - | 🔴 Missing | No metadata display |
| Validation | `validate()`, `ManifestValidationError` | - | - | 🔴 Missing | No validation UI |
| Coverage Statistics | `getCoverageStats()` (per-locale) | VoiceManagerPanel | Basic stats | 🟡 Partial | No per-locale breakdown |
| Naming Conventions | `setNamingConvention()`, `generateFilePaths()` | - | - | 🔴 Missing | No convention UI |
| Tags/Notes | `tags`, `notes` fields | - | - | 🔴 Missing | No metadata editing |
| Template Export | `exportTemplate()` | - | - | 🔴 Missing | No template export |

### Priority Actions

| Priority | Action | Effort | Files |
|----------|--------|--------|-------|
| HIGH | Add multi-locale support to VoiceManagerPanel | Medium | `nm_voice_manager_panel.cpp`, `voice_manager.cpp` |
| HIGH | Integrate VoiceManifest with VoiceManager | High | `voice_manager.cpp` |
| MEDIUM | Add per-channel volume controls | Low | New panel or settings |
| MEDIUM | Add take management UI | Medium | `nm_voice_manager_panel.cpp` |
| LOW | Add audio metadata display | Low | `nm_voice_manager_panel.cpp` |
| LOW | Add recording format selection | Low | `nm_recording_studio_panel.cpp` |

---

## 2. Localization System

### Core API Location
- `engine_core/include/NovelMind/localization/localization_manager.hpp`

### UI Exposure
- `editor/src/qt/panels/nm_localization_panel.cpp`

### Feature Parity Table

| Core Feature | Core API | UI Panel | Actions | Status | Gap Description |
|-------------|----------|----------|---------|--------|-----------------|
| String Tables | `StringTable`, `addString()`, `getString()` | LocalizationPanel | Table view | ✅ Full | - |
| Multiple Locales | `addLocale()`, `getAvailableLocales()` | LocalizationPanel | Locale selector | ✅ Full | - |
| Locale Config | `registerLocale()`, `LocaleConfig` | - | - | 🟡 Partial | Display name editable, but no RTL/font config UI |
| Set Current Locale | `setCurrentLocale()` | - | - | 🔴 Missing | No runtime locale switch in editor |
| Plural Forms | `PluralCategory`, `addPluralString()`, `getPlural()` | - | - | 🔴 Missing | No plural editing UI |
| Variable Interpolation | `get(id, variables)` | - | - | 🔴 Missing | No preview with variables |
| RTL Support | `isRightToLeft()` | - | - | 🔴 Missing | No RTL preview |
| Import CSV | `loadStrings(CSV)` | LocalizationPanel | Import button | ✅ Full | - |
| Import JSON | `loadStrings(JSON)` | LocalizationPanel | Import button | ✅ Full | - |
| Import PO | `loadStrings(PO)` | - | - | 🔴 Missing | No PO import UI |
| Import XLIFF | `loadStrings(XLIFF)` | - | - | 🔴 Missing | No XLIFF import UI |
| Export CSV | `exportStrings(CSV)` | LocalizationPanel | Export button | ✅ Full | - |
| Export JSON | `exportStrings(JSON)` | LocalizationPanel | Export button | ✅ Full | - |
| Export PO | `exportStrings(PO)` | - | - | 🔴 Missing | No PO export (core stub) |
| Export Missing | `exportMissingStrings()` | - | - | 🔴 Missing | No missing export UI |
| Find Missing Keys | `hasString()` comparison | LocalizationPanel | Filter | ✅ Full | Via findMissingTranslations |
| Find Unused Keys | - | LocalizationPanel | Filter | ✅ Full | Via findUnusedKeys |
| Navigate to Usage | - | LocalizationPanel | Double-click | ✅ Full | - |
| String Search | - | LocalizationPanel | Search bar | ✅ Full | - |
| Missing Callback | `setOnStringMissing()` | - | - | 🔴 Missing | No runtime missing key reporting |
| Language Changed Callback | `setOnLanguageChanged()` | - | - | 🔴 Missing | No UI notification |

### Priority Actions

| Priority | Action | Effort | Files |
|----------|--------|--------|-------|
| MEDIUM | Add plural forms editing UI | Medium | `nm_localization_panel.cpp` |
| MEDIUM | Add PO/XLIFF import support | Low | `nm_localization_panel.cpp` |
| LOW | Add RTL preview mode | Medium | `nm_localization_panel.cpp` |
| LOW | Add variable interpolation preview | Low | `nm_localization_panel.cpp` |
| LOW | Add "Export Missing" button | Low | `nm_localization_panel.cpp` |

---

## 3. Animation/Timeline System

### Core API Location
- `engine_core/include/NovelMind/scene/animation.hpp`
- `engine_core/src/scene/` (animation classes in scene_graph)

### UI Exposure
- `editor/src/qt/panels/nm_timeline_panel.cpp`
- `editor/src/qt/panels/nm_curve_editor_panel.cpp`
- `editor/src/curve_editor.cpp`
- `editor/src/timeline_playback.cpp`

### Feature Parity Table

| Core Feature | Core API | UI Panel | Actions | Status | Gap Description |
|-------------|----------|----------|---------|--------|-----------------|
| **Easing Functions** | | | | | |
| 22 Easing Types | `EaseType` enum, `ease()` function | TimelinePanel | Dropdown (13 types) | 🟡 Partial | Missing: EaseInOutExpo, Elastic variants |
| Custom Curves | - | CurveEditorPanel | Bezier editing | ✅ Full | - |
| **Tween System** | | | | | |
| FloatTween | `FloatTween` class | TimelinePanel | Animation track | ✅ Full | Via keyframes |
| PositionTween | `PositionTween` class | TimelinePanel | Position track | ✅ Full | - |
| ColorTween | `ColorTween` class | TimelinePanel | Color keyframes | 🟡 Partial | Basic support |
| CallbackTween | `CallbackTween` class | - | - | 🔴 Missing | No callback keyframes |
| Loop/Yoyo | `setLoops()`, `setYoyo()` | - | - | 🔴 Missing | No loop/yoyo UI |
| **AnimationTimeline** | | | | | |
| Sequential Tweens | `append()` | TimelinePanel | Track sequencing | ✅ Full | - |
| Parallel Tweens | `join()` | TimelinePanel | Multiple tracks | ✅ Full | - |
| Delays | `delay()` | - | - | 🔴 Missing | No explicit delay markers |
| **AnimationManager** | | | | | |
| Add/Remove Animations | `add()`, `stop()` | TimelinePanel | Track management | ✅ Full | - |
| Named Animations | ID-based lookup | TimelinePanel | Track names | ✅ Full | - |
| Stop All | `stopAll()` | TimelinePanel | Stop button | ✅ Full | - |
| **Timeline Features** | | | | | |
| Keyframe Editing | - | TimelinePanel | Add/Delete/Move | ✅ Full | - |
| Multi-track Types | - | TimelinePanel | 8 track types | ✅ Full | Audio, Animation, Event, etc. |
| Grid Snapping | - | TimelinePanel | Snap toggle | ✅ Full | - |
| Copy/Paste Keyframes | - | TimelinePanel | Clipboard ops | ✅ Full | - |
| Playback Controls | - | TimelinePanel | Play/Pause/Stop | ✅ Full | - |
| Frame Scrubbing | - | TimelinePanel | Playhead drag | ✅ Full | - |
| Zoom | - | TimelinePanel | Zoom in/out | ✅ Full | - |
| Play Mode Sync | - | TimelinePanel | Sync toggle | 🟡 Partial | Framework ready |

### Priority Actions

| Priority | Action | Effort | Files |
|----------|--------|--------|-------|
| LOW | Add remaining easing types to dropdown | Low | `nm_timeline_panel.cpp` |
| LOW | Add loop/yoyo controls for tracks | Medium | `nm_timeline_panel.cpp` |
| LOW | Add delay markers | Low | `nm_timeline_panel.cpp` |

---

## 4. SceneGraph/Inspector System

### Core API Location
- `engine_core/include/NovelMind/scene/scene_graph.hpp`
- `engine_core/include/NovelMind/scene/scene_inspector.hpp`
- `engine_core/include/NovelMind/scene/scene_object_handle.hpp`
- `engine_core/include/NovelMind/scene/transition.hpp`

### UI Exposure
- `editor/src/qt/panels/nm_scene_view_panel.cpp`
- `editor/src/qt/panels/nm_hierarchy_panel.cpp`
- `editor/src/qt/panels/nm_inspector_panel.cpp`

### Feature Parity Table

| Core Feature | Core API | UI Panel | Actions | Status | Gap Description |
|-------------|----------|----------|---------|--------|-----------------|
| **SceneGraph** | | | | | |
| Layer Management | `getBackgroundLayer()`, etc. (4 layers) | HierarchyPanel | Tree view | ✅ Full | - |
| Add Objects | `addToLayer()` | SceneViewPanel | Create menu | ✅ Full | - |
| Remove Objects | `removeFromLayer()` | SceneViewPanel | Delete | ✅ Full | - |
| Find by ID | `findObject()` | HierarchyPanel | Selection | ✅ Full | - |
| Find by Tag | `findObjectsByTag()` | - | - | 🔴 Missing | No tag filter UI |
| Find by Type | `findObjectsByType()` | - | - | 🔴 Missing | No type filter UI |
| **SceneObjectBase** | | | | | |
| Transform | `setPosition()`, `setScale()`, `setRotation()` | SceneViewPanel, Inspector | Gizmo, fields | ✅ Full | - |
| Visibility | `setVisible()` | HierarchyPanel | Eye icon | ✅ Full | - |
| Alpha/Opacity | `setAlpha()` | Inspector | Slider | ✅ Full | - |
| Z-Order | `setZOrder()` | HierarchyPanel | Context menu | ✅ Full | - |
| Tags | `addTag()`, `removeTag()`, `hasTag()` | - | - | 🔴 Missing | No tag editing UI |
| Custom Properties | `setProperty()`, `getProperty()` | Inspector | Property fields | ✅ Full | - |
| Parent/Child | `setParent()`, `addChild()` | HierarchyPanel | Tree structure | 🟡 Partial | Display only, no drag reparenting |
| Animate Methods | `animatePosition()`, `animateAlpha()`, `animateScale()` | - | - | 🟡 Partial | Via Timeline, not direct |
| **Object Types** | | | | | |
| BackgroundObject | Full implementation | SceneViewPanel | Create/edit | ✅ Full | - |
| CharacterObject | Expressions, poses, slots | SceneViewPanel | Create/edit | ✅ Full | - |
| DialogueUIObject | Typewriter, styling | SceneViewPanel | Create/edit | ✅ Full | - |
| ChoiceUIObject | Options, callbacks | SceneViewPanel | Create/edit | ✅ Full | - |
| EffectOverlayObject | 6 effect types | SceneViewPanel | Create/edit | 🟡 Partial | Basic effects |
| **SceneInspectorAPI** | | | | | |
| Property Access | `getProperties()`, `setProperty()` | Inspector | Property grid | ✅ Full | - |
| Object CRUD | `createObject()`, `deleteObject()`, `duplicateObject()` | SceneViewPanel | Context menu | ✅ Full | - |
| Selection | `selectObject()`, `getSelection()` | SceneViewPanel, Hierarchy | Click/list | ✅ Full | - |
| Multi-Select | `getSelection()` (multiple) | - | - | 🟡 Partial | Framework ready |
| Undo/Redo | `undo()`, `redo()`, command stack | MainWindow | Ctrl+Z/Y | ✅ Full | - |
| Clipboard | `copySelection()`, `paste()` | SceneViewPanel | Ctrl+C/V | ✅ Full | - |
| **Transitions** | | | | | |
| 10 Transition Types | `TransitionType` enum | - | - | 🔴 Missing | No transition preview UI |
| Create Transition | `createTransition()` factory | - | - | 🔴 Missing | Only via script |
| Transition Progress | `getProgress()`, callbacks | - | - | 🔴 Missing | No visual feedback |
| **SceneObjectHandle** | | | | | |
| Safe References | `isValid()`, `get()`, `withObject()` | Inspector | Selection binding | ✅ Full | Properly integrated |

### Priority Actions

| Priority | Action | Effort | Files |
|----------|--------|--------|-------|
| HIGH | Add drag-and-drop reparenting in Hierarchy | Medium | `nm_hierarchy_panel.cpp` |
| MEDIUM | Add tag editing UI in Inspector | Low | `nm_inspector_panel.cpp` |
| MEDIUM | Add transition preview panel | Medium | New panel |
| LOW | Add object type/tag filters | Low | `nm_hierarchy_panel.cpp` |

---

## 5. Script Runtime System

### Core API Location
- `engine_core/include/NovelMind/scripting/script_runtime.hpp`
- `engine_core/include/NovelMind/scripting/vm.hpp`
- `engine_core/include/NovelMind/scripting/compiler.hpp`
- `engine_core/include/NovelMind/scripting/validator.hpp`
- `engine_core/include/NovelMind/scripting/ir.hpp`

### UI Exposure
- `editor/src/qt/panels/nm_script_editor_panel.cpp`
- `editor/src/qt/panels/nm_story_graph_panel.cpp`
- `editor/src/qt/panels/nm_debug_overlay_panel.cpp`
- `editor/src/qt/nm_play_mode_controller.cpp`

### Feature Parity Table

| Core Feature | Core API | UI Panel | Actions | Status | Gap Description |
|-------------|----------|----------|---------|--------|-----------------|
| **ScriptRuntime** | | | | | |
| Load Script | `load()` | PlayModeController | Auto-load | ✅ Full | - |
| Start/Stop | `start()`, `stop()` | PlayToolbarPanel | Play/Stop | ✅ Full | - |
| Pause/Resume | `pause()`, `resume()` | PlayToolbarPanel | Pause | ✅ Full | - |
| Goto Scene | `gotoScene()` | StoryGraphPanel | Double-click | ✅ Full | - |
| Continue | `continueExecution()` | PlayToolbarPanel | Continue/Step | ✅ Full | - |
| Select Choice | `selectChoice()` | Runtime preview | Click | ✅ Full | - |
| Variables | `setVariable()`, `getVariable()` | DebugOverlayPanel | Edit on pause | ✅ Full | - |
| Flags | `setFlag()`, `getFlag()` | DebugOverlayPanel | Edit on pause | ✅ Full | - |
| Skip Mode | `setSkipMode()` | - | - | 🔴 Missing | No skip mode toggle |
| Runtime State | `getState()` | DebugOverlayPanel | Status display | ✅ Full | - |
| Save/Load State | `saveState()`, `loadState()` | PlayToolbarPanel | Save/Load slots | ✅ Full | - |
| Event Callbacks | `setEventCallback()` | StoryGraphPanel | Current node | ✅ Full | - |
| **VirtualMachine** | | | | | |
| Execution | `step()`, `run()` | PlayModeController | Step-through | ✅ Full | - |
| Instruction Pointer | `getIP()`, `setIP()` | DebugOverlayPanel | Current instruction | ✅ Full | - |
| Call Stack | Stack tracking | DebugOverlayPanel | Stack frames | ✅ Full | - |
| **Compiler** | | | | | |
| Compile | `compile()` | ScriptEditorPanel | Background compile | ✅ Full | - |
| Errors | `getErrors()` | DiagnosticsPanel | Error list | ✅ Full | - |
| **Validator** | | | | | |
| Validate | `validate()` | ScriptEditorPanel | On-save validation | ✅ Full | - |
| Unused Warnings | `setReportUnused()` | - | - | 🟡 Partial | Not configurable |
| Dead Code | `setReportDeadCode()` | - | - | 🟡 Partial | Not configurable |
| **IR System** | | | | | |
| AST to IR | `ASTToIRConverter` | StoryGraphPanel | Graph visualization | ✅ Full | - |
| IR to AST | `IRToASTConverter` | StoryGraphPanel | Code generation | ✅ Full | - |
| Visual Graph | `VisualGraph` | StoryGraphPanel | Node editing | ✅ Full | - |
| Graph Validation | `validate()` | StoryGraphPanel | Error indicators | ✅ Full | - |
| Auto Layout | `autoLayout()` | - | - | 🔴 Missing | No auto-layout UI |
| **Lexer/Parser** | | | | | |
| Syntax Highlight | Token types | ScriptEditorPanel | Coloring | ✅ Full | - |
| Error Recovery | Error collection | ScriptEditorPanel | Inline errors | ✅ Full | - |
| **VM Security** | | | | | |
| Security Limits | `VMSecurityLimits` | - | - | 🔴 Missing | No limits configuration UI |
| Violation Reporting | `SecurityViolation` | Console | Error messages | 🟡 Partial | Basic logging |

### Priority Actions

| Priority | Action | Effort | Files |
|----------|--------|--------|-------|
| MEDIUM | Add skip mode toggle | Low | `nm_play_toolbar_panel.cpp` |
| MEDIUM | Add auto-layout button to StoryGraph | Low | `nm_story_graph_panel.cpp` |
| LOW | Add validator configuration UI | Low | Settings dialog |
| LOW | Add security limits configuration | Low | Settings dialog |

---

## 6. Project/Settings System

### Core API Location
- `editor/src/project_manager.cpp`
- `editor/src/settings_registry.cpp`

### UI Exposure
- `editor/src/qt/panels/nm_project_settings_panel.cpp`
- `editor/src/qt/nm_settings_dialog.cpp`
- `editor/src/qt/nm_welcome_dialog.cpp`
- `editor/src/qt/nm_new_project_dialog.cpp`

### Feature Parity Table

| Core Feature | Core API | UI Panel | Actions | Status | Gap Description |
|-------------|----------|----------|---------|--------|-----------------|
| **ProjectManager** | | | | | |
| Create Project | `createProject()` | NewProjectDialog | Wizard | ✅ Full | - |
| Open Project | `openProject()` | WelcomeDialog, Menu | Open button | ✅ Full | - |
| Save Project | `saveProject()`, `saveProjectAs()` | MainWindow | Ctrl+S | ✅ Full | - |
| Close Project | `closeProject()` | MainWindow | Menu | ✅ Full | - |
| Project Templates | 3 templates | NewProjectDialog | Selection | ✅ Full | - |
| Recent Projects | `getRecentProjects()` | WelcomeDialog | List | ✅ Full | - |
| Auto-Save | `setAutoSaveEnabled()`, `setAutoSaveInterval()` | SettingsDialog | Toggle/interval | ✅ Full | - |
| Backups | `createBackup()`, `restoreFromBackup()` | - | - | 🔴 Missing | No backup management UI |
| Validate Project | `validateProject()` | - | - | 🔴 Missing | Stub implementation |
| Project Metadata | `getMetadata()`, `setMetadata()` | ProjectSettingsPanel | Form fields | 🟡 Partial | JSON parsing incomplete |
| **SettingsRegistry** | | | | | |
| User Settings | 26 settings | SettingsDialog | Tree view | ✅ Full | - |
| Project Settings | 11 settings | ProjectSettingsPanel | Tabs | ✅ Full | - |
| Search Settings | `search()` | SettingsDialog | Search bar | ✅ Full | - |
| Reset to Defaults | `resetToDefault()`, `resetAllToDefaults()` | SettingsDialog | Reset buttons | ✅ Full | - |
| Schema Versioning | `getSchemaVersion()` | - | - | ✅ Full | Internal |
| **Build System** | | | | | |
| Build Settings | Platform, profiles, compression | BuildSettingsPanel | Form | ✅ Full | - |
| Build Size Preview | `calculateBuildSize()` | BuildSettingsPanel | Estimate | ✅ Full | - |
| Build Warnings | `scanForWarnings()` | BuildSettingsPanel | Warning list | ✅ Full | - |
| Execute Build | `startBuild()` | BuildSettingsPanel | Build button | 🟡 Partial | UI exists, backend stub |
| Build Progress | `buildProgress` signal | BuildSettingsPanel | Progress bar | 🟡 Partial | Framework ready |

### Priority Actions

| Priority | Action | Effort | Files |
|----------|--------|--------|-------|
| HIGH | Complete project validation | Medium | `project_manager.cpp` |
| HIGH | Fix JSON metadata parsing | Medium | `project_manager.cpp` |
| MEDIUM | Add backup management UI | Medium | New dialog |
| MEDIUM | Implement build execution backend | High | `build_system.cpp` |

---

## 7. Diagnostics System

### Core API Location
- `engine_core/include/NovelMind/scripting/script_error.hpp`
- `engine_core/include/NovelMind/core/logger.hpp`

### UI Exposure
- `editor/src/qt/panels/nm_diagnostics_panel.cpp`
- `editor/src/qt/panels/nm_console_panel.cpp`
- `editor/src/qt/panels/nm_issues_panel.cpp`
- `editor/src/error_reporter.cpp`

### Feature Parity Table

| Core Feature | Core API | UI Panel | Actions | Status | Gap Description |
|-------------|----------|----------|---------|--------|-----------------|
| **ScriptError** | | | | | |
| Error Display | `ScriptError` struct | DiagnosticsPanel | Tree view | ✅ Full | - |
| Severity Levels | `Severity` enum (4 levels) | DiagnosticsPanel | Color coding | ✅ Full | - |
| Error Codes | `ErrorCode` enum (40+ codes) | DiagnosticsPanel | Code display | ✅ Full | - |
| Location Info | `SourceSpan`, line/column | DiagnosticsPanel | Location column | ✅ Full | - |
| Navigate to Source | `span` property | DiagnosticsPanel | Double-click | ✅ Full | Via `handleNavigationRequest()` |
| Related Info | `withRelated()` | - | - | 🔴 Missing | No related info display |
| Suggestions | `withSuggestion()` | - | - | 🔴 Missing | No suggestions display |
| **ErrorList** | | | | | |
| Aggregate Errors | `errors()`, `warnings()` | DiagnosticsPanel | Filter tabs | ✅ Full | - |
| Error Count | `errorCount()`, `warningCount()` | DiagnosticsPanel | Status bar | ✅ Full | - |
| **Logger** | | | | | |
| Log Messages | `log()` levels | ConsolePanel | Message list | ✅ Full | - |
| Level Filtering | Debug/Info/Warning/Error | ConsolePanel | Filter dropdown | ✅ Full | - |
| Auto-scroll | - | ConsolePanel | Toggle | ✅ Full | - |
| Copy to Clipboard | - | ConsolePanel | Context menu | ✅ Full | - |
| Clear | - | ConsolePanel | Clear button | ✅ Full | - |
| **Quick Fixes** | | | | | |
| Suggested Fixes | - | - | - | 🔴 Missing | No quick fix system |
| Auto-Fix | - | - | - | 🔴 Missing | No auto-fix |

### Priority Actions

| Priority | Action | Effort | Files |
|----------|--------|--------|-------|
| MEDIUM | Add suggestions display | Low | `nm_diagnostics_panel.cpp` |
| MEDIUM | Add related info display | Low | `nm_diagnostics_panel.cpp` |
| LOW | Add quick fix system | High | New system |

---

## 8. Property System

### Core API Location
- `engine_core/include/NovelMind/core/property_system.hpp`
- `engine_core/include/NovelMind/scene/scene_object_properties.hpp`

### UI Exposure
- `editor/src/qt/panels/nm_inspector_panel.cpp`
- `editor/src/inspector_binding.cpp`

### Feature Parity Table

| Core Feature | Core API | UI Panel | Actions | Status | Gap Description |
|-------------|----------|----------|---------|--------|-----------------|
| **PropertyRegistry** | | | | | |
| Type Registration | `registerType()` | - | - | ✅ Full | Automatic |
| Property Access | `IPropertyAccessor` | Inspector | Property grid | ✅ Full | - |
| **PropertyTypes** | | | | | |
| Bool | `PropertyType::Bool` | Inspector | Checkbox | ✅ Full | - |
| Int | `PropertyType::Int` | Inspector | Spin box | ✅ Full | - |
| Float | `PropertyType::Float` | Inspector | Spin box | ✅ Full | - |
| String | `PropertyType::String` | Inspector | Text field | ✅ Full | - |
| MultiLine String | `PropertyFlags::MultiLine` | Inspector | Text area | ✅ Full | - |
| Vector2 | `PropertyType::Vector2` | Inspector | XY fields | ✅ Full | - |
| Vector3 | `PropertyType::Vector3` | Inspector | XYZ fields | ✅ Full | - |
| Color | `PropertyType::Color` | Inspector | Color picker | ✅ Full | - |
| Enum | `PropertyType::Enum` | Inspector | Dropdown | ✅ Full | - |
| AssetRef | `PropertyType::AssetRef` | Inspector | File picker | ✅ Full | - |
| CurveRef | `PropertyType::CurveRef` | Inspector | Curve button | 🟡 Partial | Opens curve editor |
| **PropertyFlags** | | | | | |
| ReadOnly | `PropertyFlags::ReadOnly` | Inspector | Disabled field | ✅ Full | - |
| Hidden | `PropertyFlags::Hidden` | Inspector | Not shown | ✅ Full | - |
| Foldout | `PropertyFlags::Foldout` | Inspector | Collapsible | ✅ Full | - |
| Slider | `PropertyFlags::Slider` | Inspector | Slider control | ✅ Full | - |
| Range | `RangeConstraint` | Inspector | Min/Max validation | ✅ Full | - |
| Tooltip | `PropertyMeta.tooltip` | Inspector | Hover text | ✅ Full | - |
| NoUndo | `PropertyFlags::NoUndo` | Inspector | Skip undo | 🟡 Partial | Framework ready |
| Required | `PropertyFlags::Required` | Inspector | Highlight | 🟡 Partial | Framework ready |
| **Utilities** | | | | | |
| Value Validation | `PropertyUtils::validate()` | Inspector | Input validation | ✅ Full | - |
| Type Conversion | `PropertyUtils::fromString()` | Inspector | String parsing | ✅ Full | - |

### Priority Actions

| Priority | Action | Effort | Files |
|----------|--------|--------|-------|
| LOW | Complete CurveRef inline editing | Low | `nm_inspector_panel.cpp` |
| LOW | Add Required field highlighting | Low | `nm_inspector_panel.cpp` |

---

## Gap Summary & Action Plan

### Critical Gaps (Must Fix)

| # | System | Gap | Impact | Fix Plan |
|---|--------|-----|--------|----------|
| 1 | Audio | VoiceManifest not integrated with GUI | Multi-locale voice production impossible | Integrate VoiceManifest into VoiceManager |
| 2 | Project | JSON metadata parsing incomplete | Data loss on save | Implement proper JSON parser |
| 3 | Project | Validation stubbed | No integrity checking | Complete validateProject() |

### High Priority Gaps

| # | System | Gap | Impact | Fix Plan |
|---|--------|-----|--------|----------|
| 5 | Audio | No multi-locale UI | Single language only | Add locale selector to VoiceManagerPanel |
| 6 | Audio | No take management | No recording workflow | Add takes UI |
| 7 | Scene | No drag-drop reparenting | Inefficient hierarchy editing | Implement drag handlers |
| 8 | Build | Build execution not implemented | Cannot produce builds | Implement build backend |

### Medium Priority Gaps

| # | System | Gap | Fix Plan |
|---|--------|-----|----------|
| 9 | Audio | No per-channel volume | Add AudioMixer panel |
| 10 | Localization | No plural editing | Add plural forms UI |
| 11 | Script | No skip mode toggle | Add toolbar button |
| 12 | Script | No auto-layout | Add layout button |
| 13 | Project | No backup management | Add backup dialog |
| 14 | Diagnostics | No suggestions display | Enhance error view |

### Low Priority Gaps (Future Work)

| # | System | Gap |
|---|--------|-----|
| 15 | Audio | Recording format selection |
| 16 | Audio | Input monitoring toggle |
| 17 | Localization | RTL preview |
| 18 | Localization | PO/XLIFF export |
| 19 | Animation | Remaining easing types |
| 20 | Animation | Loop/yoyo controls |
| 21 | Scene | Tag editing UI |
| 22 | Scene | Transition preview |
| 23 | Script | Validator config |
| 24 | Script | Security limits config |
| 25 | Diagnostics | Quick fix system |

---

## Regression Prevention Checklist

### Mandatory Panel Capabilities

Every core system MUST have the following GUI exposure:

- [ ] **Audio**: Playback preview, volume control, file management
- [ ] **Localization**: String editing, import/export, missing key detection
- [ ] **Animation**: Keyframe editing, easing selection, preview playback
- [ ] **SceneGraph**: Object CRUD, transform gizmo, property inspector
- [ ] **Script**: Syntax highlighting, error display, breakpoints
- [ ] **Project**: Open/save/close, settings editing, recent projects
- [ ] **Diagnostics**: Error list, severity filtering, navigation

### Editor Capabilities Check

Add to editor startup (optional debug mode):

```cpp
void checkEditorCapabilities() {
    // Audio
    assert(hasPanel<NMVoiceManagerPanel>());
    assert(canPreviewAudio());

    // Localization
    assert(hasPanel<NMLocalizationPanel>());
    assert(canImportExportStrings());

    // Animation
    assert(hasPanel<NMTimelinePanel>());
    assert(hasPanel<NMCurveEditorPanel>());

    // Scene
    assert(hasPanel<NMSceneViewPanel>());
    assert(hasPanel<NMHierarchyPanel>());
    assert(hasPanel<NMInspectorPanel>());

    // Script
    assert(hasPanel<NMScriptEditorPanel>());
    assert(hasPanel<NMStoryGraphPanel>());

    // Diagnostics
    assert(hasPanel<NMDiagnosticsPanel>());
    assert(hasPanel<NMConsolePanel>());

    // Play Mode
    assert(hasPanel<NMPlayToolbarPanel>());
    assert(hasPanel<NMDebugOverlayPanel>());
}
```

### Feature Addition Checklist

When adding new core features:

1. [ ] Document feature in this parity matrix
2. [ ] Identify required UI exposure
3. [ ] Create/update panel(s) to expose feature
4. [ ] Add to regression checklist
5. [ ] Test feature through UI

---

*Document is a living document and should be updated as features are added or gaps are closed.*
