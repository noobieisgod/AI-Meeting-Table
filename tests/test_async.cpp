#include <QtTest>

#include <algorithm>

#include "core/event_bus.h"
#include "core/session_runner.h"
#include "core/workflow_engine.h"
#include "providers/provider_gateway.h"
#include "services/artifact_manager.h"
#include "services/budget_manager.h"

using namespace amt;

namespace {

class FakeProviderGateway final : public ProviderGateway {
public:
  using ProviderGateway::ProviderGateway;

  void sendAsync(const ProviderRequest &request) override {
    requests.append(request);
  }

  QVector<ProviderRequest> requests;
};

std::shared_ptr<SessionState> makeSession(Phase phase) {
  auto state = std::make_shared<SessionState>();
  state->tableId = "session";
  state->phase = phase;
  state->round = 1;

  SeatConfig participant;
  participant.seatId = "seat-participant";
  participant.displayName = "Participant";
  participant.occupied = true;
  participant.enabled = true;
  state->seats.append(participant);

  SeatConfig decisionMaker = participant;
  decisionMaker.seatId = "seat-fdm";
  decisionMaker.displayName = "Decision Maker";
  decisionMaker.role = Role::FinalDecisionMaker;
  state->seats.append(decisionMaker);
  state->finalDecisionMakerSeatId = decisionMaker.seatId;
  return state;
}

ProviderResponse responseFor(const ProviderRequest &request,
                             const QString &outcome, const QString &content) {
  ProviderResponse response;
  response.requestId = request.requestId;
  response.sessionId = request.sessionId;
  response.seatId = request.seatId;
  response.success = true;
  response.content = content;
  response.decisionOutcome = outcome;
  response.runGeneration = request.runGeneration;
  return response;
}

} // namespace

class AsyncTests final : public QObject {
  Q_OBJECT

private slots:
  void unknownAndGenerationStaleResponsesAreRejected();
  void duplicateArbitrationResponseIsConsumedOnce();
  void phaseAndRoundStaleResponsesAreRejected();
  void qualityControlRevisionReturnsToExecutionWithSafeDiagnostic();
  void presentProceedAliasCompletesOnce();
};

void AsyncTests::unknownAndGenerationStaleResponsesAreRejected() {
  auto state = makeSession(Phase::Research);
  EventBus eventBus;
  WorkflowEngine workflow;
  FakeProviderGateway gateway;
  BudgetManager budget;
  ArtifactManager artifacts;
  SessionRunner runner(&eventBus, &workflow, &gateway, &budget, &artifacts,
                       [state](const QString &tableId) {
                         return tableId == state->tableId
                                    ? state
                                    : std::shared_ptr<SessionState>{};
                       });

  ProviderResponse unknown;
  unknown.requestId = "unknown";
  unknown.sessionId = state->tableId;
  unknown.seatId = "seat-participant";
  unknown.success = true;
  unknown.content = "Unknown content";
  unknown.runGeneration = 0;
  gateway.responseReady(unknown);
  QCOMPARE(state->transcript.size(), 0);
  QCOMPARE(state->usedTokens, 0);

  WorkflowCommand turn;
  turn.commandType = RunnerCommandType::RequestSeatTurn;
  turn.sessionId = state->tableId;
  turn.targetPhase = state->phase;
  turn.targetSeatId = "seat-participant";
  runner.executeCommand(*state, turn);
  QCOMPARE(gateway.requests.size(), 1);

  ProviderResponse wrongSession =
      responseFor(gateway.requests.last(), {}, "Wrong session");
  wrongSession.sessionId = "other-session";
  gateway.responseReady(wrongSession);
  QCOMPARE(state->transcript.size(), 0);

  runner.executeCommand(*state, turn);
  QCOMPARE(gateway.requests.size(), 2);
  ProviderResponse wrongSeat =
      responseFor(gateway.requests.last(), {}, "Wrong seat");
  wrongSeat.seatId = "seat-fdm";
  gateway.responseReady(wrongSeat);
  QCOMPARE(state->transcript.size(), 0);

  runner.executeCommand(*state, turn);
  QCOMPARE(gateway.requests.size(), 3);
  ProviderResponse stale = responseFor(gateway.requests.last(), {}, "Stale");
  stale.runGeneration += 1;
  gateway.responseReady(stale);
  QCOMPARE(state->transcript.size(), 0);

  runner.executeCommand(*state, turn);
  const ProviderResponse previousRun =
      responseFor(gateway.requests.last(), {}, "Previous run");
  runner.startSession(*state);
  gateway.responseReady(previousRun);
  QCOMPARE(state->transcript.size(), 0);
}

void AsyncTests::duplicateArbitrationResponseIsConsumedOnce() {
  auto state = makeSession(Phase::Planning);
  EventBus eventBus;
  WorkflowEngine workflow;
  FakeProviderGateway gateway;
  BudgetManager budget;
  ArtifactManager artifacts;
  SessionRunner runner(&eventBus, &workflow, &gateway, &budget, &artifacts,
                       [state](const QString &tableId) {
                         return tableId == state->tableId
                                    ? state
                                    : std::shared_ptr<SessionState>{};
                       });

  WorkflowCommand decision;
  decision.commandType = RunnerCommandType::RequestDecision;
  decision.sessionId = state->tableId;
  decision.targetPhase = state->phase;
  decision.targetSeatId = state->finalDecisionMakerSeatId;
  decision.payload.insert("mode", "arbitration");
  runner.executeCommand(*state, decision);
  QCOMPARE(gateway.requests.size(), 1);
  QVERIFY(gateway.requests.first()
              .prompt.value("instruction")
              .toString()
              .contains("Planning to Execution"));

  const ProviderResponse proceed =
      responseFor(gateway.requests.first(), "Proceed",
                  "Explanation: move forward.\nFINAL_RULING: PROCEED");
  gateway.responseReady(proceed);
  QCOMPARE(state->transcript.size(), 1);
  QVERIFY(state->transcript.first().content.startsWith(
      "Planning arbitration: Proceed to Execution"));
  QCOMPARE(static_cast<int>(state->phase), static_cast<int>(Phase::Execution));

  gateway.responseReady(proceed);
  QCOMPARE(state->transcript.size(), 1);
  QCOMPARE(static_cast<int>(state->phase), static_cast<int>(Phase::Execution));
}

void AsyncTests::phaseAndRoundStaleResponsesAreRejected() {
  auto state = makeSession(Phase::QualityControl);
  EventBus eventBus;
  WorkflowEngine workflow;
  FakeProviderGateway gateway;
  BudgetManager budget;
  ArtifactManager artifacts;
  SessionRunner runner(&eventBus, &workflow, &gateway, &budget, &artifacts,
                       [state](const QString &tableId) {
                         return tableId == state->tableId
                                    ? state
                                    : std::shared_ptr<SessionState>{};
                       });

  WorkflowCommand decision;
  decision.commandType = RunnerCommandType::RequestDecision;
  decision.sessionId = state->tableId;
  decision.targetPhase = state->phase;
  decision.targetSeatId = state->finalDecisionMakerSeatId;
  decision.payload.insert("mode", "arbitration");
  runner.executeCommand(*state, decision);
  QCOMPARE(gateway.requests.size(), 1);
  state->round += 1;
  gateway.responseReady(responseFor(gateway.requests.first(), "Proceed",
                                    "FINAL_RULING: PROCEED"));
  QCOMPARE(state->transcript.size(), 0);

  state->round = 1;
  runner.executeCommand(*state, decision);
  QCOMPARE(gateway.requests.size(), 2);
  state->phase = Phase::Execution;
  gateway.responseReady(
      responseFor(gateway.requests.last(), "Revise", "FINAL_RULING: REVISE"));
  QCOMPARE(state->transcript.size(), 0);
}

void AsyncTests::qualityControlRevisionReturnsToExecutionWithSafeDiagnostic() {
  auto state = makeSession(Phase::QualityControl);
  EventBus eventBus;
  WorkflowEngine workflow;
  FakeProviderGateway gateway;
  BudgetManager budget;
  ArtifactManager artifacts;
  SessionRunner runner(&eventBus, &workflow, &gateway, &budget, &artifacts,
                       [state](const QString &tableId) {
                         return tableId == state->tableId
                                    ? state
                                    : std::shared_ptr<SessionState>{};
                       });

  WorkflowCommand decision;
  decision.commandType = RunnerCommandType::RequestDecision;
  decision.sessionId = state->tableId;
  decision.targetPhase = state->phase;
  decision.targetSeatId = state->finalDecisionMakerSeatId;
  decision.payload.insert("mode", "arbitration");
  runner.executeCommand(*state, decision);

  ProviderResponse revise =
      responseFor(gateway.requests.first(), "Revise",
                  "private response content\nFINAL_RULING: REVISE");
  revise.multipleDecisionRulings = true;
  gateway.responseReady(revise);
  QCOMPARE(static_cast<int>(state->phase), static_cast<int>(Phase::Execution));
  QCOMPARE(state->transcript.size(), 1);
  QVERIFY(state->transcript.first().content.startsWith(
      "Quality Control arbitration: Revise via Execution"));
  QVERIFY(std::any_of(state->log.cbegin(), state->log.cend(),
                      [](const LogEvent &event) {
                        return event.summary.contains(
                            "Multiple explicit final decision ruling lines");
                      }));
  QVERIFY(std::none_of(
      state->log.cbegin(), state->log.cend(), [](const LogEvent &event) {
        return event.summary.contains("private response content");
      }));
}

void AsyncTests::presentProceedAliasCompletesOnce() {
  auto state = makeSession(Phase::Present);
  EventBus eventBus;
  WorkflowEngine workflow;
  FakeProviderGateway gateway;
  BudgetManager budget;
  ArtifactManager artifacts;
  SessionRunner runner(&eventBus, &workflow, &gateway, &budget, &artifacts,
                       [state](const QString &tableId) {
                         return tableId == state->tableId
                                    ? state
                                    : std::shared_ptr<SessionState>{};
                       });

  WorkflowCommand decision;
  decision.commandType = RunnerCommandType::RequestDecision;
  decision.sessionId = state->tableId;
  decision.targetPhase = state->phase;
  decision.targetSeatId = state->finalDecisionMakerSeatId;
  runner.executeCommand(*state, decision);

  const ProviderResponse approve = responseFor(
      gateway.requests.first(), "Approve",
      "Explanation: ready.\nFINAL_RULING: PROCEED");
  gateway.responseReady(approve);
  QCOMPARE(static_cast<int>(state->phase), static_cast<int>(Phase::Completed));
  QCOMPARE(state->transcript.size(), 1);
  QVERIFY(state->transcript.first().content.startsWith(
      "Final decision: Approved and complete"));

  gateway.responseReady(approve);
  QCOMPARE(state->transcript.size(), 1);
  QCOMPARE(static_cast<int>(state->phase), static_cast<int>(Phase::Completed));
}

QTEST_GUILESS_MAIN(AsyncTests)

#include "test_async.moc"
