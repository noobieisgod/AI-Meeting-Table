#include "services/budget_manager.h"

#include <algorithm>
#include <QtGlobal>

namespace amt {

namespace {

int tokenReserveForPolicy(const BudgetPolicy &policy)
{
    const int reserve = policy.maxTokensPerPhase / 10;
    return std::clamp(reserve, 600, 2000);
}

} // namespace

BudgetManager::BudgetManager(QObject *parent)
    : QObject(parent)
{
}

void BudgetManager::applyUsage(SessionState &state,
                               const QString &seatId,
                               int inputTokens,
                               int outputTokens,
                               int totalTokens,
                               bool usageEstimated) const
{
    state.usedTokens += totalTokens;
    state.phaseUsedTokens += totalTokens;
    state.usageEstimateUsed = state.usageEstimateUsed || usageEstimated;

    auto it = std::find_if(state.seatUsage.begin(), state.seatUsage.end(), [&seatId](const SeatUsageTally &usage) {
        return usage.seatId == seatId;
    });
    if (it == state.seatUsage.end()) {
        SeatUsageTally usage;
        usage.seatId = seatId;
        state.seatUsage.append(usage);
        it = std::prev(state.seatUsage.end());
    }
    it->totalTokens += totalTokens;
    it->inputTokens += inputTokens;
    it->outputTokens += outputTokens;
    it->phaseTokens += totalTokens;
}

int BudgetManager::tokenReserve(const SessionState &state) const
{
    return tokenReserveForPolicy(state.budgetPolicy);
}

BudgetStatus BudgetManager::status(const SessionState &state,
                                   int reservedTokensInFlight,
                                   const BudgetPolicy *budgetOverride) const
{
    const BudgetPolicy &budget = budgetOverride ? *budgetOverride : state.budgetPolicy;
    if (state.round > budget.maxRounds) {
        return {BudgetLimitKind::MaxRoundsPerPhase, BudgetLimitAction::EndPhase, "The maximum round limit for this phase has been reached."};
    }
    if (state.execQcLoopCount > budget.maxExecQcLoops) {
        return {BudgetLimitKind::MaxExecQcLoops, BudgetLimitAction::EndPhase, "The maximum execution/QC loop limit has been reached."};
    }
    if (state.stopPolicy.stopOnSessionTimeout && state.elapsedSeconds >= budget.maxSessionSeconds) {
        return {BudgetLimitKind::MaxSessionSeconds, BudgetLimitAction::PromptToContinue, "The maximum session time limit has been reached."};
    }
    if (state.stopPolicy.stopOnPhaseTimeout && state.phaseElapsedSeconds >= budget.maxPhaseSeconds) {
        return {BudgetLimitKind::MaxPhaseSeconds, BudgetLimitAction::PromptToContinue, "The maximum phase time limit has been reached."};
    }
    if (state.stopPolicy.stopOnBudgetExceeded && state.usedTokens >= budget.maxTotalTokens) {
        return {BudgetLimitKind::MaxTotalTokens, BudgetLimitAction::PromptToContinue, "The maximum total token limit has been reached."};
    }
    if (state.stopPolicy.stopOnBudgetExceeded && state.phaseUsedTokens >= budget.maxTokensPerPhase) {
        return {BudgetLimitKind::MaxPhaseTokens, BudgetLimitAction::EndPhase, "The maximum phase token limit has been reached."};
    }

    const int reserve = tokenReserveForPolicy(budget);
    const int remaining = budget.maxTotalTokens
        - state.usedTokens
        - qMax(0, reservedTokensInFlight);
    if (state.stopPolicy.stopOnBudgetExceeded && remaining < reserve) {
        return {
            BudgetLimitKind::SafetyReserve,
            BudgetLimitAction::PromptToContinue,
            QString("Only %1 tokens remain, which is below the %2-token safety reserve.")
                .arg(qMax(0, remaining))
                .arg(reserve)
        };
    }

    return {};
}

}
