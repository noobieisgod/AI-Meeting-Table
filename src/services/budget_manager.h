#pragma once

#include <QObject>

#include "domain/models.h"

namespace amt {

enum class BudgetLimitKind {
    None,
    MaxTotalCost,
    MaxRoundsPerPhase,
    MaxExecQcLoops,
    MaxSessionSeconds,
    MaxPhaseSeconds,
    MaxTotalTokens,
    MaxPhaseTokens,
    SafetyReserve
};

enum class BudgetLimitAction {
    None,
    EndPhase,
    PromptToContinue
};

struct BudgetStatus {
    BudgetLimitKind kind = BudgetLimitKind::None;
    BudgetLimitAction action = BudgetLimitAction::None;
    QString reason;
};

class BudgetManager final : public QObject
{
    Q_OBJECT

public:
    explicit BudgetManager(QObject *parent = nullptr);

    void applyUsage(SessionState &state, const QString &seatId, int tokens, double cost) const;
    int tokenReserve(const SessionState &state) const;
    BudgetStatus status(const SessionState &state,
                        int reservedTokensInFlight = 0,
                        const BudgetPolicy *budgetOverride = nullptr) const;
};

} // namespace amt
