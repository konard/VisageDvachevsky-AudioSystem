/**
 * @file nm_voice_studio_panel.cpp
 * @brief Voice Studio panel implementation
 */

#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"
#include "NovelMind/audio/audio_recorder.hpp"
#include "NovelMind/audio/voice_manifest.hpp"

#include <QApplication>
#include <QAudioOutput>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QUndoCommand>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace NovelMind::editor::qt {

// ============================================================================
// StudioVUMeterWidget Implementation
// ============================================================================

StudioVUMeterWidget::StudioVUMeterWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(200, 30);
  setMaximumHeight(40);
}

void StudioVUMeterWidget::setLevel(float rmsDb, float peakDb, bool clipping) {
  m_rmsDb = rmsDb;
  m_peakDb = peakDb;
  m_clipping = clipping;
  update();
}

void StudioVUMeterWidget::reset() {
  m_rmsDb = -60.0f;
  m_peakDb = -60.0f;
  m_clipping = false;
  update();
}

void StudioVUMeterWidget::paintEvent(QPaintEvent *event) {
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
  gradient.setColorAt(0.0, QColor(40, 180, 40));
  gradient.setColorAt(0.7, QColor(200, 200, 40));
  gradient.setColorAt(0.9, QColor(200, 100, 40));
  gradient.setColorAt(1.0, QColor(200, 40, 40));

  painter.fillRect(margin, margin, rmsWidth, barHeight, gradient);

  // Peak indicator (thin line)
  int peakPos = margin + static_cast<int>(peakNorm * widthAvailable);
  painter.setPen(QPen(Qt::white, 2));
  painter.drawLine(peakPos, margin, peakPos, margin + barHeight);

  // Scale bar
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
// WaveformWidget Implementation
// ============================================================================

WaveformWidget::WaveformWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(400, 100);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
}

void WaveformWidget::setClip(const VoiceClip *clip) {
  m_clip = clip;
  updatePeakCache();
  update();
}

void WaveformWidget::setSelection(double startSec, double endSec) {
  m_selectionStart = startSec;
  m_selectionEnd = endSec;
  update();
  emit selectionChanged(startSec, endSec);
}

void WaveformWidget::clearSelection() {
  m_selectionStart = 0.0;
  m_selectionEnd = 0.0;
  update();
  emit selectionChanged(0.0, 0.0);
}

void WaveformWidget::setPlayheadPosition(double seconds) {
  m_playheadPos = seconds;
  update();
}

void WaveformWidget::setZoom(double samplesPerPixel) {
  m_samplesPerPixel = std::clamp(samplesPerPixel, 10.0, 10000.0);
  updatePeakCache();
  update();
  emit zoomChanged(m_samplesPerPixel);
}

void WaveformWidget::zoomIn() {
  setZoom(m_samplesPerPixel / 1.5);
}

void WaveformWidget::zoomOut() {
  setZoom(m_samplesPerPixel * 1.5);
}

void WaveformWidget::zoomToFit() {
  if (!m_clip || m_clip->samples.empty()) return;

  double totalSamples = static_cast<double>(m_clip->samples.size());
  double availableWidth = static_cast<double>(width() - 20);
  if (availableWidth > 0) {
    setZoom(totalSamples / availableWidth);
  }
}

void WaveformWidget::setScrollPosition(double seconds) {
  m_scrollPos = std::max(0.0, seconds);
  update();
}

double WaveformWidget::timeToX(double seconds) const {
  if (!m_clip || m_clip->format.sampleRate == 0) return 0.0;

  double samplePos = seconds * m_clip->format.sampleRate;
  double scrollSample = m_scrollPos * m_clip->format.sampleRate;
  return (samplePos - scrollSample) / m_samplesPerPixel + 10.0;
}

double WaveformWidget::xToTime(double x) const {
  if (!m_clip || m_clip->format.sampleRate == 0) return 0.0;

  double samplePos = (x - 10.0) * m_samplesPerPixel;
  double scrollSample = m_scrollPos * m_clip->format.sampleRate;
  return (samplePos + scrollSample) / m_clip->format.sampleRate;
}

void WaveformWidget::updatePeakCache() {
  m_displayPeaks.clear();

  if (!m_clip || m_clip->samples.empty()) return;

  int pixelWidth = width() - 20;
  if (pixelWidth <= 0) return;

  m_displayPeaks.resize(static_cast<size_t>(pixelWidth));

  size_t scrollSample = static_cast<size_t>(m_scrollPos * m_clip->format.sampleRate);

  for (int px = 0; px < pixelWidth; ++px) {
    size_t startSample = scrollSample + static_cast<size_t>(px * m_samplesPerPixel);
    size_t endSample = std::min(
        scrollSample + static_cast<size_t>((px + 1) * m_samplesPerPixel),
        m_clip->samples.size());

    float maxPeak = 0.0f;
    for (size_t s = startSample; s < endSample && s < m_clip->samples.size(); ++s) {
      maxPeak = std::max(maxPeak, std::abs(m_clip->samples[s]));
    }
    m_displayPeaks[static_cast<size_t>(px)] = maxPeak;
  }
}

void WaveformWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const int w = width();
  const int h = height();
  const int margin = 10;
  const int waveHeight = h - 2 * margin;
  const int centerY = margin + waveHeight / 2;

  // Background
  painter.fillRect(rect(), QColor(25, 25, 30));

  // Draw grid lines
  painter.setPen(QColor(50, 50, 55));
  painter.drawLine(margin, centerY, w - margin, centerY);

  // Draw selection highlight
  if (m_selectionStart < m_selectionEnd && m_clip) {
    int selStartX = static_cast<int>(timeToX(m_selectionStart));
    int selEndX = static_cast<int>(timeToX(m_selectionEnd));
    painter.fillRect(selStartX, margin, selEndX - selStartX, waveHeight,
                     QColor(70, 130, 180, 80));
  }

  // Draw trim regions (darker)
  if (m_clip && m_clip->edit.trimStartSamples > 0) {
    double trimStartSec = static_cast<double>(m_clip->edit.trimStartSamples) /
                          m_clip->format.sampleRate;
    int trimX = static_cast<int>(timeToX(trimStartSec));
    painter.fillRect(margin, margin, trimX - margin, waveHeight,
                     QColor(0, 0, 0, 150));
  }

  if (m_clip && m_clip->edit.trimEndSamples > 0) {
    int64_t totalSamples = static_cast<int64_t>(m_clip->samples.size());
    double trimEndSec = static_cast<double>(totalSamples - m_clip->edit.trimEndSamples) /
                        m_clip->format.sampleRate;
    int trimX = static_cast<int>(timeToX(trimEndSec));
    painter.fillRect(trimX, margin, w - margin - trimX, waveHeight,
                     QColor(0, 0, 0, 150));
  }

  // Draw waveform
  if (!m_displayPeaks.empty()) {
    painter.setPen(QPen(QColor(100, 180, 255), 1));

    for (size_t px = 0; px < m_displayPeaks.size(); ++px) {
      float peak = m_displayPeaks[px];
      int peakPixels = static_cast<int>(peak * static_cast<float>(waveHeight) / 2.0f);
      int x = margin + static_cast<int>(px);
      painter.drawLine(x, centerY - peakPixels, x, centerY + peakPixels);
    }
  }

  // Draw playhead
  if (m_clip) {
    int playheadX = static_cast<int>(timeToX(m_playheadPos));
    if (playheadX >= margin && playheadX <= w - margin) {
      painter.setPen(QPen(QColor(255, 100, 100), 2));
      painter.drawLine(playheadX, margin, playheadX, h - margin);

      // Playhead triangle
      QPolygon triangle;
      triangle << QPoint(playheadX - 5, margin)
               << QPoint(playheadX + 5, margin)
               << QPoint(playheadX, margin + 8);
      painter.setBrush(QColor(255, 100, 100));
      painter.drawPolygon(triangle);
    }
  }

  // Draw time ruler
  painter.setPen(QColor(150, 150, 150));
  painter.setFont(QFont("Arial", 8));

  if (m_clip && m_clip->format.sampleRate > 0) {
    double duration = m_clip->getDurationSeconds();
    double interval = 1.0; // 1 second intervals

    // Adjust interval based on zoom
    if (m_samplesPerPixel > 5000) interval = 5.0;
    else if (m_samplesPerPixel > 2000) interval = 2.0;
    else if (m_samplesPerPixel < 500) interval = 0.5;
    else if (m_samplesPerPixel < 200) interval = 0.1;

    for (double t = 0; t <= duration; t += interval) {
      int x = static_cast<int>(timeToX(t));
      if (x >= margin && x <= w - margin) {
        painter.drawLine(x, h - margin, x, h - margin + 5);
        QString timeStr = QString::number(t, 'f', 1) + "s";
        painter.drawText(x - 15, h - 2, timeStr);
      }
    }
  }

  // Border
  painter.setPen(QColor(80, 80, 80));
  painter.drawRect(margin - 1, margin - 1, w - 2 * margin + 1, waveHeight + 1);
}

void WaveformWidget::mousePressEvent(QMouseEvent *event) {
  if (!m_clip) return;

  double clickTime = xToTime(event->position().x());
  clickTime = std::clamp(clickTime, 0.0, m_clip->getDurationSeconds());

  if (event->button() == Qt::LeftButton) {
    if (event->modifiers() & Qt::ShiftModifier) {
      // Shift+click extends selection
      if (clickTime < m_selectionStart) {
        setSelection(clickTime, m_selectionEnd);
      } else {
        setSelection(m_selectionStart, clickTime);
      }
    } else {
      // Start new selection
      m_isSelecting = true;
      m_dragStartTime = clickTime;
      setSelection(clickTime, clickTime);
    }
  } else if (event->button() == Qt::RightButton) {
    // Right-click to set playhead
    emit playheadClicked(clickTime);
  }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!m_clip || !m_isSelecting) return;

  double currentTime = xToTime(event->position().x());
  currentTime = std::clamp(currentTime, 0.0, m_clip->getDurationSeconds());

  if (currentTime < m_dragStartTime) {
    setSelection(currentTime, m_dragStartTime);
  } else {
    setSelection(m_dragStartTime, currentTime);
  }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event) {
  Q_UNUSED(event);
  m_isSelecting = false;
}

void WaveformWidget::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() & Qt::ControlModifier) {
    // Ctrl+wheel for zoom
    if (event->angleDelta().y() > 0) {
      zoomIn();
    } else {
      zoomOut();
    }
    event->accept();
  } else {
    // Regular wheel for scroll
    if (m_clip) {
      double scrollDelta = event->angleDelta().y() > 0 ? -0.5 : 0.5;
      setScrollPosition(m_scrollPos + scrollDelta);
    }
    event->accept();
  }
}

void WaveformWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updatePeakCache();
}

// ============================================================================
// AudioProcessor Implementation
// ============================================================================

std::vector<float> AudioProcessor::process(const std::vector<float> &source,
                                           const VoiceClipEdit &edit,
                                           const AudioFormat &format) {
  if (source.empty()) return {};

  // Apply trim
  std::vector<float> result = applyTrim(source, edit.trimStartSamples, edit.trimEndSamples);
  if (result.empty()) return {};

  // Apply pre-gain
  if (edit.preGainDb != 0.0f) {
    applyGain(result, edit.preGainDb);
  }

  // Apply high-pass filter
  if (edit.highPassEnabled) {
    applyHighPass(result, edit.highPassFreqHz, format.sampleRate);
  }

  // Apply low-pass filter
  if (edit.lowPassEnabled) {
    applyLowPass(result, edit.lowPassFreqHz, format.sampleRate);
  }

  // Apply EQ
  if (edit.eqEnabled) {
    applyEQ(result, edit.eqLowGainDb, edit.eqMidGainDb, edit.eqHighGainDb,
            edit.eqLowFreqHz, edit.eqHighFreqHz, format.sampleRate);
  }

  // Apply noise gate
  if (edit.noiseGateEnabled) {
    applyNoiseGate(result, edit.noiseGateThresholdDb, edit.noiseGateReductionDb,
                   edit.noiseGateAttackMs, edit.noiseGateReleaseMs, format.sampleRate);
  }

  // Apply normalization
  if (edit.normalizeEnabled) {
    applyNormalize(result, edit.normalizeTargetDbFS);
  }

  // Apply fades
  if (edit.fadeInMs > 0 || edit.fadeOutMs > 0) {
    applyFades(result, edit.fadeInMs, edit.fadeOutMs, format.sampleRate);
  }

  return result;
}

std::vector<float> AudioProcessor::applyTrim(const std::vector<float> &samples,
                                             int64_t trimStart, int64_t trimEnd) {
  int64_t totalSamples = static_cast<int64_t>(samples.size());
  int64_t start = std::clamp(trimStart, static_cast<int64_t>(0), totalSamples);
  int64_t end = std::clamp(totalSamples - trimEnd, start, totalSamples);

  if (start >= end) return {};

  return std::vector<float>(samples.begin() + start, samples.begin() + end);
}

void AudioProcessor::applyFades(std::vector<float> &samples,
                                float fadeInMs, float fadeOutMs,
                                uint32_t sampleRate) {
  if (samples.empty() || sampleRate == 0) return;

  // Fade in
  if (fadeInMs > 0) {
    size_t fadeInSamples = static_cast<size_t>(fadeInMs * static_cast<float>(sampleRate) / 1000.0f);
    fadeInSamples = std::min(fadeInSamples, samples.size());

    for (size_t i = 0; i < fadeInSamples; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(fadeInSamples);
      // Use cosine fade for smooth curve
      float gain = 0.5f * (1.0f - std::cos(t * static_cast<float>(M_PI)));
      samples[i] *= gain;
    }
  }

  // Fade out
  if (fadeOutMs > 0) {
    size_t fadeOutSamples = static_cast<size_t>(fadeOutMs * static_cast<float>(sampleRate) / 1000.0f);
    fadeOutSamples = std::min(fadeOutSamples, samples.size());

    size_t startIdx = samples.size() - fadeOutSamples;
    for (size_t i = 0; i < fadeOutSamples; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(fadeOutSamples);
      float gain = 0.5f * (1.0f + std::cos(t * static_cast<float>(M_PI)));
      samples[startIdx + i] *= gain;
    }
  }
}

void AudioProcessor::applyGain(std::vector<float> &samples, float gainDb) {
  float gainLinear = std::pow(10.0f, gainDb / 20.0f);
  for (auto &sample : samples) {
    sample *= gainLinear;
  }
}

void AudioProcessor::applyNormalize(std::vector<float> &samples, float targetDbFS) {
  if (samples.empty()) return;

  float peak = calculatePeakDb(samples);
  if (peak <= -60.0f) return; // Too quiet, don't normalize

  float gainDb = targetDbFS - peak;
  applyGain(samples, gainDb);
}

void AudioProcessor::applyHighPass(std::vector<float> &samples,
                                   float cutoffHz, uint32_t sampleRate) {
  if (samples.empty() || sampleRate == 0) return;

  // Simple first-order high-pass filter
  float RC = 1.0f / (2.0f * static_cast<float>(M_PI) * cutoffHz);
  float dt = 1.0f / static_cast<float>(sampleRate);
  float alpha = RC / (RC + dt);

  float prevInput = samples[0];
  float prevOutput = samples[0];

  for (size_t i = 1; i < samples.size(); ++i) {
    float output = alpha * (prevOutput + samples[i] - prevInput);
    prevInput = samples[i];
    prevOutput = output;
    samples[i] = output;
  }
}

void AudioProcessor::applyLowPass(std::vector<float> &samples,
                                  float cutoffHz, uint32_t sampleRate) {
  if (samples.empty() || sampleRate == 0) return;

  // Simple first-order low-pass filter
  float RC = 1.0f / (2.0f * static_cast<float>(M_PI) * cutoffHz);
  float dt = 1.0f / static_cast<float>(sampleRate);
  float alpha = dt / (RC + dt);

  float prevOutput = samples[0];

  for (size_t i = 1; i < samples.size(); ++i) {
    float output = prevOutput + alpha * (samples[i] - prevOutput);
    prevOutput = output;
    samples[i] = output;
  }
}

void AudioProcessor::applyEQ(std::vector<float> &samples,
                             float lowGainDb, float midGainDb, float highGainDb,
                             float lowFreq, float highFreq, uint32_t sampleRate) {
  if (samples.empty() || sampleRate == 0) return;

  // Create three copies for each band
  std::vector<float> lowBand = samples;
  std::vector<float> midBand = samples;
  std::vector<float> highBand = samples;

  // Low band: low-pass at lowFreq
  applyLowPass(lowBand, lowFreq, sampleRate);

  // High band: high-pass at highFreq
  applyHighPass(highBand, highFreq, sampleRate);

  // Mid band: band-pass (high-pass at lowFreq, low-pass at highFreq)
  applyHighPass(midBand, lowFreq, sampleRate);
  applyLowPass(midBand, highFreq, sampleRate);

  // Apply gains
  float lowGain = std::pow(10.0f, lowGainDb / 20.0f);
  float midGain = std::pow(10.0f, midGainDb / 20.0f);
  float highGain = std::pow(10.0f, highGainDb / 20.0f);

  // Sum bands
  for (size_t i = 0; i < samples.size(); ++i) {
    samples[i] = lowBand[i] * lowGain + midBand[i] * midGain + highBand[i] * highGain;
  }
}

void AudioProcessor::applyNoiseGate(std::vector<float> &samples,
                                    float thresholdDb, float reductionDb,
                                    float attackMs, float releaseMs,
                                    uint32_t sampleRate) {
  if (samples.empty() || sampleRate == 0) return;

  float threshold = std::pow(10.0f, thresholdDb / 20.0f);
  float reductionGain = std::pow(10.0f, reductionDb / 20.0f);

  float attackSamples = attackMs * static_cast<float>(sampleRate) / 1000.0f;
  float releaseSamples = releaseMs * static_cast<float>(sampleRate) / 1000.0f;

  float attackCoef = attackSamples > 0 ? 1.0f / attackSamples : 1.0f;
  float releaseCoef = releaseSamples > 0 ? 1.0f / releaseSamples : 1.0f;

  float envelope = 0.0f;
  float gateGain = reductionGain;

  for (size_t i = 0; i < samples.size(); ++i) {
    float absLevel = std::abs(samples[i]);

    // Update envelope
    if (absLevel > envelope) {
      envelope += attackCoef * (absLevel - envelope);
    } else {
      envelope += releaseCoef * (absLevel - envelope);
    }

    // Update gate gain
    if (envelope > threshold) {
      gateGain = std::min(gateGain + attackCoef, 1.0f);
    } else {
      gateGain = std::max(gateGain - releaseCoef, reductionGain);
    }

    samples[i] *= gateGain;
  }
}

float AudioProcessor::calculatePeakDb(const std::vector<float> &samples) {
  if (samples.empty()) return -60.0f;

  float peak = 0.0f;
  for (const auto &sample : samples) {
    peak = std::max(peak, std::abs(sample));
  }

  if (peak <= 0.0f) return -60.0f;
  return 20.0f * std::log10(peak);
}

float AudioProcessor::calculateRmsDb(const std::vector<float> &samples) {
  if (samples.empty()) return -60.0f;

  float sumSquares = 0.0f;
  for (const auto &sample : samples) {
    sumSquares += sample * sample;
  }

  float rms = std::sqrt(sumSquares / static_cast<float>(samples.size()));
  if (rms <= 0.0f) return -60.0f;
  return 20.0f * std::log10(rms);
}

// ============================================================================
// Undo Command
// ============================================================================

class VoiceEditCommand : public QUndoCommand {
public:
  VoiceEditCommand(VoiceClip *clip, const VoiceClipEdit &oldEdit,
                   const VoiceClipEdit &newEdit, const QString &description)
      : QUndoCommand(description), m_clip(clip), m_oldEdit(oldEdit), m_newEdit(newEdit) {}

  void undo() override {
    if (m_clip) {
      m_clip->edit = m_oldEdit;
    }
  }

  void redo() override {
    if (m_clip) {
      m_clip->edit = m_newEdit;
    }
  }

private:
  VoiceClip *m_clip;
  VoiceClipEdit m_oldEdit;
  VoiceClipEdit m_newEdit;
};

// ============================================================================
// NMVoiceStudioPanel Implementation
// ============================================================================

NMVoiceStudioPanel::NMVoiceStudioPanel(QWidget *parent)
    : NMDockPanel(tr("Voice Studio"), parent) {
  setPanelId("voice_studio");
}

NMVoiceStudioPanel::~NMVoiceStudioPanel() {
  if (m_recorder) {
    m_recorder->shutdown();
  }
}

void NMVoiceStudioPanel::onInitialize() {
  setupUI();
  setupMediaPlayer();
  setupRecorder();

  // Initialize undo stack
  m_undoStack = std::make_unique<QUndoStack>(this);

  // Initialize presets
  {
    VoiceClipEdit cleanVoice;
    cleanVoice.highPassEnabled = true;
    cleanVoice.highPassFreqHz = 80.0f;
    cleanVoice.normalizeEnabled = true;
    cleanVoice.normalizeTargetDbFS = -1.0f;
    m_presets.push_back({"Clean Voice", cleanVoice});

    VoiceClipEdit telephone;
    telephone.highPassEnabled = true;
    telephone.highPassFreqHz = 300.0f;
    telephone.lowPassEnabled = true;
    telephone.lowPassFreqHz = 3400.0f;
    m_presets.push_back({"Telephone", telephone});

    VoiceClipEdit warmVoice;
    warmVoice.eqEnabled = true;
    warmVoice.eqLowGainDb = 3.0f;
    warmVoice.eqMidGainDb = -1.0f;
    warmVoice.eqHighGainDb = -2.0f;
    m_presets.push_back({"Warm Voice", warmVoice});

    VoiceClipEdit crispVoice;
    crispVoice.eqEnabled = true;
    crispVoice.eqLowGainDb = -2.0f;
    crispVoice.eqMidGainDb = 0.0f;
    crispVoice.eqHighGainDb = 3.0f;
    m_presets.push_back({"Crisp Voice", crispVoice});

    VoiceClipEdit noiseReduced;
    noiseReduced.noiseGateEnabled = true;
    noiseReduced.noiseGateThresholdDb = -35.0f;
    m_presets.push_back({"Noise Reduced", noiseReduced});
  }

  // Update preset combo
  if (m_presetCombo) {
    m_presetCombo->clear();
    m_presetCombo->addItem(tr("(No Preset)"));
    for (const auto &preset : m_presets) {
      m_presetCombo->addItem(preset.name);
    }
  }

  // Set up update timer
  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, this, &NMVoiceStudioPanel::onUpdateTimer);
  m_updateTimer->start(100);

  updateUI();
}

void NMVoiceStudioPanel::onShutdown() {
  if (m_updateTimer) {
    m_updateTimer->stop();
  }

  if (m_mediaPlayer) {
    m_mediaPlayer->stop();
  }

  if (m_recorder) {
    m_recorder->stopMetering();
    m_recorder->shutdown();
  }
}

void NMVoiceStudioPanel::onUpdate([[maybe_unused]] double deltaTime) {
  // Updates happen via timer and callbacks
}

void NMVoiceStudioPanel::setManifest(audio::VoiceManifest *manifest) {
  m_manifest = manifest;
}

bool NMVoiceStudioPanel::loadFile(const QString &filePath) {
  if (!loadWavFile(filePath)) {
    return false;
  }

  m_currentFilePath = filePath;
  m_lastSavedEdit = VoiceClipEdit{};

  if (m_waveformWidget) {
    m_waveformWidget->setClip(m_clip.get());
    m_waveformWidget->zoomToFit();
  }

  updateUI();
  updateEditControls();
  updateStatusBar();

  return true;
}

bool NMVoiceStudioPanel::loadFromLineId(const QString &lineId, const QString &locale) {
  if (!m_manifest) return false;

  auto *line = m_manifest->getLine(lineId.toStdString());
  if (!line) return false;

  auto it = line->files.find(locale.toStdString());
  if (it == line->files.end()) return false;

  // VoiceLocaleFile contains filePath as a member
  QString filePath = QString::fromStdString(m_manifest->getBasePath() + "/" + it->second.filePath);
  if (!loadFile(filePath)) return false;

  m_currentLineId = lineId;
  m_currentLocale = locale;

  return true;
}

bool NMVoiceStudioPanel::hasUnsavedChanges() const {
  if (!m_clip) return false;

  // Compare current edit with last saved edit
  const auto &current = m_clip->edit;
  const auto &saved = m_lastSavedEdit;

  return current.trimStartSamples != saved.trimStartSamples ||
         current.trimEndSamples != saved.trimEndSamples ||
         current.fadeInMs != saved.fadeInMs ||
         current.fadeOutMs != saved.fadeOutMs ||
         current.preGainDb != saved.preGainDb ||
         current.normalizeEnabled != saved.normalizeEnabled ||
         current.highPassEnabled != saved.highPassEnabled ||
         current.lowPassEnabled != saved.lowPassEnabled ||
         current.eqEnabled != saved.eqEnabled ||
         current.noiseGateEnabled != saved.noiseGateEnabled;
}

void NMVoiceStudioPanel::setupUI() {
  m_contentWidget = new QWidget(this);
  setContentWidget(m_contentWidget);

  auto *mainLayout = new QVBoxLayout(m_contentWidget);
  mainLayout->setContentsMargins(4, 4, 4, 4);
  mainLayout->setSpacing(4);

  setupToolbar();
  mainLayout->addWidget(m_toolbar);

  // Main splitter
  m_mainSplitter = new QSplitter(Qt::Vertical, m_contentWidget);

  // Top section: waveform and transport
  auto *topWidget = new QWidget(m_mainSplitter);
  auto *topLayout = new QVBoxLayout(topWidget);
  topLayout->setContentsMargins(0, 0, 0, 0);
  topLayout->setSpacing(4);

  setupWaveformSection();
  topLayout->addWidget(m_waveformWidget, 1);

  setupTransportSection();
  topLayout->addWidget(m_transportGroup);

  m_mainSplitter->addWidget(topWidget);

  // Bottom section: controls (scrollable)
  auto *scrollArea = new QScrollArea(m_mainSplitter);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);

  auto *controlsWidget = new QWidget(scrollArea);
  auto *controlsLayout = new QVBoxLayout(controlsWidget);
  controlsLayout->setContentsMargins(0, 0, 0, 0);
  controlsLayout->setSpacing(4);

  setupDeviceSection();
  controlsLayout->addWidget(m_deviceGroup);

  setupEditSection();
  controlsLayout->addWidget(m_editGroup);

  setupFilterSection();
  controlsLayout->addWidget(m_filterGroup);

  setupPresetSection();

  controlsLayout->addStretch();

  scrollArea->setWidget(controlsWidget);
  m_mainSplitter->addWidget(scrollArea);

  m_mainSplitter->setStretchFactor(0, 2);
  m_mainSplitter->setStretchFactor(1, 1);

  mainLayout->addWidget(m_mainSplitter, 1);

  setupStatusBar();
  mainLayout->addWidget(m_statusLabel);
}

void NMVoiceStudioPanel::setupToolbar() {
  m_toolbar = new QToolBar(m_contentWidget);
  m_toolbar->setIconSize(QSize(16, 16));

  // File actions
  auto *openAction = m_toolbar->addAction(tr("Open"));
  connect(openAction, &QAction::triggered, this, &NMVoiceStudioPanel::onOpenClicked);

  auto *saveAction = m_toolbar->addAction(tr("Save"));
  connect(saveAction, &QAction::triggered, this, &NMVoiceStudioPanel::onSaveClicked);

  auto *saveAsAction = m_toolbar->addAction(tr("Save As..."));
  connect(saveAsAction, &QAction::triggered, this, &NMVoiceStudioPanel::onSaveAsClicked);

  auto *exportAction = m_toolbar->addAction(tr("Export"));
  connect(exportAction, &QAction::triggered, this, &NMVoiceStudioPanel::onExportClicked);

  m_toolbar->addSeparator();

  // Undo/Redo
  auto *undoAction = m_toolbar->addAction(tr("Undo"));
  connect(undoAction, &QAction::triggered, this, &NMVoiceStudioPanel::onUndoClicked);

  auto *redoAction = m_toolbar->addAction(tr("Redo"));
  connect(redoAction, &QAction::triggered, this, &NMVoiceStudioPanel::onRedoClicked);

  m_toolbar->addSeparator();

  // Zoom controls
  auto *zoomInAction = m_toolbar->addAction(tr("Zoom In"));
  connect(zoomInAction, &QAction::triggered, this, [this]() {
    if (m_waveformWidget) m_waveformWidget->zoomIn();
  });

  auto *zoomOutAction = m_toolbar->addAction(tr("Zoom Out"));
  connect(zoomOutAction, &QAction::triggered, this, [this]() {
    if (m_waveformWidget) m_waveformWidget->zoomOut();
  });

  auto *zoomFitAction = m_toolbar->addAction(tr("Fit"));
  connect(zoomFitAction, &QAction::triggered, this, [this]() {
    if (m_waveformWidget) m_waveformWidget->zoomToFit();
  });
}

void NMVoiceStudioPanel::setupDeviceSection() {
  m_deviceGroup = new QGroupBox(tr("Recording Input"), m_contentWidget);
  auto *layout = new QVBoxLayout(m_deviceGroup);

  // Device selection row
  auto *deviceRow = new QHBoxLayout();
  deviceRow->addWidget(new QLabel(tr("Device:"), m_deviceGroup));

  m_inputDeviceCombo = new QComboBox(m_deviceGroup);
  m_inputDeviceCombo->setMinimumWidth(200);
  connect(m_inputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMVoiceStudioPanel::onInputDeviceChanged);
  deviceRow->addWidget(m_inputDeviceCombo, 1);

  deviceRow->addWidget(new QLabel(tr("Gain:"), m_deviceGroup));

  m_inputGainSlider = new QSlider(Qt::Horizontal, m_deviceGroup);
  m_inputGainSlider->setRange(0, 100);
  m_inputGainSlider->setValue(100);
  m_inputGainSlider->setMaximumWidth(80);
  deviceRow->addWidget(m_inputGainSlider);

  m_inputGainLabel = new QLabel("100%", m_deviceGroup);
  m_inputGainLabel->setMinimumWidth(40);
  deviceRow->addWidget(m_inputGainLabel);

  layout->addLayout(deviceRow);

  // VU meter
  m_vuMeter = new StudioVUMeterWidget(m_deviceGroup);
  layout->addWidget(m_vuMeter);

  m_levelLabel = new QLabel(tr("Level: -60 dB"), m_deviceGroup);
  layout->addWidget(m_levelLabel);

  // Recording controls row
  auto *recordRow = new QHBoxLayout();

  m_recordBtn = new QPushButton(tr("Record"), m_deviceGroup);
  m_recordBtn->setStyleSheet(
      "QPushButton { background-color: #c44; color: white; font-weight: bold; padding: 6px 12px; }"
      "QPushButton:hover { background-color: #d66; }"
      "QPushButton:disabled { background-color: #666; }");
  connect(m_recordBtn, &QPushButton::clicked, this, &NMVoiceStudioPanel::onRecordClicked);
  recordRow->addWidget(m_recordBtn);

  m_stopRecordBtn = new QPushButton(tr("Stop"), m_deviceGroup);
  m_stopRecordBtn->setEnabled(false);
  connect(m_stopRecordBtn, &QPushButton::clicked, this, &NMVoiceStudioPanel::onStopRecordClicked);
  recordRow->addWidget(m_stopRecordBtn);

  m_cancelRecordBtn = new QPushButton(tr("Cancel"), m_deviceGroup);
  m_cancelRecordBtn->setEnabled(false);
  connect(m_cancelRecordBtn, &QPushButton::clicked, this, &NMVoiceStudioPanel::onCancelRecordClicked);
  recordRow->addWidget(m_cancelRecordBtn);

  recordRow->addStretch();

  m_recordingTimeLabel = new QLabel("0:00.0", m_deviceGroup);
  m_recordingTimeLabel->setStyleSheet("font-size: 14px; font-family: monospace;");
  recordRow->addWidget(m_recordingTimeLabel);

  layout->addLayout(recordRow);
}

void NMVoiceStudioPanel::setupTransportSection() {
  m_transportGroup = new QGroupBox(tr("Playback"), m_contentWidget);
  auto *layout = new QHBoxLayout(m_transportGroup);

  m_playBtn = new QPushButton(tr("Play"), m_transportGroup);
  m_playBtn->setCheckable(true);
  connect(m_playBtn, &QPushButton::clicked, this, &NMVoiceStudioPanel::onPlayClicked);
  layout->addWidget(m_playBtn);

  m_stopBtn = new QPushButton(tr("Stop"), m_transportGroup);
  connect(m_stopBtn, &QPushButton::clicked, this, &NMVoiceStudioPanel::onStopClicked);
  layout->addWidget(m_stopBtn);

  m_loopBtn = new QPushButton(tr("Loop"), m_transportGroup);
  m_loopBtn->setCheckable(true);
  connect(m_loopBtn, &QPushButton::toggled, this, &NMVoiceStudioPanel::onLoopClicked);
  layout->addWidget(m_loopBtn);

  layout->addStretch();

  m_positionLabel = new QLabel("0:00.0", m_transportGroup);
  m_positionLabel->setStyleSheet("font-family: monospace;");
  layout->addWidget(m_positionLabel);

  layout->addWidget(new QLabel("/", m_transportGroup));

  m_durationLabel = new QLabel("0:00.0", m_transportGroup);
  m_durationLabel->setStyleSheet("font-family: monospace;");
  layout->addWidget(m_durationLabel);
}

void NMVoiceStudioPanel::setupWaveformSection() {
  m_waveformWidget = new WaveformWidget(m_contentWidget);
  m_waveformWidget->setMinimumHeight(150);

  connect(m_waveformWidget, &WaveformWidget::selectionChanged,
          this, &NMVoiceStudioPanel::onWaveformSelectionChanged);
  connect(m_waveformWidget, &WaveformWidget::playheadClicked,
          this, &NMVoiceStudioPanel::onWaveformPlayheadClicked);
}

void NMVoiceStudioPanel::setupEditSection() {
  m_editGroup = new QGroupBox(tr("Edit"), m_contentWidget);
  auto *layout = new QGridLayout(m_editGroup);

  int row = 0;

  // Trim controls
  layout->addWidget(new QLabel(tr("Trim:"), m_editGroup), row, 0);

  m_trimToSelectionBtn = new QPushButton(tr("Trim to Selection"), m_editGroup);
  connect(m_trimToSelectionBtn, &QPushButton::clicked, this, &NMVoiceStudioPanel::onTrimToSelection);
  layout->addWidget(m_trimToSelectionBtn, row, 1);

  m_resetTrimBtn = new QPushButton(tr("Reset Trim"), m_editGroup);
  connect(m_resetTrimBtn, &QPushButton::clicked, this, &NMVoiceStudioPanel::onResetTrim);
  layout->addWidget(m_resetTrimBtn, row, 2);
  row++;

  // Fade controls
  layout->addWidget(new QLabel(tr("Fade In (ms):"), m_editGroup), row, 0);
  m_fadeInSpin = new QDoubleSpinBox(m_editGroup);
  m_fadeInSpin->setRange(0, 5000);
  m_fadeInSpin->setSingleStep(10);
  m_fadeInSpin->setValue(0);
  connect(m_fadeInSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onFadeInChanged);
  layout->addWidget(m_fadeInSpin, row, 1);

  layout->addWidget(new QLabel(tr("Fade Out (ms):"), m_editGroup), row, 2);
  m_fadeOutSpin = new QDoubleSpinBox(m_editGroup);
  m_fadeOutSpin->setRange(0, 5000);
  m_fadeOutSpin->setSingleStep(10);
  m_fadeOutSpin->setValue(0);
  connect(m_fadeOutSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onFadeOutChanged);
  layout->addWidget(m_fadeOutSpin, row, 3);
  row++;

  // Gain control
  layout->addWidget(new QLabel(tr("Pre-Gain (dB):"), m_editGroup), row, 0);
  m_preGainSpin = new QDoubleSpinBox(m_editGroup);
  m_preGainSpin->setRange(-24, 24);
  m_preGainSpin->setSingleStep(0.5);
  m_preGainSpin->setValue(0);
  connect(m_preGainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onPreGainChanged);
  layout->addWidget(m_preGainSpin, row, 1);
  row++;

  // Normalize
  m_normalizeCheck = new QCheckBox(tr("Normalize"), m_editGroup);
  connect(m_normalizeCheck, &QCheckBox::toggled, this, &NMVoiceStudioPanel::onNormalizeToggled);
  layout->addWidget(m_normalizeCheck, row, 0);

  layout->addWidget(new QLabel(tr("Target (dBFS):"), m_editGroup), row, 1);
  m_normalizeTargetSpin = new QDoubleSpinBox(m_editGroup);
  m_normalizeTargetSpin->setRange(-24, 0);
  m_normalizeTargetSpin->setSingleStep(0.5);
  m_normalizeTargetSpin->setValue(-1);
  connect(m_normalizeTargetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onNormalizeTargetChanged);
  layout->addWidget(m_normalizeTargetSpin, row, 2);
}

void NMVoiceStudioPanel::setupFilterSection() {
  m_filterGroup = new QGroupBox(tr("Filters / EQ"), m_contentWidget);
  auto *layout = new QGridLayout(m_filterGroup);

  int row = 0;

  // High-pass filter
  m_highPassCheck = new QCheckBox(tr("High-Pass"), m_filterGroup);
  connect(m_highPassCheck, &QCheckBox::toggled, this, &NMVoiceStudioPanel::onHighPassToggled);
  layout->addWidget(m_highPassCheck, row, 0);

  layout->addWidget(new QLabel(tr("Hz:"), m_filterGroup), row, 1);
  m_highPassFreqSpin = new QDoubleSpinBox(m_filterGroup);
  m_highPassFreqSpin->setRange(20, 500);
  m_highPassFreqSpin->setValue(80);
  connect(m_highPassFreqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onHighPassFreqChanged);
  layout->addWidget(m_highPassFreqSpin, row, 2);
  row++;

  // Low-pass filter
  m_lowPassCheck = new QCheckBox(tr("Low-Pass"), m_filterGroup);
  connect(m_lowPassCheck, &QCheckBox::toggled, this, &NMVoiceStudioPanel::onLowPassToggled);
  layout->addWidget(m_lowPassCheck, row, 0);

  layout->addWidget(new QLabel(tr("Hz:"), m_filterGroup), row, 1);
  m_lowPassFreqSpin = new QDoubleSpinBox(m_filterGroup);
  m_lowPassFreqSpin->setRange(1000, 20000);
  m_lowPassFreqSpin->setValue(12000);
  connect(m_lowPassFreqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onLowPassFreqChanged);
  layout->addWidget(m_lowPassFreqSpin, row, 2);
  row++;

  // 3-band EQ
  m_eqCheck = new QCheckBox(tr("3-Band EQ"), m_filterGroup);
  connect(m_eqCheck, &QCheckBox::toggled, this, &NMVoiceStudioPanel::onEQToggled);
  layout->addWidget(m_eqCheck, row, 0);
  row++;

  layout->addWidget(new QLabel(tr("Low (dB):"), m_filterGroup), row, 0);
  m_eqLowSpin = new QDoubleSpinBox(m_filterGroup);
  m_eqLowSpin->setRange(-12, 12);
  m_eqLowSpin->setValue(0);
  connect(m_eqLowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onEQLowChanged);
  layout->addWidget(m_eqLowSpin, row, 1);

  layout->addWidget(new QLabel(tr("Mid (dB):"), m_filterGroup), row, 2);
  m_eqMidSpin = new QDoubleSpinBox(m_filterGroup);
  m_eqMidSpin->setRange(-12, 12);
  m_eqMidSpin->setValue(0);
  connect(m_eqMidSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onEQMidChanged);
  layout->addWidget(m_eqMidSpin, row, 3);

  layout->addWidget(new QLabel(tr("High (dB):"), m_filterGroup), row, 4);
  m_eqHighSpin = new QDoubleSpinBox(m_filterGroup);
  m_eqHighSpin->setRange(-12, 12);
  m_eqHighSpin->setValue(0);
  connect(m_eqHighSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onEQHighChanged);
  layout->addWidget(m_eqHighSpin, row, 5);
  row++;

  // Noise gate
  m_noiseGateCheck = new QCheckBox(tr("Noise Gate"), m_filterGroup);
  connect(m_noiseGateCheck, &QCheckBox::toggled, this, &NMVoiceStudioPanel::onNoiseGateToggled);
  layout->addWidget(m_noiseGateCheck, row, 0);

  layout->addWidget(new QLabel(tr("Threshold (dB):"), m_filterGroup), row, 1);
  m_noiseGateThresholdSpin = new QDoubleSpinBox(m_filterGroup);
  m_noiseGateThresholdSpin->setRange(-60, 0);
  m_noiseGateThresholdSpin->setValue(-40);
  connect(m_noiseGateThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onNoiseGateThresholdChanged);
  layout->addWidget(m_noiseGateThresholdSpin, row, 2);
}

void NMVoiceStudioPanel::setupPresetSection() {
  auto *presetLayout = new QHBoxLayout();

  presetLayout->addWidget(new QLabel(tr("Preset:"), m_filterGroup));

  m_presetCombo = new QComboBox(m_filterGroup);
  m_presetCombo->addItem(tr("(No Preset)"));
  connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMVoiceStudioPanel::onPresetSelected);
  presetLayout->addWidget(m_presetCombo, 1);

  m_savePresetBtn = new QPushButton(tr("Save Preset"), m_filterGroup);
  connect(m_savePresetBtn, &QPushButton::clicked, this, &NMVoiceStudioPanel::onSavePresetClicked);
  presetLayout->addWidget(m_savePresetBtn);

  if (auto *filterLayout = qobject_cast<QGridLayout *>(m_filterGroup->layout())) {
    filterLayout->addLayout(presetLayout, filterLayout->rowCount(), 0, 1, 6);
  }
}

void NMVoiceStudioPanel::setupStatusBar() {
  auto *statusLayout = new QHBoxLayout();

  m_statusLabel = new QLabel(tr("Ready"), m_contentWidget);
  statusLayout->addWidget(m_statusLabel, 1);

  m_fileInfoLabel = new QLabel("", m_contentWidget);
  statusLayout->addWidget(m_fileInfoLabel);

  m_progressBar = new QProgressBar(m_contentWidget);
  m_progressBar->setMaximumWidth(100);
  m_progressBar->setVisible(false);
  statusLayout->addWidget(m_progressBar);

  // Status bar is added directly in setupUI
  Q_UNUSED(statusLayout);
}

void NMVoiceStudioPanel::setupMediaPlayer() {
  m_mediaPlayer = new QMediaPlayer(this);
  m_audioOutput = new QAudioOutput(this);
  m_mediaPlayer->setAudioOutput(m_audioOutput);

  connect(m_mediaPlayer, &QMediaPlayer::positionChanged,
          this, &NMVoiceStudioPanel::onPlaybackPositionChanged);
  connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
          this, &NMVoiceStudioPanel::onPlaybackStateChanged);
  connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged,
          this, &NMVoiceStudioPanel::onPlaybackMediaStatusChanged);
}

void NMVoiceStudioPanel::setupRecorder() {
  m_recorder = std::make_unique<audio::AudioRecorder>();
  auto result = m_recorder->initialize();
  if (result.isError()) {
    m_statusLabel->setText(tr("Recorder error: %1").arg(QString::fromStdString(result.error())));
    return;
  }

  // Set up callbacks with thread-safe invocation
  m_recorder->setOnLevelUpdate([this](const audio::LevelMeter &level) {
    QMetaObject::invokeMethod(this, [this, level]() { onLevelUpdate(level); },
                              Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingStateChanged([this](audio::RecordingState state) {
    QMetaObject::invokeMethod(this, [this, state]() {
      onRecordingStateChanged(static_cast<int>(state));
    }, Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingComplete([this](const audio::RecordingResult &recResult) {
    QMetaObject::invokeMethod(this, [this, recResult]() { onRecordingComplete(recResult); },
                              Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingError([this](const std::string &error) {
    QMetaObject::invokeMethod(this, [this, error]() {
      onRecordingError(QString::fromStdString(error));
    }, Qt::QueuedConnection);
  });

  refreshDeviceList();
  m_recorder->startMetering();
}

void NMVoiceStudioPanel::refreshDeviceList() {
  if (!m_recorder || !m_inputDeviceCombo) return;

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

void NMVoiceStudioPanel::updateUI() {
  bool hasClip = m_clip != nullptr;

  // Enable/disable controls based on clip state
  if (m_playBtn) m_playBtn->setEnabled(hasClip);
  if (m_stopBtn) m_stopBtn->setEnabled(hasClip);
  if (m_trimToSelectionBtn) m_trimToSelectionBtn->setEnabled(hasClip);
  if (m_resetTrimBtn) m_resetTrimBtn->setEnabled(hasClip);

  // Update duration label
  if (m_durationLabel && m_clip) {
    m_durationLabel->setText(formatTime(m_clip->getTrimmedDurationSeconds()));
  }
}

void NMVoiceStudioPanel::updateEditControls() {
  if (!m_clip) return;

  // Block signals while updating
  m_fadeInSpin->blockSignals(true);
  m_fadeOutSpin->blockSignals(true);
  m_preGainSpin->blockSignals(true);
  m_normalizeCheck->blockSignals(true);
  m_normalizeTargetSpin->blockSignals(true);
  m_highPassCheck->blockSignals(true);
  m_highPassFreqSpin->blockSignals(true);
  m_lowPassCheck->blockSignals(true);
  m_lowPassFreqSpin->blockSignals(true);
  m_eqCheck->blockSignals(true);
  m_eqLowSpin->blockSignals(true);
  m_eqMidSpin->blockSignals(true);
  m_eqHighSpin->blockSignals(true);
  m_noiseGateCheck->blockSignals(true);
  m_noiseGateThresholdSpin->blockSignals(true);

  const auto &edit = m_clip->edit;

  m_fadeInSpin->setValue(edit.fadeInMs);
  m_fadeOutSpin->setValue(edit.fadeOutMs);
  m_preGainSpin->setValue(edit.preGainDb);
  m_normalizeCheck->setChecked(edit.normalizeEnabled);
  m_normalizeTargetSpin->setValue(edit.normalizeTargetDbFS);
  m_highPassCheck->setChecked(edit.highPassEnabled);
  m_highPassFreqSpin->setValue(edit.highPassFreqHz);
  m_lowPassCheck->setChecked(edit.lowPassEnabled);
  m_lowPassFreqSpin->setValue(edit.lowPassFreqHz);
  m_eqCheck->setChecked(edit.eqEnabled);
  m_eqLowSpin->setValue(edit.eqLowGainDb);
  m_eqMidSpin->setValue(edit.eqMidGainDb);
  m_eqHighSpin->setValue(edit.eqHighGainDb);
  m_noiseGateCheck->setChecked(edit.noiseGateEnabled);
  m_noiseGateThresholdSpin->setValue(edit.noiseGateThresholdDb);

  // Unblock signals
  m_fadeInSpin->blockSignals(false);
  m_fadeOutSpin->blockSignals(false);
  m_preGainSpin->blockSignals(false);
  m_normalizeCheck->blockSignals(false);
  m_normalizeTargetSpin->blockSignals(false);
  m_highPassCheck->blockSignals(false);
  m_highPassFreqSpin->blockSignals(false);
  m_lowPassCheck->blockSignals(false);
  m_lowPassFreqSpin->blockSignals(false);
  m_eqCheck->blockSignals(false);
  m_eqLowSpin->blockSignals(false);
  m_eqMidSpin->blockSignals(false);
  m_eqHighSpin->blockSignals(false);
  m_noiseGateCheck->blockSignals(false);
  m_noiseGateThresholdSpin->blockSignals(false);
}

void NMVoiceStudioPanel::updatePlaybackState() {
  if (m_playBtn) {
    m_playBtn->setChecked(m_isPlaying);
    m_playBtn->setText(m_isPlaying ? tr("Pause") : tr("Play"));
  }
}

void NMVoiceStudioPanel::updateStatusBar() {
  if (!m_clip) {
    m_statusLabel->setText(tr("No file loaded"));
    m_fileInfoLabel->setText("");
    return;
  }

  m_statusLabel->setText(tr("Ready"));

  QString info = QString("%1 Hz, %2 ch, %3 samples")
                     .arg(m_clip->format.sampleRate)
                     .arg(m_clip->format.channels)
                     .arg(m_clip->samples.size());
  m_fileInfoLabel->setText(info);
}

bool NMVoiceStudioPanel::loadWavFile(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    m_statusLabel->setText(tr("Failed to open file"));
    return false;
  }

  QByteArray wavData = file.readAll();
  file.close();

  if (wavData.size() < 44) {
    m_statusLabel->setText(tr("Invalid WAV file"));
    return false;
  }

  // Parse WAV header
  const char *ptr = wavData.constData();

  // Check RIFF header
  if (memcmp(ptr, "RIFF", 4) != 0) {
    m_statusLabel->setText(tr("Not a RIFF file"));
    return false;
  }

  // Check WAVE format
  if (memcmp(ptr + 8, "WAVE", 4) != 0) {
    m_statusLabel->setText(tr("Not a WAVE file"));
    return false;
  }

  // Find fmt chunk
  int pos = 12;
  uint32_t sampleRate = 0;
  uint16_t channels = 0;
  uint16_t bitsPerSample = 0;

  while (pos < wavData.size() - 8) {
    const char *chunkId = ptr + pos;
    uint32_t chunkSize = *reinterpret_cast<const uint32_t *>(ptr + pos + 4);

    if (memcmp(chunkId, "fmt ", 4) == 0) {
      uint16_t audioFormat = *reinterpret_cast<const uint16_t *>(ptr + pos + 8);
      if (audioFormat != 1 && audioFormat != 3) {
        m_statusLabel->setText(tr("Unsupported audio format (not PCM)"));
        return false;
      }

      channels = *reinterpret_cast<const uint16_t *>(ptr + pos + 10);
      sampleRate = *reinterpret_cast<const uint32_t *>(ptr + pos + 12);
      bitsPerSample = *reinterpret_cast<const uint16_t *>(ptr + pos + 22);
    } else if (memcmp(chunkId, "data", 4) == 0) {
      // Found data chunk
      const char *audioData = ptr + pos + 8;
      size_t audioSize = std::min(static_cast<size_t>(chunkSize),
                                  static_cast<size_t>(wavData.size() - pos - 8));

      // Create new clip
      m_clip = std::make_unique<VoiceClip>();
      m_clip->sourcePath = filePath.toStdString();
      m_clip->format.sampleRate = sampleRate;
      m_clip->format.channels = static_cast<uint8_t>(channels);
      m_clip->format.bitsPerSample = static_cast<uint8_t>(bitsPerSample);

      // Convert to float samples (mono)
      size_t numSamples = audioSize / (bitsPerSample / 8) / channels;
      m_clip->samples.resize(numSamples);

      if (bitsPerSample == 16) {
        const int16_t *src = reinterpret_cast<const int16_t *>(audioData);
        for (size_t i = 0; i < numSamples; ++i) {
          // Mix to mono if stereo
          float sample = 0.0f;
          for (size_t ch = 0; ch < channels; ++ch) {
            sample += static_cast<float>(src[i * channels + ch]) / 32768.0f;
          }
          m_clip->samples[i] = sample / static_cast<float>(channels);
        }
      } else if (bitsPerSample == 24) {
        const uint8_t *src = reinterpret_cast<const uint8_t *>(audioData);
        for (size_t i = 0; i < numSamples; ++i) {
          float sample = 0.0f;
          for (size_t ch = 0; ch < channels; ++ch) {
            size_t byteIdx = (i * channels + ch) * 3;
            int32_t val = (static_cast<int32_t>(src[byteIdx + 2]) << 16) |
                          (static_cast<int32_t>(src[byteIdx + 1]) << 8) |
                          static_cast<int32_t>(src[byteIdx]);
            if (val & 0x800000) val |= static_cast<int32_t>(0xFF000000); // Sign extend
            sample += static_cast<float>(val) / 8388608.0f;
          }
          m_clip->samples[i] = sample / static_cast<float>(channels);
        }
      } else if (bitsPerSample == 32) {
        const float *src = reinterpret_cast<const float *>(audioData);
        for (size_t i = 0; i < numSamples; ++i) {
          float sample = 0.0f;
          for (size_t ch = 0; ch < channels; ++ch) {
            sample += src[i * channels + ch];
          }
          m_clip->samples[i] = sample / static_cast<float>(channels);
        }
      } else {
        m_statusLabel->setText(tr("Unsupported bit depth: %1").arg(bitsPerSample));
        m_clip.reset();
        return false;
      }

      return true;
    }

    pos += 8 + static_cast<int>(chunkSize);
    if (chunkSize & 1) pos++; // Pad to even boundary
  }

  m_statusLabel->setText(tr("No data chunk found"));
  return false;
}

bool NMVoiceStudioPanel::saveWavFile(const QString &filePath) {
  if (!m_clip) return false;

  // Render processed audio
  std::vector<float> processed = renderProcessedAudio();
  if (processed.empty()) return false;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    m_statusLabel->setText(tr("Failed to create file"));
    return false;
  }

  // Write WAV header
  uint32_t sampleRate = m_clip->format.sampleRate;
  uint16_t channels = 1; // Output is always mono
  uint16_t bitsPerSample = 16;
  uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
  uint16_t blockAlign = static_cast<uint16_t>(channels * bitsPerSample / 8);
  uint32_t dataSize = static_cast<uint32_t>(processed.size() * sizeof(int16_t));
  uint32_t fileSize = 36 + dataSize;

  // RIFF header
  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char *>(&fileSize), 4);
  file.write("WAVE", 4);

  // fmt chunk
  file.write("fmt ", 4);
  uint32_t fmtSize = 16;
  file.write(reinterpret_cast<const char *>(&fmtSize), 4);
  uint16_t audioFormat = 1; // PCM
  file.write(reinterpret_cast<const char *>(&audioFormat), 2);
  file.write(reinterpret_cast<const char *>(&channels), 2);
  file.write(reinterpret_cast<const char *>(&sampleRate), 4);
  file.write(reinterpret_cast<const char *>(&byteRate), 4);
  file.write(reinterpret_cast<const char *>(&blockAlign), 2);
  file.write(reinterpret_cast<const char *>(&bitsPerSample), 2);

  // data chunk
  file.write("data", 4);
  file.write(reinterpret_cast<const char *>(&dataSize), 4);

  // Write samples
  for (float sample : processed) {
    // Clamp and convert to 16-bit
    sample = std::clamp(sample, -1.0f, 1.0f);
    int16_t intSample = static_cast<int16_t>(sample * 32767.0f);
    file.write(reinterpret_cast<const char *>(&intSample), 2);
  }

  file.close();

  m_lastSavedEdit = m_clip->edit;
  m_statusLabel->setText(tr("Saved: %1").arg(filePath));

  return true;
}

std::vector<float> NMVoiceStudioPanel::renderProcessedAudio() {
  if (!m_clip) return {};

  return AudioProcessor::process(m_clip->samples, m_clip->edit, m_clip->format);
}

void NMVoiceStudioPanel::applyPreset(const QString &presetName) {
  if (!m_clip) return;

  for (const auto &preset : m_presets) {
    if (preset.name == presetName) {
      VoiceClipEdit oldEdit = m_clip->edit;
      VoiceClipEdit newEdit = preset.edit;

      // Preserve trim settings
      newEdit.trimStartSamples = oldEdit.trimStartSamples;
      newEdit.trimEndSamples = oldEdit.trimEndSamples;

      m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit,
                                             tr("Apply preset: %1").arg(presetName)));

      updateEditControls();
      if (m_waveformWidget) m_waveformWidget->update();
      return;
    }
  }
}

void NMVoiceStudioPanel::pushUndoCommand([[maybe_unused]] const QString &description) {
  if (!m_clip) return;

  // This is called after the edit has already been made
  // We need to store the current state as the "new" state
  // The undo command should be pushed before the edit is made
}

QString NMVoiceStudioPanel::formatTime(double seconds) const {
  int minutes = static_cast<int>(seconds) / 60;
  double secs = seconds - minutes * 60;
  return QString("%1:%2").arg(minutes).arg(secs, 4, 'f', 1, '0');
}

QString NMVoiceStudioPanel::formatTimeMs(double seconds) const {
  int minutes = static_cast<int>(seconds) / 60;
  int secs = static_cast<int>(seconds) % 60;
  int ms = static_cast<int>((seconds - static_cast<int>(seconds)) * 1000);
  return QString("%1:%2.%3").arg(minutes).arg(secs, 2, 10, QChar('0')).arg(ms, 3, 10, QChar('0'));
}

// ============================================================================
// Slot Implementations
// ============================================================================

void NMVoiceStudioPanel::onInputDeviceChanged(int index) {
  if (!m_recorder || !m_inputDeviceCombo) return;

  QString deviceId = m_inputDeviceCombo->itemData(index).toString();
  m_recorder->setInputDevice(deviceId.toStdString());
}

void NMVoiceStudioPanel::onRecordClicked() {
  if (!m_recorder) return;

  // Generate temp file path
  m_tempRecordingPath = QDir::temp().filePath(
      QString("novelmind_recording_%1.wav").arg(QDateTime::currentMSecsSinceEpoch()));

  auto result = m_recorder->startRecording(m_tempRecordingPath.toStdString());
  if (result.isError()) {
    onRecordingError(QString::fromStdString(result.error()));
    return;
  }

  m_isRecording = true;
  m_recordBtn->setEnabled(false);
  m_stopRecordBtn->setEnabled(true);
  m_cancelRecordBtn->setEnabled(true);
}

void NMVoiceStudioPanel::onStopRecordClicked() {
  if (!m_recorder || !m_isRecording) return;
  m_recorder->stopRecording();
}

void NMVoiceStudioPanel::onCancelRecordClicked() {
  if (!m_recorder || !m_isRecording) return;

  m_recorder->cancelRecording();
  m_isRecording = false;

  m_recordBtn->setEnabled(true);
  m_stopRecordBtn->setEnabled(false);
  m_cancelRecordBtn->setEnabled(false);
  m_recordingTimeLabel->setText("0:00.0");

  // Remove temp file
  QFile::remove(m_tempRecordingPath);
  m_tempRecordingPath.clear();
}

void NMVoiceStudioPanel::onPlayClicked() {
  if (!m_clip || !m_mediaPlayer) return;

  if (m_isPlaying) {
    m_mediaPlayer->pause();
    m_isPlaying = false;
  } else {
    // Render processed audio to temp file for playback
    QString tempPath = QDir::temp().filePath("novelmind_preview.wav");
    if (saveWavFile(tempPath)) {
      m_mediaPlayer->setSource(QUrl::fromLocalFile(tempPath));
      m_mediaPlayer->play();
      m_isPlaying = true;
    }
  }

  updatePlaybackState();
}

void NMVoiceStudioPanel::onStopClicked() {
  if (!m_mediaPlayer) return;

  m_mediaPlayer->stop();
  m_isPlaying = false;

  if (m_waveformWidget) {
    m_waveformWidget->setPlayheadPosition(0);
  }
  if (m_positionLabel) {
    m_positionLabel->setText("0:00.0");
  }

  updatePlaybackState();
}

void NMVoiceStudioPanel::onLoopClicked(bool checked) {
  m_isLooping = checked;
}

void NMVoiceStudioPanel::onTrimToSelection() {
  if (!m_clip || !m_waveformWidget) return;

  double selStart = m_waveformWidget->getSelectionStart();
  double selEnd = m_waveformWidget->getSelectionEnd();

  if (selStart >= selEnd) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;

  newEdit.trimStartSamples = static_cast<int64_t>(selStart * m_clip->format.sampleRate);
  newEdit.trimEndSamples = static_cast<int64_t>(
      (m_clip->getDurationSeconds() - selEnd) * m_clip->format.sampleRate);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Trim to selection")));

  m_waveformWidget->clearSelection();
  m_waveformWidget->update();
  updateUI();
}

void NMVoiceStudioPanel::onResetTrim() {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.trimStartSamples = 0;
  newEdit.trimEndSamples = 0;

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Reset trim")));

  if (m_waveformWidget) m_waveformWidget->update();
  updateUI();
}

void NMVoiceStudioPanel::onFadeInChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.fadeInMs = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change fade in")));
}

void NMVoiceStudioPanel::onFadeOutChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.fadeOutMs = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change fade out")));
}

void NMVoiceStudioPanel::onPreGainChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.preGainDb = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change pre-gain")));
}

void NMVoiceStudioPanel::onNormalizeToggled(bool checked) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.normalizeEnabled = checked;

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit,
                                         checked ? tr("Enable normalize") : tr("Disable normalize")));
}

void NMVoiceStudioPanel::onNormalizeTargetChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.normalizeTargetDbFS = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change normalize target")));
}

void NMVoiceStudioPanel::onHighPassToggled(bool checked) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.highPassEnabled = checked;

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit,
                                         checked ? tr("Enable high-pass") : tr("Disable high-pass")));
}

void NMVoiceStudioPanel::onHighPassFreqChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.highPassFreqHz = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change high-pass frequency")));
}

void NMVoiceStudioPanel::onLowPassToggled(bool checked) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.lowPassEnabled = checked;

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit,
                                         checked ? tr("Enable low-pass") : tr("Disable low-pass")));
}

void NMVoiceStudioPanel::onLowPassFreqChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.lowPassFreqHz = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change low-pass frequency")));
}

void NMVoiceStudioPanel::onEQToggled(bool checked) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.eqEnabled = checked;

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit,
                                         checked ? tr("Enable EQ") : tr("Disable EQ")));
}

void NMVoiceStudioPanel::onEQLowChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.eqLowGainDb = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change EQ low band")));
}

void NMVoiceStudioPanel::onEQMidChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.eqMidGainDb = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change EQ mid band")));
}

void NMVoiceStudioPanel::onEQHighChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.eqHighGainDb = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change EQ high band")));
}

void NMVoiceStudioPanel::onNoiseGateToggled(bool checked) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.noiseGateEnabled = checked;

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit,
                                         checked ? tr("Enable noise gate") : tr("Disable noise gate")));
}

void NMVoiceStudioPanel::onNoiseGateThresholdChanged(double value) {
  if (!m_clip) return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = oldEdit;
  newEdit.noiseGateThresholdDb = static_cast<float>(value);

  m_undoStack->push(new VoiceEditCommand(m_clip.get(), oldEdit, newEdit, tr("Change noise gate threshold")));
}

void NMVoiceStudioPanel::onPresetSelected(int index) {
  if (index <= 0) return;

  if (index - 1 < static_cast<int>(m_presets.size())) {
    applyPreset(m_presets[static_cast<size_t>(index - 1)].name);
  }

  // Reset combo to "No Preset"
  m_presetCombo->blockSignals(true);
  m_presetCombo->setCurrentIndex(0);
  m_presetCombo->blockSignals(false);
}

void NMVoiceStudioPanel::onSavePresetClicked() {
  if (!m_clip) return;

  QString name = QInputDialog::getText(this, tr("Save Preset"), tr("Preset name:"));
  if (name.isEmpty()) return;

  Preset preset;
  preset.name = name;
  preset.edit = m_clip->edit;

  // Clear trim from preset (trim is clip-specific)
  preset.edit.trimStartSamples = 0;
  preset.edit.trimEndSamples = 0;

  m_presets.push_back(preset);
  m_presetCombo->addItem(name);

  m_statusLabel->setText(tr("Preset saved: %1").arg(name));
}

void NMVoiceStudioPanel::onSaveClicked() {
  if (m_currentFilePath.isEmpty()) {
    onSaveAsClicked();
    return;
  }

  saveWavFile(m_currentFilePath);
  emit fileSaved(m_currentFilePath);
}

void NMVoiceStudioPanel::onSaveAsClicked() {
  QString filePath = QFileDialog::getSaveFileName(
      this, tr("Save Voice File"), QString(), tr("WAV Files (*.wav)"));

  if (filePath.isEmpty()) return;

  if (!filePath.endsWith(".wav", Qt::CaseInsensitive)) {
    filePath += ".wav";
  }

  if (saveWavFile(filePath)) {
    m_currentFilePath = filePath;
    emit fileSaved(filePath);
  }
}

void NMVoiceStudioPanel::onExportClicked() {
  if (!m_clip) return;

  QString defaultName;
  if (!m_currentLineId.isEmpty()) {
    defaultName = QString("%1_%2.wav").arg(m_currentLineId).arg(m_currentLocale);
  }

  QString filePath = QFileDialog::getSaveFileName(
      this, tr("Export Voice File"), defaultName, tr("WAV Files (*.wav)"));

  if (filePath.isEmpty()) return;

  if (!filePath.endsWith(".wav", Qt::CaseInsensitive)) {
    filePath += ".wav";
  }

  if (saveWavFile(filePath)) {
    // Update voice manifest if available
    if (m_manifest && !m_currentLineId.isEmpty()) {
      m_manifest->markAsRecorded(m_currentLineId.toStdString(),
                                 m_currentLocale.toStdString(),
                                 filePath.toStdString());
      emit assetUpdated(m_currentLineId, filePath);
    }

    m_statusLabel->setText(tr("Exported: %1").arg(filePath));
  }
}

void NMVoiceStudioPanel::onOpenClicked() {
  QString filePath = QFileDialog::getOpenFileName(
      this, tr("Open Voice File"), QString(), tr("Audio Files (*.wav *.ogg *.mp3)"));

  if (!filePath.isEmpty()) {
    loadFile(filePath);
  }
}

void NMVoiceStudioPanel::onUndoClicked() {
  if (m_undoStack && m_undoStack->canUndo()) {
    m_undoStack->undo();
    updateEditControls();
    if (m_waveformWidget) m_waveformWidget->update();
    updateUI();
  }
}

void NMVoiceStudioPanel::onRedoClicked() {
  if (m_undoStack && m_undoStack->canRedo()) {
    m_undoStack->redo();
    updateEditControls();
    if (m_waveformWidget) m_waveformWidget->update();
    updateUI();
  }
}

void NMVoiceStudioPanel::onWaveformSelectionChanged(double start, double end) {
  if (m_trimToSelectionBtn) {
    m_trimToSelectionBtn->setEnabled(start < end);
  }
}

void NMVoiceStudioPanel::onWaveformPlayheadClicked(double seconds) {
  if (m_mediaPlayer && m_isPlaying) {
    m_mediaPlayer->setPosition(static_cast<qint64>(seconds * 1000));
  }
}

void NMVoiceStudioPanel::onPlaybackPositionChanged(qint64 position) {
  double seconds = static_cast<double>(position) / 1000.0;

  if (m_waveformWidget) {
    m_waveformWidget->setPlayheadPosition(seconds);
  }

  if (m_positionLabel) {
    m_positionLabel->setText(formatTime(seconds));
  }
}

void NMVoiceStudioPanel::onPlaybackStateChanged() {
  if (!m_mediaPlayer) return;

  auto state = m_mediaPlayer->playbackState();
  m_isPlaying = (state == QMediaPlayer::PlayingState);

  updatePlaybackState();
}

void NMVoiceStudioPanel::onPlaybackMediaStatusChanged() {
  if (!m_mediaPlayer) return;

  auto status = m_mediaPlayer->mediaStatus();

  if (status == QMediaPlayer::EndOfMedia) {
    if (m_isLooping) {
      m_mediaPlayer->setPosition(0);
      m_mediaPlayer->play();
    } else {
      m_isPlaying = false;
      updatePlaybackState();
    }
  }
}

void NMVoiceStudioPanel::onLevelUpdate(const audio::LevelMeter &level) {
  if (m_vuMeter) {
    m_vuMeter->setLevel(level.rmsLevelDb, level.peakLevelDb, level.clipping);
  }

  if (m_levelLabel) {
    m_levelLabel->setText(tr("Level: %1 dB").arg(level.rmsLevelDb, 0, 'f', 1));
  }
}

void NMVoiceStudioPanel::onRecordingStateChanged(int state) {
  auto recordingState = static_cast<audio::RecordingState>(state);

  switch (recordingState) {
  case audio::RecordingState::Idle:
    m_isRecording = false;
    m_recordBtn->setEnabled(true);
    m_stopRecordBtn->setEnabled(false);
    m_cancelRecordBtn->setEnabled(false);
    break;

  case audio::RecordingState::Recording:
    m_isRecording = true;
    break;

  case audio::RecordingState::Error:
    m_isRecording = false;
    m_recordBtn->setEnabled(true);
    m_stopRecordBtn->setEnabled(false);
    m_cancelRecordBtn->setEnabled(false);
    break;

  default:
    break;
  }
}

void NMVoiceStudioPanel::onRecordingComplete(const audio::RecordingResult &result) {
  m_isRecording = false;
  m_recordBtn->setEnabled(true);
  m_stopRecordBtn->setEnabled(false);
  m_cancelRecordBtn->setEnabled(false);

  // Load the recorded file for editing
  if (loadFile(QString::fromStdString(result.filePath))) {
    m_statusLabel->setText(tr("Recording complete: %1s").arg(result.duration, 0, 'f', 1));
    emit recordingCompleted(QString::fromStdString(result.filePath));
  }
}

void NMVoiceStudioPanel::onRecordingError(const QString &error) {
  m_isRecording = false;
  m_recordBtn->setEnabled(true);
  m_stopRecordBtn->setEnabled(false);
  m_cancelRecordBtn->setEnabled(false);

  m_statusLabel->setText(tr("Recording error: %1").arg(error));

  QMessageBox::warning(this, tr("Recording Error"), error);
}

void NMVoiceStudioPanel::onUpdateTimer() {
  if (m_isRecording && m_recorder) {
    float duration = m_recorder->getRecordingDuration();
    m_recordingTimeLabel->setText(formatTime(duration));
  }
}

} // namespace NovelMind::editor::qt
