/**
 * @file nm_recording_studio_panel.cpp
 * @brief Recording Studio panel implementation
 */

#include "NovelMind/editor/qt/panels/nm_recording_studio_panel.hpp"
#include "NovelMind/audio/audio_recorder.hpp"
#include "NovelMind/audio/voice_manifest.hpp"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// ============================================================================
// VUMeterWidget Implementation
// ============================================================================

VUMeterWidget::VUMeterWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(200, 30);
  setMaximumHeight(40);
}

void VUMeterWidget::setLevel(float rmsDb, float peakDb, bool clipping) {
  m_rmsDb = rmsDb;
  m_peakDb = peakDb;
  m_clipping = clipping;
  update();
}

void VUMeterWidget::reset() {
  m_rmsDb = -60.0f;
  m_peakDb = -60.0f;
  m_clipping = false;
  update();
}

void VUMeterWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const int w = width();
  const int h = height();
  const int margin = 2;
  const int barHeight = (h - margin * 3) / 2;
  const float widthAvailable = static_cast<float>(w - margin * 2);

  // Background
  painter.fillRect(rect(), QColor(30, 30, 30));

  // Convert dB to normalized value (-60dB to 0dB)
  auto dbToNorm = [](float db) {
    return std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
  };

  float rmsNorm = dbToNorm(m_rmsDb);
  float peakNorm = dbToNorm(m_peakDb);

  // RMS bar (green/yellow/red gradient)
  int rmsWidth = static_cast<int>(rmsNorm * widthAvailable);

  QLinearGradient gradient(0, 0, w, 0);
  gradient.setColorAt(0.0, QColor(40, 180, 40));    // Green
  gradient.setColorAt(0.7, QColor(200, 200, 40));   // Yellow
  gradient.setColorAt(0.9, QColor(200, 100, 40));   // Orange
  gradient.setColorAt(1.0, QColor(200, 40, 40));    // Red

  painter.fillRect(margin, margin, rmsWidth, barHeight, gradient);

  // Peak indicator (thin line)
  int peakPos = margin + static_cast<int>(peakNorm * widthAvailable);
  painter.setPen(QPen(Qt::white, 2));
  painter.drawLine(peakPos, margin, peakPos, margin + barHeight);

  // Second bar for visual reference (background scale)
  int scaleY = margin * 2 + barHeight;
  painter.fillRect(margin, scaleY, w - margin * 2, barHeight, QColor(50, 50, 50));

  // Draw scale markers
  painter.setPen(QColor(100, 100, 100));
  for (int db = -60; db <= 0; db += 6) {
    float norm = dbToNorm(static_cast<float>(db));
    int x = margin + static_cast<int>(norm * widthAvailable);
    painter.drawLine(x, scaleY, x, scaleY + barHeight);
  }

  // Clipping indicator
  if (m_clipping) {
    painter.fillRect(w - 20, margin, 18, barHeight, QColor(255, 0, 0));
  }

  // Border
  painter.setPen(QColor(80, 80, 80));
  painter.drawRect(margin, margin, w - margin * 2 - 1, barHeight - 1);
}

// ============================================================================
// NMRecordingStudioPanel Implementation
// ============================================================================

NMRecordingStudioPanel::NMRecordingStudioPanel(QWidget *parent)
    : NMDockPanel(tr("Recording Studio"), parent) {
  setPanelId("recording_studio");
}

NMRecordingStudioPanel::~NMRecordingStudioPanel() {
  if (m_recorder) {
    m_recorder->shutdown();
  }
}

void NMRecordingStudioPanel::onInitialize() {
  setupUI();

  // Initialize audio recorder
  m_recorder = std::make_unique<audio::AudioRecorder>();
  auto result = m_recorder->initialize();
  if (result.isError()) {
    // Show error in UI
    if (m_lineIdLabel) {
      m_lineIdLabel->setText(tr("Error: %1").arg(QString::fromStdString(result.error())));
    }
    return;
  }

  // Set up callbacks
  m_recorder->setOnLevelUpdate([this](const audio::LevelMeter &level) {
    // Use Qt::QueuedConnection for thread safety
    QMetaObject::invokeMethod(
        this, [this, level]() { onLevelUpdate(level); }, Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingStateChanged([this](audio::RecordingState state) {
    QMetaObject::invokeMethod(
        this, [this, state]() { onRecordingStateChanged(static_cast<int>(state)); },
        Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingComplete([this](const audio::RecordingResult &rec_result) {
    QMetaObject::invokeMethod(
        this, [this, rec_result]() { onRecordingComplete(rec_result); },
        Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingError([this](const std::string &error) {
    QMetaObject::invokeMethod(
        this, [this, error]() { onRecordingError(QString::fromStdString(error)); },
        Qt::QueuedConnection);
  });

  // Refresh device list
  refreshDeviceList();

  // Start level metering
  m_recorder->startMetering();

  // Set up update timer
  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, this, [this]() {
    if (m_isRecording && m_recorder) {
      float duration = m_recorder->getRecordingDuration();
      int minutes = static_cast<int>(duration) / 60;
      int seconds = static_cast<int>(duration) % 60;
      int tenths = static_cast<int>((duration - static_cast<int>(duration)) * 10);
      m_recordingTimeLabel->setText(
          QString("%1:%2.%3").arg(minutes).arg(seconds, 2, 10, QChar('0')).arg(tenths));
    }
  });
  m_updateTimer->start(100);
}

void NMRecordingStudioPanel::onShutdown() {
  if (m_updateTimer) {
    m_updateTimer->stop();
  }

  if (m_recorder) {
    m_recorder->stopMetering();
    m_recorder->shutdown();
  }
}

void NMRecordingStudioPanel::onUpdate([[maybe_unused]] double deltaTime) {
  // Level meter updates happen via callbacks
}

void NMRecordingStudioPanel::setManifest(audio::VoiceManifest *manifest) {
  m_manifest = manifest;
  updateLineInfo();
  updateTakeList();
}

void NMRecordingStudioPanel::setCurrentLine(const std::string &lineId) {
  m_currentLineId = lineId;
  updateLineInfo();
  updateTakeList();
  generateOutputPath();
}

void NMRecordingStudioPanel::setupUI() {
  m_contentWidget = new QWidget(this);
  setContentWidget(m_contentWidget);

  auto *mainLayout = new QVBoxLayout(m_contentWidget);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(8);

  // Create sections
  setupDeviceSection();
  setupLevelMeterSection();
  setupLineInfoSection();
  setupRecordingControls();
  setupTakeManagement();
  setupNavigationSection();
}

void NMRecordingStudioPanel::setupDeviceSection() {
  auto *group = new QGroupBox(tr("Input Device"), m_contentWidget);
  auto *layout = new QHBoxLayout(group);

  m_inputDeviceCombo = new QComboBox(group);
  m_inputDeviceCombo->setMinimumWidth(200);
  connect(m_inputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMRecordingStudioPanel::onInputDeviceChanged);
  layout->addWidget(m_inputDeviceCombo, 1);

  layout->addWidget(new QLabel(tr("Gain:"), group));

  m_inputVolumeSlider = new QSlider(Qt::Horizontal, group);
  m_inputVolumeSlider->setRange(0, 100);
  m_inputVolumeSlider->setValue(100);
  m_inputVolumeSlider->setMaximumWidth(80);
  connect(m_inputVolumeSlider, &QSlider::valueChanged, this,
          &NMRecordingStudioPanel::onInputVolumeChanged);
  layout->addWidget(m_inputVolumeSlider);

  m_inputVolumeLabel = new QLabel("100%", group);
  m_inputVolumeLabel->setMinimumWidth(40);
  layout->addWidget(m_inputVolumeLabel);

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupLevelMeterSection() {
  auto *group = new QGroupBox(tr("Level Meter"), m_contentWidget);
  auto *layout = new QVBoxLayout(group);

  // VU meter widget
  m_vuMeter = new VUMeterWidget(group);
  layout->addWidget(m_vuMeter);

  // Level info row
  auto *infoLayout = new QHBoxLayout();

  m_levelDbLabel = new QLabel(tr("Level: -60 dB"), group);
  infoLayout->addWidget(m_levelDbLabel);

  infoLayout->addStretch();

  m_clippingWarning = new QLabel(tr("CLIPPING"), group);
  m_clippingWarning->setStyleSheet("color: red; font-weight: bold;");
  m_clippingWarning->setVisible(false);
  infoLayout->addWidget(m_clippingWarning);

  layout->addLayout(infoLayout);

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupLineInfoSection() {
  auto *group = new QGroupBox(tr("Current Line"), m_contentWidget);
  auto *layout = new QGridLayout(group);

  layout->addWidget(new QLabel(tr("ID:"), group), 0, 0);
  m_lineIdLabel = new QLabel("-", group);
  m_lineIdLabel->setStyleSheet("font-weight: bold;");
  layout->addWidget(m_lineIdLabel, 0, 1);

  layout->addWidget(new QLabel(tr("Speaker:"), group), 1, 0);
  m_speakerLabel = new QLabel("-", group);
  layout->addWidget(m_speakerLabel, 1, 1);

  layout->addWidget(new QLabel(tr("Dialogue:"), group), 2, 0, Qt::AlignTop);
  m_dialogueText = new QTextEdit(group);
  m_dialogueText->setReadOnly(true);
  m_dialogueText->setMaximumHeight(80);
  m_dialogueText->setPlaceholderText(tr("Select a line to record..."));
  layout->addWidget(m_dialogueText, 2, 1);

  layout->addWidget(new QLabel(tr("Notes:"), group), 3, 0);
  m_notesLabel = new QLabel("-", group);
  m_notesLabel->setWordWrap(true);
  layout->addWidget(m_notesLabel, 3, 1);

  layout->setColumnStretch(1, 1);

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupRecordingControls() {
  auto *group = new QGroupBox(tr("Recording"), m_contentWidget);
  auto *layout = new QVBoxLayout(group);

  // Button row
  auto *btnLayout = new QHBoxLayout();

  m_recordBtn = new QPushButton(tr("Record"), group);
  m_recordBtn->setStyleSheet(
      "QPushButton { background-color: #c44; color: white; font-weight: bold; "
      "padding: 8px 16px; }"
      "QPushButton:hover { background-color: #d66; }"
      "QPushButton:disabled { background-color: #666; }");
  connect(m_recordBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onRecordClicked);
  btnLayout->addWidget(m_recordBtn);

  m_stopBtn = new QPushButton(tr("Stop"), group);
  m_stopBtn->setEnabled(false);
  connect(m_stopBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onStopClicked);
  btnLayout->addWidget(m_stopBtn);

  m_cancelBtn = new QPushButton(tr("Cancel"), group);
  m_cancelBtn->setEnabled(false);
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onCancelClicked);
  btnLayout->addWidget(m_cancelBtn);

  btnLayout->addStretch();

  m_recordingTimeLabel = new QLabel("0:00.0", group);
  m_recordingTimeLabel->setStyleSheet("font-size: 16px; font-family: monospace;");
  btnLayout->addWidget(m_recordingTimeLabel);

  layout->addLayout(btnLayout);

  // Progress bar (for visual feedback during recording)
  m_recordingProgress = new QProgressBar(group);
  m_recordingProgress->setRange(0, 0); // Indeterminate
  m_recordingProgress->setVisible(false);
  layout->addWidget(m_recordingProgress);

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupTakeManagement() {
  auto *group = new QGroupBox(tr("Takes"), m_contentWidget);
  auto *layout = new QHBoxLayout(group);

  // Takes list
  m_takesList = new QListWidget(group);
  m_takesList->setMaximumHeight(100);
  connect(m_takesList, &QListWidget::currentRowChanged, this,
          &NMRecordingStudioPanel::onTakeSelected);
  layout->addWidget(m_takesList, 1);

  // Take controls
  auto *controlsLayout = new QVBoxLayout();

  m_playTakeBtn = new QPushButton(tr("Play"), group);
  m_playTakeBtn->setEnabled(false);
  connect(m_playTakeBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onPlayClicked);
  controlsLayout->addWidget(m_playTakeBtn);

  m_setActiveBtn = new QPushButton(tr("Set Active"), group);
  m_setActiveBtn->setEnabled(false);
  controlsLayout->addWidget(m_setActiveBtn);

  m_deleteTakeBtn = new QPushButton(tr("Delete"), group);
  m_deleteTakeBtn->setEnabled(false);
  connect(m_deleteTakeBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onDeleteTakeClicked);
  controlsLayout->addWidget(m_deleteTakeBtn);

  controlsLayout->addStretch();

  layout->addLayout(controlsLayout);

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupNavigationSection() {
  auto *layout = new QHBoxLayout();

  m_prevLineBtn = new QPushButton(tr("<< Previous"), m_contentWidget);
  connect(m_prevLineBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onPrevLineClicked);
  layout->addWidget(m_prevLineBtn);

  layout->addStretch();

  m_progressLabel = new QLabel(tr("Line 0 of 0"), m_contentWidget);
  layout->addWidget(m_progressLabel);

  layout->addStretch();

  m_nextLineBtn = new QPushButton(tr("Next >>"), m_contentWidget);
  connect(m_nextLineBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onNextLineClicked);
  layout->addWidget(m_nextLineBtn);

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addLayout(layout);
    mainLayout->addStretch();
  }
}

void NMRecordingStudioPanel::refreshDeviceList() {
  if (!m_recorder || !m_inputDeviceCombo) {
    return;
  }

  m_inputDeviceCombo->clear();
  m_inputDeviceCombo->addItem(tr("(Default Device)"), QString());

  auto devices = m_recorder->getInputDevices();
  for (const auto &device : devices) {
    QString name = QString::fromStdString(device.name);
    if (device.isDefault) {
      name += tr(" (Default)");
    }
    m_inputDeviceCombo->addItem(name, QString::fromStdString(device.id));
  }
}

void NMRecordingStudioPanel::updateLineInfo() {
  if (!m_manifest || m_currentLineId.empty()) {
    m_lineIdLabel->setText("-");
    m_speakerLabel->setText("-");
    m_dialogueText->clear();
    m_notesLabel->setText("-");
    return;
  }

  auto *line = m_manifest->getLine(m_currentLineId);
  if (!line) {
    m_lineIdLabel->setText(QString::fromStdString(m_currentLineId) + tr(" (not found)"));
    return;
  }

  m_lineIdLabel->setText(QString::fromStdString(line->id));
  m_speakerLabel->setText(QString::fromStdString(line->speaker));
  m_dialogueText->setText(QString::fromStdString(line->textKey));
  m_notesLabel->setText(line->notes.empty() ? "-" : QString::fromStdString(line->notes));

  // Update progress
  auto stats = m_manifest->getCoverageStats(m_currentLocale);
  m_progressLabel->setText(tr("Line %1 of %2").arg(1).arg(stats.totalLines));
}

void NMRecordingStudioPanel::updateTakeList() {
  if (!m_takesList) {
    return;
  }

  m_takesList->clear();

  if (!m_manifest || m_currentLineId.empty()) {
    return;
  }

  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);
  for (const auto &take : takes) {
    QString label = tr("Take %1 (%2s)%3")
                        .arg(take.takeNumber)
                        .arg(take.duration, 0, 'f', 1)
                        .arg(take.isActive ? tr(" [Active]") : "");
    auto *item = new QListWidgetItem(label, m_takesList);
    item->setData(Qt::UserRole, take.takeNumber);
    if (take.isActive) {
      item->setForeground(QColor(60, 180, 60));
    }
  }
}

void NMRecordingStudioPanel::updateRecordingState() {
  bool isRecording = m_isRecording;

  m_recordBtn->setEnabled(!isRecording && !m_currentLineId.empty());
  m_stopBtn->setEnabled(isRecording);
  m_cancelBtn->setEnabled(isRecording);
  m_recordingProgress->setVisible(isRecording);

  m_inputDeviceCombo->setEnabled(!isRecording);
  m_prevLineBtn->setEnabled(!isRecording);
  m_nextLineBtn->setEnabled(!isRecording);
}

void NMRecordingStudioPanel::generateOutputPath() {
  if (!m_manifest || m_currentLineId.empty()) {
    m_outputPath.clear();
    return;
  }

  auto *line = m_manifest->getLine(m_currentLineId);
  if (!line) {
    m_outputPath.clear();
    return;
  }

  // Get the next take number
  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);
  u32 takeNum = static_cast<u32>(takes.size()) + 1;

  // Generate path using naming convention
  const auto &convention = m_manifest->getNamingConvention();
  m_outputPath = m_manifest->getBasePath() + "/" +
                 convention.generatePath(m_currentLocale, m_currentLineId, line->scene,
                                          line->speaker, takeNum);
}

void NMRecordingStudioPanel::onInputDeviceChanged(int index) {
  if (!m_recorder || !m_inputDeviceCombo) {
    return;
  }

  QString deviceId = m_inputDeviceCombo->itemData(index).toString();
  m_recorder->setInputDevice(deviceId.toStdString());
}

void NMRecordingStudioPanel::onRecordClicked() {
  if (!m_recorder || m_currentLineId.empty()) {
    return;
  }

  generateOutputPath();
  if (m_outputPath.empty()) {
    onRecordingError(tr("Cannot generate output path"));
    return;
  }

  auto result = m_recorder->startRecording(m_outputPath);
  if (result.isError()) {
    onRecordingError(QString::fromStdString(result.error()));
    return;
  }

  m_isRecording = true;
  updateRecordingState();
}

void NMRecordingStudioPanel::onStopClicked() {
  if (!m_recorder || !m_isRecording) {
    return;
  }

  m_recorder->stopRecording();
}

void NMRecordingStudioPanel::onCancelClicked() {
  if (!m_recorder || !m_isRecording) {
    return;
  }

  m_recorder->cancelRecording();
  m_isRecording = false;
  updateRecordingState();
  m_recordingTimeLabel->setText("0:00.0");
}

void NMRecordingStudioPanel::onPlayClicked() {
  // TODO: Implement playback of selected take
}

void NMRecordingStudioPanel::onPlayStopClicked() {
  // TODO: Implement stop playback
}

void NMRecordingStudioPanel::onNextLineClicked() { emit requestNextLine(); }

void NMRecordingStudioPanel::onPrevLineClicked() { emit requestPrevLine(); }

void NMRecordingStudioPanel::onTakeSelected(int index) {
  bool hasSelection = index >= 0;
  m_playTakeBtn->setEnabled(hasSelection);
  m_setActiveBtn->setEnabled(hasSelection);
  m_deleteTakeBtn->setEnabled(hasSelection);
}

void NMRecordingStudioPanel::onDeleteTakeClicked() {
  // TODO: Implement take deletion
}

void NMRecordingStudioPanel::onInputVolumeChanged(int value) {
  if (m_inputVolumeLabel) {
    m_inputVolumeLabel->setText(QString("%1%").arg(value));
  }
  // Note: Input gain is typically controlled at the OS level
  // This could be used for a software gain if implemented
}

void NMRecordingStudioPanel::onLevelUpdate(const audio::LevelMeter &level) {
  if (m_vuMeter) {
    m_vuMeter->setLevel(level.rmsLevelDb, level.peakLevelDb, level.clipping);
  }

  if (m_levelDbLabel) {
    m_levelDbLabel->setText(tr("Level: %1 dB").arg(level.rmsLevelDb, 0, 'f', 1));
  }

  if (m_clippingWarning) {
    m_clippingWarning->setVisible(level.clipping);
  }
}

void NMRecordingStudioPanel::onRecordingStateChanged(int state) {
  auto recordingState = static_cast<audio::RecordingState>(state);

  switch (recordingState) {
  case audio::RecordingState::Idle:
    m_isRecording = false;
    break;
  case audio::RecordingState::Preparing:
  case audio::RecordingState::Recording:
    m_isRecording = true;
    break;
  case audio::RecordingState::Stopping:
  case audio::RecordingState::Processing:
    // Still recording (finishing up)
    break;
  case audio::RecordingState::Error:
    m_isRecording = false;
    break;
  }

  updateRecordingState();
}

void NMRecordingStudioPanel::onRecordingComplete(const audio::RecordingResult &result) {
  m_isRecording = false;
  updateRecordingState();

  if (!m_manifest || m_currentLineId.empty()) {
    return;
  }

  // Add take to manifest
  audio::VoiceTake take;
  take.takeNumber =
      static_cast<uint32_t>(m_manifest->getTakes(m_currentLineId, m_currentLocale).size()) +
      1;
  take.filePath = result.filePath;
  take.duration = result.duration;
  take.recordedTimestamp = static_cast<uint64_t>(std::time(nullptr));
  take.isActive = true;

  m_manifest->addTake(m_currentLineId, m_currentLocale, take);

  // Update the file status
  m_manifest->markAsRecorded(m_currentLineId, m_currentLocale, result.filePath);

  // Refresh UI
  updateTakeList();

  emit recordingCompleted(QString::fromStdString(m_currentLineId),
                          QString::fromStdString(result.filePath));
}

void NMRecordingStudioPanel::onRecordingError(const QString &error) {
  m_isRecording = false;
  updateRecordingState();

  // Show error - could use a message box or status label
  if (m_lineIdLabel) {
    m_lineIdLabel->setText(tr("Error: %1").arg(error));
  }
}

} // namespace NovelMind::editor::qt
