#include <QtTest>

#include "core/response_parser.h"
#include "core/workflow_engine.h"
#include "services/budget_manager.h"

using namespace amt;

class CoreTests final : public QObject {
  Q_OBJECT

private slots:
  void roleValidationAndSerialization();
  void workflowTransitionsRemainStable();
  void arbitrationTransitionsRouteCorrections();
  void responseParsingRemainsStable();
  void budgetBoundariesAndContinuationOverrides();
};

void CoreTests::roleValidationAndSerialization() {
  SeatConfig participant;
  participant.seatId = "seat-1";
  participant.displayName = "Planner";
  participant.provider = ProviderKind::Gemini;
  participant.modelId = "gemini-test";
  participant.effort = ModelEffort::Deep;
  participant.role = Role::LeadPlanner;
  participant.occupied = true;
  participant.enabled = true;

  SeatConfig decisionMaker = participant;
  decisionMaker.seatId = "seat-2";
  decisionMaker.role = Role::FinalDecisionMaker;

  QVector<SeatConfig> seats{participant, decisionMaker};
  QCOMPARE(validateSeatRoleAssignments(seats), QString{});
  QCOMPARE(findFinalDecisionMakerSeatId(seats), QString("seat-2"));

  SeatConfig duplicateDecisionMaker = decisionMaker;
  duplicateDecisionMaker.seatId = "seat-3";
  seats.append(duplicateDecisionMaker);
  QVERIFY(!validateSeatRoleAssignments(seats).isEmpty());

  const SeatConfig restored = seatFromJson(seatToJson(participant));
  QCOMPARE(restored.seatId, participant.seatId);
  QCOMPARE(static_cast<int>(restored.provider),
           static_cast<int>(participant.provider));
  QCOMPARE(static_cast<int>(restored.role), static_cast<int>(participant.role));
  QCOMPARE(static_cast<int>(restored.effort),
           static_cast<int>(participant.effort));
  QCOMPARE(restored.occupied, participant.occupied);

  BudgetPolicy policy;
  policy.maxTokensPerPhase = 321;
  policy.maxTotalTokens = 654;
  policy.maxTotalCost = 7.5;
  QCOMPARE(budgetPolicyFromJson(budgetPolicyToJson(policy)).maxTotalTokens,
           654);
}

void CoreTests::workflowTransitionsRemainStable() {
  SessionState state;
  state.tableId = "workflow";
  SeatConfig seat;
  seat.seatId = "seat-1";
  seat.occupied = true;
  seat.enabled = true;
  state.seats.append(seat);

  WorkflowEngine engine;
  WorkflowEvent started;
  started.eventType = EventType::SessionStarted;
  auto commands = engine.handleEvent(state, started);
  QCOMPARE(static_cast<int>(state.phase), static_cast<int>(Phase::Research));
  QCOMPARE(commands.size(), 1);
  QCOMPARE(static_cast<int>(commands.first().commandType),
           static_cast<int>(RunnerCommandType::StartPhase));

  state.pendingResearchResponses = 0;
  WorkflowEvent researchComplete;
  researchComplete.eventType = EventType::TurnCompleted;
  researchComplete.payload.insert("seatId", "research-batch");
  commands = engine.handleEvent(state, researchComplete);
  QCOMPARE(static_cast<int>(state.phase), static_cast<int>(Phase::Planning));
  QCOMPARE(static_cast<int>(commands.first().targetPhase),
           static_cast<int>(Phase::Planning));

  state.phase = Phase::Present;
  WorkflowEvent approved;
  approved.eventType = EventType::DecisionIssued;
  approved.payload.insert("outcome", "Approve");
  commands = engine.handleEvent(state, approved);
  QCOMPARE(static_cast<int>(state.phase), static_cast<int>(Phase::Completed));
  QCOMPARE(static_cast<int>(commands.first().commandType),
           static_cast<int>(RunnerCommandType::StopSession));
}

void CoreTests::arbitrationTransitionsRouteCorrections() {
  WorkflowEngine engine;
  WorkflowEvent decision;
  decision.eventType = EventType::DecisionIssued;
  decision.payload.insert("mode", "arbitration");

  SessionState planning;
  planning.tableId = "planning";
  planning.phase = Phase::Planning;
  planning.round = 2;
  decision.payload.insert("outcome", "Revise");
  auto commands = engine.handleEvent(planning, decision);
  QCOMPARE(static_cast<int>(planning.phase), static_cast<int>(Phase::Planning));
  QCOMPARE(planning.round, 3);
  QCOMPARE(static_cast<int>(commands.first().targetPhase),
           static_cast<int>(Phase::Planning));

  planning.phase = Phase::Planning;
  decision.payload.insert("outcome", "Proceed");
  commands = engine.handleEvent(planning, decision);
  QCOMPARE(static_cast<int>(planning.phase),
           static_cast<int>(Phase::Execution));
  QCOMPARE(static_cast<int>(commands.first().targetPhase),
           static_cast<int>(Phase::Execution));

  SessionState qualityControl;
  qualityControl.tableId = "quality-control";
  qualityControl.phase = Phase::QualityControl;
  qualityControl.round = 2;
  decision.payload.insert("outcome", "Revise");
  commands = engine.handleEvent(qualityControl, decision);
  QCOMPARE(static_cast<int>(qualityControl.phase),
           static_cast<int>(Phase::Execution));
  QCOMPARE(qualityControl.execQcLoopCount, 1);
  QCOMPARE(static_cast<int>(commands.first().targetPhase),
           static_cast<int>(Phase::Execution));

  qualityControl.phase = Phase::QualityControl;
  decision.payload.insert("outcome", "Proceed");
  commands = engine.handleEvent(qualityControl, decision);
  QCOMPARE(static_cast<int>(qualityControl.phase),
           static_cast<int>(Phase::Present));
  QCOMPARE(static_cast<int>(commands.first().targetPhase),
           static_cast<int>(Phase::Present));
}

void CoreTests::responseParsingRemainsStable() {
  QVERIFY(response::hasSkipPrefix("  SKIPPED because duplicate"));
  QVERIFY(response::isSkipResponse("SKIP\nNo new evidence."));
  QVERIFY(!response::isSkipResponse("SKIPPED\nNo new evidence."));
  QCOMPARE(response::skipReason("SKIP"),
           QString("No additional reason provided."));
  QCOMPARE(response::skipReason("SKIP\nNo new evidence."),
           QString("No new evidence."));

  QCOMPARE(response::parseDecisionOutcome("Explanation\nFINAL_RULING: APPROVE",
                                          false),
           QString("Approve"));
  QCOMPARE(response::parseDecisionOutcome("Explanation\nFINAL_RULING: APPROVE",
                                          true),
           QString("Proceed"));
  QCOMPARE(
      response::parseDecisionOutcome("acceptable\nFINAL_RULING: REVISE", false),
      QString("Revise"));
  QCOMPARE(response::parseDecisionOutcome(
               "proceed with this version\nFINAL_RULING: REVISE", false),
           QString("Revise"));
  QCOMPARE(response::parseDecisionOutcome(
               "missing evidence\nFINAL_RULING: REVISE", false),
           QString("Revise"));
  const auto multiple = response::parseDecision(
      "FINAL_RULING: PROCEED\nExplanation\nFINAL_RULING: REVISE", true);
  QCOMPARE(multiple.outcome, QString("Revise"));
  QCOMPARE(multiple.explicitRulingCount, 2);
  QVERIFY(multiple.hasMultipleExplicitRulings());
  QCOMPARE(response::parseDecisionOutcome("FINAL_RULING: REVISE\n"
                                          "FINAL_RULING: PROCEED",
                                          true),
           QString("Proceed"));
  QCOMPARE(response::fallbackDecisionOutcome("ready to deliver"),
           QString("Approve"));
  QCOMPARE(response::fallbackDecisionOutcome("PROCEED: legacy explanation"),
           QString("Approve"));
}

void CoreTests::budgetBoundariesAndContinuationOverrides() {
  BudgetManager manager;
  SessionState state;
  state.budgetPolicy.maxTokensPerPhase = 10000;
  state.budgetPolicy.maxTotalTokens = 5000;
  state.budgetPolicy.maxTotalCost = 10.0;
  state.stopPolicy.stopOnBudgetExceeded = true;

  state.usedTokens = 5000;
  QCOMPARE(static_cast<int>(manager.status(state).kind),
           static_cast<int>(BudgetLimitKind::MaxTotalTokens));

  state.usedTokens = 4401;
  QCOMPARE(static_cast<int>(manager.status(state).kind),
           static_cast<int>(BudgetLimitKind::SafetyReserve));

  BudgetPolicy continuation = state.budgetPolicy;
  continuation.maxTotalTokens = 8000;
  QCOMPARE(static_cast<int>(manager.status(state, 0, &continuation).kind),
           static_cast<int>(BudgetLimitKind::None));

  state.usedTokens = 0;
  state.phaseUsedTokens = state.budgetPolicy.maxTokensPerPhase;
  QCOMPARE(static_cast<int>(manager.status(state).action),
           static_cast<int>(BudgetLimitAction::EndPhase));
}

QTEST_GUILESS_MAIN(CoreTests)

#include "test_core.moc"
