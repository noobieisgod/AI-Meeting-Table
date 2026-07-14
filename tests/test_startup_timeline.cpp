#include <QtTest>

#include "core/startup_timeline.h"

using namespace amt;

class StartupTimelineTests final : public QObject
{
    Q_OBJECT

private slots:
    void markerOrderingAndFormat();
    void fullyDrawnReportsExactlyOnce();
    void completionOrderDoesNotMatter();
    void ordinaryReleaseLoggingCanRemainDisabled();
};

void StartupTimelineTests::markerOrderingAndFormat()
{
    qint64 now = 0;
    StartupTimeline timeline(true, {}, [&now]() { return now; });
    timeline.begin();
    now = 7;
    timeline.mark(StartupStage::GuiApplicationConstruction, 7);
    now = 11;
    timeline.mark(StartupStage::SslDiagnostics, 4);

    const auto markers = timeline.markers();
    QCOMPARE(markers.size(), 3);
    QCOMPARE(markers.at(0).stage, StartupStage::ProcessEntry);
    QCOMPARE(markers.at(1).stage, StartupStage::GuiApplicationConstruction);
    QCOMPARE(markers.at(2).stage, StartupStage::SslDiagnostics);
    QCOMPARE(markers.at(2).elapsedMs, 11);
    QCOMPARE(markers.at(2).durationMs, 4);

    const QString formatted = StartupTimeline::formatMarker(markers.at(2));
    QCOMPARE(formatted, QString("AMT_STARTUP stage=ssl_diagnostics elapsed_ms=11 duration_ms=4"));
    QVERIFY(!formatted.contains("prompt", Qt::CaseInsensitive));
    QVERIFY(!formatted.contains("credential", Qt::CaseInsensitive));
    QVERIFY(!formatted.contains("filename", Qt::CaseInsensitive));
    QVERIFY(!formatted.contains("response", Qt::CaseInsensitive));
}

void StartupTimelineTests::fullyDrawnReportsExactlyOnce()
{
    qint64 now = 0;
    int reports = 0;
    StartupTimeline timeline(true, [&reports]() { ++reports; }, [&now]() { return now; });
    timeline.begin();
    now = 1;
    timeline.beginInitialRefresh();
    now = 2;
    timeline.completeInitialRefresh();
    now = 3;
    timeline.markPrimaryControlsReady();
    now = 4;
    timeline.markTranscriptVisualStable();
    QCOMPARE(reports, 0);
    now = 5;
    timeline.markFirstFrameVisible();
    QCOMPARE(reports, 1);
    QVERIFY(timeline.fullyDrawnReported());

    timeline.markFirstFrameVisible();
    timeline.markPrimaryControlsReady();
    timeline.completeInitialRefresh();
    QCOMPARE(reports, 1);

    int interactiveMarkers = 0;
    int fullyDrawnMarkers = 0;
    for (const auto &marker : timeline.markers()) {
        interactiveMarkers += marker.stage == StartupStage::InteractiveReady ? 1 : 0;
        fullyDrawnMarkers += marker.stage == StartupStage::AndroidFullyDrawn ? 1 : 0;
    }
    QCOMPARE(interactiveMarkers, 1);
    QCOMPARE(fullyDrawnMarkers, 1);
}

void StartupTimelineTests::completionOrderDoesNotMatter()
{
    const QList<QList<int>> orders = {{0, 1, 2, 3}, {3, 2, 1, 0}, {1, 3, 0, 2}};
    for (const auto &order : orders) {
        int reports = 0;
        StartupTimeline timeline(false, [&reports]() { ++reports; }, []() { return 10; });
        timeline.begin();
        for (int stage : order) {
            switch (stage) {
            case 0:
                timeline.beginInitialRefresh();
                timeline.completeInitialRefresh();
                break;
            case 1: timeline.markFirstFrameVisible(); break;
            case 2: timeline.markTranscriptVisualStable(); break;
            case 3: timeline.markPrimaryControlsReady(); break;
            }
        }
        QCOMPARE(reports, 1);
    }
}

void StartupTimelineTests::ordinaryReleaseLoggingCanRemainDisabled()
{
    int reports = 0;
    StartupTimeline timeline(false, [&reports]() { ++reports; }, []() { return 1; });
    QVERIFY(!timeline.detailedLoggingEnabled());
    timeline.begin();
    timeline.beginInitialRefresh();
    timeline.completeInitialRefresh();
    timeline.markFirstFrameVisible();
    timeline.markTranscriptVisualStable();
    timeline.markPrimaryControlsReady();
    QCOMPARE(reports, 1);
    QVERIFY(timeline.fullyDrawnReported());
}

QTEST_GUILESS_MAIN(StartupTimelineTests)

#include "test_startup_timeline.moc"
