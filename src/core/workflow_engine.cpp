#include "core/workflow_engine.h"

#include <algorithm>

namespace amt {

namespace {

WorkflowCommand makeCommand(RunnerCommandType type, const SessionState &state, Phase phase, const QString &seatId = {})
{
    WorkflowCommand command;
    command.commandType = type;
    command.sessionId = state.tableId;
    command.targetPhase = phase;
    command.targetSeatId = seatId;
    return command;
}

Phase nextPhase(Phase phase)
{
    switch (phase) {
    case Phase::Idle: return Phase::Research;
    case Phase::Research: return Phase::Planning;
    case Phase::Planning: return Phase::Execution;
    case Phase::Execution: return Phase::QualityControl;
    case Phase::QualityControl: return Phase::Present;
    case Phase::Paused: return Phase::Paused;
    case Phase::Present: return Phase::Completed;
    case Phase::Completed:
    case Phase::Stopped:
    case Phase::Failed:
        return phase;
    }
    return Phase::Failed;
}

bool hasUnresolvedDisagreement(const SessionState &state)
{
    if (state.phase == Phase::QualityControl) {
        for (auto it = state.transcript.crbegin(); it != state.transcript.crend(); ++it) {
            if (it->phase != state.phase || it->round != state.round || it->isUser) {
                continue;
            }
            const auto seat = std::find_if(state.seats.cbegin(), state.seats.cend(),
                                           [&](const SeatConfig &candidate) {
                                               return candidate.seatId == it->speakerSeatId;
                                           });
            if (seat == state.seats.cend() || seat->role != Role::LeadQualityControl) {
                continue;
            }
            QString review = it->content.toLower();
            review.remove('*');
            review.remove('_');
            review = review.simplified();
            if (review.contains("blocking correctness issues: none")
                || review.contains("no new blocking issue")) {
                return false;
            }
            if (review.contains("blocking correctness issues:")) {
                return true;
            }
            break;
        }
    }

    QStringList discussionPoints;
    for (const auto &entry : state.transcript) {
        if (entry.phase != state.phase || entry.round != state.round || entry.isUser || entry.isDecision) {
            continue;
        }
        if (entry.speakerSeatId == state.finalDecisionMakerSeatId) {
            continue;
        }
        const QString content = entry.content.trimmed();
        if (!content.isEmpty()) {
            discussionPoints.append(content.toLower());
        }
    }

    if (discussionPoints.size() < 2) {
        return false;
    }

    static const QStringList disagreementMarkers = {
        "disagree", "however", "but ", "conflict", "concern", "risk", "blocker",
        "not convinced", "alternative", "instead", "counterpoint", "problem",
        "issue", "reconsider", "objection"
    };

    for (const auto &point : discussionPoints) {
        for (const auto &marker : disagreementMarkers) {
            if (point.contains(marker)) {
                return true;
            }
        }
    }
    return false;
}

}

WorkflowEngine::WorkflowEngine(QObject *parent)
    : QObject(parent)
{
}

QList<WorkflowCommand> WorkflowEngine::handleEvent(SessionState &state, const WorkflowEvent &event)
{
    QList<WorkflowCommand> commands;

    switch (event.eventType) {
    case EventType::SessionStarted:
        state.round = 1;
        state.queuedInputIds.clear();
        state.arbitrationSatisfied = false;
        commands = startPhase(state, Phase::Research);
        break;
    case EventType::PhaseStarted:
        if (state.phase == Phase::Present) {
            state.arbitrationSatisfied = false;
            commands.append(makeCommand(RunnerCommandType::RequestDecision, state, state.phase, state.finalDecisionMakerSeatId));
        } else if (state.phase == Phase::Research) {
            commands.append(makeCommand(RunnerCommandType::RunResearchBatch, state, state.phase));
        } else {
            const auto seats = activeSeatIdsForPhase(state);
            if (!seats.isEmpty()) {
                state.activeSeatId = seats.first();
                commands.append(makeCommand(RunnerCommandType::RequestSeatTurn, state, state.phase, state.activeSeatId));
            } else {
                state.phase = Phase::Failed;
                WorkflowCommand stop = makeCommand(RunnerCommandType::StopSession, state, state.phase);
                stop.payload.insert("reason", QString("No eligible seats are available for the %1 phase.").arg(toString(state.phase)));
                commands.append(stop);
            }
        }
        break;
    case EventType::TurnCompleted:
    case EventType::TurnSkipped: {
        const QString payloadSeatId = event.payload.value("seatId").toString();
        if (state.phase == Phase::Research) {
            if (payloadSeatId == "research-batch" && state.pendingResearchResponses == 0) {
                commands = startPhase(state, Phase::Planning);
            }
            break;
        }
        if (state.phase == Phase::Present) {
            break;
        }
        const auto seats = activeSeatIdsForPhase(state);
        const int index = seats.indexOf(state.activeSeatId);
        if (index >= 0 && index + 1 < seats.size()) {
            state.activeSeatId = seats.at(index + 1);
            commands.append(makeCommand(RunnerCommandType::RequestSeatTurn, state, state.phase, state.activeSeatId));
        } else {
            if ((state.phase == Phase::Planning || state.phase == Phase::QualityControl)
                && !state.finalDecisionMakerSeatId.isEmpty()
                && hasUnresolvedDisagreement(state)) {
                state.activeSeatId = state.finalDecisionMakerSeatId;
                WorkflowCommand decision = makeCommand(RunnerCommandType::RequestDecision, state, state.phase, state.finalDecisionMakerSeatId);
                decision.payload.insert("mode", "arbitration");
                decision.payload.insert("originPhase", toString(state.phase));
                commands.append(decision);
                break;
            }
            const auto followingPhase = nextPhase(state.phase);
            if (followingPhase == Phase::Completed) {
                state.phase = Phase::Completed;
            } else {
                commands = startPhase(state, followingPhase);
            }
        }
        break;
    }
    case EventType::DecisionIssued: {
        const QString outcome = event.payload.value("outcome").toString();
        const QString mode = event.payload.value("mode").toString();
        if (mode == "arbitration") {
            if (event.payload.value("failed").toBool()) {
                state.arbitrationSatisfied = false;
                const auto followingPhase = nextPhase(state.phase);
                if (followingPhase == Phase::Completed) {
                    state.phase = Phase::Completed;
                    WorkflowCommand stop = makeCommand(RunnerCommandType::StopSession, state, state.phase);
                    stop.payload.insert("reason", "The session ended after arbitration could not complete.");
                    commands.append(stop);
                } else {
                    commands = startPhase(state, followingPhase);
                }
            } else if (outcome == "Revise") {
                state.arbitrationSatisfied = false;
                if (state.phase == Phase::QualityControl) {
                    state.execQcLoopCount += 1;
                    state.queuedInputIds.clear();
                    commands = startPhase(state, Phase::Execution);
                } else {
                    state.round += 1;
                    state.queuedInputIds.clear();
                    commands = startPhase(state, state.phase, false);
                }
            } else if (outcome == "Proceed" || outcome == "Approve") {
                state.arbitrationSatisfied = false;
                const auto followingPhase = nextPhase(state.phase);
                if (followingPhase == Phase::Completed) {
                    state.phase = Phase::Completed;
                    WorkflowCommand stop = makeCommand(RunnerCommandType::StopSession, state, state.phase);
                    stop.payload.insert("reason", "The session completed after arbitration.");
                    commands.append(stop);
                } else {
                    commands = startPhase(state, followingPhase);
                }
            } else {
                state.arbitrationSatisfied = false;
                state.phase = Phase::Stopped;
                WorkflowCommand stop = makeCommand(RunnerCommandType::StopSession, state, state.phase);
                stop.payload.insert("reason", "The final decision maker stopped the session early.");
                commands.append(stop);
            }
        } else if (outcome == "Revise") {
            state.arbitrationSatisfied = false;
            state.round += 1;
            state.execQcLoopCount += 1; // Issue #6: track Exec/QC loop iterations
            state.queuedInputIds.clear();
            commands = startPhase(state, Phase::Execution);
        } else if (outcome == "Approve") {
            state.arbitrationSatisfied = false;
            state.phase = Phase::Completed;
            WorkflowCommand stop = makeCommand(RunnerCommandType::StopSession, state, state.phase);
            stop.payload.insert("reason", "The final decision maker approved the session.");
            commands.append(stop);
        } else {
            state.arbitrationSatisfied = false;
            state.phase = Phase::Stopped;
            WorkflowCommand stop = makeCommand(RunnerCommandType::StopSession, state, state.phase);
            stop.payload.insert("reason", "The final decision maker stopped the session.");
            commands.append(stop);
        }
        break;
    }
    case EventType::PhaseEnded: {
        const auto followingPhase = nextPhase(state.phase);
        if (followingPhase == Phase::Completed) {
            state.phase = Phase::Completed;
            WorkflowCommand stop = makeCommand(RunnerCommandType::StopSession, state, state.phase);
            stop.payload.insert("reason", event.payload.value("reason").toString());
            commands.append(stop);
        } else {
            commands = startPhase(state, followingPhase);
        }
        break;
    }
    case EventType::BudgetExceeded:
    case EventType::SessionStopped:
        state.phase = Phase::Stopped;
        commands.append(makeCommand(RunnerCommandType::StopSession, state, state.phase));
        break;
    case EventType::TurnStarted:
    case EventType::ProviderCallFailed:
    case EventType::RetryScheduled:
    case EventType::InputQueued:
    case EventType::RoundEnded:
        break;
    }

    return commands;
}

QList<WorkflowCommand> WorkflowEngine::startPhase(SessionState &state, Phase phase, bool resetRound)
{
    state.phase = phase;
    state.activeSeatId.clear();
    if (resetRound) {
        state.round = 1;
    }
    // Issue #4: phase counter resets are handled solely by SessionRunner::executeCommand(StartPhase)
    return {makeCommand(RunnerCommandType::StartPhase, state, phase)};
}

QStringList WorkflowEngine::activeSeatIdsForPhase(const SessionState &state) const
{
    return amt::activeSeatIdsForPhase(state);
}

} // namespace amt
