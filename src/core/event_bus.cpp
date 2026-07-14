#include "core/event_bus.h"

namespace amt {

EventBus::EventBus(QObject *parent)
    : QObject(parent)
{
}

bool EventBus::publish(const WorkflowEvent &event)
{
    if (event.eventId.isEmpty() || m_processedEvents.contains(event.eventId)) {
        return false;
    }

    rememberEventId(event.eventId);
    emit eventPublished(event);
    return true;
}

void EventBus::rememberEventId(const QString &eventId)
{
    m_processedEvents.insert(eventId);
    m_processedEventOrder.enqueue(eventId);
    while (m_processedEventOrder.size() > kMaxProcessedEvents) {
        const QString oldest = m_processedEventOrder.dequeue();
        m_processedEvents.remove(oldest);
    }
}

}
