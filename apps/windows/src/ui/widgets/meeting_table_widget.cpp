#include "ui/widgets/meeting_table_widget.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace amt {

namespace {

qreal boundedSize(qreal preferred, qreal minimum, qreal maximum, qreal available)
{
    const qreal upper = qMax(1.0, qMin(maximum, available));
    const qreal lower = qMin(minimum, upper);
    return qBound(lower, preferred, upper);
}

bool isDarkSurface(const QPalette &palette)
{
    return palette.window().color().lightnessF() < 0.45;
}

}

MeetingTableWidget::MeetingTableWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(440);
    setMouseTracking(true);
    setObjectName("meetingTableWidget");
}

void MeetingTableWidget::setSessionState(const SessionState &state)
{
    m_state = state;
    m_lastSkippedSeatIds.clear();
    for (const auto &entry : state.transcript) {
        if (entry.content.trimmed().startsWith("SKIP", Qt::CaseInsensitive)) {
            m_lastSkippedSeatIds.insert(entry.speakerSeatId);
        } else {
            m_lastSkippedSeatIds.remove(entry.speakerSeatId);
        }
    }
    update();
}

QVector<SeatConfig> MeetingTableWidget::displayedSeats() const
{
    return seatSource();
}

void MeetingTableWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    const bool dark = isDarkSurface(pal);
    const QColor canvasColor = dark ? QColor("#212731") : QColor("#f6f0e5");
    const QColor tableColor = dark ? QColor("#4f5866") : QColor("#d8c0a0");
    const QColor tableBorder = dark ? QColor("#8d98a9") : QColor("#8f6f4b");
    const QColor titleText = dark ? QColor("#f4f7fb") : QColor("#2f2924");
    const QColor modelText = dark ? QColor("#dce4ef") : QColor("#3a332e");
    const QColor roleText = dark ? QColor("#b4bfcd") : QColor("#5d554f");
    const QColor emptySeatColor = dark ? QColor("#2d3641") : QColor("#e9dfcf");
    const QColor occupiedSeatColor = dark ? QColor("#304b5c") : QColor("#d9e9f5");
    const QColor decisionSeatColor = dark ? QColor("#7b6840") : QColor("#f4db8f");
    const QColor activeSeatColor = dark ? QColor("#2f7ca1") : QColor("#7fd2ff");
    const QColor pendingSeatColor = dark ? QColor("#49623d") : QColor("#cfe8bf");
    const QColor skippedSeatColor = dark ? QColor("#404650") : QColor("#e0e4eb");
    const QColor decisionPendingColor = dark ? QColor("#997b3d") : QColor("#ffe47f");
    const QColor seatBorderColor = dark ? QColor("#748194") : QColor("#4a4037");
    painter.fillRect(rect(), canvasColor);

    const LayoutMetrics metrics = computeLayoutMetrics();
    const QRectF &tableRect = metrics.tableRect;
    painter.setBrush(tableColor);
    painter.setPen(QPen(tableBorder, 1.2));
    painter.drawEllipse(tableRect);

    const auto seats = seatSource();
    const auto seatRects = computeSeatRects(seats, metrics);
    const QFont baseFont = painter.font();
    QFont titleFont = baseFont;
    titleFont.setPointSizeF(qMax(9.5, metrics.seatSize.height() / 11.0));
    titleFont.setBold(true);
    QFont subtitleFont = baseFont;
    subtitleFont.setPointSizeF(qMax(8.5, metrics.seatSize.height() / 12.5));
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics subtitleMetrics(subtitleFont);

    for (int i = 0; i < seats.size(); ++i) {
        const auto &seat = seats.at(i);
        const QRectF seatRect = seatRects.value(seat.seatId);

        QColor fill = emptySeatColor;
        QString title = displaySeatName(seat, i);
        QString modelLine = "None";
        QString subtitle = "Default";

        if (seat.occupied) {
            fill = occupiedSeatColor;
            title = displaySeatName(seat, i);
            modelLine = effectiveModelName(seat);
            subtitle = displaySeatRole(seat.role);

            if (m_lastSkippedSeatIds.contains(seat.seatId)) {
                fill = skippedSeatColor;
            }
            if (seat.role == Role::FinalDecisionMaker) {
                fill = decisionSeatColor;
            }
            if (m_state.activeSeatId == seat.seatId) {
                if (seat.role == Role::FinalDecisionMaker && m_state.phase == Phase::Present) {
                    fill = decisionPendingColor;
                } else {
                    fill = activeSeatColor;
                }
                if (!m_state.waitingForNextTurn && !m_state.paused && isRunningPhase(m_state.phase)) {
                    subtitle = "Thinking...";
                }
            }
        }

        const bool pending = hasPendingSeatChanges(m_state) && i < m_state.pendingSeats.size()
            && (effectiveModelName(m_state.pendingSeats.at(i)) != effectiveModelName(seat)
                || m_state.pendingSeats.at(i).role != seat.role
                || m_state.pendingSeats.at(i).occupied != seat.occupied);
        if (pending) {
            fill = pendingSeatColor;
        }

        painter.save();
        if (m_state.phase == Phase::Stopped || m_state.phase == Phase::Completed || m_state.phase == Phase::Failed) {
            painter.setOpacity(0.4);
        }

        painter.setBrush(fill);
        painter.setPen(QPen(seatBorderColor, 1.15));
        painter.drawRoundedRect(seatRect, 14, 14);
        const QRectF textBounds = seatRect.adjusted(12, 10, -12, -10);
        const int lineSpacing = 2;
        const int titleHeight = titleMetrics.height();
        const int modelHeight = subtitleMetrics.height();
        const int subtitleHeight = subtitleMetrics.height();
        const qreal blockHeight = titleHeight + lineSpacing + modelHeight + lineSpacing + subtitleHeight;
        const qreal topOffset = textBounds.top() + qMax(0.0, (textBounds.height() - blockHeight) / 2.0);
        const QRect titleRect(qRound(textBounds.left()),
                              qRound(topOffset),
                              qRound(textBounds.width()),
                              titleHeight);
        const QRect modelRect(qRound(textBounds.left()),
                                 qRound(topOffset + titleHeight + lineSpacing),
                                 qRound(textBounds.width()),
                                 modelHeight);
        const QRect subtitleRect(qRound(textBounds.left()),
                                 qRound(topOffset + titleHeight + lineSpacing + modelHeight + lineSpacing),
                                 qRound(textBounds.width()),
                                 subtitleHeight);
        painter.setFont(titleFont);
        painter.setPen(titleText);
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                         titleMetrics.elidedText(title, Qt::ElideRight, titleRect.width()));
        painter.setFont(subtitleFont);
        painter.setPen(modelText);
        painter.drawText(modelRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                         subtitleMetrics.elidedText(modelLine, Qt::ElideRight, modelRect.width()));
        painter.setPen(roleText);
        painter.drawText(subtitleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                         subtitleMetrics.elidedText(subtitle, Qt::ElideRight, subtitleRect.width()));
        painter.restore();
    }
    painter.setFont(baseFont);

    if (hasPendingSeatChanges(m_state)) {
        painter.setPen(dark ? QColor("#d7e0ea") : QColor("#44382f"));
        painter.drawText(metrics.pendingRect, Qt::AlignRight | Qt::AlignBottom | Qt::TextWordWrap,
                         "Pending seat changes will apply after the current phase.");
    }
}

bool MeetingTableWidget::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *helpEvent = static_cast<QHelpEvent *>(event);
        const auto seats = seatSource();
        const auto metrics = computeLayoutMetrics();
        const auto seatRects = computeSeatRects(seats, metrics);

        for (int i = 0; i < seats.size(); ++i) {
            const auto &seat = seats.at(i);
            if (!seat.occupied) continue;

            if (seatRects.value(seat.seatId).contains(helpEvent->pos())) {
                int seatTokens = 0;
                for (const auto &usage : m_state.seatUsage) {
                    if (usage.seatId == seat.seatId) {
                        seatTokens = usage.totalTokens;
                        break;
                    }
                }
                const QString tooltip = QString("<b>%1</b><br>Tokens: %2")
                                            .arg(seat.displayName)
                                            .arg(seatTokens);
                QToolTip::showText(helpEvent->globalPos(), tooltip, this);
                return true;
            }
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    return QWidget::event(event);
}

void MeetingTableWidget::mousePressEvent(QMouseEvent *event)
{
    const auto seats = seatSource();
    const auto seatRects = computeSeatRects(seats, computeLayoutMetrics());

    for (int i = 0; i < seats.size(); ++i) {
        const auto &seat = seats.at(i);
        if (seatRects.value(seat.seatId).contains(event->position())) {
            emit seatClicked(seat.seatId, i);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

QVector<SeatConfig> MeetingTableWidget::seatSource() const
{
    QVector<SeatConfig> seats = hasPendingSeatChanges(m_state) ? m_state.pendingSeats : m_state.seats;
    while (seats.size() < 8) {
        SeatConfig seat;
        seat.seatId = QString("__placeholder-seat-%1").arg(seats.size() + 1);
        seats.append(seat);
    }
    return seats;
}

MeetingTableWidget::LayoutMetrics MeetingTableWidget::computeLayoutMetrics() const
{
    LayoutMetrics metrics;
    const qreal outerMargin = qMin(18.0, qMin(width(), height()) * 0.04);
    const qreal safeWidth = qMax(1.0, width() - (outerMargin * 2.0));
    const qreal safeHeight = qMax(1.0, height() - (outerMargin * 2.0));
    metrics.safeRect = QRectF(outerMargin, outerMargin, safeWidth, safeHeight);

    const qreal captionHeight = hasPendingSeatChanges(m_state)
        ? boundedSize(metrics.safeRect.height() * 0.18, 58.0, 78.0, metrics.safeRect.height() * 0.28)
        : boundedSize(metrics.safeRect.height() * 0.11, 42.0, 56.0, metrics.safeRect.height() * 0.22);
    const qreal bottomSpacing = qMin(10.0, metrics.safeRect.height() * 0.03);
    const qreal contentHeight = qMax(1.0, metrics.safeRect.height() - captionHeight - bottomSpacing);
    metrics.contentRect = QRectF(metrics.safeRect.left(),
                                 metrics.safeRect.top(),
                                 metrics.safeRect.width(),
                                 contentHeight);

    const qreal statusWidth = 0.0;
    metrics.statusRect = QRectF(metrics.safeRect.left(),
                                metrics.safeRect.bottom() - captionHeight,
                                statusWidth,
                                captionHeight);
    const qreal pendingLeft = qMin(metrics.safeRect.right(), metrics.statusRect.right() + 12.0);
    metrics.pendingRect = QRectF(pendingLeft,
                                 metrics.safeRect.bottom() - captionHeight,
                                 qMax(1.0, metrics.safeRect.right() - pendingLeft),
                                 captionHeight);

    const qreal seatWidth = boundedSize(metrics.contentRect.width() * 0.17, 88.0, 156.0, metrics.contentRect.width() * 0.24);
    const qreal seatHeight = boundedSize(metrics.contentRect.height() * 0.15, 58.0, 96.0, metrics.contentRect.height() * 0.20);
    metrics.seatSize = QSizeF(seatWidth, seatHeight);

    const qreal maxTableWidth = qMax(1.0, metrics.contentRect.width() - (seatWidth * 2.65) - 36.0);
    const qreal maxTableHeight = qMax(1.0, metrics.contentRect.height() - (seatHeight * 2.35) - 24.0);
    const qreal tableWidth = boundedSize(metrics.contentRect.width() * 0.31, 112.0, maxTableWidth, maxTableWidth);
    const qreal tableHeight = boundedSize(metrics.contentRect.height() * 0.72, 170.0, maxTableHeight, maxTableHeight);
    metrics.tableRect = QRectF(metrics.contentRect.center().x() - tableWidth / 2.0,
                               metrics.contentRect.center().y() - tableHeight / 2.0,
                               tableWidth,
                               tableHeight);
    return metrics;
}

QHash<QString, QRectF> MeetingTableWidget::computeSeatRects(const QVector<SeatConfig> &seats, const LayoutMetrics &metrics) const
{
    QHash<QString, QRectF> rects;
    const qreal seatWidth = metrics.seatSize.width();
    const qreal seatHeight = metrics.seatSize.height();
    const qreal ringX = metrics.tableRect.width() / 2.0 + seatWidth / 2.0 + qMin(20.0, metrics.contentRect.width() * 0.04);
    const qreal ringY = metrics.tableRect.height() / 2.0 + seatHeight / 2.0 + qMin(16.0, metrics.contentRect.height() * 0.04);
    const QPointF center = metrics.tableRect.center();
    const QVector<QPointF> anchors = {
        {center.x(), center.y() - ringY},
        {center.x() + ringX * 0.82, center.y() - ringY * 0.74},
        {center.x() + ringX, center.y()},
        {center.x() + ringX * 0.82, center.y() + ringY * 0.74},
        {center.x(), center.y() + ringY},
        {center.x() - ringX * 0.82, center.y() + ringY * 0.74},
        {center.x() - ringX, center.y()},
        {center.x() - ringX * 0.82, center.y() - ringY * 0.74}
    };

    for (int i = 0; i < seats.size() && i < anchors.size(); ++i) {
        QRectF seatRect(anchors.at(i).x() - seatWidth / 2.0,
                        anchors.at(i).y() - seatHeight / 2.0,
                        seatWidth,
                        seatHeight);
        const qreal maxLeft = qMax(metrics.safeRect.left(), metrics.safeRect.right() - seatWidth);
        const qreal maxTop = qMax(metrics.safeRect.top(), metrics.contentRect.bottom() - seatHeight);
        const qreal left = qBound(metrics.safeRect.left(), seatRect.left(), maxLeft);
        const qreal top = qBound(metrics.safeRect.top(), seatRect.top(), maxTop);
        seatRect.moveTopLeft(QPointF(left, top));
        rects.insert(seats.at(i).seatId, seatRect);
    }

    return rects;
}

QString MeetingTableWidget::activeSeatLabel() const
{
    if (m_state.activeSeatId.isEmpty()) {
        return "None";
    }

    for (const auto &seat : m_state.seats) {
        if (seat.seatId == m_state.activeSeatId) {
            if (!seat.displayName.trimmed().isEmpty()) {
                return seat.displayName.trimmed();
            }
            return displaySeatName(seat);
        }
    }

    return m_state.activeSeatId;
}

}
