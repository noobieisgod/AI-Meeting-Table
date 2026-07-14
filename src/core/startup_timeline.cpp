#include "core/startup_timeline.h"

#include <algorithm>

#include <QCoreApplication>
#include <QLoggingCategory>

#include "core/logging.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace amt {

namespace {

void reportAndroidFullyDrawn()
{
#ifdef Q_OS_ANDROID
    if (!QNativeInterface::QAndroidApplication::isActivityContext()) {
        return;
    }
    const QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (activity.isValid()) {
        activity.callMethod<void>("reportFullyDrawn");
    }
#endif
}

} // namespace

StartupTimeline::StartupTimeline(bool detailedLoggingEnabled, Reporter reporter, Clock clock)
    : m_reporter(reporter ? std::move(reporter) : Reporter(reportAndroidFullyDrawn)),
      m_clock(std::move(clock)),
      m_detailedLoggingEnabled(detailedLoggingEnabled)
{
}

StartupTimeline &StartupTimeline::instance()
{
    static StartupTimeline timeline(defaultDetailedLoggingEnabled());
    return timeline;
}

QString StartupTimeline::stageId(StartupStage stage)
{
    switch (stage) {
    case StartupStage::ProcessEntry: return QStringLiteral("process_entry");
    case StartupStage::GuiApplicationConstruction: return QStringLiteral("gui_application_construction");
    case StartupStage::SslDiagnostics: return QStringLiteral("ssl_diagnostics");
    case StartupStage::ApplicationContextConstruction: return QStringLiteral("application_context_construction");
    case StartupStage::ControllerConstruction: return QStringLiteral("controller_construction");
    case StartupStage::DatabaseOpen: return QStringLiteral("database_open");
    case StartupStage::SchemaInitialization: return QStringLiteral("schema_initialization");
    case StartupStage::TableRestoration: return QStringLiteral("table_restoration");
    case StartupStage::TranscriptRestoration: return QStringLiteral("transcript_restoration");
    case StartupStage::LogRestoration: return QStringLiteral("log_restoration");
    case StartupStage::ArtifactRestoration: return QStringLiteral("artifact_restoration");
    case StartupStage::ControllerSnapshotConstruction: return QStringLiteral("controller_snapshot_construction");
    case StartupStage::BeforeQmlModuleLoad: return QStringLiteral("before_qml_module_load");
    case StartupStage::QmlRootCreation: return QStringLiteral("qml_root_creation");
    case StartupStage::InitialRefreshAll: return QStringLiteral("initial_refresh_all");
    case StartupStage::FirstFrameSwapped: return QStringLiteral("first_frame_swapped");
    case StartupStage::TranscriptVisualStabilization: return QStringLiteral("transcript_visual_stabilization");
    case StartupStage::PrimaryControlsReady: return QStringLiteral("primary_controls_ready");
    case StartupStage::InteractiveReady: return QStringLiteral("interactive_ready");
    case StartupStage::AndroidFullyDrawn: return QStringLiteral("android_fully_drawn");
    }
    return QStringLiteral("unknown");
}

QString StartupTimeline::formatMarker(const StartupMarker &marker)
{
    QString result = QStringLiteral("AMT_STARTUP stage=%1 elapsed_ms=%2")
                         .arg(stageId(marker.stage))
                         .arg(marker.elapsedMs);
    if (marker.durationMs >= 0) {
        result.append(QStringLiteral(" duration_ms=%1").arg(marker.durationMs));
    }
    return result;
}

bool StartupTimeline::defaultDetailedLoggingEnabled()
{
#ifdef QT_DEBUG
    return true;
#else
    return qEnvironmentVariableIsSet("AMT_STARTUP_TIMELINE");
#endif
}

void StartupTimeline::begin()
{
    if (m_started) {
        return;
    }
    m_started = true;
    if (!m_clock) {
        m_timer.start();
    }
    mark(StartupStage::ProcessEntry, 0);
}

void StartupTimeline::mark(StartupStage stage, qint64 durationMs)
{
    if (!m_started) {
        begin();
    }
    if (contains(stage)) {
        return;
    }
    const StartupMarker marker{stage, elapsed(), durationMs};
    m_markers.append(marker);
    if (m_detailedLoggingEnabled) {
        qCInfo(startupLog).noquote() << formatMarker(marker);
    }
}

void StartupTimeline::beginInitialRefresh()
{
    if (!m_started) {
        begin();
    }
    if (m_initialRefreshStartMs < 0 && !contains(StartupStage::InitialRefreshAll)) {
        m_initialRefreshStartMs = elapsed();
    }
}

void StartupTimeline::completeInitialRefresh()
{
    if (m_initialRefreshStartMs < 0) {
        beginInitialRefresh();
    }
    mark(StartupStage::InitialRefreshAll, std::max<qint64>(0, elapsed() - m_initialRefreshStartMs));
    m_initialStateApplied = true;
    evaluateInteractiveReady();
}

void StartupTimeline::markFirstFrameVisible()
{
    mark(StartupStage::FirstFrameSwapped);
    m_firstFrameVisible = true;
    evaluateInteractiveReady();
}

void StartupTimeline::markTranscriptVisualStable()
{
    mark(StartupStage::TranscriptVisualStabilization);
    m_transcriptVisualStable = true;
    evaluateInteractiveReady();
}

void StartupTimeline::markPrimaryControlsReady()
{
    mark(StartupStage::PrimaryControlsReady);
    m_primaryControlsReady = true;
    evaluateInteractiveReady();
}

bool StartupTimeline::fullyDrawnReported() const
{
    return m_fullyDrawnReported;
}

bool StartupTimeline::detailedLoggingEnabled() const
{
    return m_detailedLoggingEnabled;
}

const QVector<StartupMarker> &StartupTimeline::markers() const
{
    return m_markers;
}

qint64 StartupTimeline::elapsed() const
{
    return m_clock ? m_clock() : (m_timer.isValid() ? m_timer.elapsed() : 0);
}

bool StartupTimeline::contains(StartupStage stage) const
{
    return std::any_of(m_markers.cbegin(), m_markers.cend(), [stage](const StartupMarker &marker) {
        return marker.stage == stage;
    });
}

void StartupTimeline::evaluateInteractiveReady()
{
    if (m_fullyDrawnReported
        || !m_firstFrameVisible
        || !m_initialStateApplied
        || !m_transcriptVisualStable
        || !m_primaryControlsReady) {
        return;
    }

    mark(StartupStage::InteractiveReady);
    m_fullyDrawnReported = true;
    if (m_reporter) {
        m_reporter();
    }
    mark(StartupStage::AndroidFullyDrawn);
}

} // namespace amt
