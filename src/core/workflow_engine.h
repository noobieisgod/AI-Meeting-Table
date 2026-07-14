#pragma once

#include <QList>
#include <QObject>

#include "domain/models.h"

namespace amt {

class WorkflowEngine final : public QObject
{
    Q_OBJECT

public:
    explicit WorkflowEngine(QObject *parent = nullptr);

    QList<WorkflowCommand> handleEvent(SessionState &state, const WorkflowEvent &event);

private:
    QList<WorkflowCommand> startPhase(SessionState &state, Phase phase, bool resetRound = true);
    QStringList activeSeatIdsForPhase(const SessionState &state) const;
};

} // namespace amt
