#include "app/mobile_app_controller.h"

#include <algorithm>
#include <cmath>

#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include "core/logging.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace amt {

namespace {

QString formatElapsed(int totalSeconds)
{
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString phaseBadge(const SessionState &state)
{
    if (state.continuationPending) {
        return "Needs continuation";
    }
    if (state.paused || state.phase == Phase::Paused) {
        return "Paused";
    }
    return toString(state.phase);
}

bool hasUserMessage(const SessionState &state)
{
    for (const auto &entry : state.transcript) {
        if (entry.isUser && !entry.content.trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}

QString attachmentImportRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/attachments";
}

QString logEventTypeLabel(LogEventType type)
{
    switch (type) {
    case LogEventType::SessionStarted: return "Session";
    case LogEventType::UserMessageAdded: return "User";
    case LogEventType::PhaseStarted: return "Phase";
    case LogEventType::TurnStarted: return "Turn";
    case LogEventType::AISpoke: return "AI";
    case LogEventType::AISkipped: return "Skip";
    case LogEventType::ProviderCallFailed: return "Provider Error";
    case LogEventType::RetryScheduled: return "Retry";
    case LogEventType::PhaseEnded: return "Phase";
    case LogEventType::FinalDecisionMade: return "Decision";
    case LogEventType::SessionStopped: return "Stopped";
    case LogEventType::LimitReached: return "Limit";
    }
    return "Log";
}

QString sanitizedFileName(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        value = "attachment";
    }
    static const QRegularExpression invalid(R"([\\/:*?"<>|])");
    value.replace(invalid, "_");
    return value;
}

} // namespace

MobileAppController::MobileAppController(QObject *parent)
    : QObject(parent)
{
    connect(m_context.sessionRunner(), &SessionRunner::sessionStateChanged, this, [this](const SessionState &state) {
        notifyStateChange(state, true);
        schedulePersistence(state.tableId);
    });
    connect(m_context.sessionRunner(), &SessionRunner::continuationRequested, this, [this](const QString &tableId, const QString &reason, int) {
        if (tableId == m_currentTableId) {
            emit continuationRequested(reason);
            emit stateChanged();
        }
    });
    connect(m_context.modelCatalogManager(), &ModelCatalogManager::fetchCompleted, this, [this]() {
        emit settingsChanged();
    });
}

bool MobileAppController::initialized() const
{
    return m_initialized;
}

bool MobileAppController::running() const
{
    const auto *state = currentState();
    return state && (isRunningPhase(state->phase) || state->waitingForNextTurn) && !state->paused;
}

QString MobileAppController::currentTableId() const
{
    return m_currentTableId;
}

bool MobileAppController::initialize()
{
    if (m_initialized) {
        return true;
    }
    if (!m_context.initialize()) {
        setError("Failed to initialize local storage.");
        return false;
    }
    QSettings settings;
    m_currentTableId = settings.value("mobile/currentTableId").toString();
    qCDebug(diagnosticsLog).noquote()
        << QString("Persistence restore: saved selection exists=%1")
               .arg((!m_currentTableId.isEmpty() && m_context.tableHandle(m_currentTableId)) ? "true" : "false");
    selectFirstTableIfNeeded();
    for (const auto &table : m_context.tables()) {
        if (table) {
            m_uiSnapshots.insert(table->tableId, uiSnapshot(*table));
        }
    }
    m_initialized = true;
    if (const auto *state = currentState()) {
        qCDebug(diagnosticsLog).noquote() << QString("Persistence restore: selected transcript=%1 artifacts=%2 logs=%3")
                                 .arg(QString::number(state->transcript.size()),
                                      QString::number(state->artifacts.size()),
                                      QString::number(state->log.size()));
    } else {
        qWarning().noquote() << "Persistence restore: no current table selected";
    }
    emit initializedChanged();
    emit tablesChanged();
    emit stateChanged();
    emit seatsChanged();
    emit transcriptChanged();
    emit artifactsChanged();
    emit logsChanged();
    return true;
}

QVariantList MobileAppController::tables() const
{
    QVariantList rows;
    QVector<const SessionState *> sorted;
    for (const auto &table : m_context.tables()) {
        if (table) {
            sorted.append(table.get());
        }
    }
    std::sort(sorted.begin(), sorted.end(), [](const SessionState *lhs, const SessionState *rhs) {
        if (lhs->pinned != rhs->pinned) {
            return lhs->pinned && !rhs->pinned;
        }
        if (lhs->updatedAt != rhs->updatedAt) {
            return lhs->updatedAt > rhs->updatedAt;
        }
        return lhs->title.toLower() < rhs->title.toLower();
    });
    for (const auto *state : sorted) {
        rows.append(tableSummary(*state));
    }
    return rows;
}

QVariantMap MobileAppController::currentTable() const
{
    const auto *state = currentState();
    return state ? tableSummary(*state) : QVariantMap{};
}

QVariantList MobileAppController::seats() const
{
    QVariantList rows;
    const auto *state = currentState();
    if (!state) {
        return rows;
    }
    QVector<SeatConfig> source = hasPendingSeatChanges(*state) ? state->pendingSeats : state->seats;
    while (source.size() < 8) {
        SeatConfig seat;
        seat.seatId = QString("seat-%1").arg(source.size() + 1);
        source.append(seat);
    }
    for (int i = 0; i < source.size(); ++i) {
        rows.append(seatSummary(source.at(i), i));
    }
    return rows;
}

QVariantList MobileAppController::transcript() const
{
    QVariantList rows;
    const auto *state = currentState();
    if (!state) {
        return rows;
    }
    for (const auto &entry : state->transcript) {
        rows.append(transcriptSummary(entry));
    }
    return rows;
}

QString MobileAppController::fullTranscriptText() const
{
    const auto *state = currentState();
    if (!state || state->transcript.isEmpty()) {
        return {};
    }

    QStringList entries;
    entries.reserve(state->transcript.size());
    for (const auto &entry : state->transcript) {
        const QString speaker = entry.isUser ? QStringLiteral("You") : entry.speakerName;
        entries.append(QStringLiteral("[%1] %2 | %3 | Round %4\n%5")
                           .arg(entry.timestamp.toLocalTime().toString("HH:mm:ss"),
                                speaker,
                                toString(entry.phase),
                                QString::number(entry.round),
                                entry.content));
    }
    return entries.join("\n\n");
}

bool MobileAppController::copyFullTranscript()
{
    const QString text = fullTranscriptText();
    if (text.isEmpty()) {
        setError("There is no transcript to copy.");
        return false;
    }
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        setError("The system clipboard is unavailable.");
        return false;
    }
    clipboard->setText(text, QClipboard::Clipboard);
    setError({});
    return true;
}

QVariantList MobileAppController::artifacts() const
{
    QVariantList rows;
    const auto *state = currentState();
    if (!state) {
        return rows;
    }
    for (const auto &artifact : state->artifacts) {
        rows.append(artifactSummary(artifact));
    }
    return rows;
}

QString MobileAppController::artifactContent(const QString &versionId) const
{
    const auto *state = currentState();
    if (!state) {
        return {};
    }

    const auto it = std::find_if(state->artifacts.cbegin(), state->artifacts.cend(), [&versionId](const ArtifactVersion &artifact) {
        return artifact.versionId == versionId;
    });
    if (it == state->artifacts.cend()) {
        return {};
    }

    QFile file(it->filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    constexpr qint64 maxArtifactPreviewBytes = 65536;
    QString content = QString::fromUtf8(file.read(maxArtifactPreviewBytes));
    if (file.size() > maxArtifactPreviewBytes) {
        content += QString("\n\n[Artifact preview truncated. Full artifact is %1 bytes.]").arg(file.size());
    }
    return content;
}

QVariantList MobileAppController::logs() const
{
    QVariantList rows;
    const auto *state = currentState();
    if (!state) {
        return rows;
    }
    for (const auto &event : state->log) {
        rows.append(logSummary(event));
    }
    return rows;
}

QVariantList MobileAppController::modelsForProvider(int providerIndex) const
{
    QVariantList rows;
    for (const auto &entry : m_context.modelCatalogManager()->catalogForProvider(providerFromIndex(providerIndex))) {
        QVariantMap row;
        row.insert("id", entry.id);
        row.insert("displayName", entry.displayName);
        row.insert("supportsEffort", entry.supportsEffort);
        row.insert("preview", entry.isPreview);
        rows.append(row);
    }
    return rows;
}

QVariantList MobileAppController::modelRefreshStatuses() const
{
    return m_context.modelCatalogManager()->fetchStatuses();
}

QString MobileAppController::apiKey(int providerIndex) const
{
    return m_context.credentialStore()->loadApiKey(providerFromIndex(providerIndex));
}

QString MobileAppController::apiKeyStatus(int providerIndex) const
{
    const QString key = apiKey(providerIndex).trimmed();
    if (key.isEmpty()) {
        return "No key saved";
    }
    if (key.size() <= 8) {
        return "Saved key";
    }
    return QString("Saved: %1...%2").arg(key.left(4), key.right(4));
}

QString MobileAppController::lastError() const
{
    return m_lastError;
}

void MobileAppController::selectTable(const QString &tableId)
{
    if (!m_context.tableHandle(tableId)) {
        return;
    }
    m_currentTableId = tableId;
    QSettings settings;
    settings.setValue("mobile/currentTableId", m_currentTableId);
    settings.sync();
    if (const auto *state = currentState()) {
        m_uiSnapshots.insert(state->tableId, uiSnapshot(*state));
    }
    emit stateChanged();
    emit tablesChanged();
    emit seatsChanged();
    emit transcriptChanged();
    emit artifactsChanged();
    emit logsChanged();
}

bool MobileAppController::createTable(const QString &title, int seatCount)
{
    SessionState state;
    state.tableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    state.title = title.trimmed().isEmpty() ? "New Meeting Table" : title.trimmed();
    state.updatedAt = QDateTime::currentDateTimeUtc();
    state.phase = Phase::Idle;
    state.round = 1;
    state.logVisible = false;
    const int boundedSeatCount = qBound(1, seatCount, 8);
    for (int i = 0; i < boundedSeatCount; ++i) {
        SeatConfig seat;
        seat.seatId = QString("seat-%1").arg(i + 1);
        seat.displayName = QString("Seat %1").arg(i + 1);
        seat.occupied = false;
        seat.enabled = false;
        state.seats.append(seat);
    }
    m_context.applyEffectiveBudgetPolicy(state);
    if (!m_context.save(state)) {
        setError("The table could not be created.");
        return false;
    }
    selectTable(state.tableId);
    return true;
}

bool MobileAppController::duplicateCurrentTable()
{
    auto *state = currentState();
    if (!state) {
        setError("No table is selected.");
        return false;
    }
    SessionState copy = *state;
    copy.tableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.title = state->title + " Copy";
    copy.updatedAt = QDateTime::currentDateTimeUtc();
    copy.phase = Phase::Idle;
    copy.round = 1;
    copy.execQcLoopCount = 0;
    copy.elapsedSeconds = 0;
    copy.phaseElapsedSeconds = 0;
    copy.usedTokens = 0;
    copy.usedCost = 0.0;
    copy.phaseUsedTokens = 0;
    copy.phaseUsedCost = 0.0;
    copy.activeSeatId.clear();
    copy.transcript.clear();
    copy.log.clear();
    copy.artifacts.clear();
    copy.attachments.clear();
    copy.queuedInputIds.clear();
    copy.currentArtifactVersionId.clear();
    copy.waitingForNextTurn = false;
    copy.paused = false;
    copy.pauseRequested = false;
    copy.continuationPending = false;
    if (!m_context.save(copy)) {
        setError("The table copy could not be saved.");
        return false;
    }
    selectTable(copy.tableId);
    return true;
}

bool MobileAppController::renameCurrentTable(const QString &title)
{
    auto *state = currentState();
    const QString trimmed = title.trimmed();
    if (!state || trimmed.isEmpty()) {
        setError("A table name is required.");
        return false;
    }
    SessionState candidate = *state;
    candidate.title = trimmed;
    return saveAndNotify(candidate, true);
}

bool MobileAppController::deleteCurrentTable()
{
    const QString tableId = m_currentTableId;
    if (tableId.isEmpty()) {
        return false;
    }
    m_context.sessionRunner()->discardSession(tableId);
    if (!m_context.removeTable(tableId)) {
        setError("The table could not be deleted.");
        return false;
    }
    m_uiSnapshots.remove(tableId);
    m_pendingSaveIds.remove(tableId);
    m_currentTableId.clear();
    selectFirstTableIfNeeded();
    emit tablesChanged();
    emit stateChanged();
    emit seatsChanged();
    emit transcriptChanged();
    emit artifactsChanged();
    emit logsChanged();
    return true;
}

bool MobileAppController::togglePinCurrentTable()
{
    auto *state = currentState();
    if (!state) {
        return false;
    }
    SessionState candidate = *state;
    candidate.pinned = !candidate.pinned;
    return saveAndNotify(candidate, true);
}

bool MobileAppController::saveSeat(int seatIndex,
                                   bool occupied,
                                   const QString &displayName,
                                   int providerIndex,
                                   const QString &modelId,
                                   int effortIndex,
                                   int roleIndex)
{
    auto *state = currentState();
    if (!state || seatIndex < 0 || seatIndex >= 8) {
        setError("Invalid seat.");
        return false;
    }
    SessionState candidate = *state;
    QVector<SeatConfig> targetSeats = hasPendingSeatChanges(candidate) ? candidate.pendingSeats : candidate.seats;
    while (targetSeats.size() < 8) {
        SeatConfig seat;
        seat.seatId = QString("seat-%1").arg(targetSeats.size() + 1);
        seat.displayName = QString("Seat %1").arg(targetSeats.size() + 1);
        targetSeats.append(seat);
    }
    SeatConfig seat;
    seat.seatId = QString("seat-%1").arg(seatIndex + 1);
    seat.displayName = displayName.trimmed().isEmpty() ? QString("Seat %1").arg(seatIndex + 1) : displayName.trimmed();
    seat.occupied = occupied;
    seat.enabled = occupied;
    seat.provider = providerFromIndex(providerIndex);
    seat.modelId = modelId;
    seat.modelPreset = preferredModelDisplayName(seat.provider, modelId, modelId);
    seat.effort = effortFromEditorIndex(effortIndex);
    seat.role = roleFromEditorIndex(roleIndex);
    if (!occupied) {
        seat.role = Role::None;
        seat.effort = ModelEffort::Auto;
    }
    normalizeSeatModel(seat);
    targetSeats[seatIndex] = seat;
    const QString roleError = validateSeatRoleAssignments(targetSeats);
    const bool anyOccupied = std::any_of(targetSeats.cbegin(), targetSeats.cend(), [](const SeatConfig &item) {
        return item.occupied && item.enabled;
    });
    if (anyOccupied && !roleError.isEmpty()) {
        setError(roleError);
        return false;
    }
    if (isRunningPhase(candidate.phase)) {
        candidate.pendingSeats = targetSeats;
    } else {
        candidate.seats = targetSeats;
        candidate.pendingSeats.clear();
        candidate.finalDecisionMakerSeatId = findFinalDecisionMakerSeatId(candidate.seats);
    }
    return saveAndNotify(candidate);
}

bool MobileAppController::sendMessage(const QString &message)
{
    auto *state = currentState();
    const QString trimmed = message.trimmed();
    if (!state || trimmed.isEmpty()) {
        setError("Enter a message first.");
        return false;
    }
    SessionState candidate = *state;
    TranscriptEntry entry;
    entry.entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.tableId = candidate.tableId;
    entry.phase = candidate.phase;
    entry.round = candidate.round;
    entry.speakerSeatId = "user";
    entry.speakerName = "You";
    entry.isUser = true;
    entry.content = trimmed;
    entry.timestamp = QDateTime::currentDateTimeUtc();
    candidate.transcript.append(entry);
    if (isRunningPhase(candidate.phase) || candidate.waitingForNextTurn) {
        candidate.queuedInputIds.append(entry.entryId);
    }
    LogEvent log;
    log.logId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    log.tableId = candidate.tableId;
    log.type = LogEventType::UserMessageAdded;
    log.actorName = "You";
    log.phase = candidate.phase;
    log.round = candidate.round;
    log.timestamp = QDateTime::currentDateTimeUtc();
    log.summary = "User added new instructions.";
    candidate.log.append(log);
    return saveAndNotify(candidate);
}

bool MobileAppController::runOrResume()
{
    auto *state = currentState();
    if (!state) {
        return false;
    }
    if (state->continuationPending) {
        m_context.sessionRunner()->grantContinuation(*state, static_cast<BudgetLimitKind>(state->continuationLimitKind));
        m_context.sessionRunner()->resumeSession(*state);
        return true;
    }
    if (state->paused || state->phase == Phase::Paused) {
        m_context.sessionRunner()->resumeSession(*state);
        return true;
    }
    if (!validateRunnable(*state)) {
        return false;
    }
    m_context.sessionRunner()->startSession(*state);
    return true;
}

bool MobileAppController::pauseSession()
{
    auto *state = currentState();
    if (!state) {
        return false;
    }
    m_context.sessionRunner()->requestPause(*state);
    return true;
}

bool MobileAppController::stopSession()
{
    auto *state = currentState();
    if (!state) {
        return false;
    }
    m_context.sessionRunner()->stopSession(*state, "Stopped from Android app.");
    return true;
}

bool MobileAppController::addAttachment(const QUrl &url)
{
    auto *state = currentState();
    if (!state) {
        return false;
    }
    QString error;
    const QString filePath = importAttachmentToPrivateStorage(url, &error);
    if (filePath.isEmpty()) {
        setError(error.isEmpty() ? "Attachment import failed." : error);
        return false;
    }
    AttachmentRecord attachment = m_context.uploadManager()->createAttachment(filePath, &error);
    if (attachment.attachmentId.isEmpty()) {
        m_context.cleanupAttachmentFileIfUnreferenced(filePath);
        setError(error);
        return false;
    }
    SessionState candidate = *state;
    candidate.attachments.append(attachment);
    if (isRunningPhase(candidate.phase) || candidate.waitingForNextTurn) {
        candidate.queuedInputIds.append(attachment.attachmentId);
    }
    if (!saveAndNotify(candidate)) {
        m_context.cleanupAttachmentFileIfUnreferenced(filePath);
        return false;
    }
    return true;
}

bool MobileAppController::removeAttachment(const QString &attachmentId)
{
    auto *state = currentState();
    if (!state) {
        return false;
    }
    SessionState candidate = *state;
    QString removedFilePath;
    const auto newEnd = std::remove_if(candidate.attachments.begin(), candidate.attachments.end(), [&](const AttachmentRecord &attachment) {
        if (attachment.attachmentId == attachmentId) {
            removedFilePath = attachment.filePath;
            return true;
        }
        return attachment.attachmentId == attachmentId;
    });
    if (newEnd == candidate.attachments.end()) {
        return false;
    }
    candidate.attachments.erase(newEnd, candidate.attachments.end());
    candidate.queuedInputIds.removeAll(attachmentId);
    if (!saveAndNotify(candidate)) {
        return false;
    }
    m_context.cleanupAttachmentFileIfUnreferenced(removedFilePath);
    return true;
}

bool MobileAppController::saveApiKey(int providerIndex, const QString &apiKey)
{
    QString error;
    if (!m_context.credentialStore()->saveApiKey(providerFromIndex(providerIndex), apiKey, &error, true)) {
        setError(error);
        return false;
    }
    emit settingsChanged();
    return true;
}

void MobileAppController::refreshModels()
{
    m_context.modelCatalogManager()->fetchModelsAsync();
}

void MobileAppController::setTheme(const QString &theme)
{
    m_context.appSettings().theme = themeModeFromString(theme);
    m_context.saveAppSettings();
    emit settingsChanged();
}

bool MobileAppController::saveGlobalBudget(int maxTokensPerPhase,
                                           int maxTotalTokens,
                                           double maxTotalCost,
                                           int maxRounds,
                                           int maxExecQcLoops,
                                           int maxPhaseSeconds,
                                           int maxSessionSeconds)
{
    if (maxTokensPerPhase <= 0
        || maxTotalTokens <= 0
        || !std::isfinite(maxTotalCost)
        || maxTotalCost <= 0.0
        || maxRounds <= 0
        || maxExecQcLoops <= 0
        || maxPhaseSeconds <= 0
        || maxSessionSeconds <= 0) {
        setError("All hard-stop limits must be positive values.");
        return false;
    }
    if (maxTotalTokens < maxTokensPerPhase) {
        setError("Maximum total tokens must be at least the per-phase token limit.");
        return false;
    }
    if (maxSessionSeconds < maxPhaseSeconds) {
        setError("Maximum session seconds must be at least the per-phase time limit.");
        return false;
    }

    BudgetPolicy policy;
    policy.maxTokensPerPhase = maxTokensPerPhase;
    policy.maxTotalTokens = maxTotalTokens;
    policy.maxTotalCost = maxTotalCost;
    policy.maxRounds = maxRounds;
    policy.maxExecQcLoops = maxExecQcLoops;
    policy.maxPhaseSeconds = maxPhaseSeconds;
    policy.maxSessionSeconds = maxSessionSeconds;

    QVector<SessionState> originals;
    QVector<SessionState> candidates;
    for (const auto &table : m_context.tables()) {
        if (table && !table->useBudgetOverrides) {
            originals.append(*table);
            SessionState candidate = *table;
            candidate.budgetPolicy = policy;
            candidates.append(candidate);
        }
    }
    for (int i = 0; i < candidates.size(); ++i) {
        if (m_context.save(candidates.at(i))) {
            continue;
        }
        for (int rollbackIndex = 0; rollbackIndex < i; ++rollbackIndex) {
            m_context.save(originals.at(rollbackIndex));
        }
        setError("The hard-stop settings could not be saved to every table.");
        return false;
    }

    auto &settings = m_context.appSettings();
    settings.globalBudgetDefaults = policy;
    m_context.saveAppSettings();
    emit settingsChanged();
    emit stateChanged();
    return true;
}

bool MobileAppController::flushCurrentSession()
{
    auto *state = currentState();
    if (!state) {
        qWarning().noquote() << "Persistence flush skipped: no current table";
        return false;
    }
    m_pendingSaveIds.remove(state->tableId);
    const bool saved = m_context.saveExisting(state->tableId);
    QSettings settings;
    settings.setValue("mobile/currentTableId", m_currentTableId);
    settings.sync();
    qCDebug(diagnosticsLog).noquote() << QString("Persistence flush: saved=%1 transcript=%2 artifacts=%3 logs=%4")
                             .arg(saved ? "true" : "false",
                                  QString::number(state->transcript.size()),
                                  QString::number(state->artifacts.size()),
                                  QString::number(state->log.size()));
    return saved;
}

SessionState *MobileAppController::currentState() const
{
    const auto handle = currentHandle();
    return handle ? handle.get() : nullptr;
}

ApplicationContext::SessionHandle MobileAppController::currentHandle() const
{
    return m_currentTableId.isEmpty() ? nullptr : m_context.tableHandle(m_currentTableId);
}

void MobileAppController::selectFirstTableIfNeeded()
{
    if (!m_currentTableId.isEmpty() && m_context.tableHandle(m_currentTableId)) {
        return;
    }

    ApplicationContext::SessionHandle newestWithContent;
    ApplicationContext::SessionHandle newest;
    for (const auto &table : m_context.tables()) {
        if (!table) {
            continue;
        }
        const bool hasContent = !table->transcript.isEmpty() || !table->artifacts.isEmpty() || !table->log.isEmpty();
        if (hasContent && (!newestWithContent || table->updatedAt > newestWithContent->updatedAt)) {
            newestWithContent = table;
        }
        if (!newest || table->updatedAt > newest->updatedAt) {
            newest = table;
        }
    }

    const auto selected = newestWithContent ? newestWithContent : newest;
    if (selected) {
        m_currentTableId = selected->tableId;
        QSettings settings;
        settings.setValue("mobile/currentTableId", m_currentTableId);
        settings.sync();
        qCDebug(diagnosticsLog).noquote() << QString("Persistence restore: selected fallback reason=%1 transcript=%2 artifacts=%3 logs=%4")
                                 .arg(newestWithContent ? "newest with content" : "newest",
                                      QString::number(selected->transcript.size()),
                                      QString::number(selected->artifacts.size()),
                                      QString::number(selected->log.size()));
    }
}

bool MobileAppController::saveAndNotify(const SessionState &state, bool tableListChanged)
{
    SessionState candidate = state;
    m_context.applyEffectiveBudgetPolicy(candidate);
    if (!m_context.save(candidate)) {
        setError("The current table could not be saved.");
        return false;
    }
    const auto persisted = m_context.tableHandle(candidate.tableId);
    if (persisted) {
        notifyStateChange(*persisted, tableListChanged);
    }
    return true;
}

void MobileAppController::schedulePersistence(const QString &tableId)
{
    if (tableId.isEmpty()) {
        return;
    }
    m_pendingSaveIds.insert(tableId);
    if (m_persistenceScheduled) {
        return;
    }

    m_persistenceScheduled = true;
    QTimer::singleShot(0, this, [this]() { persistScheduledSessions(); });
}

void MobileAppController::persistScheduledSessions()
{
    const QSet<QString> pending = m_pendingSaveIds;
    m_pendingSaveIds.clear();
    m_persistenceScheduled = false;
    for (const auto &tableId : pending) {
        if (!m_context.saveExisting(tableId) && tableId == m_currentTableId) {
            setError("The current table could not be saved.");
        }
    }
}

void MobileAppController::notifyStateChange(const SessionState &state, bool tableListChanged)
{
    const UiSnapshot previous = m_uiSnapshots.value(state.tableId);
    const UiSnapshot current = uiSnapshot(state);
    m_uiSnapshots.insert(state.tableId, current);

    if (tableListChanged) {
        emit tablesChanged();
    }
    if (state.tableId != m_currentTableId) {
        return;
    }

    emit stateChanged();
    if (previous.activeSeatId != current.activeSeatId
        || previous.seatConfiguration != current.seatConfiguration) {
        emit seatsChanged();
    }
    if (previous.transcriptCount != current.transcriptCount) {
        emit transcriptChanged();
    }
    if (previous.artifactCount != current.artifactCount) {
        emit artifactsChanged();
    }
    if (previous.logCount != current.logCount) {
        emit logsChanged();
    }
}

MobileAppController::UiSnapshot MobileAppController::uiSnapshot(const SessionState &state) const
{
    UiSnapshot snapshot;
    snapshot.transcriptCount = state.transcript.size();
    snapshot.artifactCount = state.artifacts.size();
    snapshot.logCount = state.log.size();
    snapshot.activeSeatId = state.activeSeatId;
    snapshot.seatConfiguration = QJsonDocument(seatsToJson(state.seats)).toJson(QJsonDocument::Compact);
    snapshot.seatConfiguration.append('\0');
    snapshot.seatConfiguration.append(QJsonDocument(seatsToJson(state.pendingSeats)).toJson(QJsonDocument::Compact));
    return snapshot;
}

bool MobileAppController::validateRunnable(const SessionState &state)
{
    int participantCount = 0;
    for (const auto &seat : state.seats) {
        if (!seat.occupied || !seat.enabled) {
            continue;
        }
        if (!hasConcreteModelSelection(seat)) {
            setError(QString("%1 needs a model selection.").arg(displaySeatName(seat)));
            return false;
        }
        if (seat.role != Role::FinalDecisionMaker) {
            participantCount += 1;
        }
    }
    const QString roleError = validateSeatRoleAssignments(state.seats);
    if (!roleError.isEmpty()) {
        setError(roleError);
        return false;
    }
    if (participantCount == 0) {
        setError("At least one non-final participant is required.");
        return false;
    }
    if (!hasUserMessage(state)) {
        setError("Send a user message before running the session.");
        return false;
    }
    return true;
}

ProviderKind MobileAppController::providerFromIndex(int providerIndex) const
{
    return providerKindFromIndex(providerIndex);
}

QVariantMap MobileAppController::tableSummary(const SessionState &state) const
{
    QVariantMap row;
    row.insert("tableId", state.tableId);
    row.insert("title", state.title);
    row.insert("pinned", state.pinned);
    row.insert("phase", phaseBadge(state));
    row.insert("round", state.round);
    row.insert("activeSeatId", state.activeSeatId);
    row.insert("usedTokens", state.usedTokens);
    row.insert("maxTokens", state.budgetPolicy.maxTotalTokens);
    row.insert("usedCost", state.usedCost);
    row.insert("maxCost", state.budgetPolicy.maxTotalCost);
    row.insert("elapsed", formatElapsed(state.elapsedSeconds));
    row.insert("transcriptCount", state.transcript.size());
    row.insert("attachmentCount", state.attachments.size());
    row.insert("artifactCount", state.artifacts.size());
    row.insert("updatedAt", state.updatedAt.toLocalTime().toString("yyyy-MM-dd hh:mm"));
    row.insert("selected", state.tableId == m_currentTableId);
    return row;
}

QVariantMap MobileAppController::seatSummary(const SeatConfig &seat, int index) const
{
    const auto *state = currentState();
    QVariantMap row;
    row.insert("seatId", seat.seatId);
    row.insert("index", index);
    row.insert("displayName", displaySeatName(seat, index));
    row.insert("providerIndex", indexFromProviderKind(seat.provider));
    row.insert("provider", toString(seat.provider));
    row.insert("modelId", effectiveModelId(seat));
    row.insert("model", effectiveModelName(seat));
    row.insert("effortIndex", indexFromEffort(seat.effort));
    row.insert("effort", toString(seat.effort));
    row.insert("roleIndex", indexFromRole(seat.role));
    row.insert("role", displaySeatRole(seat.role));
    row.insert("occupied", seat.occupied);
    row.insert("enabled", seat.enabled);
    row.insert("active", state && state->activeSeatId == seat.seatId);
    row.insert("decisionMaker", seat.role == Role::FinalDecisionMaker);
    row.insert("pending", state && hasPendingSeatChanges(*state));
    return row;
}

QVariantMap MobileAppController::transcriptSummary(const TranscriptEntry &entry) const
{
    QVariantMap row;
    row.insert("entryId", entry.entryId);
    row.insert("speaker", entry.isUser ? "You" : entry.speakerName);
    row.insert("content", entry.content);
    row.insert("phase", toString(entry.phase));
    row.insert("round", entry.round);
    row.insert("isUser", entry.isUser);
    row.insert("isDecision", entry.isDecision);
    row.insert("timestamp", entry.timestamp.toLocalTime().toString("HH:mm:ss"));
    return row;
}

QVariantMap MobileAppController::artifactSummary(const ArtifactVersion &artifact) const
{
    QVariantMap row;
    row.insert("versionId", artifact.versionId);
    row.insert("summary", artifact.summary);
    row.insert("phase", toString(artifact.createdByPhase));
    row.insert("round", artifact.createdByRound);
    row.insert("createdAt", artifact.createdAt.toLocalTime().toString("yyyy-MM-dd hh:mm"));
    row.insert("filePath", artifact.filePath);
    return row;
}

QVariantMap MobileAppController::logSummary(const LogEvent &event) const
{
    QVariantMap row;
    row.insert("logId", event.logId);
    row.insert("summary", event.summary);
    row.insert("type", logEventTypeLabel(event.type));
    row.insert("actorName", event.actorName);
    row.insert("phase", toString(event.phase));
    row.insert("round", event.round);
    row.insert("timestamp", event.timestamp.toLocalTime().toString("HH:mm:ss"));
    return row;
}

QString MobileAppController::importAttachmentToPrivateStorage(const QUrl &url, QString *error) const
{
    const QString root = attachmentImportRoot();
    QDir().mkpath(root);

#ifdef Q_OS_ANDROID
    if (url.scheme() == "content") {
        const QJniObject context = QNativeInterface::QAndroidApplication::context();
        const QJniObject uri = QJniObject::fromString(url.toString());
        const QJniObject target = QJniObject::fromString(root);
        const QJniObject imported = QJniObject::callStaticObjectMethod(
            "com/aimeetingtable/mobile/FileBridge",
            "importUriToPrivateFile",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
            context.object<jobject>(),
            uri.object<jstring>(),
            target.object<jstring>());
        const QString path = imported.toString();
        if (path.isEmpty() && error) {
            *error = "Android content import failed.";
        }
        return path;
    }
#endif

    const QString sourcePath = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        if (error) {
            *error = "The selected attachment is not a readable local file.";
        }
        return {};
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString targetPath = root + "/" + id + "-" + sanitizedFileName(sourceInfo.fileName());
    if (!QFile::copy(sourceInfo.absoluteFilePath(), targetPath)) {
        QFile::remove(targetPath);
        if (error) {
            *error = "Failed to copy attachment into app storage.";
        }
        return {};
    }
    return targetPath;
}

void MobileAppController::setError(const QString &error) const
{
    m_lastError = error;
}

} // namespace amt
