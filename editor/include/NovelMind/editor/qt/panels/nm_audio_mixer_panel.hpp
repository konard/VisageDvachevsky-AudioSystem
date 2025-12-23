#pragma once

/**
 * @file nm_audio_mixer_panel.hpp
 * @brief Audio Mixer & Preview panel for editor
 *
 * Provides comprehensive audio preview and mixing capabilities:
 * - Music playback controls (play/pause/resume/stop)
 * - Seek slider with position display
 * - Loop toggle
 * - Crossfade controls (duration + action)
 * - Auto-ducking configuration (enable, amount, attack/release, threshold)
 * - Master volume control
 * - Per-channel volume controls (6 channels)
 * - Mute/solo per channel (if applicable)
 * - Asset selection from Asset Browser
 * - Preview playback without affecting runtime state
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/audio/audio_manager.hpp"

#include <QHash>
#include <QPointer>
#include <QToolBar>
#include <QWidget>
#include <QTimer>

#include <memory>

// Forward declarations for Qt Multimedia
class QMediaPlayer;
class QAudioOutput;

class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QToolBar;
class QPushButton;
class QLabel;
class QLineEdit;
class QComboBox;
class QSlider;
class QSplitter;
class QProgressBar;
class QGroupBox;
class QCheckBox;
class QDoubleSpinBox;

namespace NovelMind::editor::qt {

/**
 * @brief Audio channel control widget
 */
struct ChannelControl {
  audio::AudioChannel channel;
  QLabel *nameLabel = nullptr;
  QSlider *volumeSlider = nullptr;
  QLabel *volumeLabel = nullptr;
  QPushButton *muteButton = nullptr;
  QPushButton *soloButton = nullptr;
};

class NMAudioMixerPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMAudioMixerPanel(QWidget *parent = nullptr);
  ~NMAudioMixerPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Set the currently selected audio asset for preview
   */
  void setSelectedAudioAsset(const QString &assetPath);

  /**
   * @brief Get the current audio manager instance (for editor preview)
   */
  [[nodiscard]] audio::AudioManager *getPreviewAudioManager() const {
    return m_previewAudioManager.get();
  }

signals:
  /**
   * @brief Emitted when an audio asset is selected for preview
   */
  void audioAssetSelected(const QString &assetPath);

  /**
   * @brief Emitted when a playback error occurs
   */
  void playbackError(const QString &errorMessage);

private slots:
  // Music preview controls
  void onPlayClicked();
  void onPauseClicked();
  void onStopClicked();
  void onLoopToggled(bool checked);
  void onSeekSliderMoved(int value);
  void onSeekSliderReleased();

  // Crossfade controls
  void onCrossfadeDurationChanged(double value);
  void onCrossfadeToClicked();

  // Auto-ducking controls
  void onDuckingEnabledToggled(bool checked);
  void onDuckAmountChanged(double value);
  void onDuckAttackChanged(double value);
  void onDuckReleaseChanged(double value);

  // Mixer controls
  void onMasterVolumeChanged(int value);
  void onChannelVolumeChanged(int value);
  void onChannelMuteToggled(bool checked);
  void onChannelSoloToggled(bool checked);

  // Asset browser integration
  void onBrowseAudioClicked();
  void onAssetSelected(const QString &assetPath);

  // Position update timer
  void onUpdatePosition();

private:
  void setupUI();
  void setupMusicPreviewControls(QWidget *parent);
  void setupCrossfadeControls(QWidget *parent);
  void setupDuckingControls(QWidget *parent);
  void setupMixerControls(QWidget *parent);
  void setupAssetBrowser(QWidget *parent);

  void updatePlaybackState();
  void updatePositionDisplay();
  void resetPlaybackUI();
  void setPlaybackError(const QString &message);
  QString formatTime(f32 seconds) const;

  void applyChannelVolumes();
  void updateSoloState();

  // UI Elements - Music Preview
  QGroupBox *m_previewGroup = nullptr;
  QLabel *m_currentTrackLabel = nullptr;
  QPushButton *m_playBtn = nullptr;
  QPushButton *m_pauseBtn = nullptr;
  QPushButton *m_stopBtn = nullptr;
  QCheckBox *m_loopCheckBox = nullptr;
  QSlider *m_seekSlider = nullptr;
  QLabel *m_positionLabel = nullptr;
  QLabel *m_durationLabel = nullptr;
  QPushButton *m_browseBtn = nullptr;

  // UI Elements - Crossfade
  QGroupBox *m_crossfadeGroup = nullptr;
  QDoubleSpinBox *m_crossfadeDurationSpin = nullptr;
  QPushButton *m_crossfadeBtn = nullptr;

  // UI Elements - Auto-ducking
  QGroupBox *m_duckingGroup = nullptr;
  QCheckBox *m_duckingEnabledCheckBox = nullptr;
  QDoubleSpinBox *m_duckAmountSpin = nullptr;
  QDoubleSpinBox *m_duckAttackSpin = nullptr;
  QDoubleSpinBox *m_duckReleaseSpin = nullptr;

  // UI Elements - Mixer
  QGroupBox *m_mixerGroup = nullptr;
  QSlider *m_masterVolumeSlider = nullptr;
  QLabel *m_masterVolumeLabel = nullptr;
  std::vector<ChannelControl> m_channelControls;

  // Audio playback state
  std::unique_ptr<audio::AudioManager> m_previewAudioManager;
  audio::AudioHandle m_currentMusicHandle;
  QString m_currentAudioAsset;
  QString m_nextCrossfadeAsset;
  bool m_isPlaying = false;
  bool m_isPaused = false;
  bool m_isSeeking = false;
  f32 m_currentPosition = 0.0f;
  f32 m_currentDuration = 0.0f;

  // Crossfade settings
  f32 m_crossfadeDuration = 1000.0f; // ms

  // Ducking settings
  bool m_duckingEnabled = true;
  f32 m_duckAmount = 0.3f;
  f32 m_duckFadeDuration = 0.2f;

  // Solo state
  int m_soloChannelIndex = -1; // -1 = no solo

  // Update timer for position display
  QTimer *m_positionTimer = nullptr;

  // Verbose logging flag
  static constexpr bool VERBOSE_LOGGING = false;
};

} // namespace NovelMind::editor::qt
