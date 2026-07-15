#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace amt {

enum class Phase { Idle, Research, Planning, Execution, QualityControl, Present, Paused, Completed, Stopped, Failed };
enum class Role { None, FinalDecisionMaker, LeadPlanner, LeadExecutioner, LeadQualityControl };
enum class LogEventType { SessionStarted, UserMessageAdded, PhaseStarted, TurnStarted, AISpoke, AISkipped, ProviderCallFailed, RetryScheduled, PhaseEnded, FinalDecisionMade, SessionStopped, LimitReached };
enum class EventType { SessionStarted, PhaseStarted, TurnStarted, TurnCompleted, TurnSkipped, ProviderCallFailed, RetryScheduled, BudgetExceeded, DecisionIssued, InputQueued, RoundEnded, PhaseEnded, SessionStopped };
enum class ProviderKind { OpenAI, Gemini, Anthropic };
enum class RunnerCommandType { None, StartPhase, RunResearchBatch, RequestSeatTurn, RequestDecision, StopSession };
enum class ModelEffort { Auto, Light, Balanced, Deep };
enum class ThemeMode { System, Light, Dark };

struct ModelCatalogEntry {
    QString id;
    QString displayName;
    bool isPreview = false;
    bool supportsEffort = true;
};

struct BudgetPolicy {
    int maxTokensPerPhase = 12000;
    int maxTotalTokens = 48000;
    double maxTotalCost = 10.0;
    int maxRounds = 6;
    int maxExecQcLoops = 4;
    int maxPhaseSeconds = 120;
    int maxSessionSeconds = 900;
};

struct StopPolicy {
    bool allowEarlyStopByDecisionMaker = true;
    bool stopOnBudgetExceeded = true;
    bool stopOnSessionTimeout = true;
    bool stopOnPhaseTimeout = true;
};

struct SeatUsageTally {
    QString seatId;
    int totalTokens = 0;
    double totalCost = 0.0;
    int phaseTokens = 0;
    double phaseCost = 0.0;
};

struct SeatConfig {
    QString seatId;
    QString displayName;
    ProviderKind provider = ProviderKind::OpenAI;
    QString modelId;
    QString modelPreset;
    QString modelOverride;
    ModelEffort effort = ModelEffort::Auto;
    Role role = Role::None;
    QString color;
    bool occupied = false;
    bool enabled = true;
};

struct AppSettings {
    BudgetPolicy globalBudgetDefaults;
    ThemeMode theme = ThemeMode::System;
    QString colorTheme = "Signal Session";
    QString fontStyle = "System";
};

struct AttachmentRecord {
    QString attachmentId;
    QString displayName;
    QString filePath;
    QString fileHash;
    QDateTime addedAt;
    QJsonObject providerHandles;
};

struct ArtifactVersion {
    QString versionId;
    QString parentVersionId;
    Phase createdByPhase = Phase::Idle;
    int createdByRound = 0;
    QDateTime createdAt;
    QString summary;
    QString filePath;
};

struct TranscriptEntry {
    QString entryId;
    QString tableId;
    Phase phase = Phase::Idle;
    int round = 0;
    QString speakerSeatId;
    QString speakerName;
    bool isUser = false;
    bool isDecision = false;
    QString content;
    QDateTime timestamp;
};

struct LogEvent {
    QString logId;
    QString tableId;
    LogEventType type = LogEventType::SessionStarted;
    QString actorSeatId;
    QString actorName;
    Phase phase = Phase::Idle;
    int round = 0;
    QDateTime timestamp;
    QString summary;
};

struct WorkflowEvent {
    QString eventId;
    QString sessionId;
    EventType eventType = EventType::SessionStarted;
    QDateTime createdAt;
    QString causationId;
    QString correlationId;
    QJsonObject payload;
};

struct WorkflowCommand {
    RunnerCommandType commandType = RunnerCommandType::None;
    QString sessionId;
    Phase targetPhase = Phase::Idle;
    QString targetSeatId;
    QJsonObject payload;
};

struct SessionState {
    QString tableId;
    QString title;
    bool pinned = false;
    QDateTime updatedAt;
    Phase phase = Phase::Idle;
    int round = 0;
    int execQcLoopCount = 0;
    QString activeSeatId;
    QString finalDecisionMakerSeatId;
    BudgetPolicy budgetPolicy;
    StopPolicy stopPolicy;
    bool useBudgetOverrides = false;
    BudgetPolicy budgetOverrides;
    int usedTokens = 0;
    double usedCost = 0.0;
    bool usageEstimateUsed = false;
    bool costEstimateComplete = true;
    int phaseUsedTokens = 0;
    double phaseUsedCost = 0.0;
    int elapsedSeconds = 0;
    int phaseElapsedSeconds = 0;
    int pendingResearchResponses = 0;
    bool logVisible = true;
    bool pauseRequested = false;
    bool paused = false;
    Phase pausedResumePhase = Phase::Idle;
    bool continuationPending = false;
    int continuationLimitKind = 0;
    QString continuationReason;
    bool waitingForNextTurn = false;
    bool arbitrationSatisfied = false;
    QVector<SeatConfig> seats;
    QVector<SeatConfig> pendingSeats;
    QVector<SeatUsageTally> seatUsage;
    QVector<TranscriptEntry> transcript;
    QVector<LogEvent> log;
    QVector<AttachmentRecord> attachments;
    QVector<ArtifactVersion> artifacts;
    QString currentArtifactVersionId;
    QStringList queuedInputIds;
};

inline QString toString(Phase phase)
{
    switch (phase) {
    case Phase::Idle: return "Idle";
    case Phase::Research: return "Research";
    case Phase::Planning: return "Planning";
    case Phase::Execution: return "Execution";
    case Phase::QualityControl: return "Quality Control";
    case Phase::Present: return "Present";
    case Phase::Paused: return "Paused";
    case Phase::Completed: return "Completed";
    case Phase::Stopped: return "Stopped";
    case Phase::Failed: return "Failed";
    }
    return "Unknown";
}

inline QString toString(Role role)
{
    switch (role) {
    case Role::None: return "Participant";
    case Role::FinalDecisionMaker: return "Final Decision Maker";
    case Role::LeadPlanner: return "Lead Planner";
    case Role::LeadExecutioner: return "Lead Executioner";
    case Role::LeadQualityControl: return "Lead Quality Control";
    }
    return "Participant";
}

inline QString toString(ModelEffort effort)
{
    switch (effort) {
    case ModelEffort::Auto: return "Auto";
    case ModelEffort::Light: return "Light";
    case ModelEffort::Balanced: return "Balanced";
    case ModelEffort::Deep: return "Deep";
    }
    return "Auto";
}

inline QString toString(ThemeMode theme)
{
    switch (theme) {
    case ThemeMode::Dark: return "Dark";
    case ThemeMode::Light: return "Light";
    case ThemeMode::System:
    default: return "System";
    }
}

inline QString displayRole(Role role)
{
    switch (role) {
    case Role::None: return "Default";
    case Role::FinalDecisionMaker: return "Final Decision Maker";
    case Role::LeadPlanner: return "Lead Planner";
    case Role::LeadExecutioner: return "Lead Executioner";
    case Role::LeadQualityControl: return "Lead Quality Control";
    }
    return "Default";
}

inline QString displaySeatRole(Role role)
{
    if (role == Role::FinalDecisionMaker) {
        return "Decision Maker";
    }
    return displayRole(role);
}

inline QString toString(ProviderKind provider)
{
    switch (provider) {
    case ProviderKind::OpenAI: return "ChatGPT";
    case ProviderKind::Gemini: return "Gemini";
    case ProviderKind::Anthropic: return "Claude";
    }
    return "Unknown";
}

inline ProviderKind providerKindFromString(const QString &value)
{
    if (value.compare("Gemini", Qt::CaseInsensitive) == 0) {
        return ProviderKind::Gemini;
    }
    if (value.compare("Claude", Qt::CaseInsensitive) == 0
        || value.compare("Anthropic", Qt::CaseInsensitive) == 0) {
        return ProviderKind::Anthropic;
    }
    if (value.compare("ChatGPT", Qt::CaseInsensitive) == 0
        || value.compare("OpenAI", Qt::CaseInsensitive) == 0) {
        return ProviderKind::OpenAI;
    }
    return ProviderKind::OpenAI;
}

// Issue #14: Persistence-safe provider name (distinct from UI display name)
inline QString providerKindToString(ProviderKind provider)
{
    switch (provider) {
    case ProviderKind::OpenAI: return "OpenAI";
    case ProviderKind::Gemini: return "Gemini";
    case ProviderKind::Anthropic: return "Anthropic";
    }
    return "OpenAI";
}

inline ProviderKind providerKindFromIndex(int index)
{
    switch (index) {
    case 1: return ProviderKind::Gemini;
    case 2: return ProviderKind::Anthropic;
    default:
        return ProviderKind::OpenAI;
    }
}

inline Role roleFromEditorIndex(int index)
{
    switch (index) {
    case 1: return Role::FinalDecisionMaker;
    case 2: return Role::LeadPlanner;
    case 3: return Role::LeadExecutioner;
    case 4: return Role::LeadQualityControl;
    default:
        return Role::None;
    }
}

inline int indexFromRole(Role role)
{
    switch (role) {
    case Role::FinalDecisionMaker: return 1;
    case Role::LeadPlanner: return 2;
    case Role::LeadExecutioner: return 3;
    case Role::LeadQualityControl: return 4;
    case Role::None:
    default:
        return 0;
    }
}

inline ModelEffort effortFromEditorIndex(int index)
{
    switch (index) {
    case 1: return ModelEffort::Light;
    case 2: return ModelEffort::Balanced;
    case 3: return ModelEffort::Deep;
    default:
        return ModelEffort::Auto;
    }
}

inline int indexFromEffort(ModelEffort effort)
{
    switch (effort) {
    case ModelEffort::Light: return 1;
    case ModelEffort::Balanced: return 2;
    case ModelEffort::Deep: return 3;
    case ModelEffort::Auto:
    default:
        return 0;
    }
}

inline int indexFromProviderKind(ProviderKind provider)
{
    switch (provider) {
    case ProviderKind::Gemini: return 1;
    case ProviderKind::Anthropic: return 2;
    case ProviderKind::OpenAI:
    default:
        return 0;
    }
}

inline ThemeMode themeModeFromString(const QString &value)
{
    if (value.compare("Dark", Qt::CaseInsensitive) == 0) {
        return ThemeMode::Dark;
    }
    if (value.compare("Light", Qt::CaseInsensitive) == 0) {
        return ThemeMode::Light;
    }
    return ThemeMode::System;
}

inline ModelEffort effortFromString(const QString &value)
{
    if (value == "Light") {
        return ModelEffort::Light;
    }
    if (value == "Balanced") {
        return ModelEffort::Balanced;
    }
    if (value == "Deep") {
        return ModelEffort::Deep;
    }
    return ModelEffort::Auto;
}

inline QString providerCredentialTarget(ProviderKind provider)
{
    switch (provider) {
    case ProviderKind::OpenAI: return "AI_MEETING_TABLE_OPENAI";
    case ProviderKind::Gemini: return "AI_MEETING_TABLE_GEMINI";
    case ProviderKind::Anthropic: return "AI_MEETING_TABLE_ANTHROPIC";
    }
    return "AI_MEETING_TABLE_UNKNOWN";
}

inline QVector<ModelCatalogEntry> modelCatalogForProvider(ProviderKind provider)
{
    switch (provider) {
    case ProviderKind::OpenAI:
        return {
            {"gpt-4o", "GPT-4o", false, true},
            {"gpt-4o-mini", "GPT-4o Mini", false, true},
            {"o1-mini", "o1 Mini", false, true},
            {"o1-preview", "o1 Preview", false, true},
            {"o3-mini", "o3 Mini", false, true},
            {"gpt-5", "GPT-5", false, true},
            {"gpt-5-mini", "GPT-5 Mini", false, true},
            {"gpt-5-nano", "GPT-5 Nano", false, true},
            {"gpt-5.2", "GPT-5.2", false, true},
            {"gpt-5.2-pro", "GPT-5.2 Pro", false, true},
            {"gpt-5.1", "GPT-5.1", false, true},
            {"gpt-5.1-codex", "GPT-5.1 Codex", false, true},
            {"gpt-5-pro", "GPT-5 Pro", false, true},
            {"o3", "o3", false, true},
            {"o3-pro", "o3 Pro", false, true},
            {"o4-mini", "o4 Mini", false, true}
        };
    case ProviderKind::Gemini:
        return {
            {"gemini-1.5-pro", "Gemini 1.5 Pro", false, true},
            {"gemini-1.5-flash", "Gemini 1.5 Flash", false, true},
            {"gemini-2.0-flash", "Gemini 2.0 Flash", false, true},
            {"gemini-2.5-pro", "Gemini 2.5 Pro", false, true},
            {"gemini-2.5-flash", "Gemini 2.5 Flash", false, true},
            {"gemini-2.5-flash-lite", "Gemini 2.5 Flash-Lite", false, true},
            {"gemini-3.1-pro-preview", "Gemini 3.1 Pro", true, true},
            {"gemini-3-flash-preview", "Gemini 3 Flash", true, true},
            {"gemini-3.1-flash-lite-preview", "Gemini 3.1 Flash-Lite", true, true}
        };
    case ProviderKind::Anthropic:
        return {
            {"claude-3-5-sonnet-latest", "Claude 3.5 Sonnet", false, true},
            {"claude-3-5-haiku-latest", "Claude 3.5 Haiku", false, true},
            {"claude-3-opus-latest", "Claude 3 Opus", false, true},
            {"claude-opus-4-6", "Claude Opus 4.6", false, true},
            {"claude-opus-4-1-20250805", "Claude Opus 4.1", false, true},
            {"claude-opus-4-20250514", "Claude Opus 4", false, true},
            {"claude-sonnet-4-5", "Claude Sonnet 4.5", false, true},
            {"claude-sonnet-4-20250514", "Claude Sonnet 4", false, true}
        };
    }
    return {};
}

inline QString legacyModelAlias(ProviderKind provider, const QString &value)
{
    const QString normalized = value.trimmed();
    if (provider == ProviderKind::OpenAI) {
        if (normalized.compare("GPT-5", Qt::CaseInsensitive) == 0) {
            return "gpt-5";
        }
        if (normalized.compare("GPT-5 mini", Qt::CaseInsensitive) == 0) {
            return "gpt-5-mini";
        }
    } else if (provider == ProviderKind::Gemini) {
        if (normalized.compare("Gemini 2.5 Pro", Qt::CaseInsensitive) == 0) {
            return "gemini-2.5-pro";
        }
        if (normalized.compare("Gemini 2.5 Flash", Qt::CaseInsensitive) == 0) {
            return "gemini-2.5-flash";
        }
    } else if (provider == ProviderKind::Anthropic) {
        if (normalized.compare("Claude Sonnet 4", Qt::CaseInsensitive) == 0) {
            return "claude-sonnet-4-20250514";
        }
        if (normalized.compare("Claude Opus 4", Qt::CaseInsensitive) == 0) {
            return "claude-opus-4-20250514";
        }
        if (normalized.compare("Claude Opus 4.1", Qt::CaseInsensitive) == 0) {
            return "claude-opus-4-1-20250805";
        }
        if (normalized.compare("Claude Opus 4.6", Qt::CaseInsensitive) == 0
            || normalized.compare("claude-opus-4.6", Qt::CaseInsensitive) == 0) {
            return "claude-opus-4-6";
        }
        if (normalized.compare("Claude Opus 4.7", Qt::CaseInsensitive) == 0
            || normalized.compare("claude-opus-4.7", Qt::CaseInsensitive) == 0) {
            return "claude-opus-4-6";
        }
        if (normalized.compare("Claude Sonnet 4.5", Qt::CaseInsensitive) == 0) {
            return "claude-sonnet-4-5";
        }
        if (normalized.compare("Claude Haiku 3.5", Qt::CaseInsensitive) == 0) {
            return "claude-3-5-haiku-latest";
        }
        if (normalized.compare("claude-sonnet-4-20250514", Qt::CaseInsensitive) == 0
            || normalized.compare("claude-sonnet-4-0", Qt::CaseInsensitive) == 0) {
            return "claude-sonnet-4-20250514";
        }
        if (normalized.compare("claude-opus-4-20250514", Qt::CaseInsensitive) == 0
            || normalized.compare("claude-opus-4-0", Qt::CaseInsensitive) == 0) {
            return "claude-opus-4-20250514";
        }
        if (normalized.compare("claude-opus-4-1-20250805", Qt::CaseInsensitive) == 0) {
            return "claude-opus-4-1-20250805";
        }
        if (normalized.compare("claude-3-5-haiku-latest", Qt::CaseInsensitive) == 0) {
            return "claude-3-5-haiku-latest";
        }
    }
    return {};
}

inline ModelCatalogEntry findModelCatalogEntry(ProviderKind provider, const QString &value)
{
    const QString needle = value.trimmed();
    const auto catalog = modelCatalogForProvider(provider);
    for (const auto &entry : catalog) {
        if (entry.id.compare(needle, Qt::CaseInsensitive) == 0
            || entry.displayName.compare(needle, Qt::CaseInsensitive) == 0) {
            return entry;
        }
    }

    const QString alias = legacyModelAlias(provider, needle);
    if (!alias.isEmpty()) {
        for (const auto &entry : catalog) {
            if (entry.id == alias) {
                return entry;
            }
        }
    }

    return {};
}

inline QString preferredModelDisplayName(ProviderKind provider, const QString &modelId, const QString &fallbackDisplay = {})
{
    const auto entry = findModelCatalogEntry(provider, modelId);
    if (!entry.id.isEmpty()) {
        return entry.displayName;
    }
    return fallbackDisplay.trimmed().isEmpty() ? modelId.trimmed() : fallbackDisplay.trimmed();
}

inline void normalizeSeatModel(SeatConfig &seat)
{
    if (!seat.modelOverride.trimmed().isEmpty()) {
        const auto overrideEntry = findModelCatalogEntry(seat.provider, seat.modelOverride);
        if (!overrideEntry.id.isEmpty()) {
            seat.modelId = overrideEntry.id;
            seat.modelPreset = overrideEntry.displayName;
            seat.modelOverride.clear();
            return;
        }
        seat.modelId = seat.modelOverride.trimmed();
        if (seat.modelPreset.trimmed().isEmpty()) {
            seat.modelPreset = seat.modelOverride.trimmed();
        }
        return;
    }

    const ModelCatalogEntry entry = !seat.modelId.trimmed().isEmpty()
        ? findModelCatalogEntry(seat.provider, seat.modelId)
        : findModelCatalogEntry(seat.provider, seat.modelPreset);
    if (!entry.id.isEmpty()) {
        seat.modelId = entry.id;
        seat.modelPreset = entry.displayName;
        return;
    }

    if (!seat.modelId.trimmed().isEmpty() && seat.modelPreset.trimmed().isEmpty()) {
        seat.modelPreset = seat.modelId.trimmed();
    }
}

inline QString effectiveModelName(const SeatConfig &seat)
{
    if (!seat.modelOverride.trimmed().isEmpty()) {
        const auto entry = findModelCatalogEntry(seat.provider, seat.modelOverride);
        if (!entry.id.isEmpty()) {
            return entry.displayName;
        }
        return seat.modelPreset.trimmed().isEmpty() ? seat.modelOverride.trimmed() : seat.modelPreset.trimmed();
    }
    if (!seat.modelId.trimmed().isEmpty()) {
        const auto entry = findModelCatalogEntry(seat.provider, seat.modelId);
        if (!entry.id.isEmpty()) {
            return entry.displayName;
        }
    }
    if (!seat.modelPreset.trimmed().isEmpty()) {
        const auto entry = findModelCatalogEntry(seat.provider, seat.modelPreset);
        if (!entry.id.isEmpty()) {
            return entry.displayName;
        }
        return seat.modelPreset.trimmed();
    }
    return "None";
}

inline QString effectiveModelId(const SeatConfig &seat)
{
    if (!seat.modelOverride.trimmed().isEmpty()) {
        return seat.modelOverride.trimmed();
    }
    if (!seat.modelId.trimmed().isEmpty()) {
        return seat.modelId.trimmed();
    }
    const auto entry = findModelCatalogEntry(seat.provider, seat.modelPreset);
    if (!entry.id.isEmpty()) {
        return entry.id;
    }
    return seat.modelPreset.trimmed();
}

inline bool hasConcreteModelSelection(const SeatConfig &seat)
{
    return !effectiveModelId(seat).trimmed().isEmpty();
}

inline QString displaySeatName(const SeatConfig &seat, int seatIndex = -1)
{
    if (!seat.displayName.trimmed().isEmpty()) {
        return seat.displayName.trimmed();
    }
    if (seatIndex >= 0) {
        return QString("Seat %1").arg(seatIndex + 1);
    }
    return "Seat";
}

inline QString validateSeatRoleAssignments(const QVector<SeatConfig> &seats)
{
    int finalDecisionCount = 0;
    int leadPlannerCount = 0;
    int leadExecutionerCount = 0;
    int leadQualityCount = 0;

    for (const auto &seat : seats) {
        if (!seat.occupied || !seat.enabled) {
            continue;
        }
        switch (seat.role) {
        case Role::FinalDecisionMaker:
            finalDecisionCount += 1;
            break;
        case Role::LeadPlanner:
            leadPlannerCount += 1;
            break;
        case Role::LeadExecutioner:
            leadExecutionerCount += 1;
            break;
        case Role::LeadQualityControl:
            leadQualityCount += 1;
            break;
        case Role::None:
        default:
            break;
        }
    }

    if (finalDecisionCount == 0) {
        return "A final decision maker must be assigned.";
    }
    if (finalDecisionCount > 1) {
        return "Only one final decision maker can be assigned.";
    }
    if (leadPlannerCount > 1) {
        return "Only one lead planner can be assigned.";
    }
    if (leadExecutionerCount > 1) {
        return "Only one lead executioner can be assigned.";
    }
    if (leadQualityCount > 1) {
        return "Only one lead quality control checker can be assigned.";
    }
    return {};
}

inline bool seatSupportsEffort(const SeatConfig &seat)
{
    if (!seat.modelOverride.trimmed().isEmpty()) {
        return false;
    }
    const auto entry = !seat.modelId.trimmed().isEmpty()
        ? findModelCatalogEntry(seat.provider, seat.modelId)
        : findModelCatalogEntry(seat.provider, seat.modelPreset);
    return !entry.id.isEmpty() && entry.supportsEffort;
}

inline QString openAiEffortValue(ModelEffort effort)
{
    switch (effort) {
    case ModelEffort::Light: return "low";
    case ModelEffort::Balanced: return "medium";
    case ModelEffort::Deep: return "high";
    case ModelEffort::Auto:
    default:
        return {};
    }
}

inline QString geminiThinkingLevel(ModelEffort effort)
{
    switch (effort) {
    case ModelEffort::Light: return "low";
    case ModelEffort::Balanced: return "medium";
    case ModelEffort::Deep: return "high";
    case ModelEffort::Auto:
    default:
        return {};
    }
}

inline int geminiThinkingBudget(ModelEffort effort)
{
    switch (effort) {
    case ModelEffort::Light: return 1024;
    case ModelEffort::Balanced: return 4096;
    case ModelEffort::Deep: return 8192;
    case ModelEffort::Auto:
    default:
        return 0;
    }
}

inline int anthropicThinkingBudget(ModelEffort effort)
{
    switch (effort) {
    case ModelEffort::Light: return 1024;
    case ModelEffort::Balanced: return 4096;
    case ModelEffort::Deep: return 8192;
    case ModelEffort::Auto:
    default:
        return 0;
    }
}

inline QString findFinalDecisionMakerSeatId(const QVector<SeatConfig> &seats)
{
    for (const auto &seat : seats) {
        if (seat.occupied && seat.role == Role::FinalDecisionMaker) {
            return seat.seatId;
        }
    }
    return {};
}

inline QStringList modelsForProvider(ProviderKind provider)
{
    QStringList models;
    for (const auto &entry : modelCatalogForProvider(provider)) {
        models.append(entry.displayName);
    }
    return models;
}

inline bool hasPendingSeatChanges(const SessionState &state)
{
    return !state.pendingSeats.isEmpty();
}

inline QStringList activeSeatIdsForPhase(const SessionState &state)
{
    QStringList ids;
    for (const auto &seat : state.seats) {
        if (seat.occupied && seat.enabled && seat.role != Role::FinalDecisionMaker) {
            ids.append(seat.seatId);
        }
    }
    return ids;
}

inline QJsonObject seatToJson(const SeatConfig &seat)
{
    return {
        {"seatId", seat.seatId},
        {"displayName", seat.displayName},
        {"provider", providerKindToString(seat.provider)},
        {"modelId", seat.modelId},
        {"modelPreset", seat.modelPreset},
        {"modelOverride", seat.modelOverride},
        {"effort", toString(seat.effort)},
        {"role", toString(seat.role)},
        {"color", seat.color},
        {"occupied", seat.occupied},
        {"enabled", seat.enabled}
    };
}

inline QJsonObject attachmentToJson(const AttachmentRecord &attachment)
{
    return {
        {"attachmentId", attachment.attachmentId},
        {"displayName", attachment.displayName},
        {"filePath", attachment.filePath},
        {"fileHash", attachment.fileHash},
        {"addedAt", attachment.addedAt.toUTC().toString(Qt::ISODate)},
        {"providerHandles", attachment.providerHandles}
    };
}

inline AttachmentRecord attachmentFromJson(const QJsonObject &object)
{
    AttachmentRecord attachment;
    attachment.attachmentId = object.value("attachmentId").toString();
    attachment.displayName = object.value("displayName").toString();
    attachment.filePath = object.value("filePath").toString();
    attachment.fileHash = object.value("fileHash").toString();
    attachment.addedAt = QDateTime::fromString(object.value("addedAt").toString(), Qt::ISODate);
    attachment.providerHandles = object.value("providerHandles").toObject();
    return attachment;
}

inline SeatConfig seatFromJson(const QJsonObject &object)
{
    SeatConfig seat;
    seat.seatId = object.value("seatId").toString();
    seat.displayName = object.value("displayName").toString();
    seat.modelId = object.value("modelId").toString();
    seat.modelPreset = object.value("modelPreset").toString();
    seat.modelOverride = object.value("modelOverride").toString();
    seat.effort = effortFromString(object.value("effort").toString());
    seat.color = object.value("color").toString();
    seat.occupied = object.value("occupied").toBool();
    seat.enabled = object.value("enabled").toBool(true);
    seat.provider = providerKindFromString(object.value("provider").toString());
    const QString role = object.value("role").toString();
    seat.role = role == "Final Decision Maker" ? Role::FinalDecisionMaker
        : role == "Lead Planner" ? Role::LeadPlanner
        : role == "Lead Executioner" ? Role::LeadExecutioner
        : role == "Lead Quality Control" ? Role::LeadQualityControl
        : Role::None;
    normalizeSeatModel(seat);
    return seat;
}

inline QJsonArray seatsToJson(const QVector<SeatConfig> &seats)
{
    QJsonArray array;
    for (const auto &seat : seats) {
        array.append(seatToJson(seat));
    }
    return array;
}

inline QJsonArray attachmentsToJson(const QVector<AttachmentRecord> &attachments)
{
    QJsonArray array;
    for (const auto &attachment : attachments) {
        array.append(attachmentToJson(attachment));
    }
    return array;
}

inline QJsonObject budgetPolicyToJson(const BudgetPolicy &policy)
{
    return {
        {"maxTokensPerPhase", policy.maxTokensPerPhase},
        {"maxTotalTokens", policy.maxTotalTokens},
        {"maxTotalCost", policy.maxTotalCost},
        {"maxRounds", policy.maxRounds},
        {"maxExecQcLoops", policy.maxExecQcLoops},
        {"maxPhaseSeconds", policy.maxPhaseSeconds},
        {"maxSessionSeconds", policy.maxSessionSeconds}
    };
}

inline QJsonObject stopPolicyToJson(const StopPolicy &policy)
{
    return {
        {"allowEarlyStopByDecisionMaker", policy.allowEarlyStopByDecisionMaker},
        {"stopOnBudgetExceeded", policy.stopOnBudgetExceeded},
        {"stopOnSessionTimeout", policy.stopOnSessionTimeout},
        {"stopOnPhaseTimeout", policy.stopOnPhaseTimeout}
    };
}

inline QJsonObject seatUsageToJson(const SeatUsageTally &usage)
{
    return {
        {"seatId", usage.seatId},
        {"totalTokens", usage.totalTokens},
        {"totalCost", usage.totalCost},
        {"phaseTokens", usage.phaseTokens},
        {"phaseCost", usage.phaseCost}
    };
}

inline SeatUsageTally seatUsageFromJson(const QJsonObject &object)
{
    SeatUsageTally usage;
    usage.seatId = object.value("seatId").toString();
    usage.totalTokens = object.value("totalTokens").toInt();
    usage.totalCost = object.value("totalCost").toDouble();
    usage.phaseTokens = object.value("phaseTokens").toInt();
    usage.phaseCost = object.value("phaseCost").toDouble();
    return usage;
}

inline QJsonArray seatUsageToJson(const QVector<SeatUsageTally> &usageRows)
{
    QJsonArray array;
    for (const auto &usage : usageRows) {
        array.append(seatUsageToJson(usage));
    }
    return array;
}

inline BudgetPolicy budgetPolicyFromJson(const QJsonObject &object)
{
    BudgetPolicy policy;
    if (object.isEmpty()) {
        return policy;
    }
    policy.maxTokensPerPhase = object.value("maxTokensPerPhase").toInt(policy.maxTokensPerPhase);
    policy.maxTotalTokens = object.value("maxTotalTokens").toInt(policy.maxTotalTokens);
    policy.maxTotalCost = object.value("maxTotalCost").toDouble(policy.maxTotalCost);
    policy.maxRounds = object.value("maxRounds").toInt(policy.maxRounds);
    policy.maxExecQcLoops = object.value("maxExecQcLoops").toInt(policy.maxExecQcLoops);
    policy.maxPhaseSeconds = object.value("maxPhaseSeconds").toInt(policy.maxPhaseSeconds);
    policy.maxSessionSeconds = object.value("maxSessionSeconds").toInt(policy.maxSessionSeconds);
    return policy;
}

inline StopPolicy stopPolicyFromJson(const QJsonObject &object)
{
    StopPolicy policy;
    if (object.isEmpty()) {
        return policy;
    }
    policy.allowEarlyStopByDecisionMaker = object.value("allowEarlyStopByDecisionMaker").toBool(policy.allowEarlyStopByDecisionMaker);
    policy.stopOnBudgetExceeded = object.value("stopOnBudgetExceeded").toBool(policy.stopOnBudgetExceeded);
    policy.stopOnSessionTimeout = object.value("stopOnSessionTimeout").toBool(policy.stopOnSessionTimeout);
    policy.stopOnPhaseTimeout = object.value("stopOnPhaseTimeout").toBool(policy.stopOnPhaseTimeout);
    return policy;
}

inline bool isRunningPhase(Phase phase)
{
    return phase != Phase::Idle
        && phase != Phase::Paused
        && phase != Phase::Completed
        && phase != Phase::Stopped
        && phase != Phase::Failed;
}

inline QString prettyJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

} // namespace amt

Q_DECLARE_METATYPE(amt::WorkflowEvent)
Q_DECLARE_METATYPE(amt::SessionState)
