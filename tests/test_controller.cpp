#include <QtTest>

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/application_context.h"
#include "app/mobile_app_controller.h"

using namespace amt;

class ControllerTests final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void granularSignalsOnlyRefreshChangedCollections();
  void fullTranscriptTextPreservesOrderAndMetadata();
  void budgetValidationRejectsInvalidRelationships();
  void attachmentCleanupIsReferenceAndPathSafe();
};

void ControllerTests::initTestCase() {
  QStandardPaths::setTestModeEnabled(true);
  QCoreApplication::setOrganizationName("AI Meeting Table Tests");
  QCoreApplication::setApplicationName("Controller Tests");
  QDir testData(
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
  if (testData.exists()) {
    QVERIFY(testData.removeRecursively());
  }
  QSettings().clear();
}

void ControllerTests::granularSignalsOnlyRefreshChangedCollections() {
  MobileAppController controller;
  QVERIFY(controller.initialize());

  QSignalSpy stateChanged(&controller, &MobileAppController::stateChanged);
  QSignalSpy tablesChanged(&controller, &MobileAppController::tablesChanged);
  QSignalSpy seatsChanged(&controller, &MobileAppController::seatsChanged);
  QSignalSpy transcriptChanged(&controller,
                               &MobileAppController::transcriptChanged);
  QSignalSpy artifactsChanged(&controller,
                              &MobileAppController::artifactsChanged);
  QSignalSpy logsChanged(&controller, &MobileAppController::logsChanged);

  QVERIFY(controller.sendMessage("Test objective"));
  QCOMPARE(stateChanged.count(), 1);
  QCOMPARE(tablesChanged.count(), 0);
  QCOMPARE(seatsChanged.count(), 0);
  QCOMPARE(transcriptChanged.count(), 1);
  QCOMPARE(artifactsChanged.count(), 0);
  QCOMPARE(logsChanged.count(), 1);

  QVERIFY(controller.renameCurrentTable("Renamed"));
  QCOMPARE(stateChanged.count(), 2);
  QCOMPARE(tablesChanged.count(), 1);
  QCOMPARE(transcriptChanged.count(), 1);
  QCOMPARE(logsChanged.count(), 1);

  QVERIFY(controller.saveSeat(0, false, "Seat 1", 0, "", 0, 0));
  QCOMPARE(stateChanged.count(), 3);
  QCOMPARE(seatsChanged.count(), 1);
  QCOMPARE(transcriptChanged.count(), 1);
  QCOMPARE(artifactsChanged.count(), 0);
  QCOMPARE(logsChanged.count(), 1);
}

void ControllerTests::fullTranscriptTextPreservesOrderAndMetadata() {
  MobileAppController controller;
  QVERIFY(controller.initialize());
  QVERIFY(controller.createTable("Transcript copy", 1));
  QCOMPARE(controller.fullTranscriptText(), QString{});
  QVERIFY(controller.sendMessage("First copied message"));
  QVERIFY(controller.sendMessage("Second copied message"));

  const QString transcript = controller.fullTranscriptText();
  const qsizetype firstIndex = transcript.indexOf("First copied message");
  const qsizetype secondIndex = transcript.indexOf("Second copied message");
  QVERIFY(firstIndex >= 0);
  QVERIFY(secondIndex > firstIndex);
  QVERIFY(transcript.contains("You | Idle | Round 1"));
  QVERIFY(transcript.contains("First copied message\n\n["));
}

void ControllerTests::budgetValidationRejectsInvalidRelationships() {
  MobileAppController controller;
  QVERIFY(controller.initialize());
  QVERIFY(!controller.saveGlobalBudget(1000, 999, 1.0, 1, 1, 10, 20));
  QVERIFY(controller.lastError().contains("total tokens"));
  QVERIFY(!controller.saveGlobalBudget(1000, 2000, 1.0, 1, 1, 30, 20));
  QVERIFY(controller.lastError().contains("session seconds"));
  QVERIFY(!controller.saveGlobalBudget(0, 2000, 1.0, 1, 1, 10, 20));
  QVERIFY(controller.lastError().contains("positive"));
}

void ControllerTests::attachmentCleanupIsReferenceAndPathSafe() {
  ApplicationContext context;
  QVERIFY(context.initialize());
  QVERIFY(!context.tables().isEmpty());

  const QString attachmentRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/attachments";
  QVERIFY(QDir().mkpath(attachmentRoot));
  const QString managedPath = attachmentRoot + "/managed-test.txt";
  {
    QFile file(managedPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("managed"), qint64(7));
  }

  AttachmentRecord attachment;
  attachment.attachmentId = "shared";
  attachment.filePath = managedPath;
  context.tables().first()->attachments.append(attachment);

  auto secondTable =
      std::make_shared<SessionState>(context.createSampleTable());
  secondTable->attachments.append(attachment);
  context.tables().append(secondTable);
  QVERIFY(!context.cleanupAttachmentFileIfUnreferenced(managedPath));
  QVERIFY(QFile::exists(managedPath));

  context.tables().first()->attachments.clear();
  QVERIFY(!context.cleanupAttachmentFileIfUnreferenced(managedPath));
  QVERIFY(QFile::exists(managedPath));

  secondTable->attachments.clear();
  QVERIFY(context.cleanupAttachmentFileIfUnreferenced(managedPath));
  QVERIFY(!QFile::exists(managedPath));

  QTemporaryDir externalDirectory;
  QVERIFY(externalDirectory.isValid());
  const QString externalPath = externalDirectory.filePath("external.txt");
  {
    QFile file(externalPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("external"), qint64(8));
  }
  QVERIFY(!context.cleanupAttachmentFileIfUnreferenced(externalPath));
  QVERIFY(QFile::exists(externalPath));
}

QTEST_GUILESS_MAIN(ControllerTests)

#include "test_controller.moc"
