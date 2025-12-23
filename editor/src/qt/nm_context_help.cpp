/**
 * @file nm_context_help.cpp
 * @brief Implementation of Context-sensitive Help components
 */

#include "NovelMind/editor/qt/nm_context_help.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_tutorial_manager.hpp"
#include <QApplication>
#include <QDesktopServices>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// ============================================================================
// NMEmptyStateWidget Implementation
// ============================================================================

NMEmptyStateWidget::NMEmptyStateWidget(QWidget *parent) : QWidget(parent) {
  setupUI();
}

void NMEmptyStateWidget::setupUI() {
  const auto &palette = NMStyleManager::instance().palette();
  const auto &spacing = NMStyleManager::instance().spacing();

  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(spacing.xxl, spacing.xxl, spacing.xxl,
                               spacing.xxl);
  m_layout->setSpacing(spacing.md);
  m_layout->setAlignment(Qt::AlignCenter);

  // Icon (emoji or symbolic)
  m_iconLabel = new QLabel(this);
  m_iconLabel->setAlignment(Qt::AlignCenter);
  m_iconLabel->setStyleSheet(
      QString("QLabel { font-size: 48px; color: %1; }")
          .arg(NMStyleManager::colorToStyleString(palette.textMuted)));
  m_layout->addWidget(m_iconLabel);

  // Title
  m_titleLabel = new QLabel(this);
  m_titleLabel->setAlignment(Qt::AlignCenter);
  m_titleLabel->setWordWrap(true);
  m_titleLabel->setStyleSheet(
      QString("QLabel { font-size: 14px; font-weight: bold; color: %1; }")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));
  m_layout->addWidget(m_titleLabel);

  // Description
  m_descriptionLabel = new QLabel(this);
  m_descriptionLabel->setAlignment(Qt::AlignCenter);
  m_descriptionLabel->setWordWrap(true);
  m_descriptionLabel->setMaximumWidth(300);
  m_descriptionLabel->setStyleSheet(
      QString("QLabel { font-size: 12px; color: %1; line-height: 1.4; }")
          .arg(NMStyleManager::colorToStyleString(palette.textMuted)));
  m_layout->addWidget(m_descriptionLabel);

  m_layout->addSpacing(spacing.md);

  // Primary action button
  m_primaryButton = new QPushButton(this);
  m_primaryButton->setCursor(Qt::PointingHandCursor);
  m_primaryButton->setStyleSheet(
      QString("QPushButton { background: %1; color: %2; border: none; "
              "border-radius: 4px; padding: 8px 16px; font-size: 12px; }"
              "QPushButton:hover { background: %3; }")
          .arg(NMStyleManager::colorToStyleString(palette.accentPrimary))
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary))
          .arg(NMStyleManager::colorToStyleString(palette.accentHover)));
  m_primaryButton->hide();
  connect(m_primaryButton, &QPushButton::clicked, this, [this]() {
    if (m_primaryCallback) {
      m_primaryCallback();
    }
    emit primaryActionClicked();
  });
  m_layout->addWidget(m_primaryButton, 0, Qt::AlignCenter);

  // Secondary action button (link style)
  m_secondaryButton = new QPushButton(this);
  m_secondaryButton->setCursor(Qt::PointingHandCursor);
  m_secondaryButton->setStyleSheet(
      QString("QPushButton { background: transparent; color: %1; border: "
              "none; font-size: 11px; }"
              "QPushButton:hover { color: %2; text-decoration: underline; }")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary)));
  m_secondaryButton->hide();
  connect(m_secondaryButton, &QPushButton::clicked, this, [this]() {
    if (m_secondaryCallback) {
      m_secondaryCallback();
    }
    emit secondaryActionClicked();
  });
  m_layout->addWidget(m_secondaryButton, 0, Qt::AlignCenter);

  // Learn more link
  m_learnMoreButton = new QPushButton(this);
  m_learnMoreButton->setCursor(Qt::PointingHandCursor);
  m_learnMoreButton->setStyleSheet(
      QString("QPushButton { background: transparent; color: %1; border: "
              "none; font-size: 11px; }"
              "QPushButton:hover { color: %2; text-decoration: underline; }")
          .arg(NMStyleManager::colorToStyleString(palette.accentPrimary))
          .arg(NMStyleManager::colorToStyleString(palette.accentHover)));
  m_learnMoreButton->hide();
  connect(m_learnMoreButton, &QPushButton::clicked, this, [this]() {
    if (!m_learnMoreUrl.isEmpty()) {
      QDesktopServices::openUrl(QUrl(m_learnMoreUrl));
      emit learnMoreClicked(m_learnMoreUrl);
    }
  });
  m_layout->addWidget(m_learnMoreButton, 0, Qt::AlignCenter);

  // Tutorial button
  m_tutorialButton = new QPushButton("Start tutorial", this);
  m_tutorialButton->setCursor(Qt::PointingHandCursor);
  m_tutorialButton->setStyleSheet(
      QString("QPushButton { background: transparent; color: %1; border: 1px "
              "solid %1; border-radius: 4px; padding: 6px 12px; font-size: "
              "11px; }"
              "QPushButton:hover { background: %2; }")
          .arg(NMStyleManager::colorToStyleString(palette.borderLight))
          .arg(NMStyleManager::colorToStyleString(palette.bgLight)));
  m_tutorialButton->hide();
  connect(m_tutorialButton, &QPushButton::clicked, this, [this]() {
    if (!m_tutorialId.isEmpty()) {
      NMTutorialManager::instance().startTutorial(m_tutorialId);
      emit startTutorialClicked(m_tutorialId);
    }
  });
  m_layout->addWidget(m_tutorialButton, 0, Qt::AlignCenter);

  m_layout->addStretch();
}

void NMEmptyStateWidget::setIcon(const QString &icon) {
  m_iconLabel->setText(icon);
}

void NMEmptyStateWidget::setTitle(const QString &title) {
  m_titleLabel->setText(title);
}

void NMEmptyStateWidget::setDescription(const QString &description) {
  m_descriptionLabel->setText(description);
}

void NMEmptyStateWidget::setPrimaryAction(const QString &text,
                                          std::function<void()> callback) {
  m_primaryButton->setText(text);
  m_primaryCallback = callback;
  m_primaryButton->setVisible(!text.isEmpty());
}

void NMEmptyStateWidget::setSecondaryAction(const QString &text,
                                            std::function<void()> callback) {
  m_secondaryButton->setText(text);
  m_secondaryCallback = callback;
  m_secondaryButton->setVisible(!text.isEmpty());
}

void NMEmptyStateWidget::setLearnMoreLink(const QString &url,
                                          const QString &label) {
  m_learnMoreUrl = url;
  m_learnMoreButton->setText(label.isEmpty() ? "Learn more" : label);
  m_learnMoreButton->setVisible(!url.isEmpty());
}

void NMEmptyStateWidget::setRelatedTutorial(const QString &tutorialId) {
  m_tutorialId = tutorialId;
  m_tutorialButton->setVisible(!tutorialId.isEmpty());
}

// ============================================================================
// NMHelpIcon Implementation
// ============================================================================

NMHelpIcon::NMHelpIcon(const QString &helpText, QWidget *parent)
    : QLabel(parent), m_helpText(helpText) {
  const auto &palette = NMStyleManager::instance().palette();

  setText("?");
  setAlignment(Qt::AlignCenter);
  setFixedSize(16, 16);
  setCursor(Qt::PointingHandCursor);

  setStyleSheet(
      QString("QLabel { background: %1; color: %2; border-radius: 8px; "
              "font-size: 10px; font-weight: bold; }"
              "QLabel:hover { background: %3; }")
          .arg(NMStyleManager::colorToStyleString(palette.bgLight))
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
          .arg(NMStyleManager::colorToStyleString(palette.borderLight)));

  setToolTip(m_helpText);
}

void NMHelpIcon::setHelpText(const QString &text) {
  m_helpText = text;
  setToolTip(m_helpText);
}

void NMHelpIcon::setLearnMoreUrl(const QString &url) {
  m_learnMoreUrl = url;

  if (!url.isEmpty()) {
    setToolTip(m_helpText + "\n\nClick for more information.");
  }
}

void NMHelpIcon::enterEvent(QEnterEvent *event) {
  QLabel::enterEvent(event);
}

void NMHelpIcon::leaveEvent(QEvent *event) {
  QLabel::leaveEvent(event);
}

void NMHelpIcon::mousePressEvent(QMouseEvent *event) {
  QLabel::mousePressEvent(event);

  if (!m_learnMoreUrl.isEmpty()) {
    QDesktopServices::openUrl(QUrl(m_learnMoreUrl));
  }

  emit clicked();
}

// ============================================================================
// NMQuickTip Implementation
// ============================================================================

NMQuickTip::NMQuickTip(QWidget *parent) : QWidget(parent) {
  const auto &palette = NMStyleManager::instance().palette();
  const auto &spacing = NMStyleManager::instance().spacing();

  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(spacing.md, spacing.sm, spacing.md, spacing.sm);

  m_label = new QLabel(this);
  m_label->setStyleSheet(
      QString("QLabel { color: %1; font-size: 11px; }")
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary)));
  layout->addWidget(m_label);

  setStyleSheet(QString("NMQuickTip { background: %1; border: 1px solid %2; "
                        "border-radius: 4px; }")
                    .arg(NMStyleManager::colorToStyleString(palette.bgElevated))
                    .arg(NMStyleManager::colorToStyleString(palette.borderLight)));
}

void NMQuickTip::show(QWidget *anchor, const QString &message,
                      int durationMs) {
  if (!anchor) {
    return;
  }

  QPoint anchorPos = anchor->mapToGlobal(QPoint(anchor->width() / 2, 0));
  showAt(anchorPos, message, durationMs);
}

void NMQuickTip::showAt(const QPoint &pos, const QString &message,
                        int durationMs) {
  auto *tip = new NMQuickTip();
  tip->showTip(message, durationMs);

  // Position above the point
  tip->adjustSize();
  QPoint tipPos = pos - QPoint(tip->width() / 2, tip->height() + 10);

  // Keep on screen
  if (QScreen *screen = QApplication::screenAt(pos)) {
    QRect screenRect = screen->availableGeometry();
    tipPos.setX(qBound(screenRect.left(), tipPos.x(),
                       screenRect.right() - tip->width()));
    tipPos.setY(qBound(screenRect.top(), tipPos.y(),
                       screenRect.bottom() - tip->height()));
  }

  tip->move(tipPos);
}

void NMQuickTip::showTip(const QString &message, int durationMs) {
  m_label->setText(message);
  adjustSize();

  // Fade in
  auto *effect = new QGraphicsOpacityEffect(this);
  setGraphicsEffect(effect);

  auto *fadeIn = new QPropertyAnimation(effect, "opacity", this);
  fadeIn->setDuration(150);
  fadeIn->setStartValue(0.0);
  fadeIn->setEndValue(1.0);

  QWidget::show();
  fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

  // Schedule fade out and deletion
  QTimer::singleShot(durationMs, this, [this]() {
    auto *fadeOut =
        new QPropertyAnimation(graphicsEffect(), "opacity", this);
    fadeOut->setDuration(300);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    connect(fadeOut, &QPropertyAnimation::finished, this, &QObject::deleteLater);
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
  });
}

} // namespace NovelMind::editor::qt
