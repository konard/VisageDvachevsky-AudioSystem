/**
 * @file nm_help_overlay.cpp
 * @brief Implementation of the Help Overlay visual components
 */

#include "NovelMind/editor/qt/nm_help_overlay.hpp"
#include "NovelMind/editor/qt/nm_anchor_registry.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include <QApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

namespace NovelMind::editor::qt {

// ============================================================================
// NMCalloutBubble Implementation
// ============================================================================

NMCalloutBubble::NMCalloutBubble(QWidget *parent) : QWidget(parent) {
  setupUI();
  setFixedWidth(BUBBLE_WIDTH);
}

void NMCalloutBubble::setupUI() {
  const auto &palette = NMStyleManager::instance().palette();
  const auto &spacing = NMStyleManager::instance().spacing();

  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(spacing.md, spacing.md, spacing.md,
                                   spacing.md);
  m_mainLayout->setSpacing(spacing.sm);

  // Header with title and close button
  m_header = new QWidget(this);
  auto *headerLayout = new QHBoxLayout(m_header);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(spacing.sm);

  m_titleLabel = new QLabel(m_header);
  m_titleLabel->setStyleSheet(
      QString("QLabel { color: %1; font-weight: bold; font-size: 13px; }")
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary)));
  headerLayout->addWidget(m_titleLabel, 1);

  m_closeButton = new QPushButton("×", m_header);
  m_closeButton->setFixedSize(20, 20);
  m_closeButton->setCursor(Qt::PointingHandCursor);
  m_closeButton->setStyleSheet(
      QString("QPushButton { background: transparent; color: %1; border: "
              "none; font-size: 16px; }"
              "QPushButton:hover { color: %2; }")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary)));
  connect(m_closeButton, &QPushButton::clicked, this,
          &NMCalloutBubble::closeClicked);
  headerLayout->addWidget(m_closeButton);

  m_mainLayout->addWidget(m_header);

  // Content
  m_contentLabel = new QLabel(this);
  m_contentLabel->setWordWrap(true);
  m_contentLabel->setStyleSheet(
      QString("QLabel { color: %1; font-size: 12px; line-height: 1.4; }")
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary)));
  m_mainLayout->addWidget(m_contentLabel);

  // Detail text (optional, hidden by default)
  m_detailLabel = new QLabel(this);
  m_detailLabel->setWordWrap(true);
  m_detailLabel->setStyleSheet(
      QString("QLabel { color: %1; font-size: 11px; }")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));
  m_detailLabel->hide();
  m_mainLayout->addWidget(m_detailLabel);

  // Learn more button (optional, hidden by default)
  m_learnMoreButton = new QPushButton("Learn more →", this);
  m_learnMoreButton->setCursor(Qt::PointingHandCursor);
  m_learnMoreButton->setStyleSheet(
      QString("QPushButton { background: transparent; color: %1; border: "
              "none; text-align: left; font-size: 11px; padding: 0; }"
              "QPushButton:hover { color: %2; text-decoration: underline; }")
          .arg(NMStyleManager::colorToStyleString(palette.accentPrimary))
          .arg(NMStyleManager::colorToStyleString(palette.accentHover)));
  m_learnMoreButton->hide();
  connect(m_learnMoreButton, &QPushButton::clicked, this, [this]() {
    emit learnMoreClicked(m_learnMoreUrl);
  });
  m_mainLayout->addWidget(m_learnMoreButton);

  m_mainLayout->addStretch();

  // Footer with navigation
  m_footer = new QWidget(this);
  auto *footerLayout = new QHBoxLayout(m_footer);
  footerLayout->setContentsMargins(0, spacing.sm, 0, 0);
  footerLayout->setSpacing(spacing.sm);

  // Progress indicator
  m_progressLabel = new QLabel(m_footer);
  m_progressLabel->setStyleSheet(
      QString("QLabel { color: %1; font-size: 10px; }")
          .arg(NMStyleManager::colorToStyleString(palette.textMuted)));
  footerLayout->addWidget(m_progressLabel);

  footerLayout->addStretch();

  // Don't show again button
  m_dontShowButton = new QPushButton("Don't show again", m_footer);
  m_dontShowButton->setCursor(Qt::PointingHandCursor);
  m_dontShowButton->setStyleSheet(
      QString("QPushButton { background: transparent; color: %1; border: "
              "none; font-size: 10px; }"
              "QPushButton:hover { color: %2; }")
          .arg(NMStyleManager::colorToStyleString(palette.textMuted))
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));
  connect(m_dontShowButton, &QPushButton::clicked, this,
          &NMCalloutBubble::dontShowAgainClicked);
  footerLayout->addWidget(m_dontShowButton);

  // Skip button
  m_skipButton = new QPushButton("Skip", m_footer);
  m_skipButton->setCursor(Qt::PointingHandCursor);
  m_skipButton->setStyleSheet(
      QString("QPushButton { background: transparent; color: %1; border: "
              "none; font-size: 11px; padding: 4px 8px; }"
              "QPushButton:hover { color: %2; }")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary)));
  connect(m_skipButton, &QPushButton::clicked, this,
          &NMCalloutBubble::skipClicked);
  footerLayout->addWidget(m_skipButton);

  // Back button
  m_backButton = new QPushButton("← Back", m_footer);
  m_backButton->setCursor(Qt::PointingHandCursor);
  m_backButton->setStyleSheet(
      QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
              "border-radius: 4px; padding: 6px 12px; font-size: 11px; }"
              "QPushButton:hover { background: %4; }"
              "QPushButton:disabled { color: %5; border-color: %5; }")
          .arg(NMStyleManager::colorToStyleString(palette.bgMedium))
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary))
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault))
          .arg(NMStyleManager::colorToStyleString(palette.bgLight))
          .arg(NMStyleManager::colorToStyleString(palette.textDisabled)));
  connect(m_backButton, &QPushButton::clicked, this,
          &NMCalloutBubble::backClicked);
  footerLayout->addWidget(m_backButton);

  // Next button
  m_nextButton = new QPushButton("Next →", m_footer);
  m_nextButton->setCursor(Qt::PointingHandCursor);
  m_nextButton->setStyleSheet(
      QString("QPushButton { background: %1; color: %2; border: none; "
              "border-radius: 4px; padding: 6px 16px; font-size: 11px; "
              "font-weight: bold; }"
              "QPushButton:hover { background: %3; }")
          .arg(NMStyleManager::colorToStyleString(palette.accentPrimary))
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary))
          .arg(NMStyleManager::colorToStyleString(palette.accentHover)));
  connect(m_nextButton, &QPushButton::clicked, this,
          &NMCalloutBubble::nextClicked);
  footerLayout->addWidget(m_nextButton);

  m_mainLayout->addWidget(m_footer);
}

void NMCalloutBubble::setStep(const TutorialStep &step, int currentIndex,
                              int totalSteps) {
  m_titleLabel->setText(step.title);
  m_contentLabel->setText(step.content);

  if (!step.detailText.isEmpty()) {
    m_detailLabel->setText(step.detailText);
    m_detailLabel->show();
  } else {
    m_detailLabel->hide();
  }

  if (!step.learnMoreUrl.isEmpty()) {
    m_learnMoreUrl = step.learnMoreUrl;
    m_learnMoreButton->setText(step.learnMoreLabel.isEmpty()
                                   ? "Learn more →"
                                   : step.learnMoreLabel);
    m_learnMoreButton->show();
  } else {
    m_learnMoreButton->hide();
  }

  m_progressLabel->setText(
      QString("Step %1 of %2").arg(currentIndex + 1).arg(totalSteps));

  m_skipButton->setVisible(step.allowSkip);
  m_placement = step.placement;

  adjustSize();
}

void NMCalloutBubble::setPlacement(CalloutPlacement placement) {
  m_placement = placement;
  update();
}

void NMCalloutBubble::setBackEnabled(bool enabled) {
  m_backButton->setEnabled(enabled);
  m_backButton->setVisible(enabled);
}

void NMCalloutBubble::setIsLastStep(bool isLast) {
  m_nextButton->setText(isLast ? "Finish ✓" : "Next →");
}

void NMCalloutBubble::setOpacity(qreal opacity) {
  m_opacity = opacity;
  setWindowOpacity(opacity);
  update();
}

void NMCalloutBubble::positionRelativeTo(const QRect &anchorRect,
                                         const QRect &parentRect) {
  // Determine actual placement based on available space
  int bubbleHeight = sizeHint().height();
  int bubbleWidth = BUBBLE_WIDTH;

  CalloutPlacement actual = m_placement;
  if (actual == CalloutPlacement::Auto) {
    // Prefer bottom, then top, then right, then left
    int spaceBelow = parentRect.bottom() - anchorRect.bottom();
    int spaceAbove = anchorRect.top() - parentRect.top();
    int spaceRight = parentRect.right() - anchorRect.right();
    int spaceLeft = anchorRect.left() - parentRect.left();

    if (spaceBelow >= bubbleHeight + ARROW_SIZE + MARGIN) {
      actual = CalloutPlacement::Bottom;
    } else if (spaceAbove >= bubbleHeight + ARROW_SIZE + MARGIN) {
      actual = CalloutPlacement::Top;
    } else if (spaceRight >= bubbleWidth + ARROW_SIZE + MARGIN) {
      actual = CalloutPlacement::Right;
    } else if (spaceLeft >= bubbleWidth + ARROW_SIZE + MARGIN) {
      actual = CalloutPlacement::Left;
    } else {
      actual = CalloutPlacement::Bottom; // Fallback
    }
  }

  m_actualPlacement = actual;

  // Calculate position
  int x = 0, y = 0;
  switch (actual) {
  case CalloutPlacement::Top:
  case CalloutPlacement::TopLeft:
  case CalloutPlacement::TopRight:
    x = anchorRect.center().x() - bubbleWidth / 2;
    y = anchorRect.top() - bubbleHeight - ARROW_SIZE - MARGIN;
    m_arrowTip = QPoint(bubbleWidth / 2, bubbleHeight);
    break;

  case CalloutPlacement::Bottom:
  case CalloutPlacement::BottomLeft:
  case CalloutPlacement::BottomRight:
    x = anchorRect.center().x() - bubbleWidth / 2;
    y = anchorRect.bottom() + ARROW_SIZE + MARGIN;
    m_arrowTip = QPoint(bubbleWidth / 2, 0);
    break;

  case CalloutPlacement::Left:
    x = anchorRect.left() - bubbleWidth - ARROW_SIZE - MARGIN;
    y = anchorRect.center().y() - bubbleHeight / 2;
    m_arrowTip = QPoint(bubbleWidth, bubbleHeight / 2);
    break;

  case CalloutPlacement::Right:
    x = anchorRect.right() + ARROW_SIZE + MARGIN;
    y = anchorRect.center().y() - bubbleHeight / 2;
    m_arrowTip = QPoint(0, bubbleHeight / 2);
    break;

  case CalloutPlacement::Center:
  case CalloutPlacement::Auto:
    x = parentRect.center().x() - bubbleWidth / 2;
    y = parentRect.center().y() - bubbleHeight / 2;
    m_arrowTip = QPoint(-1, -1); // No arrow
    break;
  }

  // Clamp to parent bounds
  x = qBound(parentRect.left() + MARGIN, x,
             parentRect.right() - bubbleWidth - MARGIN);
  y = qBound(parentRect.top() + MARGIN, y,
             parentRect.bottom() - bubbleHeight - MARGIN);

  // Convert to local coordinates
  QPoint localPos(x - parentRect.left(), y - parentRect.top());
  move(localPos);

  update();
}

void NMCalloutBubble::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const auto &palette = NMStyleManager::instance().palette();
  const auto &radius = NMStyleManager::instance().radius();

  // Draw bubble background
  QPainterPath path;
  QRectF bubbleRect = rect().adjusted(0, 0, 0, 0);

  // Adjust rect for arrow
  if (m_arrowTip.x() >= 0) {
    switch (m_actualPlacement) {
    case CalloutPlacement::Top:
    case CalloutPlacement::TopLeft:
    case CalloutPlacement::TopRight:
      bubbleRect.setBottom(bubbleRect.bottom() - ARROW_SIZE);
      break;
    case CalloutPlacement::Bottom:
    case CalloutPlacement::BottomLeft:
    case CalloutPlacement::BottomRight:
      bubbleRect.setTop(bubbleRect.top() + ARROW_SIZE);
      break;
    case CalloutPlacement::Left:
      bubbleRect.setRight(bubbleRect.right() - ARROW_SIZE);
      break;
    case CalloutPlacement::Right:
      bubbleRect.setLeft(bubbleRect.left() + ARROW_SIZE);
      break;
    default:
      break;
    }
  }

  path.addRoundedRect(bubbleRect, radius.lg, radius.lg);

  // Draw arrow
  if (m_arrowTip.x() >= 0) {
    QPolygonF arrow;
    switch (m_actualPlacement) {
    case CalloutPlacement::Top:
    case CalloutPlacement::TopLeft:
    case CalloutPlacement::TopRight:
      arrow << QPointF(m_arrowTip.x() - ARROW_SIZE, bubbleRect.bottom())
            << QPointF(m_arrowTip.x(), height())
            << QPointF(m_arrowTip.x() + ARROW_SIZE, bubbleRect.bottom());
      break;
    case CalloutPlacement::Bottom:
    case CalloutPlacement::BottomLeft:
    case CalloutPlacement::BottomRight:
      arrow << QPointF(m_arrowTip.x() - ARROW_SIZE, bubbleRect.top())
            << QPointF(m_arrowTip.x(), 0)
            << QPointF(m_arrowTip.x() + ARROW_SIZE, bubbleRect.top());
      break;
    case CalloutPlacement::Left:
      arrow << QPointF(bubbleRect.right(), m_arrowTip.y() - ARROW_SIZE)
            << QPointF(width(), m_arrowTip.y())
            << QPointF(bubbleRect.right(), m_arrowTip.y() + ARROW_SIZE);
      break;
    case CalloutPlacement::Right:
      arrow << QPointF(bubbleRect.left(), m_arrowTip.y() - ARROW_SIZE)
            << QPointF(0, m_arrowTip.y())
            << QPointF(bubbleRect.left(), m_arrowTip.y() + ARROW_SIZE);
      break;
    default:
      break;
    }
    if (!arrow.isEmpty()) {
      path.addPolygon(arrow);
    }
  }

  // Fill background
  painter.fillPath(path, palette.bgElevated);

  // Draw border
  painter.setPen(QPen(palette.borderLight, 1));
  painter.drawPath(path);
}

// ============================================================================
// NMHelpOverlay Implementation
// ============================================================================

NMHelpOverlay::NMHelpOverlay(QWidget *parent) : QWidget(parent) {
  setupUI();

  // Make overlay transparent to mouse events outside spotlight
  setAttribute(Qt::WA_TransparentForMouseEvents, false);
  setMouseTracking(true);

  // Install event filter on parent to track resize
  if (parent) {
    parent->installEventFilter(this);
  }
}

NMHelpOverlay::~NMHelpOverlay() {
  if (m_fadeAnimation) {
    m_fadeAnimation->stop();
    delete m_fadeAnimation;
  }
  if (m_pulseAnimation) {
    m_pulseAnimation->stop();
    delete m_pulseAnimation;
  }
}

void NMHelpOverlay::setupUI() {
  m_bubble = new NMCalloutBubble(this);
  m_bubble->hide();

  // Connect bubble signals
  connect(m_bubble, &NMCalloutBubble::nextClicked, this,
          &NMHelpOverlay::nextClicked);
  connect(m_bubble, &NMCalloutBubble::backClicked, this,
          &NMHelpOverlay::backClicked);
  connect(m_bubble, &NMCalloutBubble::skipClicked, this,
          &NMHelpOverlay::skipClicked);
  connect(m_bubble, &NMCalloutBubble::closeClicked, this,
          &NMHelpOverlay::closeClicked);
  connect(m_bubble, &NMCalloutBubble::dontShowAgainClicked, this,
          &NMHelpOverlay::dontShowAgainClicked);
  connect(m_bubble, &NMCalloutBubble::learnMoreClicked, this,
          &NMHelpOverlay::learnMoreClicked);

  // Setup fade animation
  m_fadeAnimation = new QPropertyAnimation(this, "windowOpacity", this);
  m_fadeAnimation->setDuration(ANIMATION_DURATION);
  connect(m_fadeAnimation, &QPropertyAnimation::finished, this,
          &NMHelpOverlay::onAnimationFinished);

  // Setup pulse animation for highlight
  m_pulseAnimation = new QPropertyAnimation(this, "windowOpacity", this);
  m_pulseAnimation->setDuration(1500);
  m_pulseAnimation->setLoopCount(-1); // Infinite
  connect(m_pulseAnimation, &QPropertyAnimation::valueChanged, this,
          &NMHelpOverlay::onPulseAnimation);

  hide();
}

void NMHelpOverlay::showStep(const TutorialStep &step, int currentIndex,
                             int totalSteps) {
  m_currentStep = step;
  m_anchorId = step.anchorId;
  m_highlightStyle = step.highlightStyle;

  // Update spotlight rect
  updateSpotlight();

  // Update bubble
  m_bubble->setStep(step, currentIndex, totalSteps);
  m_bubble->setBackEnabled(currentIndex > 0);
  m_bubble->setIsLastStep(currentIndex == totalSteps - 1);

  // Position bubble relative to spotlight
  if (m_spotlightRect.isValid()) {
    m_bubble->positionRelativeTo(m_spotlightRect, rect());
  } else {
    // Center the bubble if no anchor
    m_bubble->move((width() - m_bubble->width()) / 2,
                   (height() - m_bubble->height()) / 2);
  }

  m_bubble->show();

  if (!m_isVisible) {
    startShowAnimation();
  } else {
    update();
  }
}

void NMHelpOverlay::hideOverlay() {
  if (m_isVisible) {
    startHideAnimation();
  }
}

void NMHelpOverlay::updateSpotlight() {
  if (m_anchorId.isEmpty()) {
    m_spotlightRect = QRect();
    return;
  }

  m_spotlightRect = NMAnchorRegistry::instance().getGlobalRect(m_anchorId);

  // Convert from global to local coordinates
  if (m_spotlightRect.isValid() && parentWidget()) {
    QPoint topLeft = parentWidget()->mapFromGlobal(m_spotlightRect.topLeft());
    m_spotlightRect = QRect(topLeft, m_spotlightRect.size());

    // Add padding
    m_spotlightRect.adjust(-SPOTLIGHT_PADDING, -SPOTLIGHT_PADDING,
                           SPOTLIGHT_PADDING, SPOTLIGHT_PADDING);
  }

  // Reposition bubble
  if (m_bubble->isVisible() && m_spotlightRect.isValid()) {
    m_bubble->positionRelativeTo(m_spotlightRect, rect());
  }

  update();
}

void NMHelpOverlay::setHighlightStyle(HighlightStyle style) {
  m_highlightStyle = style;

  // Start/stop pulse animation
  if (style == HighlightStyle::Pulse) {
    m_pulseAnimation->start();
  } else {
    m_pulseAnimation->stop();
    m_pulsePhase = 0;
  }

  update();
}

void NMHelpOverlay::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)

  if (!m_isVisible && m_overlayOpacity <= 0) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const auto &palette = NMStyleManager::instance().palette();

  // Draw semi-transparent overlay
  QColor overlayColor = palette.bgDarkest;
  overlayColor.setAlphaF(OVERLAY_OPACITY * m_overlayOpacity);

  if (m_highlightStyle == HighlightStyle::Spotlight &&
      m_spotlightRect.isValid()) {
    drawSpotlight(painter);
  } else {
    // No spotlight - just draw overlay
    painter.fillRect(rect(), overlayColor);
  }

  // Draw highlight effect
  if (m_spotlightRect.isValid()) {
    switch (m_highlightStyle) {
    case HighlightStyle::Outline:
      drawOutlineHighlight(painter);
      break;
    case HighlightStyle::Pulse:
      drawPulseHighlight(painter);
      break;
    case HighlightStyle::Arrow:
      drawArrowHighlight(painter);
      break;
    default:
      break;
    }
  }
}

void NMHelpOverlay::drawSpotlight(QPainter &painter) {
  const auto &palette = NMStyleManager::instance().palette();

  // Create a path that covers everything except the spotlight area
  QPainterPath overlayPath;
  overlayPath.addRect(rect());

  QPainterPath spotlightPath;
  spotlightPath.addRoundedRect(m_spotlightRect, SPOTLIGHT_RADIUS,
                               SPOTLIGHT_RADIUS);

  QPainterPath finalPath = overlayPath.subtracted(spotlightPath);

  QColor overlayColor = palette.bgDarkest;
  overlayColor.setAlphaF(OVERLAY_OPACITY * m_overlayOpacity);
  painter.fillPath(finalPath, overlayColor);

  // Draw subtle glow around spotlight
  QColor glowColor = palette.accentPrimary;
  glowColor.setAlphaF(0.3 * m_overlayOpacity);
  QPen glowPen(glowColor, 2);
  painter.setPen(glowPen);
  painter.drawRoundedRect(m_spotlightRect.adjusted(-1, -1, 1, 1),
                          SPOTLIGHT_RADIUS, SPOTLIGHT_RADIUS);
}

void NMHelpOverlay::drawOutlineHighlight(QPainter &painter) {
  const auto &palette = NMStyleManager::instance().palette();

  // Animated dashed outline
  QPen pen(palette.accentPrimary, 2, Qt::DashLine);
  pen.setDashOffset(m_pulsePhase * 20);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(m_spotlightRect, SPOTLIGHT_RADIUS, SPOTLIGHT_RADIUS);
}

void NMHelpOverlay::drawPulseHighlight(QPainter &painter) {
  const auto &palette = NMStyleManager::instance().palette();

  // Pulsing glow effect
  qreal pulseIntensity = 0.5 + 0.5 * std::sin(m_pulsePhase * M_PI * 2);

  QColor glowColor = palette.accentPrimary;
  glowColor.setAlphaF(0.3 + 0.2 * pulseIntensity);

  for (int i = 0; i < 3; i++) {
    int offset = (i + 1) * 3;
    QColor layerColor = glowColor;
    layerColor.setAlphaF(glowColor.alphaF() * (1.0 - i * 0.3));

    QPen pen(layerColor, 2 - i * 0.5);
    painter.setPen(pen);
    painter.drawRoundedRect(
        m_spotlightRect.adjusted(-offset, -offset, offset, offset),
        SPOTLIGHT_RADIUS + offset, SPOTLIGHT_RADIUS + offset);
  }
}

void NMHelpOverlay::drawArrowHighlight(QPainter &painter) {
  const auto &palette = NMStyleManager::instance().palette();

  // Draw an arrow pointing to the spotlight
  QPointF arrowTip;
  QPointF arrowBase1, arrowBase2;

  // Position arrow above the spotlight
  arrowTip = QPointF(m_spotlightRect.center().x(),
                     m_spotlightRect.top() - 10);
  arrowBase1 = QPointF(arrowTip.x() - 15, arrowTip.y() - 25);
  arrowBase2 = QPointF(arrowTip.x() + 15, arrowTip.y() - 25);

  QPainterPath arrowPath;
  arrowPath.moveTo(arrowTip);
  arrowPath.lineTo(arrowBase1);
  arrowPath.lineTo(arrowBase2);
  arrowPath.closeSubpath();

  painter.fillPath(arrowPath, palette.accentPrimary);
}

void NMHelpOverlay::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateLayout();
}

void NMHelpOverlay::mousePressEvent(QMouseEvent *event) {
  // Allow clicking through to the spotlight area
  if (m_spotlightRect.isValid() && m_spotlightRect.contains(event->pos())) {
    event->ignore();
    return;
  }

  // Clicking outside spotlight closes the tutorial
  // (optional behavior - can be disabled)
  event->accept();
}

bool NMHelpOverlay::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget()) {
    if (event->type() == QEvent::Resize) {
      // Resize to match parent
      resize(parentWidget()->size());
      updateSpotlight();
    }
  }
  return QWidget::eventFilter(watched, event);
}

void NMHelpOverlay::onAnimationFinished() {
  if (!m_isVisible) {
    hide();
    m_bubble->hide();
  }
}

void NMHelpOverlay::onPulseAnimation() {
  // Update pulse phase for animation
  m_pulsePhase += 0.02;
  if (m_pulsePhase > 1.0) {
    m_pulsePhase = 0.0;
  }
  update();
}

void NMHelpOverlay::startShowAnimation() {
  m_isVisible = true;
  show();
  raise();

  m_overlayOpacity = 0.0;
  m_fadeAnimation->setStartValue(0.0);
  m_fadeAnimation->setEndValue(1.0);
  m_fadeAnimation->start();

  if (m_highlightStyle == HighlightStyle::Pulse ||
      m_highlightStyle == HighlightStyle::Outline) {
    QTimer::singleShot(ANIMATION_DURATION, this, [this]() {
      m_pulseAnimation->start();
    });
  }
}

void NMHelpOverlay::startHideAnimation() {
  m_isVisible = false;
  m_pulseAnimation->stop();

  m_fadeAnimation->setStartValue(1.0);
  m_fadeAnimation->setEndValue(0.0);
  m_fadeAnimation->start();
}

void NMHelpOverlay::updateLayout() {
  if (m_bubble->isVisible() && m_spotlightRect.isValid()) {
    m_bubble->positionRelativeTo(m_spotlightRect, rect());
  }
}

QRect NMHelpOverlay::getSpotlightRect() const {
  return m_spotlightRect;
}

} // namespace NovelMind::editor::qt
