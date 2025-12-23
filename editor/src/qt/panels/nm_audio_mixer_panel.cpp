#include "NovelMind/editor/qt/panels/nm_audio_mixer_panel.hpp"
#include "NovelMind/core/logger.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QTimer>
#include <QMessageBox>
#include <QStyle>

namespace NovelMind::editor::qt {

NMAudioMixerPanel::NMAudioMixerPanel(QWidget *parent)
    : NMDockPanel(tr("Audio Mixer"), parent),
      m_previewAudioManager(std::make_unique<audio::AudioManager>()),
      m_positionTimer(new QTimer(this)) {
  setupUI();

  // Connect position update timer
  connect(m_positionTimer, &QTimer::timeout, this,
          &NMAudioMixerPanel::onUpdatePosition);
  m_positionTimer->setInterval(100); // Update every 100ms
}

NMAudioMixerPanel::~NMAudioMixerPanel() { onShutdown(); }

void NMAudioMixerPanel::onInitialize() {
  if (VERBOSE_LOGGING) {
    LOG_INFO("Initializing Audio Mixer Panel");
  }

  // Initialize preview audio manager
  auto result = m_previewAudioManager->initialize();
  if (!result) {
    LOG_ERROR("Failed to initialize preview audio manager: {}",
              result.error());
    setPlaybackError(
        QString::fromStdString("Failed to initialize audio: " + result.error()));
    return;
  }

  // Set up default ducking parameters
  m_previewAudioManager->setAutoDuckingEnabled(m_duckingEnabled);
  m_previewAudioManager->setDuckingParams(m_duckAmount, m_duckFadeDuration);

  if (VERBOSE_LOGGING) {
    LOG_INFO("Audio Mixer Panel initialized successfully");
  }
}

void NMAudioMixerPanel::onShutdown() {
  if (VERBOSE_LOGGING) {
    LOG_INFO("Shutting down Audio Mixer Panel");
  }

  // Stop any playing audio
  if (m_previewAudioManager) {
    m_previewAudioManager->stopAll();
    m_previewAudioManager->shutdown();
  }

  m_positionTimer->stop();
}

void NMAudioMixerPanel::onUpdate(double deltaTime) {
  if (m_previewAudioManager) {
    m_previewAudioManager->update(deltaTime);
  }
}

void NMAudioMixerPanel::setSelectedAudioAsset(const QString &assetPath) {
  m_currentAudioAsset = assetPath;
  m_currentTrackLabel->setText(QFileInfo(assetPath).fileName());

  if (VERBOSE_LOGGING) {
    LOG_INFO("Audio asset selected: {}", assetPath.toStdString());
  }
}

void NMAudioMixerPanel::setupUI() {
  auto *mainWidget = new QWidget(this);
  auto *mainLayout = new QVBoxLayout(mainWidget);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(8);

  // Setup each section
  setupAssetBrowser(mainWidget);
  setupMusicPreviewControls(mainWidget);
  setupCrossfadeControls(mainWidget);
  setupDuckingControls(mainWidget);
  setupMixerControls(mainWidget);

  mainLayout->addStretch();
  setContentWidget(mainWidget);
}

void NMAudioMixerPanel::setupAssetBrowser(QWidget *parent) {
  auto *layout = qobject_cast<QVBoxLayout *>(parent->layout());
  if (!layout)
    return;

  auto *group = new QGroupBox(tr("Audio Asset Selection"), parent);
  auto *groupLayout = new QVBoxLayout(group);

  auto *assetLayout = new QHBoxLayout();
  m_currentTrackLabel = new QLabel(tr("No asset selected"), group);
  m_currentTrackLabel->setWordWrap(true);
  m_browseBtn = new QPushButton(tr("Browse..."), group);

  assetLayout->addWidget(m_currentTrackLabel, 1);
  assetLayout->addWidget(m_browseBtn);

  groupLayout->addLayout(assetLayout);
  layout->addWidget(group);

  connect(m_browseBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onBrowseAudioClicked);
}

void NMAudioMixerPanel::setupMusicPreviewControls(QWidget *parent) {
  auto *layout = qobject_cast<QVBoxLayout *>(parent->layout());
  if (!layout)
    return;

  m_previewGroup = new QGroupBox(tr("Music Preview"), parent);
  auto *groupLayout = new QVBoxLayout(m_previewGroup);

  // Playback controls
  auto *controlsLayout = new QHBoxLayout();
  m_playBtn = new QPushButton(tr("Play"), m_previewGroup);
  m_playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  m_pauseBtn = new QPushButton(tr("Pause"), m_previewGroup);
  m_pauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
  m_pauseBtn->setEnabled(false);
  m_stopBtn = new QPushButton(tr("Stop"), m_previewGroup);
  m_stopBtn->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
  m_stopBtn->setEnabled(false);
  m_loopCheckBox = new QCheckBox(tr("Loop"), m_previewGroup);

  controlsLayout->addWidget(m_playBtn);
  controlsLayout->addWidget(m_pauseBtn);
  controlsLayout->addWidget(m_stopBtn);
  controlsLayout->addWidget(m_loopCheckBox);
  controlsLayout->addStretch();

  groupLayout->addLayout(controlsLayout);

  // Seek slider
  auto *seekLayout = new QHBoxLayout();
  m_positionLabel = new QLabel(tr("0:00"), m_previewGroup);
  m_seekSlider = new QSlider(Qt::Horizontal, m_previewGroup);
  m_seekSlider->setRange(0, 1000);
  m_seekSlider->setValue(0);
  m_seekSlider->setEnabled(false);
  m_durationLabel = new QLabel(tr("0:00"), m_previewGroup);

  seekLayout->addWidget(m_positionLabel);
  seekLayout->addWidget(m_seekSlider, 1);
  seekLayout->addWidget(m_durationLabel);

  groupLayout->addLayout(seekLayout);
  layout->addWidget(m_previewGroup);

  // Connect signals
  connect(m_playBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onPlayClicked);
  connect(m_pauseBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onPauseClicked);
  connect(m_stopBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onStopClicked);
  connect(m_loopCheckBox, &QCheckBox::toggled, this,
          &NMAudioMixerPanel::onLoopToggled);
  connect(m_seekSlider, &QSlider::sliderMoved, this,
          &NMAudioMixerPanel::onSeekSliderMoved);
  connect(m_seekSlider, &QSlider::sliderReleased, this,
          &NMAudioMixerPanel::onSeekSliderReleased);
}

void NMAudioMixerPanel::setupCrossfadeControls(QWidget *parent) {
  auto *layout = qobject_cast<QVBoxLayout *>(parent->layout());
  if (!layout)
    return;

  m_crossfadeGroup = new QGroupBox(tr("Crossfade"), parent);
  auto *groupLayout = new QHBoxLayout(m_crossfadeGroup);

  auto *durationLabel = new QLabel(tr("Duration (ms):"), m_crossfadeGroup);
  m_crossfadeDurationSpin = new QDoubleSpinBox(m_crossfadeGroup);
  m_crossfadeDurationSpin->setRange(0, 10000);
  m_crossfadeDurationSpin->setValue(m_crossfadeDuration);
  m_crossfadeDurationSpin->setSuffix(tr(" ms"));
  m_crossfadeDurationSpin->setDecimals(0);

  m_crossfadeBtn = new QPushButton(tr("Crossfade To Selected"), m_crossfadeGroup);
  m_crossfadeBtn->setEnabled(false);

  groupLayout->addWidget(durationLabel);
  groupLayout->addWidget(m_crossfadeDurationSpin);
  groupLayout->addWidget(m_crossfadeBtn);
  groupLayout->addStretch();

  layout->addWidget(m_crossfadeGroup);

  connect(m_crossfadeDurationSpin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &NMAudioMixerPanel::onCrossfadeDurationChanged);
  connect(m_crossfadeBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onCrossfadeToClicked);
}

void NMAudioMixerPanel::setupDuckingControls(QWidget *parent) {
  auto *layout = qobject_cast<QVBoxLayout *>(parent->layout());
  if (!layout)
    return;

  m_duckingGroup = new QGroupBox(tr("Auto-Ducking"), parent);
  auto *groupLayout = new QGridLayout(m_duckingGroup);

  m_duckingEnabledCheckBox = new QCheckBox(tr("Enable Auto-Ducking"), m_duckingGroup);
  m_duckingEnabledCheckBox->setChecked(m_duckingEnabled);

  auto *amountLabel = new QLabel(tr("Duck Amount:"), m_duckingGroup);
  m_duckAmountSpin = new QDoubleSpinBox(m_duckingGroup);
  m_duckAmountSpin->setRange(0.0, 1.0);
  m_duckAmountSpin->setValue(m_duckAmount);
  m_duckAmountSpin->setSingleStep(0.05);
  m_duckAmountSpin->setDecimals(2);

  auto *attackLabel = new QLabel(tr("Attack (s):"), m_duckingGroup);
  m_duckAttackSpin = new QDoubleSpinBox(m_duckingGroup);
  m_duckAttackSpin->setRange(0.0, 5.0);
  m_duckAttackSpin->setValue(m_duckFadeDuration);
  m_duckAttackSpin->setSingleStep(0.1);
  m_duckAttackSpin->setDecimals(2);
  m_duckAttackSpin->setSuffix(tr(" s"));

  auto *releaseLabel = new QLabel(tr("Release (s):"), m_duckingGroup);
  m_duckReleaseSpin = new QDoubleSpinBox(m_duckingGroup);
  m_duckReleaseSpin->setRange(0.0, 5.0);
  m_duckReleaseSpin->setValue(m_duckFadeDuration);
  m_duckReleaseSpin->setSingleStep(0.1);
  m_duckReleaseSpin->setDecimals(2);
  m_duckReleaseSpin->setSuffix(tr(" s"));

  groupLayout->addWidget(m_duckingEnabledCheckBox, 0, 0, 1, 2);
  groupLayout->addWidget(amountLabel, 1, 0);
  groupLayout->addWidget(m_duckAmountSpin, 1, 1);
  groupLayout->addWidget(attackLabel, 2, 0);
  groupLayout->addWidget(m_duckAttackSpin, 2, 1);
  groupLayout->addWidget(releaseLabel, 3, 0);
  groupLayout->addWidget(m_duckReleaseSpin, 3, 1);

  layout->addWidget(m_duckingGroup);

  connect(m_duckingEnabledCheckBox, &QCheckBox::toggled, this,
          &NMAudioMixerPanel::onDuckingEnabledToggled);
  connect(m_duckAmountSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMAudioMixerPanel::onDuckAmountChanged);
  connect(m_duckAttackSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMAudioMixerPanel::onDuckAttackChanged);
  connect(m_duckReleaseSpin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &NMAudioMixerPanel::onDuckReleaseChanged);
}

void NMAudioMixerPanel::setupMixerControls(QWidget *parent) {
  auto *layout = qobject_cast<QVBoxLayout *>(parent->layout());
  if (!layout)
    return;

  m_mixerGroup = new QGroupBox(tr("Volume Mixer"), parent);
  auto *groupLayout = new QVBoxLayout(m_mixerGroup);

  // Master volume
  auto *masterLayout = new QHBoxLayout();
  auto *masterLabel = new QLabel(tr("Master:"), m_mixerGroup);
  masterLabel->setMinimumWidth(60);
  m_masterVolumeSlider = new QSlider(Qt::Horizontal, m_mixerGroup);
  m_masterVolumeSlider->setRange(0, 100);
  m_masterVolumeSlider->setValue(100);
  m_masterVolumeLabel = new QLabel(tr("100%"), m_mixerGroup);
  m_masterVolumeLabel->setMinimumWidth(40);

  masterLayout->addWidget(masterLabel);
  masterLayout->addWidget(m_masterVolumeSlider, 1);
  masterLayout->addWidget(m_masterVolumeLabel);

  groupLayout->addLayout(masterLayout);

  // Per-channel volumes
  const std::array<std::pair<audio::AudioChannel, QString>, 6> channels = {{
      {audio::AudioChannel::Master, tr("Master")},
      {audio::AudioChannel::Music, tr("Music")},
      {audio::AudioChannel::Sound, tr("Sound")},
      {audio::AudioChannel::Voice, tr("Voice")},
      {audio::AudioChannel::Ambient, tr("Ambient")},
      {audio::AudioChannel::UI, tr("UI")},
  }};

  for (size_t i = 1; i < channels.size(); ++i) { // Skip Master (already handled)
    const auto &[channel, name] = channels[i];

    ChannelControl control;
    control.channel = channel;

    auto *channelLayout = new QHBoxLayout();

    control.nameLabel = new QLabel(name + ":", m_mixerGroup);
    control.nameLabel->setMinimumWidth(60);

    control.volumeSlider = new QSlider(Qt::Horizontal, m_mixerGroup);
    control.volumeSlider->setRange(0, 100);
    control.volumeSlider->setValue(100);
    control.volumeSlider->setProperty("channelIndex", static_cast<int>(i - 1));

    control.volumeLabel = new QLabel(tr("100%"), m_mixerGroup);
    control.volumeLabel->setMinimumWidth(40);

    control.muteButton = new QPushButton(tr("M"), m_mixerGroup);
    control.muteButton->setCheckable(true);
    control.muteButton->setMaximumWidth(30);
    control.muteButton->setProperty("channelIndex", static_cast<int>(i - 1));

    control.soloButton = new QPushButton(tr("S"), m_mixerGroup);
    control.soloButton->setCheckable(true);
    control.soloButton->setMaximumWidth(30);
    control.soloButton->setProperty("channelIndex", static_cast<int>(i - 1));

    channelLayout->addWidget(control.nameLabel);
    channelLayout->addWidget(control.volumeSlider, 1);
    channelLayout->addWidget(control.volumeLabel);
    channelLayout->addWidget(control.muteButton);
    channelLayout->addWidget(control.soloButton);

    groupLayout->addLayout(channelLayout);
    m_channelControls.push_back(control);

    // Connect signals
    connect(control.volumeSlider, &QSlider::valueChanged, this,
            &NMAudioMixerPanel::onChannelVolumeChanged);
    connect(control.muteButton, &QPushButton::toggled, this,
            &NMAudioMixerPanel::onChannelMuteToggled);
    connect(control.soloButton, &QPushButton::toggled, this,
            &NMAudioMixerPanel::onChannelSoloToggled);
  }

  layout->addWidget(m_mixerGroup);

  connect(m_masterVolumeSlider, &QSlider::valueChanged, this,
          &NMAudioMixerPanel::onMasterVolumeChanged);
}

// Music preview control slots
void NMAudioMixerPanel::onPlayClicked() {
  if (m_currentAudioAsset.isEmpty()) {
    setPlaybackError(tr("No audio asset selected"));
    return;
  }

  if (!m_previewAudioManager) {
    setPlaybackError(tr("Audio manager not initialized"));
    return;
  }

  if (m_isPaused) {
    // Resume from pause
    m_previewAudioManager->resumeMusic();
    m_isPaused = false;
    m_isPlaying = true;
  } else {
    // Start new playback
    audio::MusicConfig config;
    config.loop = m_loopCheckBox->isChecked();
    config.volume = 1.0f;

    m_currentMusicHandle =
        m_previewAudioManager->playMusic(m_currentAudioAsset.toStdString(), config);

    if (!m_currentMusicHandle.isValid()) {
      setPlaybackError(tr("Failed to play audio"));
      return;
    }

    m_isPlaying = true;
    m_isPaused = false;

    // Get duration
    auto *source = m_previewAudioManager->getSource(m_currentMusicHandle);
    if (source) {
      m_currentDuration = source->getDuration();
      m_durationLabel->setText(formatTime(m_currentDuration));
    }
  }

  updatePlaybackState();
  m_positionTimer->start();

  if (VERBOSE_LOGGING) {
    LOG_INFO("Playback started: {}", m_currentAudioAsset.toStdString());
  }
}

void NMAudioMixerPanel::onPauseClicked() {
  if (!m_previewAudioManager || !m_isPlaying) {
    return;
  }

  m_previewAudioManager->pauseMusic();
  m_isPaused = true;
  m_isPlaying = false;
  m_positionTimer->stop();

  updatePlaybackState();

  if (VERBOSE_LOGGING) {
    LOG_INFO("Playback paused");
  }
}

void NMAudioMixerPanel::onStopClicked() {
  if (!m_previewAudioManager) {
    return;
  }

  m_previewAudioManager->stopMusic();
  m_isPlaying = false;
  m_isPaused = false;
  m_currentPosition = 0.0f;
  m_positionTimer->stop();

  resetPlaybackUI();

  if (VERBOSE_LOGGING) {
    LOG_INFO("Playback stopped");
  }
}

void NMAudioMixerPanel::onLoopToggled(bool checked) {
  if (VERBOSE_LOGGING) {
    LOG_INFO("Loop toggled: {}", checked);
  }
  // Loop state is applied when playback starts
}

void NMAudioMixerPanel::onSeekSliderMoved(int value) {
  m_isSeeking = true;
  if (m_currentDuration > 0) {
    f32 position = (value / 1000.0f) * m_currentDuration;
    m_positionLabel->setText(formatTime(position));
  }
}

void NMAudioMixerPanel::onSeekSliderReleased() {
  if (!m_previewAudioManager || !m_currentMusicHandle.isValid()) {
    m_isSeeking = false;
    return;
  }

  f32 position = (m_seekSlider->value() / 1000.0f) * m_currentDuration;
  m_previewAudioManager->seekMusic(position);
  m_currentPosition = position;
  m_isSeeking = false;

  if (VERBOSE_LOGGING) {
    LOG_INFO("Seeked to position: {:.2f}s", position);
  }
}

// Crossfade control slots
void NMAudioMixerPanel::onCrossfadeDurationChanged(double value) {
  m_crossfadeDuration = static_cast<f32>(value);

  if (VERBOSE_LOGGING) {
    LOG_INFO("Crossfade duration changed: {}ms", m_crossfadeDuration);
  }
}

void NMAudioMixerPanel::onCrossfadeToClicked() {
  if (m_currentAudioAsset.isEmpty()) {
    setPlaybackError(tr("No audio asset selected for crossfade"));
    return;
  }

  if (!m_previewAudioManager) {
    return;
  }

  audio::MusicConfig config;
  config.loop = m_loopCheckBox->isChecked();
  config.volume = 1.0f;
  config.crossfadeDuration = m_crossfadeDuration / 1000.0f; // Convert ms to seconds

  m_currentMusicHandle = m_previewAudioManager->crossfadeMusic(
      m_currentAudioAsset.toStdString(), config.crossfadeDuration, config);

  if (!m_currentMusicHandle.isValid()) {
    setPlaybackError(tr("Failed to crossfade to audio"));
    return;
  }

  m_isPlaying = true;
  m_isPaused = false;
  updatePlaybackState();

  if (VERBOSE_LOGGING) {
    LOG_INFO("Crossfade to: {}", m_currentAudioAsset.toStdString());
  }
}

// Auto-ducking control slots
void NMAudioMixerPanel::onDuckingEnabledToggled(bool checked) {
  m_duckingEnabled = checked;
  if (m_previewAudioManager) {
    m_previewAudioManager->setAutoDuckingEnabled(checked);
  }

  if (VERBOSE_LOGGING) {
    LOG_INFO("Auto-ducking enabled: {}", checked);
  }
}

void NMAudioMixerPanel::onDuckAmountChanged(double value) {
  m_duckAmount = static_cast<f32>(value);
  if (m_previewAudioManager) {
    m_previewAudioManager->setDuckingParams(m_duckAmount, m_duckFadeDuration);
  }

  if (VERBOSE_LOGGING) {
    LOG_INFO("Duck amount changed: {:.2f}", m_duckAmount);
  }
}

void NMAudioMixerPanel::onDuckAttackChanged(double value) {
  m_duckFadeDuration = static_cast<f32>(value);
  if (m_previewAudioManager) {
    m_previewAudioManager->setDuckingParams(m_duckAmount, m_duckFadeDuration);
  }

  if (VERBOSE_LOGGING) {
    LOG_INFO("Duck attack changed: {:.2f}s", m_duckFadeDuration);
  }
}

void NMAudioMixerPanel::onDuckReleaseChanged(double value) {
  // Note: Current API uses same duration for attack/release
  // This is here for future expansion if API supports separate values
  if (VERBOSE_LOGGING) {
    LOG_INFO("Duck release changed: {:.2f}s", value);
  }
}

// Mixer control slots
void NMAudioMixerPanel::onMasterVolumeChanged(int value) {
  f32 volume = value / 100.0f;
  m_masterVolumeLabel->setText(QString::number(value) + "%");

  if (m_previewAudioManager) {
    m_previewAudioManager->setMasterVolume(volume);
  }

  if (VERBOSE_LOGGING) {
    LOG_INFO("Master volume changed: {:.2f}", volume);
  }
}

void NMAudioMixerPanel::onChannelVolumeChanged(int value) {
  auto *slider = qobject_cast<QSlider *>(sender());
  if (!slider)
    return;

  int channelIndex = slider->property("channelIndex").toInt();
  if (channelIndex < 0 ||
      channelIndex >= static_cast<int>(m_channelControls.size()))
    return;

  f32 volume = value / 100.0f;
  auto &control = m_channelControls[channelIndex];
  control.volumeLabel->setText(QString::number(value) + "%");

  if (m_previewAudioManager) {
    m_previewAudioManager->setChannelVolume(control.channel, volume);
  }

  if (VERBOSE_LOGGING) {
    LOG_INFO("Channel {} volume changed: {:.2f}", channelIndex, volume);
  }
}

void NMAudioMixerPanel::onChannelMuteToggled(bool checked) {
  auto *button = qobject_cast<QPushButton *>(sender());
  if (!button)
    return;

  int channelIndex = button->property("channelIndex").toInt();
  if (channelIndex < 0 ||
      channelIndex >= static_cast<int>(m_channelControls.size()))
    return;

  auto &control = m_channelControls[channelIndex];

  if (m_previewAudioManager) {
    m_previewAudioManager->setChannelMuted(control.channel, checked);
  }

  if (VERBOSE_LOGGING) {
    LOG_INFO("Channel {} muted: {}", channelIndex, checked);
  }
}

void NMAudioMixerPanel::onChannelSoloToggled(bool checked) {
  auto *button = qobject_cast<QPushButton *>(sender());
  if (!button)
    return;

  int channelIndex = button->property("channelIndex").toInt();
  if (channelIndex < 0 ||
      channelIndex >= static_cast<int>(m_channelControls.size()))
    return;

  if (checked) {
    // Unsolo all other channels and mute them
    for (size_t i = 0; i < m_channelControls.size(); ++i) {
      if (static_cast<int>(i) != channelIndex) {
        m_channelControls[i].soloButton->setChecked(false);
        if (m_previewAudioManager) {
          m_previewAudioManager->setChannelMuted(m_channelControls[i].channel,
                                                  true);
        }
      }
    }
    // Unmute the soloed channel
    if (m_previewAudioManager) {
      m_previewAudioManager->setChannelMuted(
          m_channelControls[channelIndex].channel, false);
    }
    m_soloChannelIndex = channelIndex;
  } else {
    // Unsolo - restore all channels
    for (auto &control : m_channelControls) {
      if (m_previewAudioManager) {
        m_previewAudioManager->setChannelMuted(
            control.channel, control.muteButton->isChecked());
      }
    }
    m_soloChannelIndex = -1;
  }

  if (VERBOSE_LOGGING) {
    LOG_INFO("Channel {} solo: {}", channelIndex, checked);
  }
}

// Asset browser integration
void NMAudioMixerPanel::onBrowseAudioClicked() {
  QString filePath = QFileDialog::getOpenFileName(
      this, tr("Select Audio File"), QString(),
      tr("Audio Files (*.mp3 *.wav *.ogg *.flac);;All Files (*.*)"));

  if (!filePath.isEmpty()) {
    setSelectedAudioAsset(filePath);
    m_crossfadeBtn->setEnabled(true);
    emit audioAssetSelected(filePath);
  }
}

void NMAudioMixerPanel::onAssetSelected(const QString &assetPath) {
  setSelectedAudioAsset(assetPath);
  m_crossfadeBtn->setEnabled(true);
}

// Position update
void NMAudioMixerPanel::onUpdatePosition() {
  if (!m_previewAudioManager || !m_currentMusicHandle.isValid() || m_isSeeking) {
    return;
  }

  m_currentPosition = m_previewAudioManager->getMusicPosition();

  // Update position label and slider
  m_positionLabel->setText(formatTime(m_currentPosition));

  if (m_currentDuration > 0) {
    int sliderValue =
        static_cast<int>((m_currentPosition / m_currentDuration) * 1000);
    m_seekSlider->blockSignals(true);
    m_seekSlider->setValue(sliderValue);
    m_seekSlider->blockSignals(false);
  }

  // Check if playback has finished
  if (!m_previewAudioManager->isMusicPlaying() && m_isPlaying) {
    onStopClicked();
  }
}

// Helper methods
void NMAudioMixerPanel::updatePlaybackState() {
  m_playBtn->setEnabled(!m_isPlaying || m_isPaused);
  m_pauseBtn->setEnabled(m_isPlaying && !m_isPaused);
  m_stopBtn->setEnabled(m_isPlaying || m_isPaused);
  m_seekSlider->setEnabled(m_isPlaying || m_isPaused);
}

void NMAudioMixerPanel::updatePositionDisplay() {
  m_positionLabel->setText(formatTime(m_currentPosition));
  m_durationLabel->setText(formatTime(m_currentDuration));
}

void NMAudioMixerPanel::resetPlaybackUI() {
  m_seekSlider->setValue(0);
  m_positionLabel->setText(tr("0:00"));
  m_playBtn->setEnabled(true);
  m_pauseBtn->setEnabled(false);
  m_stopBtn->setEnabled(false);
  m_seekSlider->setEnabled(false);
}

void NMAudioMixerPanel::setPlaybackError(const QString &message) {
  LOG_ERROR("Audio playback error: {}", message.toStdString());
  emit playbackError(message);
  QMessageBox::warning(this, tr("Audio Error"), message);
}

QString NMAudioMixerPanel::formatTime(f32 seconds) const {
  int totalSeconds = static_cast<int>(seconds);
  int minutes = totalSeconds / 60;
  int secs = totalSeconds % 60;
  return QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0'));
}

void NMAudioMixerPanel::applyChannelVolumes() {
  if (!m_previewAudioManager)
    return;

  for (const auto &control : m_channelControls) {
    f32 volume = control.volumeSlider->value() / 100.0f;
    m_previewAudioManager->setChannelVolume(control.channel, volume);
  }
}

void NMAudioMixerPanel::updateSoloState() {
  if (m_soloChannelIndex < 0)
    return;

  // Ensure solo state is maintained
  for (size_t i = 0; i < m_channelControls.size(); ++i) {
    bool shouldMute = (static_cast<int>(i) != m_soloChannelIndex);
    if (m_previewAudioManager) {
      m_previewAudioManager->setChannelMuted(m_channelControls[i].channel,
                                              shouldMute);
    }
  }
}

} // namespace NovelMind::editor::qt
