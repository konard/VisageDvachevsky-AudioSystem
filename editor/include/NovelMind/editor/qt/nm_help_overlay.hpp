#pragma once

/**
 * @file nm_help_overlay.hpp
 * @brief Visual Overlay for Tutorial System
 *
 * Provides the visual layer for tutorials:
 * - Spotlight effect (dims everything except the target)
 * - Callout bubbles with text and navigation
 * - Animated highlights
 * - Progress indicator
 */

#include "nm_tutorial_types.hpp"
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QWidget>

class QVBoxLayout;
class QHBoxLayout;

namespace NovelMind::editor::qt {

/**
 * @brief Callout bubble widget for tutorial steps
 */
class NMCalloutBubble : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
  explicit NMCalloutBubble(QWidget *parent = nullptr);
  ~NMCalloutBubble() override = default;

  /**
   * @brief Set the step content
   */
  void setStep(const TutorialStep &step, int currentIndex, int totalSteps);

  /**
   * @brief Set placement relative to anchor
   */
  void setPlacement(CalloutPlacement placement);

  /**
   * @brief Set whether back button is enabled
   */
  void setBackEnabled(bool enabled);

  /**
   * @brief Set whether next button shows "Finish" text
   */
  void setIsLastStep(bool isLast);

  /**
   * @brief Get opacity for animation
   */
  [[nodiscard]] qreal opacity() const { return m_opacity; }

  /**
   * @brief Set opacity for animation
   */
  void setOpacity(qreal opacity);

  /**
   * @brief Position the bubble relative to an anchor rect
   * @param anchorRect Global rect of the anchor widget
   * @param parentRect Global rect of the parent (overlay)
   */
  void positionRelativeTo(const QRect &anchorRect, const QRect &parentRect);

signals:
  void nextClicked();
  void backClicked();
  void skipClicked();
  void closeClicked();
  void dontShowAgainClicked();
  void learnMoreClicked(const QString &url);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void setupUI();
  void updateArrowPosition();

  QVBoxLayout *m_mainLayout = nullptr;

  // Header
  QWidget *m_header = nullptr;
  QLabel *m_titleLabel = nullptr;
  QPushButton *m_closeButton = nullptr;

  // Content
  QLabel *m_contentLabel = nullptr;
  QLabel *m_detailLabel = nullptr;
  QPushButton *m_learnMoreButton = nullptr;

  // Footer
  QWidget *m_footer = nullptr;
  QLabel *m_progressLabel = nullptr;
  QPushButton *m_dontShowButton = nullptr;
  QPushButton *m_skipButton = nullptr;
  QPushButton *m_backButton = nullptr;
  QPushButton *m_nextButton = nullptr;

  // State
  CalloutPlacement m_placement = CalloutPlacement::Auto;
  CalloutPlacement m_actualPlacement = CalloutPlacement::Bottom;
  QPoint m_arrowTip;
  QString m_learnMoreUrl;
  qreal m_opacity = 1.0;

  static constexpr int BUBBLE_WIDTH = 320;
  static constexpr int ARROW_SIZE = 10;
  static constexpr int MARGIN = 16;
};

/**
 * @brief Full-screen overlay for tutorial highlights
 *
 * This widget covers the entire main window and provides:
 * - Semi-transparent darkening effect
 * - Spotlight cutout for the target widget
 * - Callout bubble positioning
 * - Smooth animations
 */
class NMHelpOverlay : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Construct the overlay
   * @param parent The main window (overlay covers entire window)
   */
  explicit NMHelpOverlay(QWidget *parent);
  ~NMHelpOverlay() override;

  /**
   * @brief Show a tutorial step
   * @param step The step to display
   * @param currentIndex Current step index (0-based)
   * @param totalSteps Total number of steps
   */
  void showStep(const TutorialStep &step, int currentIndex, int totalSteps);

  /**
   * @brief Hide the overlay with animation
   */
  void hideOverlay();

  /**
   * @brief Update the spotlight position (call if anchor widget moves)
   */
  void updateSpotlight();

  /**
   * @brief Set the highlight style
   */
  void setHighlightStyle(HighlightStyle style);

  /**
   * @brief Check if overlay is currently visible
   */
  [[nodiscard]] bool isOverlayVisible() const { return m_isVisible; }

signals:
  void nextClicked();
  void backClicked();
  void skipClicked();
  void closeClicked();
  void dontShowAgainClicked();
  void learnMoreClicked(const QString &url);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void onAnimationFinished();
  void onPulseAnimation();

private:
  void setupUI();
  void startShowAnimation();
  void startHideAnimation();
  void updateLayout();
  void drawSpotlight(QPainter &painter);
  void drawOutlineHighlight(QPainter &painter);
  void drawPulseHighlight(QPainter &painter);
  void drawArrowHighlight(QPainter &painter);
  QRect getSpotlightRect() const;

  // UI Components
  NMCalloutBubble *m_bubble = nullptr;

  // Current state
  TutorialStep m_currentStep;
  QString m_anchorId;
  QRect m_spotlightRect;
  HighlightStyle m_highlightStyle = HighlightStyle::Spotlight;
  bool m_isVisible = false;

  // Animation
  QPropertyAnimation *m_fadeAnimation = nullptr;
  QPropertyAnimation *m_pulseAnimation = nullptr;
  qreal m_overlayOpacity = 0.0;
  qreal m_pulsePhase = 0.0;
  qreal m_highlightOpacity = 1.0;

  // Visual settings
  static constexpr int SPOTLIGHT_PADDING = 8;
  static constexpr int SPOTLIGHT_RADIUS = 6;
  static constexpr int ANIMATION_DURATION = 200;
  static constexpr qreal OVERLAY_OPACITY = 0.7;
};

} // namespace NovelMind::editor::qt
