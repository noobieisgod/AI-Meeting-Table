#pragma once

#include <QObject>
#include <QQueue>
#include <QSet>

#include "domain/models.h"

namespace amt {

class EventBus final : public QObject
{
    Q_OBJECT

public:
    explicit EventBus(QObject *parent = nullptr);

    bool publish(const WorkflowEvent &event);

signals:
    void eventPublished(const amt::WorkflowEvent &event);

private:
    static constexpr int kMaxProcessedEvents = 4096;
    void rememberEventId(const QString &eventId);

    QSet<QString> m_processedEvents;
    QQueue<QString> m_processedEventOrder;
};

} // namespace amt
