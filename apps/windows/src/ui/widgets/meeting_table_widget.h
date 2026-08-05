#pragma once

#include <QHash>
#include <QRectF>
#include <QWidget>

#include "domain/models.h"

namespace amt {

class MeetingTableWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MeetingTableWidget(QWidget *parent = nullptr);

    void setSessionState(const SessionState &state);
    QVector<SeatConfig> displayedSeats() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;

private:
    struct LayoutMetrics {
        QRectF safeRect;
        QRectF contentRect;
        QRectF tableRect;
        QSizeF seatSize;
        QRectF statusRect;
        QRectF pendingRect;
    };

    QVector<SeatConfig> seatSource() const;
    LayoutMetrics computeLayoutMetrics() const;
    QHash<QString, QRectF> computeSeatRects(const QVector<SeatConfig> &seats, const LayoutMetrics &metrics) const;
    QString activeSeatLabel() const;

    SessionState m_state;
    QSet<QString> m_lastSkippedSeatIds;

signals:
    void seatClicked(const QString &seatId, int seatIndex);
};

}
