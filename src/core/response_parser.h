#pragma once

#include <QString>

namespace amt::response {

struct DecisionParseResult {
  QString outcome;
  int explicitRulingCount = 0;

  bool hasMultipleExplicitRulings() const { return explicitRulingCount > 1; }
};

bool hasSkipPrefix(const QString &content);
bool isSkipResponse(const QString &content);
QString skipReason(const QString &content);
QString fallbackDecisionOutcome(const QString &content);
DecisionParseResult parseDecision(const QString &content, bool arbitration);
QString parseDecisionOutcome(const QString &content, bool arbitration);

} // namespace amt::response
