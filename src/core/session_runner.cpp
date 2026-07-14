#include "core/session_runner.h"

#include <QtGlobal>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QUuid>

#include "core/response_parser.h"

namespace amt {

namespace {

QString arbitrationInstruction(Phase phase)
{
    QString proceedTransition;
    QString reviseTransition;
    switch (phase) {
    case Phase::Planning:
        proceedTransition = "PROCEED transitions exactly one phase from Planning to Execution and does not complete the meeting.";
        reviseTransition = "REVISE repeats Planning so the plan can be corrected.";
        break;
    case Phase::QualityControl:
        proceedTransition = "PROCEED transitions exactly one phase from Quality Control to Present and does not complete the meeting.";
        reviseTransition = "REVISE transitions to Execution so the official artifact can be corrected before Quality Control runs again.";
        break;
    default:
        proceedTransition = "PROCEED advances exactly one workflow phase and does not complete the meeting.";
        reviseTransition = "REVISE requires the current phase's issues to be corrected.";
        break;
    }

    return QStringLiteral("FDM phase arbitration for %1. Resolve only this phase's dispute and do not approve or reject the final artifact before Present. If no artifact exists, judge only the current phase discussion. %2 %3 STOP ends the meeting only when continuing is not useful. Explain briefly without using any ruling token in the explanation. Use the FINAL_RULING field exactly once, as the final non-empty line, with one value from PROCEED, REVISE, or STOP.")
        .arg(toString(phase), proceedTransition, reviseTransition);
}

QString finalDecisionInstruction()
{
    return QStringLiteral("Final Present-phase FDM decision: review the transcript, artifact, and unresolved disagreements. APPROVE delivers the current artifact and completes the meeting. REVISE transitions to Execution so specific required changes can be made. STOP ends the meeting when continuing is not useful. Explain briefly without using any ruling token in the explanation. Use the FINAL_RULING field exactly once, as the final non-empty line, with one value from APPROVE, REVISE, or STOP.");
}

QString arbitrationTranscriptLabel(Phase phase, const QString &outcome)
{
    if (phase == Phase::Planning) {
        if (outcome == "Proceed" || outcome == "Approve") {
            return "Planning arbitration: Proceed to Execution";
        }
        if (outcome == "Revise") {
            return "Planning arbitration: Revise Planning";
        }
        return "Planning arbitration: Stop meeting";
    }
    if (phase == Phase::QualityControl) {
        if (outcome == "Proceed" || outcome == "Approve") {
            return "Quality Control arbitration: Proceed to Present";
        }
        if (outcome == "Revise") {
            return "Quality Control arbitration: Revise via Execution";
        }
        return "Quality Control arbitration: Stop meeting";
    }
    return QString("%1 arbitration: %2").arg(toString(phase), outcome);
}

QString finalDecisionTranscriptLabel(const QString &outcome)
{
    if (outcome == "Approve" || outcome == "Proceed") {
        return "Final decision: Approved and complete";
    }
    if (outcome == "Revise") {
        return "Final decision: Revise via Execution";
    }
    return "Final decision: Stop meeting";
}

QString seatTurnInstruction(Phase phase, Role role)
{
    if (role == Role::FinalDecisionMaker && phase != Phase::Present) {
        return arbitrationInstruction(phase);
    }

    switch (phase) {
    case Phase::Research:
        return "Research turn: work independently. Gather evidence, note uncertainties, and do not create the final result, plan, QC ruling, or final decision. If you have nothing useful to add, start with SKIP on its own line.";
    case Phase::Planning:
        return "Planning turn: compare research and improve the plan. Lead Planner owns the plan. Other seats may recommend, challenge, or add evidence, but must not override without a concrete, evidence-backed flaw.";
    case Phase::Execution:
        if (role == Role::LeadExecutioner) {
            return "Execution turn: create or revise the official artifact. Follow the plan, resolve contradictions, and provide the artifact content directly.";
        }
        return "Execution turn: support the Lead Executioner with constraints, examples, snippets, risks, or corrections. Do not present your own answer as the official artifact.";
    case Phase::QualityControl:
        if (role == Role::LeadQualityControl) {
            return "Quality Control turn: review the artifact against the user request, identify defects, and rule whether revision is required. Do not rewrite the artifact unless explicitly instructed.";
        }
        return "Quality Control turn: identify possible issues or evidence for the reviewer. Do not issue the final QC ruling and do not rewrite the artifact unless explicitly instructed.";
    case Phase::Present:
        return finalDecisionInstruction();
    default:
        return "Contribute to the meeting within your current role. If you truly have nothing meaningful to add, start with SKIP on its own line.";
    }
}

}

SessionRunner::SessionRunner(EventBus *eventBus,
                             WorkflowEngine *engine,
                             ProviderGateway *providerGateway,
                             BudgetManager *budgetManager,
                             ArtifactManager *artifactManager,
                             SessionResolver sessionResolver,
                             QObject *parent)
    : QObject(parent),
      m_eventBus(eventBus),
      m_engine(engine),
      m_providerGateway(providerGateway),
      m_budgetManager(budgetManager),
      m_artifactManager(artifactManager),
      m_sessionResolver(std::move(sessionResolver))
{
    connect(m_providerGateway, &ProviderGateway::responseReady, this, &SessionRunner::onProviderResponse);
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, &SessionRunner::handleElapsedTick);
}

void SessionRunner::startSession(SessionState &state)
{
    m_runGenerations.insert(state.tableId, m_runGenerations.value(state.tableId) + 1);
    state.elapsedSeconds = 0;
    state.phaseElapsedSeconds = 0;
    state.phaseUsedTokens = 0;
    state.phaseUsedCost = 0.0;
    state.pendingResearchResponses = 0;
    state.execQcLoopCount = 0;
    state.paused = false;
    state.pauseRequested = false;
    state.pausedResumePhase = Phase::Idle;
    state.continuationPending = false;
    state.continuationLimitKind = 0;
    state.continuationReason.clear();
    state.waitingForNextTurn = false;
    state.arbitrationSatisfied = false;
    removePendingRequests(state.tableId);
    m_continuationAllowances.remove(state.tableId);
    m_researchRequestsBySession.remove(state.tableId);
    m_researchFailuresBySession.remove(state.tableId);
    m_lastPersistedElapsedSeconds.insert(state.tableId, state.elapsedSeconds);
    appendLog(state, LogEventType::SessionStarted, {}, {}, "Session started");
    emitAndHandle(state, makeEvent(state, EventType::SessionStarted));
    updateElapsedTimerState();
}

void SessionRunner::grantContinuation(SessionState &state, BudgetLimitKind kind)
{
    ContinuationAllowance allowance = m_continuationAllowances.value(state.tableId);
    switch (kind) {
    case BudgetLimitKind::MaxTotalCost:
        allowance.maxTotalCost = state.usedCost + qMax(1.0, state.budgetPolicy.maxTotalCost * 0.25);
        break;
    case BudgetLimitKind::MaxTotalTokens:
    case BudgetLimitKind::SafetyReserve:
        allowance.maxTotalTokens = state.usedTokens + (m_budgetManager->tokenReserve(state) * 4);
        allowance.ignoreSafetyReserve = true;
        break;
    case BudgetLimitKind::MaxSessionSeconds:
        allowance.maxSessionSeconds = state.elapsedSeconds + 300;
        break;
    case BudgetLimitKind::MaxPhaseSeconds:
        allowance.maxPhaseSeconds = state.phaseElapsedSeconds + 120;
        break;
    default:
        break;
    }
    m_continuationAllowances.insert(state.tableId, allowance);
    clearContinuationState(state);
}

void SessionRunner::discardSession(const QString &sessionId)
{
    m_runGenerations.remove(sessionId);
    removePendingRequests(sessionId);
    if (m_delayTimers.contains(sessionId) && m_delayTimers.value(sessionId)) {
        auto timer = m_delayTimers.take(sessionId);
        timer->stop();
        timer->deleteLater();
    }
    m_delayedCommands.remove(sessionId);
    m_lastPersistedElapsedSeconds.remove(sessionId);
    m_continuationAllowances.remove(sessionId);
    m_researchRequestsBySession.remove(sessionId);
    m_researchFailuresBySession.remove(sessionId);
    updateElapsedTimerState();
}

void SessionRunner::requestPause(SessionState &state)
{
    state.pauseRequested = true;
    if (m_delayTimers.contains(state.tableId) && m_delayTimers.value(state.tableId)) {
        auto timer = m_delayTimers.take(state.tableId);
        timer->stop();
        timer->deleteLater();
        state.waitingForNextTurn = false;
        state.paused = true;
        state.pauseRequested = false;
        if (state.phase != Phase::Paused) {
            state.pausedResumePhase = state.phase;
            state.phase = Phase::Paused;
        }
        emit sessionStateChanged(state);
        m_lastPersistedElapsedSeconds.insert(state.tableId, state.elapsedSeconds);
        updateElapsedTimerState();
        return;
    }
    if (isRunningPhase(state.phase) || state.waitingForNextTurn || state.phase == Phase::Paused) {
        emit sessionStateChanged(state);
    }
    updateElapsedTimerState();
}

void SessionRunner::resumeSession(SessionState &state)
{
    const bool hasDelayedCommand = m_delayedCommands.contains(state.tableId);
    const WorkflowCommand delayedCommand = hasDelayedCommand ? m_delayedCommands.value(state.tableId) : WorkflowCommand{};
    const bool restorePhaseBeforeResume = !(hasDelayedCommand && delayedCommand.commandType == RunnerCommandType::StartPhase);
    state.paused = false;
    state.pauseRequested = false;
    if (state.phase == Phase::Paused && restorePhaseBeforeResume) {
        state.phase = state.pausedResumePhase == Phase::Idle ? Phase::Research : state.pausedResumePhase;
        state.pausedResumePhase = Phase::Idle;
    }
    if (hasDelayedCommand) {
        if (m_delayTimers.contains(state.tableId) && m_delayTimers.value(state.tableId)) {
            auto timer = m_delayTimers.take(state.tableId);
            timer->stop();
            timer->deleteLater();
        }
        const WorkflowCommand command = m_delayedCommands.take(state.tableId);
        executeCommand(state, command);
        updateElapsedTimerState();
        return;
    }
    if (state.phase == Phase::Paused) {
        state.phase = state.pausedResumePhase == Phase::Idle ? Phase::Research : state.pausedResumePhase;
        state.pausedResumePhase = Phase::Idle;
    }
    if (state.phase == Phase::Idle || state.phase == Phase::Completed || state.phase == Phase::Stopped || state.phase == Phase::Failed) {
        // Terminal or idle state. Do not auto-restart. Just clear the paused flag.
        // The user must explicitly click "Run Session" (not resume) to restart.
        emit sessionStateChanged(state);
    }
    updateElapsedTimerState();
}

void SessionRunner::stopSession(SessionState &state, const QString &reason)
{
    m_continuationAllowances.remove(state.tableId);
    clearContinuationState(state);
    WorkflowCommand command;
    command.commandType = RunnerCommandType::StopSession;
    command.sessionId = state.tableId;
    command.targetPhase = Phase::Stopped;
    if (!reason.isEmpty()) {
        command.payload.insert("reason", reason);
    }
    executeCommand(state, command);
    updateElapsedTimerState();
}

void SessionRunner::handleElapsedTick()
{
    for (auto it = m_lastPersistedElapsedSeconds.begin(); it != m_lastPersistedElapsedSeconds.end();) {
        const auto handle = m_sessionResolver ? m_sessionResolver(it.key()) : nullptr;
        if (!handle) {
            it = m_lastPersistedElapsedSeconds.erase(it); // Issue #2: clean up stale entries
            continue;
        }
        auto &state = *handle;
        if (state.phase == Phase::Stopped || state.phase == Phase::Completed || state.phase == Phase::Failed) {
            it = m_lastPersistedElapsedSeconds.erase(it); // Issue #2: clean up terminated entries
            continue;
        }
        const bool active = (isRunningPhase(state.phase) || state.waitingForNextTurn) && !state.paused;
        if (!active) {
            ++it;
            continue;
        }
        state.elapsedSeconds += 1;
        state.phaseElapsedSeconds += 1;
        const BudgetStatus budgetStatus = budgetStatusFor(state);
        if (state.elapsedSeconds - it.value() >= 5 || !budgetStatus.reason.isEmpty()) {
            it.value() = state.elapsedSeconds;
            emit sessionStateChanged(state);
            if (!budgetStatus.reason.isEmpty()) {
                if (budgetStatus.action == BudgetLimitAction::EndPhase) {
                    appendLog(state, LogEventType::PhaseEnded, {}, {}, budgetStatus.reason);
                    emitAndHandle(state, makeEvent(state, EventType::PhaseEnded, {{"reason", budgetStatus.reason}}));
                } else {
                    markContinuationPending(state, budgetStatus);
                }
            }
        }
        ++it;
    }
    updateElapsedTimerState();
}

void SessionRunner::updateElapsedTimerState()
{
    bool hasActiveSession = false;
    for (auto it = m_lastPersistedElapsedSeconds.cbegin(); it != m_lastPersistedElapsedSeconds.cend(); ++it) {
        const auto handle = m_sessionResolver ? m_sessionResolver(it.key()) : nullptr;
        if (!handle) {
            continue;
        }
        const auto &state = *handle;
        if ((isRunningPhase(state.phase) || state.waitingForNextTurn) && !state.paused) {
            hasActiveSession = true;
            break;
        }
    }

    if (hasActiveSession && !m_elapsedTimer->isActive()) {
        m_elapsedTimer->start();
    } else if (!hasActiveSession && m_elapsedTimer->isActive()) {
        m_elapsedTimer->stop();
    }
}

void SessionRunner::executeCommand(SessionState &state, const WorkflowCommand &command)
{
    switch (command.commandType) {
    case RunnerCommandType::StartPhase: {
        if (state.continuationPending && command.targetPhase != Phase::Paused) {
            enterContinuationPause(state, command);
            break;
        }
        const Phase previousPhase = state.phase;
        const bool enteringNewPhase = previousPhase != command.targetPhase;
        if (state.phase != Phase::Idle && enteringNewPhase) {
            appendLog(state, LogEventType::PhaseEnded, {}, {}, QString("%1 phase ended").arg(toString(state.phase)));
            if (hasPendingSeatChanges(state)) {
                state.seats = state.pendingSeats;
                state.pendingSeats.clear();
                state.finalDecisionMakerSeatId = findFinalDecisionMakerSeatId(state.seats);
            }
        }
        state.phase = command.targetPhase;
        state.activeSeatId.clear();
        if (enteringNewPhase) {
            state.phaseElapsedSeconds = 0;
            state.phaseUsedTokens = 0;
            state.phaseUsedCost = 0.0;
            // Release queued inputs so they are visible to the incoming phase's models.
            state.queuedInputIds.clear();
            for (auto &usage : state.seatUsage) {
                usage.phaseTokens = 0;
                usage.phaseCost = 0.0;
            }
        }
        appendLog(state, LogEventType::PhaseStarted, {}, {}, QString("%1 phase started").arg(toString(state.phase)));
        // Issue #8: Generate planning summary artifact at Planning?xecution transition
        if (enteringNewPhase && command.targetPhase == Phase::Execution) {
            QString summaryContent;
            for (const auto &entry : state.transcript) {
                if (entry.phase == Phase::Planning && !entry.isUser) {
                    summaryContent += entry.speakerName + ":\n" + entry.content + "\n\n";
                }
            }
            if (!summaryContent.isEmpty()) {
                m_artifactManager->createVersion(state, Phase::Planning, state.round,
                                                  "Planning phase summary", summaryContent);
            }
        }
        emitAndHandle(state, makeEvent(state, EventType::PhaseStarted));
        break;
    }
    case RunnerCommandType::RunResearchBatch: {
        if (state.paused) {
            m_delayedCommands.insert(state.tableId, command);
            emit sessionStateChanged(state);
            return;
        }
        const QStringList seats = activeSeatIdsForPhase(state);
        if (seats.isEmpty()) {
            appendLog(state, LogEventType::SessionStopped, {}, {}, QString("No eligible seats are available for the %1 phase.").arg(toString(state.phase)));
            emitAndHandle(state, makeEvent(state, EventType::SessionStopped, {{"reason", QString("No eligible seats are available for the %1 phase.").arg(toString(state.phase))}}));
            return;
        }
        state.pendingResearchResponses = 0;
        state.activeSeatId.clear();
        int dispatchedResearchRequests = 0;
        m_researchRequestsBySession.insert(state.tableId, 0);
        m_researchFailuresBySession.insert(state.tableId, 0);
        for (const auto &seatId : seats) {
            const auto seat = seatById(state, seatId);
            if (seat.seatId.isEmpty()) {
                continue;
            }
            appendLog(state, LogEventType::TurnStarted, seat.seatId, seat.displayName, QString("%1 started independent research").arg(seat.displayName));
            if (dispatchProviderRequest(state,
                                        seat,
                                        {{"role", toString(seat.role)},
                                         {"title", state.title},
                                         {"phase", toString(state.phase)},
                                         {"instruction", seatTurnInstruction(state.phase, seat.role)}},
                                        QString("%1 was blocked before starting research.").arg(seat.displayName))) {
                state.pendingResearchResponses += 1;
                dispatchedResearchRequests += 1;
            }
        }
        m_researchRequestsBySession.insert(state.tableId, dispatchedResearchRequests);
        emit sessionStateChanged(state);
        finalizeResearchBatchIfReady(state);
        break;
    }
    case RunnerCommandType::RequestSeatTurn: {
        if (state.paused) {
            m_delayedCommands.insert(state.tableId, command);
            emit sessionStateChanged(state);
            return;
        }
        const auto seat = seatById(state, command.targetSeatId);
        if (seat.seatId.isEmpty()) {
            return;
        }
        state.activeSeatId = seat.seatId;
        appendLog(state, LogEventType::TurnStarted, seat.seatId, seat.displayName, QString("%1 is taking a turn").arg(seat.displayName));
        const QString instruction = seatTurnInstruction(state.phase, seat.role);
        dispatchProviderRequest(state,
                                seat,
                                {{"role", toString(seat.role)},
                                 {"title", state.title},
                                 {"phase", toString(state.phase)},
                                 {"instruction", instruction}},
                                QString("%1 was blocked before starting a turn").arg(seat.displayName),
                                &command);
        break;
    }
    case RunnerCommandType::RequestDecision: {
        if (state.paused) {
            m_delayedCommands.insert(state.tableId, command);
            emit sessionStateChanged(state);
            return;
        }
        const auto seat = seatById(state, command.targetSeatId);
        if (seat.seatId.isEmpty()) {
            appendLog(state, LogEventType::ProviderCallFailed, {}, {}, "Final decision maker seat is missing.");
            emitAndHandle(state, makeEvent(state, EventType::SessionStopped, {{"reason", "Final decision maker seat is missing."}}));
            return;
        }
        const QString mode = command.payload.value("mode").toString();
        const QString instruction = mode == "arbitration"
            ? arbitrationInstruction(state.phase)
            : finalDecisionInstruction();
        if (mode == "arbitration") {
            appendLog(state, LogEventType::FinalDecisionMade, seat.seatId, seat.displayName,
                      QString("FDM arbitration requested during %1 because disagreement markers were detected.").arg(toString(state.phase)));
        }
        dispatchProviderRequest(state,
                                seat,
                                {
                                    {"role", "Final Decision Maker"},
                                    {"title", state.title},
                                    {"instruction", instruction},
                                    {"decision_mode", mode}
                                },
                                "The final decision maker was blocked before dispatch.",
                                &command);
        break;
    }
    case RunnerCommandType::StopSession:
    case RunnerCommandType::None:
        if (command.commandType == RunnerCommandType::StopSession) {
            m_runGenerations.insert(state.tableId, m_runGenerations.value(state.tableId) + 1);
            state.activeSeatId.clear();
            state.waitingForNextTurn = false;
            state.pendingResearchResponses = 0;
            state.pauseRequested = false;
            state.paused = false;
            state.pausedResumePhase = Phase::Idle;
            clearContinuationState(state);
            m_continuationAllowances.remove(state.tableId);
            removePendingRequests(state.tableId);
            m_delayedCommands.remove(state.tableId);
            if (m_delayTimers.contains(state.tableId) && m_delayTimers.value(state.tableId)) {
                auto timer = m_delayTimers.take(state.tableId);
                timer->stop();
                timer->deleteLater();
            }
            m_lastPersistedElapsedSeconds.remove(state.tableId); // Issue #2: clean up timer entry
            m_researchRequestsBySession.remove(state.tableId);
            m_researchFailuresBySession.remove(state.tableId);
            const QString reason = command.payload.value("reason").toString();
            if (!reason.isEmpty()) {
                appendLog(state, LogEventType::SessionStopped, {}, {}, reason);
            }
        }
        emit sessionStateChanged(state);
        break;
    }
    updateElapsedTimerState();
}

void SessionRunner::onProviderResponse(const ProviderResponse &response)
{
    auto pendingIt = m_pendingRequests.find(response.requestId);
    if (pendingIt == m_pendingRequests.end()) {
        return;
    }
    const PendingRequestContext requestContext = pendingIt.value();
    m_pendingRequests.erase(pendingIt);

    if (response.sessionId != requestContext.sessionId
        || response.seatId != requestContext.seatId
        || response.runGeneration != requestContext.runGeneration
        || response.runGeneration != m_runGenerations.value(requestContext.sessionId)) {
        return;
    }

    const auto handle = m_sessionResolver ? m_sessionResolver(requestContext.sessionId) : nullptr;
    if (!handle) {
        return;
    }

    auto &session = *handle;
    if (session.phase != requestContext.phase || session.round != requestContext.round) {
        return;
    }
    const auto seat = seatById(session, response.seatId);
    const QString requestMode = requestContext.mode;
    if (seat.seatId.isEmpty()) {
        return;
    }
    if (session.phase == Phase::Stopped || session.phase == Phase::Completed || session.phase == Phase::Failed) {
        return;
    }

    if (!response.attachmentProviderHandles.isEmpty()) {
        for (auto &attachment : session.attachments) {
            const QJsonValue handleValue = response.attachmentProviderHandles.value(attachment.attachmentId);
            if (!handleValue.isUndefined()) {
                attachment.providerHandles.insert(toString(seat.provider), handleValue);
            }
        }
    }

    if (!response.success) {
        const QString errorMessage = response.errorMessage.isEmpty() ? "Provider request failed." : response.errorMessage;
        appendLog(session, LogEventType::ProviderCallFailed, seat.seatId, seat.displayName, errorMessage);
        if (session.phase == Phase::Research && session.pendingResearchResponses > 0) {
            m_researchFailuresBySession.insert(session.tableId, m_researchFailuresBySession.value(session.tableId) + 1);
            session.pendingResearchResponses -= 1;
            appendLog(session, LogEventType::AISkipped, seat.seatId, seat.displayName, QString("%1 failed during research and was skipped.").arg(seat.displayName));
            appendLog(session, LogEventType::ProviderCallFailed, {}, {}, QString("Research batch continuing without %1.").arg(seat.displayName));
            finalizeResearchBatchIfReady(session);
            emit sessionStateChanged(session);
        } else if (requestMode == "arbitration") {
            appendLog(session, LogEventType::AISkipped, seat.seatId, seat.displayName, QString("%1 arbitration failed; continuing without an early ruling.").arg(seat.displayName));
            emitAndHandle(session, makeEvent(session, EventType::DecisionIssued, {{"mode", "arbitration"}, {"failed", true}}));
        } else if (session.phase == Phase::Present && seat.role == Role::FinalDecisionMaker) {
            emitAndHandle(session, makeEvent(session, EventType::SessionStopped, {{"reason", errorMessage}}));
        } else {
            session.waitingForNextTurn = true;
            appendLog(session, LogEventType::AISkipped, seat.seatId, seat.displayName, QString("%1 failed and its turn was skipped.").arg(seat.displayName));
            appendLog(session, LogEventType::ProviderCallFailed, {}, {}, QString("Workflow continued after %1 failed to respond.").arg(seat.displayName));
            emitAndHandle(session, makeEvent(session, EventType::TurnSkipped, {{"seatId", seat.seatId}, {"reason", errorMessage}}));
        }
        emitAndHandle(session, makeEvent(session, EventType::ProviderCallFailed, {{"error", errorMessage}}));
        return;
    }

    if (response.multipleDecisionRulings) {
        appendLog(session, LogEventType::FinalDecisionMade, seat.seatId, seat.displayName,
                  "Multiple explicit final decision ruling lines were detected; the final valid line was used.");
    }

    m_budgetManager->applyUsage(session, seat.seatId, response.usedTokens, response.estimatedCost);
    const BudgetStatus budgetStatus = budgetStatusFor(session);

    if (requestMode == "arbitration") {
        QString outcome = response.decisionOutcome.trimmed();
        if (outcome.isEmpty()) {
            outcome = amt::response::parseDecisionOutcome(response.content, true);
        }
        if (outcome == "Approve") {
            outcome = "Proceed";
        }
        if (outcome.isEmpty() || response.skipped || amt::response::isSkipResponse(response.content)) {
            if (outcome.isEmpty() && !response.skipped && !amt::response::isSkipResponse(response.content)) {
                appendLog(session, LogEventType::ProviderCallFailed, seat.seatId, seat.displayName, "Could not parse final decision maker arbitration ruling. Defaulting to Proceed to avoid a revision loop.");
            }
            outcome = "Proceed";
        }
        if (outcome == "Stop" && !session.stopPolicy.allowEarlyStopByDecisionMaker) {
            outcome = "Revise";
        }
        session.arbitrationSatisfied = (outcome == "Proceed");
        appendTranscript(session, seat.seatId, seat.displayName,
                         QString("%1\n\n%2").arg(arbitrationTranscriptLabel(session.phase, outcome), response.content), true);
        appendLog(session, LogEventType::FinalDecisionMade, seat.seatId, seat.displayName, QString("%1 issued early arbitration: %2").arg(seat.displayName, outcome));
        emitAndHandle(session, makeEvent(session, EventType::DecisionIssued, {{"outcome", outcome}, {"mode", "arbitration"}}));
        return;
    }

    if (session.phase != Phase::Present && (response.skipped || amt::response::isSkipResponse(response.content))) {
        const QString reason = amt::response::skipReason(response.content);
        appendTranscript(session, seat.seatId, seat.displayName, QString("Skipped turn.\n%1").arg(reason));
        appendLog(session, LogEventType::AISkipped, seat.seatId, seat.displayName, QString("%1 skipped: %2").arg(seat.displayName, reason));
        if (!budgetStatus.reason.isEmpty()) {
            if (session.phase == Phase::Research && session.pendingResearchResponses > 0) {
                session.pendingResearchResponses -= 1;
            if (budgetStatus.action == BudgetLimitAction::EndPhase) {
                if (session.pendingResearchResponses == 0) {
                    emitAndHandle(session, makeEvent(session, EventType::PhaseEnded, {{"reason", budgetStatus.reason}}));
                } else {
                    emit sessionStateChanged(session);
                }
                return;
            }
            markContinuationPending(session, budgetStatus);
            finalizeResearchBatchIfReady(session);
            emit sessionStateChanged(session);
            return;
        }
        if (budgetStatus.action == BudgetLimitAction::EndPhase) {
            emitAndHandle(session, makeEvent(session, EventType::PhaseEnded, {{"reason", budgetStatus.reason}}));
            return;
        }
        session.waitingForNextTurn = true;
        markContinuationPending(session, budgetStatus);
        emitAndHandle(session, makeEvent(session, EventType::TurnSkipped, {{"seatId", seat.seatId}, {"reason", reason}}));
        return;
    }
        if (session.phase == Phase::Research && session.pendingResearchResponses > 0) {
            session.pendingResearchResponses -= 1;
            finalizeResearchBatchIfReady(session);
            emit sessionStateChanged(session);
            return;
        }
        session.waitingForNextTurn = true;
        emitAndHandle(session, makeEvent(session, EventType::TurnSkipped, {{"seatId", seat.seatId}, {"reason", reason}}));
        return;
    }

    if (session.phase == Phase::Present && seat.role == Role::FinalDecisionMaker) {
        QString outcome = response.decisionOutcome.trimmed();
        if (outcome.isEmpty()) {
            outcome = amt::response::parseDecisionOutcome(response.content, false);
        }
        if (outcome.isEmpty()) {
            appendLog(session, LogEventType::ProviderCallFailed, seat.seatId, seat.displayName, "Could not parse final decision maker ruling. Defaulting to Stop because no explicit FINAL_RULING was found.");
            outcome = "Stop";
        }
        if (outcome == "Stop" && !session.stopPolicy.allowEarlyStopByDecisionMaker) {
            outcome = "Revise";
        }
        appendTranscript(session, seat.seatId, seat.displayName,
                         QString("%1\n\n%2").arg(finalDecisionTranscriptLabel(outcome), response.content), true);
        appendLog(session, LogEventType::FinalDecisionMade, seat.seatId, seat.displayName, QString("%1 issued %2").arg(seat.displayName, outcome));
        emitAndHandle(session, makeEvent(session, EventType::DecisionIssued, {{"outcome", outcome}}));
        return;
    }

    appendTranscript(session, seat.seatId, seat.displayName, response.content);
    appendLog(session, LogEventType::AISpoke, seat.seatId, seat.displayName, QString("%1 spoke").arg(seat.displayName));

    if (session.phase == Phase::Execution && seat.role == Role::LeadExecutioner) {
        const auto artifact = m_artifactManager->createVersion(session,
                                                               session.phase,
                                                               session.round,
                                                               QString("Execution round %1").arg(session.round),
                                                               response.content);
        if (artifact.versionId.isEmpty() && !m_artifactManager->lastError().isEmpty()) {
            appendLog(session, LogEventType::ProviderCallFailed, seat.seatId, seat.displayName, m_artifactManager->lastError());
        }
    }

    if (!budgetStatus.reason.isEmpty()) {
        if (session.phase == Phase::Research && session.pendingResearchResponses > 0) {
            session.pendingResearchResponses -= 1;
            if (budgetStatus.action == BudgetLimitAction::EndPhase) {
                if (session.pendingResearchResponses == 0) {
                    emitAndHandle(session, makeEvent(session, EventType::PhaseEnded, {{"reason", budgetStatus.reason}}));
                } else {
                    emit sessionStateChanged(session);
                }
                return;
            }
            markContinuationPending(session, budgetStatus);
            finalizeResearchBatchIfReady(session);
            emit sessionStateChanged(session);
            return;
        }
        if (budgetStatus.action == BudgetLimitAction::EndPhase) {
            appendLog(session, LogEventType::PhaseEnded, {}, {}, budgetStatus.reason);
            emitAndHandle(session, makeEvent(session, EventType::PhaseEnded, {{"reason", budgetStatus.reason}}));
            return;
        }
        session.waitingForNextTurn = true;
        markContinuationPending(session, budgetStatus);
        emitAndHandle(session, makeEvent(session, EventType::TurnCompleted, {{"seatId", seat.seatId}}));
        return;
    }

    if (session.phase == Phase::Research && session.pendingResearchResponses > 0) {
        session.pendingResearchResponses -= 1;
        finalizeResearchBatchIfReady(session);
        emit sessionStateChanged(session);
        return;
    }

    session.waitingForNextTurn = true;
    emitAndHandle(session, makeEvent(session, EventType::TurnCompleted, {{"seatId", seat.seatId}}));
}

WorkflowEvent SessionRunner::makeEvent(const SessionState &state, EventType type, const QJsonObject &payload) const
{
    WorkflowEvent event;
    event.eventId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event.sessionId = state.tableId;
    event.eventType = type;
    event.createdAt = QDateTime::currentDateTimeUtc();
    event.payload = payload;
    return event;
}

SeatConfig SessionRunner::seatById(const SessionState &state, const QString &seatId) const
{
    for (const auto &seat : state.seats) {
        if (seat.seatId == seatId) {
            return seat;
        }
    }
    return {};
}

bool SessionRunner::dispatchProviderRequest(SessionState &state,
                                            const SeatConfig &seat,
                                            const QJsonObject &prompt,
                                            const QString &blockedSummary,
                                            const WorkflowCommand *resumeCommand)
{
    const int reservedInFlight = reservedTokensInFlight(state.tableId);
    const BudgetStatus budgetStatus = budgetStatusFor(state, reservedInFlight);
    if (!budgetStatus.reason.isEmpty()) {
        if (budgetStatus.action == BudgetLimitAction::EndPhase) {
            appendLog(state, LogEventType::PhaseEnded, seat.seatId, seat.displayName, QString("%1 %2").arg(blockedSummary, budgetStatus.reason));
            emitAndHandle(state, makeEvent(state, EventType::PhaseEnded, {{"reason", budgetStatus.reason}}));
        } else {
            appendLog(state, LogEventType::LimitReached, seat.seatId, seat.displayName, QString("%1 %2").arg(blockedSummary, budgetStatus.reason));
            markContinuationPending(state, budgetStatus);
            if (resumeCommand) {
                m_delayedCommands.insert(state.tableId, *resumeCommand);
            }
            if (state.phase != Phase::Paused) {
                enterContinuationPause(state, WorkflowCommand{RunnerCommandType::StartPhase, state.tableId, state.phase, {}, {}});
            }
        }
        return false;
    }

    ProviderRequest request;
    request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.sessionId = state.tableId;
    request.seatId = seat.seatId;
    request.provider = seat.provider;
    request.model = effectiveModelId(seat);
    request.apiKey = QString();
    request.phase = state.phase;
    request.runGeneration = m_runGenerations.value(state.tableId);
    request.prompt = prompt;
    request.prompt.insert("table_title", state.title);
    request.prompt.insert("latest_user_message", latestUserPrompt(state));
    request.prompt.insert("seat_display_name", seat.displayName);
    request.prompt.insert("model_display_name", effectiveModelName(seat));

    // Build roster of every other occupied, enabled seat so each model knows who it is working with.
    QJsonArray rosterArray;
    for (const auto &other : state.seats) {
        if (!other.occupied || !other.enabled || other.seatId == seat.seatId) {
            continue;
        }
        rosterArray.append(QJsonObject{
            {"name",     displaySeatName(other)},
            {"model",    effectiveModelName(other)},
            {"role",     toString(other.role)},
            {"provider", toString(other.provider)}
        });
    }
    request.prompt.insert("participant_roster", rosterArray);
    QJsonArray transcriptHistory;
    for (const auto &entry : state.transcript) {
        if (entry.isUser && state.queuedInputIds.contains(entry.entryId)) {
            continue;
        }
        transcriptHistory.append(QJsonObject{
            {"speaker", entry.isUser ? "user" : entry.speakerName},
            {"role", entry.isUser ? "user" : "assistant"},
            {"content", entry.content},
            {"is_decision", entry.isDecision},
            {"timestamp", entry.timestamp.toUTC().toString(Qt::ISODate)}
        });
    }
    request.prompt.insert("transcript_history", transcriptHistory);
    if (!state.currentArtifactVersionId.isEmpty()) {
        request.prompt.insert("current_artifact_version_id", state.currentArtifactVersionId);
        for (const auto &artifact : state.artifacts) {
            if (artifact.versionId == state.currentArtifactVersionId) {
                request.prompt.insert("current_artifact_summary", artifact.summary);
                request.prompt.insert("current_artifact_path", artifact.filePath);
                QFile artifactFile(artifact.filePath);
                if (artifactFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const qint64 artifactSize = artifactFile.size();
                    const qint64 maxArtifactRead = 32768;
                    QString artifactText = QString::fromUtf8(artifactFile.read(maxArtifactRead));
                    if (artifactSize > maxArtifactRead) {
                        artifactText += QString("\n\n[Artifact truncated. Full artifact is %1 bytes]").arg(artifactSize);
                    }
                    request.prompt.insert("current_artifact_content", artifactText);
                }
                break;
            }
        }
    }
    QJsonArray attachmentArray;
    for (const auto &attachment : state.attachments) {
        if (state.queuedInputIds.contains(attachment.attachmentId)) {
            continue;
        }
        attachmentArray.append(QJsonObject{
            {"attachmentId", attachment.attachmentId},
            {"displayName", attachment.displayName},
            {"filePath", attachment.filePath},
            {"fileName", QFileInfo(attachment.filePath).fileName()},
            {"fileHash", attachment.fileHash},
            {"providerHandle", attachment.providerHandles.value(toString(seat.provider))},
            {"addedAt", attachment.addedAt.toUTC().toString(Qt::ISODate)}
        });
    }
    request.prompt.insert("attachments", attachmentArray);
    if (seatSupportsEffort(seat)) {
        request.prompt.insert("effort", toString(seat.effort));
        if (seat.provider == ProviderKind::OpenAI) {
            const QString effort = openAiEffortValue(seat.effort);
            if (!effort.isEmpty()) {
                request.prompt.insert("reasoning_effort", effort);
            }
        } else if (seat.provider == ProviderKind::Gemini) {
            const QString thinkingLevel = geminiThinkingLevel(seat.effort);
            if (!thinkingLevel.isEmpty()) {
                request.prompt.insert("thinking_level", thinkingLevel);
            }
            const int thinkingBudget = geminiThinkingBudget(seat.effort);
            if (thinkingBudget > 0) {
                request.prompt.insert("thinking_budget_tokens", thinkingBudget);
            }
        } else if (seat.provider == ProviderKind::Anthropic) {
            const int budgetTokens = anthropicThinkingBudget(seat.effort);
            if (budgetTokens > 0) {
                request.prompt.insert("thinking_enabled", true);
                request.prompt.insert("thinking_budget_tokens", budgetTokens);
            }
        }
    }
    PendingRequestContext requestContext;
    requestContext.sessionId = state.tableId;
    requestContext.seatId = seat.seatId;
    requestContext.mode = resumeCommand ? resumeCommand->payload.value("mode").toString() : QString{};
    requestContext.phase = state.phase;
    requestContext.round = state.round;
    requestContext.reservedTokens = m_budgetManager->tokenReserve(state);
    requestContext.runGeneration = request.runGeneration;
    m_pendingRequests.insert(request.requestId, requestContext);
    emit sessionStateChanged(state);
    m_providerGateway->sendAsync(request);
    return true;
}

void SessionRunner::queueNextCommand(SessionState &state, const WorkflowCommand &command)
{
    if (command.commandType == RunnerCommandType::RequestSeatTurn
        || command.commandType == RunnerCommandType::RequestDecision) {
        m_delayedCommands.insert(state.tableId, command);
        if (state.pauseRequested || state.paused) {
            state.paused = true;
            state.pauseRequested = false;
            state.waitingForNextTurn = false;
            if (state.phase != Phase::Paused) {
                state.pausedResumePhase = state.phase;
                state.phase = Phase::Paused;
            }
            emit sessionStateChanged(state);
            return;
        }

        if (m_delayTimers.contains(state.tableId) && m_delayTimers.value(state.tableId)) {
            auto existingTimer = m_delayTimers.take(state.tableId);
            existingTimer->stop();
            existingTimer->deleteLater();
        }

        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, sessionId = state.tableId]() {
            const auto handle = m_sessionResolver ? m_sessionResolver(sessionId) : nullptr;
            const WorkflowCommand command = m_delayedCommands.take(sessionId);
            auto timerPtr = m_delayTimers.take(sessionId);
            if (timerPtr) {
                timerPtr->deleteLater();
            }
            if (!handle || handle->paused || handle->pauseRequested) {
                return;
            }
            handle->waitingForNextTurn = false;
            emit sessionStateChanged(*handle);
            executeCommand(*handle, command);
        });
        m_delayTimers.insert(state.tableId, timer);
        timer->start(5000);
        emit sessionStateChanged(state);
        return;
    }

    executeCommand(state, command);
}

QString SessionRunner::latestUserPrompt(const SessionState &state) const
{
    for (auto it = state.transcript.crbegin(); it != state.transcript.crend(); ++it) {
        if (it->isUser && !state.queuedInputIds.contains(it->entryId)) {
            return it->content;
        }
    }
    return state.title;
}

int SessionRunner::reservedTokensInFlight(const QString &sessionId) const
{
    int total = 0;
    for (auto it = m_pendingRequests.cbegin(); it != m_pendingRequests.cend(); ++it) {
        if (it.value().sessionId == sessionId) {
            total += it.value().reservedTokens;
        }
    }
    return total;
}

BudgetStatus SessionRunner::budgetStatusFor(const SessionState &state, int reservedTokensInFlight) const
{
    BudgetPolicy effectiveBudget = state.budgetPolicy;
    const ContinuationAllowance allowance = m_continuationAllowances.value(state.tableId);
    if (allowance.maxTotalCost >= 0.0) {
        effectiveBudget.maxTotalCost = qMax(effectiveBudget.maxTotalCost, allowance.maxTotalCost);
    }
    if (allowance.maxTotalTokens >= 0) {
        effectiveBudget.maxTotalTokens = qMax(effectiveBudget.maxTotalTokens, allowance.maxTotalTokens);
    }
    if (allowance.maxSessionSeconds >= 0) {
        effectiveBudget.maxSessionSeconds = qMax(effectiveBudget.maxSessionSeconds, allowance.maxSessionSeconds);
    }
    if (allowance.maxPhaseSeconds >= 0) {
        effectiveBudget.maxPhaseSeconds = qMax(effectiveBudget.maxPhaseSeconds, allowance.maxPhaseSeconds);
    }

    BudgetStatus status = m_budgetManager->status(state, reservedTokensInFlight, &effectiveBudget);
    if (allowance.ignoreSafetyReserve && status.kind == BudgetLimitKind::SafetyReserve) {
        status = {};
    }
    return status;
}

void SessionRunner::removePendingRequests(const QString &sessionId)
{
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end();) {
        if (it.value().sessionId == sessionId) {
            it = m_pendingRequests.erase(it);
        } else {
            ++it;
        }
    }
}

void SessionRunner::appendLog(SessionState &state, LogEventType type, const QString &actorSeatId, const QString &actorName, const QString &summary)
{
    LogEvent logEvent;
    logEvent.logId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    logEvent.tableId = state.tableId;
    logEvent.type = type;
    logEvent.actorSeatId = actorSeatId;
    logEvent.actorName = actorName;
    logEvent.phase = state.phase;
    logEvent.round = state.round;
    logEvent.timestamp = QDateTime::currentDateTimeUtc();
    logEvent.summary = summary;
    state.log.append(logEvent);
}

void SessionRunner::appendTranscript(SessionState &state, const QString &seatId, const QString &actorName, const QString &content, bool isDecision, bool isUser)
{
    TranscriptEntry entry;
    entry.entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.tableId = state.tableId;
    entry.phase = state.phase;
    entry.round = state.round;
    entry.speakerSeatId = seatId;
    entry.speakerName = actorName;
    entry.isUser = isUser;
    entry.isDecision = isDecision;
    entry.content = content;
    entry.timestamp = QDateTime::currentDateTimeUtc();
    state.transcript.append(entry);
}

void SessionRunner::emitAndHandle(SessionState &state, const WorkflowEvent &event)
{
    m_eventBus->publish(event);
    const auto commands = m_engine->handleEvent(state, event);
    emit sessionStateChanged(state);
    for (const auto &command : commands) {
        queueNextCommand(state, command);
    }
}

void SessionRunner::finalizeResearchBatchIfReady(SessionState &state)
{
    if (state.phase != Phase::Research || state.pendingResearchResponses > 0) {
        return;
    }
    const int researchRequests = m_researchRequestsBySession.value(state.tableId, 0);
    const int researchFailures = m_researchFailuresBySession.value(state.tableId, 0);
    if (researchRequests > 0 && researchFailures >= researchRequests) {
        const QString reason = "All model requests failed during Research. Stopping table.";
        appendLog(state, LogEventType::ProviderCallFailed, {}, {}, reason);
        m_researchRequestsBySession.remove(state.tableId);
        m_researchFailuresBySession.remove(state.tableId);
        emitAndHandle(state, makeEvent(state, EventType::SessionStopped, {{"reason", reason}}));
        return;
    }
    m_researchRequestsBySession.remove(state.tableId);
    m_researchFailuresBySession.remove(state.tableId);
    state.waitingForNextTurn = true;
    emitAndHandle(state, makeEvent(state, EventType::TurnCompleted, {{"seatId", "research-batch"}}));
}

void SessionRunner::markContinuationPending(SessionState &state, const BudgetStatus &status)
{
    if (status.reason.isEmpty()) {
        return;
    }
    if (state.continuationPending
        && state.continuationLimitKind == static_cast<int>(status.kind)
        && state.continuationReason == status.reason) {
        return;
    }
    state.continuationPending = true;
    state.continuationLimitKind = static_cast<int>(status.kind);
    state.continuationReason = status.reason;
    appendLog(state, LogEventType::LimitReached, {}, {}, QString("Continuation will be requested after this phase: %1").arg(status.reason));
}

void SessionRunner::enterContinuationPause(SessionState &state, const WorkflowCommand &resumeCommand)
{
    if (!m_delayedCommands.contains(state.tableId)) {
        m_delayedCommands.insert(state.tableId, resumeCommand);
    }
    if (m_delayTimers.contains(state.tableId) && m_delayTimers.value(state.tableId)) {
        auto timer = m_delayTimers.take(state.tableId);
        timer->stop();
        timer->deleteLater();
    }
    state.activeSeatId.clear();
    state.pendingResearchResponses = 0;
    state.paused = true;
    state.pauseRequested = false;
    state.waitingForNextTurn = false;
    state.pausedResumePhase = resumeCommand.targetPhase == Phase::Idle ? state.phase : resumeCommand.targetPhase;
    state.phase = Phase::Paused;
    appendLog(state, LogEventType::LimitReached, {}, {}, QString("Meeting paused for continuation: %1").arg(state.continuationReason));
    emit sessionStateChanged(state);
    emit continuationRequested(state.tableId, state.continuationReason, state.continuationLimitKind);
    updateElapsedTimerState();
}

void SessionRunner::clearContinuationState(SessionState &state)
{
    state.continuationPending = false;
    state.continuationLimitKind = 0;
    state.continuationReason.clear();
}

} // namespace amt


