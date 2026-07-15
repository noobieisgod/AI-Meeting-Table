#include <QtTest>

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
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
  void appearanceAndSeatColorAreExposed();
  void missingAttachmentCannotBeOpened();
  void attachmentCleanupIsReferenceAndPathSafe();
  void attachmentMetadataAddedOnlyAfterSuccess();
  void tableDeletionDuringImportRemovesCompletedResult();
  void staleAttachmentCompletionIsIgnored();
  void startupCleanupPreservesOwnedAttachments();
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

  QVERIFY(controller.saveSeat(0, false, "Seat 1", 0, "", 0, 0, "#49bd99"));
  QCOMPARE(stateChanged.count(), 3);
  QCOMPARE(seatsChanged.count(), 1);
  QCOMPARE(transcriptChanged.count(), 2);
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

void ControllerTests::appearanceAndSeatColorAreExposed() {
  MobileAppController controller;
  QVERIFY(controller.initialize());

  QVERIFY(controller.saveAppearance("Dark", "Calm Workspace", "Console"));
  const QVariantMap settings = controller.settings();
  QCOMPARE(settings.value("appearance").toString(), QString("Dark"));
  QCOMPARE(settings.value("colorTheme").toString(), QString("Calm Workspace"));
  QCOMPARE(settings.value("fontStyle").toString(), QString("Console"));

  QVERIFY(controller.saveSeat(0, true, "Decision seat", 0, "", 0, 1,
                              "#ABCDEF"));
  const QVariantList seats = controller.seats();
  QCOMPARE(seats.first().toMap().value("color").toString(),
           QString("#abcdef"));

  QVERIFY(controller.saveSeat(0, true, "Decision seat", 0, "", 0, 1,
                              "#4f86c6"));
  QCOMPARE(controller.seats().first().toMap().value("color").toString(),
           QString("#4f86c6"));

  const QVariantMap safeguards = controller.attachmentSafeguards();
  QCOMPARE(safeguards.value("maximumAttachmentMiB").toLongLong(), qint64(25));
  QCOMPARE(safeguards.value("freeSpaceReserveMiB").toLongLong(), qint64(64));
  QCOMPARE(safeguards.value("noProgressTimeoutSeconds").toInt(), 60);
}

void ControllerTests::missingAttachmentCannotBeOpened() {
  MobileAppController controller;
  QVERIFY(controller.initialize());
  QVERIFY(controller.createTable("Missing attachment", 1));
  const auto handle = controller.m_context.tableHandle(controller.currentTableId());
  QVERIFY(handle);

  AttachmentRecord missing;
  missing.attachmentId = "missing-attachment";
  missing.displayName = "missing notes.txt";
  missing.filePath = QDir::temp().filePath("amt-file-that-does-not-exist.txt");
  QFile::remove(missing.filePath);
  handle->attachments.append(missing);

  QVERIFY(!controller.openAttachment(missing.attachmentId));
  QVERIFY(controller.lastError().contains("no longer exists"));
  QVERIFY(!controller.openAttachment("unknown-attachment"));
  QVERIFY(controller.lastError().contains("no longer available"));
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

void ControllerTests::attachmentMetadataAddedOnlyAfterSuccess() {
  MobileAppController controller;
  QVERIFY(controller.initialize());
  QVERIFY(controller.createTable("Async attachment", 1));
  const QString tableId = controller.currentTableId();
  const auto handle = controller.m_context.tableHandle(tableId);
  QVERIFY(handle);
  const qsizetype initialCount = handle->attachments.size();

  QTemporaryDir sourceDirectory;
  QVERIFY(sourceDirectory.isValid());
  const QString sourcePath = sourceDirectory.filePath("small-benign.txt");
  const QByteArray content("bounded attachment content");
  {
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(content), qint64(content.size()));
  }

  QSignalSpy importChanged(&controller,
                           &MobileAppController::attachmentImportChanged);
  QVERIFY(controller.addAttachment(QUrl::fromLocalFile(sourcePath)));
  QVERIFY(controller.attachmentImportInProgress());
  QCOMPARE(handle->attachments.size(), initialCount);
  QVERIFY(!controller.addAttachment(QUrl::fromLocalFile(sourcePath)));
  QVERIFY(controller.lastError().contains("already in progress"));

  QTRY_VERIFY_WITH_TIMEOUT(!controller.attachmentImportInProgress(), 5000);
  QVERIFY(importChanged.count() >= 2);
  QCOMPARE(handle->attachments.size(), initialCount + 1);
  const AttachmentRecord imported = handle->attachments.last();
  QCOMPARE(imported.fileHash,
           QString::fromLatin1(
               QCryptographicHash::hash(content, QCryptographicHash::Sha256)
                   .toHex()));
  QCOMPARE(QFileInfo(imported.filePath).size(), qint64(content.size()));
  QCOMPARE(controller.attachments().size(), initialCount + 1);
  QVERIFY(controller.removeAttachment(imported.attachmentId));
  QVERIFY(!QFile::exists(imported.filePath));
  QVERIFY(QFile::exists(sourcePath));
}

void ControllerTests::tableDeletionDuringImportRemovesCompletedResult() {
  MobileAppController controller;
  QVERIFY(controller.initialize());
  QVERIFY(controller.createTable("Delete during import", 1));
  const QString tableId = controller.currentTableId();
  const QString attachmentRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/attachments";
  QVERIFY(QDir().mkpath(attachmentRoot));
  const QString completedPath = attachmentRoot + "/deleted-table-result.txt";
  {
    QFile file(completedPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("result"), qint64(6));
  }

  controller.m_attachmentImportOperationId = "delete-operation";
  controller.m_attachmentImportTableId = tableId;
  controller.m_attachmentImportInProgress = true;
  QVERIFY(controller.deleteCurrentTable());
  QVERIFY(!controller.m_context.tableHandle(tableId));

  AttachmentImportResult result;
  result.operationId = "delete-operation";
  result.status = AttachmentImportStatus::Success;
  result.finalPath = completedPath;
  result.byteCount = 6;
  result.sha256 = QString(64, 'a');
  controller.handleAttachmentImportFinished(result);
  QVERIFY(!QFile::exists(completedPath));
  QVERIFY(!controller.attachmentImportInProgress());
}

void ControllerTests::staleAttachmentCompletionIsIgnored() {
  MobileAppController controller;
  QVERIFY(controller.initialize());
  if (controller.currentTableId().isEmpty()) {
    QVERIFY(controller.createTable("Stale import", 1));
  }
  const auto handle = controller.m_context.tableHandle(controller.currentTableId());
  QVERIFY(handle);
  const qsizetype initialCount = handle->attachments.size();

  const QString attachmentRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/attachments";
  QVERIFY(QDir().mkpath(attachmentRoot));
  const QString stalePath = attachmentRoot + "/stale-result.txt";
  {
    QFile file(stalePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("stale"), qint64(5));
  }

  controller.m_attachmentImportOperationId = "active-operation";
  controller.m_attachmentImportTableId = controller.currentTableId();
  controller.m_attachmentImportInProgress = true;
  AttachmentImportResult stale;
  stale.operationId = "stale-operation";
  stale.status = AttachmentImportStatus::Success;
  stale.finalPath = stalePath;
  stale.byteCount = 5;
  stale.sha256 = QString(64, 'b');
  controller.handleAttachmentImportFinished(stale);

  QVERIFY(!QFile::exists(stalePath));
  QVERIFY(controller.attachmentImportInProgress());
  QCOMPARE(handle->attachments.size(), initialCount);
  controller.m_attachmentImportOperationId.clear();
  controller.m_attachmentImportTableId.clear();
  controller.m_attachmentImportInProgress = false;
}

void ControllerTests::startupCleanupPreservesOwnedAttachments() {
  ApplicationContext context;
  QVERIFY(context.initialize());
  QVERIFY(!context.tables().isEmpty());
  auto table = context.tables().first();
  const QString attachmentRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/attachments";
  QVERIFY(QDir().mkpath(attachmentRoot));

  const QString ownedPath = attachmentRoot + "/owned-existing.txt";
  const QString orphanPath = attachmentRoot + "/completed-unowned.txt";
  const QString partialPath = attachmentRoot + "/interrupted.part";
  for (const QString &path : {ownedPath, orphanPath, partialPath}) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("data"), qint64(4));
  }

  AttachmentRecord owned;
  owned.attachmentId = "owned-existing";
  owned.displayName = "owned-existing.txt";
  owned.filePath = ownedPath;
  owned.fileHash = QString(64, 'c');
  owned.addedAt = QDateTime::currentDateTimeUtc();
  table->attachments.append(owned);
  QVERIFY(context.save(*table));

  ApplicationContext restored;
  QVERIFY(restored.initialize());
  QVERIFY(QFile::exists(ownedPath));
  QVERIFY(!QFile::exists(orphanPath));
  QVERIFY(!QFile::exists(partialPath));

  const auto restoredTable = restored.tableHandle(table->tableId);
  QVERIFY(restoredTable);
  restoredTable->attachments.erase(
      std::remove_if(restoredTable->attachments.begin(),
                     restoredTable->attachments.end(),
                     [](const AttachmentRecord &attachment) {
                       return attachment.attachmentId == "owned-existing";
                     }),
      restoredTable->attachments.end());
  QVERIFY(restored.save(*restoredTable));
  QVERIFY(restored.cleanupAttachmentFileIfUnreferenced(ownedPath));
}

QTEST_GUILESS_MAIN(ControllerTests)

#include "test_controller.moc"
