#pragma once

#include <functional>

#include <QElapsedTimer>
#include <QString>
#include <QVector>

namespace amt {

enum class StartupStage {
    ProcessEntry,
    GuiApplicationConstruction,
    SslDiagnostics,
    ApplicationContextConstruction,
    ControllerConstruction,
    DatabaseOpen,
    SchemaInitialization,
    TableRestoration,
    TranscriptRestoration,
    LogRestoration,
    ArtifactRestoration,
    ControllerSnapshotConstruction,
    BeforeQmlModuleLoad,
    QmlRootCreation,
    InitialRefreshAll,
    FirstFrameSwapped,
    TranscriptVisualStabilization,
    PrimaryControlsReady,
    InteractiveReady,
    AndroidFullyDrawn
};

struct StartupMarker {
    StartupStage stage;
    qint64 elapsedMs = 0;
    qint64 durationMs = -1;
};

class StartupTimeline final
{
public:
    using Reporter = std::function<void()>;
    using Clock = std::function<qint64()>;

    StartupTimeline(bool detailedLoggingEnabled, Reporter reporter = {}, Clock clock = {});

    static StartupTimeline &instance();
    static QString stageId(StartupStage stage);
    static QString formatMarker(const StartupMarker &marker);
    static bool defaultDetailedLoggingEnabled();

    void begin();
    void mark(StartupStage stage, qint64 durationMs = -1);
    void beginInitialRefresh();
    void completeInitialRefresh();
    void markFirstFrameVisible();
    void markTranscriptVisualStable();
    void markPrimaryControlsReady();

    bool fullyDrawnReported() const;
    bool detailedLoggingEnabled() const;
    const QVector<StartupMarker> &markers() const;

private:
    qint64 elapsed() const;
    bool contains(StartupStage stage) const;
    void evaluateInteractiveReady();

    QElapsedTimer m_timer;
    Reporter m_reporter;
    Clock m_clock;
    QVector<StartupMarker> m_markers;
    qint64 m_initialRefreshStartMs = -1;
    bool m_detailedLoggingEnabled = false;
    bool m_started = false;
    bool m_firstFrameVisible = false;
    bool m_initialStateApplied = false;
    bool m_transcriptVisualStable = false;
    bool m_primaryControlsReady = false;
    bool m_fullyDrawnReported = false;
};

} // namespace amt
