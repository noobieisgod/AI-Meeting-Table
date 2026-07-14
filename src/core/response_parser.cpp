#include "core/response_parser.h"

#include <QRegularExpression>

namespace amt::response {

namespace {

QString normalizeDecisionToken(QString token) {
  token = token.trimmed().toUpper();
  token.remove(QRegularExpression(R"([^A-Z_])"));
  if (token == "APPROVE" || token == "PROCEED" || token == "ACCEPT" ||
      token == "FINAL" || token == "READY" || token == "ACCEPTABLE" ||
      token == "PROCEEDWITHTHISVERSION" || token == "READYTODELIVER" ||
      token == "READYFORDELIVERY" || token == "FINALVERSION" ||
      token == "GOODENOUGHTODELIVER" || token == "SATISFIESTHEUSERREQUEST") {
    return "Approve";
  }
  if (token == "REVISE" || token == "REVISION" || token == "CHANGES_REQUIRED") {
    return "Revise";
  }
  if (token == "STOP" || token == "HALT") {
    return "Stop";
  }
  return {};
}

} // namespace

bool hasSkipPrefix(const QString &content) {
  return content.trimmed().startsWith("SKIP", Qt::CaseInsensitive);
}

bool isSkipResponse(const QString &content) {
  const QString trimmed = content.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }
  const QString firstLine = trimmed.section('\n', 0, 0).trimmed();
  return firstLine.compare("SKIP", Qt::CaseInsensitive) == 0;
}

QString skipReason(const QString &content) {
  const QString trimmed = content.trimmed();
  const qsizetype lineBreak = trimmed.indexOf('\n');
  if (lineBreak < 0) {
    return "No additional reason provided.";
  }
  const QString reason = trimmed.mid(lineBreak + 1).trimmed();
  return reason.isEmpty() ? "No additional reason provided." : reason;
}

QString fallbackDecisionOutcome(const QString &content) {
  const QString firstLine = content.trimmed().section('\n', 0, 0).trimmed();
  const QString normalized = normalizeDecisionToken(firstLine);
  if (!normalized.isEmpty()) {
    return normalized;
  }
  const QString lowered = firstLine.toLower();
  if (lowered.startsWith("approve") || lowered.startsWith("proceed") ||
      lowered.startsWith("accept")) {
    return "Approve";
  }
  if (lowered.startsWith("revise")) {
    return "Revise";
  }
  if (lowered.startsWith("stop")) {
    return "Stop";
  }
  return {};
}

DecisionParseResult parseDecision(const QString &content, bool arbitration) {
  DecisionParseResult result;
  static const QRegularExpression finalRulingPattern(
      R"((?:^|\n)\s*FINAL_RULING\s*:\s*([A-Za-z_]+))",
      QRegularExpression::CaseInsensitiveOption);
  auto matches = finalRulingPattern.globalMatch(content);
  while (matches.hasNext()) {
    const auto match = matches.next();
    result.explicitRulingCount += 1;
    const QString candidate = normalizeDecisionToken(match.captured(1));
    if (!candidate.isEmpty()) {
      result.outcome = candidate;
    }
  }
  if (result.outcome.isEmpty()) {
    result.outcome = fallbackDecisionOutcome(content);
  }
  if (arbitration && result.outcome == "Approve") {
    result.outcome = "Proceed";
  }
  return result;
}

QString parseDecisionOutcome(const QString &content, bool arbitration) {
  return parseDecision(content, arbitration).outcome;
}

} // namespace amt::response
