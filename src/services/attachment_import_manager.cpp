#include "services/attachment_import_manager.h"

#include <atomic>

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QThread>
#include <QUuid>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace amt {

struct AttachmentImportCancellationState {
    std::atomic<int> status{0};
};

namespace {

struct ImportRequest {
    QString operationId;
    QString tableId;
    QUrl sourceUrl;
    QString targetDirectory;
};

AttachmentImportResult failure(const QString &operationId, AttachmentImportStatus status)
{
    AttachmentImportResult result;
    result.operationId = operationId;
    result.status = status;
    return result;
}

AttachmentImportStatus cancellationStatus(const std::shared_ptr<AttachmentImportCancellationState> &cancellation)
{
    const int value = cancellation ? cancellation->status.load(std::memory_order_acquire) : 0;
    return value == static_cast<int>(AttachmentImportStatus::Timeout)
        ? AttachmentImportStatus::Timeout
        : AttachmentImportStatus::Cancelled;
}

bool isCancelled(const std::shared_ptr<AttachmentImportCancellationState> &cancellation)
{
    return cancellation && cancellation->status.load(std::memory_order_acquire) != 0;
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

bool hasRequiredSpace(qint64 available, qint64 required)
{
    return available >= AttachmentImportManager::freeSpaceReserveBytes
        && required <= available - AttachmentImportManager::freeSpaceReserveBytes;
}

AttachmentImportResult importLocalFile(const ImportRequest &request,
                                       const std::shared_ptr<AttachmentImportCancellationState> &cancellation)
{
    const QString sourcePath = request.sourceUrl.isLocalFile()
        ? request.sourceUrl.toLocalFile()
        : request.sourceUrl.toString();
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return failure(request.operationId, AttachmentImportStatus::ProviderFailure);
    }
    if (sourceInfo.size() > AttachmentImportManager::maximumAttachmentBytes) {
        return failure(request.operationId, AttachmentImportStatus::TooLarge);
    }
    if (!QDir().mkpath(request.targetDirectory)) {
        return failure(request.operationId, AttachmentImportStatus::DestinationFailure);
    }

    QStorageInfo storage(request.targetDirectory);
    if (!storage.isValid() || !storage.isReady()
        || !hasRequiredSpace(storage.bytesAvailable(), sourceInfo.size())) {
        return failure(request.operationId, AttachmentImportStatus::InsufficientStorage);
    }

    QFile source(sourceInfo.absoluteFilePath());
    if (!source.open(QIODevice::ReadOnly)) {
        return failure(request.operationId, AttachmentImportStatus::ProviderFailure);
    }

    const QString finalName = QUuid::createUuid().toString(QUuid::WithoutBraces)
        + "-" + sanitizedFileName(sourceInfo.fileName());
    const QString finalPath = QDir(request.targetDirectory).filePath(finalName);
    const QString partPath = finalPath + ".part";
    QFile::remove(partPath);

    QFile part(partPath);
    if (!part.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        return failure(request.operationId, AttachmentImportStatus::DestinationFailure);
    }

    auto failAndClean = [&](AttachmentImportStatus status) {
        part.close();
        source.close();
        QFile::remove(partPath);
        QFile::remove(finalPath);
        return failure(request.operationId, status);
    };

    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(AttachmentImportManager::copyBufferBytes, Qt::Uninitialized);
    qint64 copied = 0;
    QElapsedTimer noProgress;
    noProgress.start();
    while (true) {
        if (isCancelled(cancellation)) {
            return failAndClean(cancellationStatus(cancellation));
        }
        const qint64 read = source.read(buffer.data(), buffer.size());
        if (read < 0) {
            return failAndClean(AttachmentImportStatus::ProviderFailure);
        }
        if (read == 0) {
            if (source.atEnd()) {
                break;
            }
            if (noProgress.elapsed() >= AttachmentImportManager::noProgressTimeoutMs) {
                return failAndClean(AttachmentImportStatus::Timeout);
            }
            QThread::msleep(10);
            continue;
        }
        if (copied > AttachmentImportManager::maximumAttachmentBytes - read) {
            return failAndClean(AttachmentImportStatus::TooLarge);
        }

        storage.refresh();
        if (!hasRequiredSpace(storage.bytesAvailable(), read)) {
            return failAndClean(AttachmentImportStatus::InsufficientStorage);
        }
        if (part.write(buffer.constData(), read) != read) {
            return failAndClean(AttachmentImportStatus::DestinationFailure);
        }
        hash.addData(QByteArrayView(buffer.constData(), read));
        copied += read;
        noProgress.restart();
    }

    if (!part.flush()) {
        return failAndClean(AttachmentImportStatus::DestinationFailure);
    }
    part.close();
    source.close();
    if (isCancelled(cancellation)) {
        return failAndClean(cancellationStatus(cancellation));
    }
    if (!QFile::rename(partPath, finalPath)) {
        return failAndClean(AttachmentImportStatus::RenameFailure);
    }
    if (isCancelled(cancellation)) {
        return failAndClean(cancellationStatus(cancellation));
    }

    AttachmentImportResult result;
    result.operationId = request.operationId;
    result.status = AttachmentImportStatus::Success;
    result.finalPath = finalPath;
    result.byteCount = copied;
    result.sha256 = QString::fromLatin1(hash.result().toHex());
    return result;
}

#ifdef Q_OS_ANDROID
AttachmentImportStatus statusFromJava(int status)
{
    if (status < static_cast<int>(AttachmentImportStatus::Success)
        || status > static_cast<int>(AttachmentImportStatus::RenameFailure)) {
        return AttachmentImportStatus::ProviderFailure;
    }
    return static_cast<AttachmentImportStatus>(status);
}

void cancelAndroidImport(const QString &operationId, AttachmentImportStatus status)
{
    const QJniObject operation = QJniObject::fromString(operationId);
    QJniObject::callStaticMethod<void>(
        "com/aimeetingtable/mobile/FileBridge",
        "cancelImport",
        "(Ljava/lang/String;I)V",
        operation.object<jstring>(),
        static_cast<jint>(status));
}

void clearAndroidCancellation(const QString &operationId)
{
    const QJniObject operation = QJniObject::fromString(operationId);
    QJniObject::callStaticMethod<void>(
        "com/aimeetingtable/mobile/FileBridge",
        "clearCancellation",
        "(Ljava/lang/String;)V",
        operation.object<jstring>());
}

AttachmentImportResult importAndroidContent(const ImportRequest &request,
                                            const std::shared_ptr<AttachmentImportCancellationState> &cancellation)
{
    if (isCancelled(cancellation)) {
        return failure(request.operationId, cancellationStatus(cancellation));
    }
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    const QJniObject operation = QJniObject::fromString(request.operationId);
    const QJniObject uri = QJniObject::fromString(request.sourceUrl.toString());
    const QJniObject target = QJniObject::fromString(request.targetDirectory);
    const QJniObject imported = QJniObject::callStaticObjectMethod(
        "com/aimeetingtable/mobile/FileBridge",
        "importUriToPrivateFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/aimeetingtable/mobile/FileBridge$ImportResult;",
        context.object<jobject>(),
        operation.object<jstring>(),
        uri.object<jstring>(),
        target.object<jstring>());
    if (!imported.isValid()) {
        return failure(request.operationId, AttachmentImportStatus::ProviderFailure);
    }

    AttachmentImportResult result;
    result.operationId = imported.callMethod<jstring>(
        "getOperationId", "()Ljava/lang/String;").toString();
    result.status = statusFromJava(imported.callMethod<jint>("getStatusCode", "()I"));
    result.finalPath = imported.callMethod<jstring>(
        "getFinalPath", "()Ljava/lang/String;").toString();
    result.byteCount = imported.callMethod<jlong>("getByteCount", "()J");
    result.sha256 = imported.callMethod<jstring>(
        "getSha256", "()Ljava/lang/String;").toString();
    if (result.operationId.isEmpty()) {
        result.operationId = request.operationId;
    }
    if (isCancelled(cancellation)) {
        if (!result.finalPath.isEmpty()) {
            QFile::remove(result.finalPath);
        }
        return failure(request.operationId, cancellationStatus(cancellation));
    }
    return result;
}
#endif

AttachmentImportResult performImport(const ImportRequest &request,
                                     const std::shared_ptr<AttachmentImportCancellationState> &cancellation)
{
#ifdef Q_OS_ANDROID
    if (request.sourceUrl.scheme() == "content") {
        return importAndroidContent(request, cancellation);
    }
#endif
    return importLocalFile(request, cancellation);
}

} // namespace

AttachmentImportManager::AttachmentImportManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<AttachmentImportResult>();
}

AttachmentImportManager::~AttachmentImportManager()
{
    cancelActive();
    QThread *thread = m_thread;
    if (thread && thread->isRunning() && !thread->wait(2000)) {
        disconnect(thread, nullptr, this, nullptr);
    }
}

QString AttachmentImportManager::startImport(const QString &tableId,
                                             const QUrl &sourceUrl,
                                             const QString &targetDirectory)
{
    if (active() || (m_thread && m_thread->isRunning()) || tableId.isEmpty()
        || sourceUrl.isEmpty() || targetDirectory.isEmpty()) {
        return {};
    }

    const QString operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeOperationId = operationId;
    m_activeTableId = tableId;
    m_activeSourceUrl = sourceUrl;
    m_cancellation = std::make_shared<AttachmentImportCancellationState>();
    const ImportRequest request{operationId, tableId, sourceUrl, targetDirectory};
    const auto cancellation = m_cancellation;
    QPointer<AttachmentImportManager> manager(this);

    QThread *thread = QThread::create([manager, request, cancellation]() {
        const AttachmentImportResult result = performImport(request, cancellation);
        if (manager) {
            QMetaObject::invokeMethod(manager, [manager, result]() {
                if (manager) {
                    manager->handleWorkerResult(result);
                } else if (!result.finalPath.isEmpty()) {
                    QFile::remove(result.finalPath);
                }
            }, Qt::QueuedConnection);
        } else if (!result.finalPath.isEmpty()) {
            QFile::remove(result.finalPath);
        }
    });
    m_thread = thread;
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_thread == thread) {
            m_thread = nullptr;
        }
    });
    thread->start();
    return operationId;
}

bool AttachmentImportManager::cancelActive()
{
    if (!active() || !m_cancellation) {
        return false;
    }
    int expected = 0;
    m_cancellation->status.compare_exchange_strong(
        expected,
        static_cast<int>(AttachmentImportStatus::Cancelled),
        std::memory_order_acq_rel);
#ifdef Q_OS_ANDROID
    if (m_activeSourceUrl.scheme() == "content") {
        cancelAndroidImport(m_activeOperationId, AttachmentImportStatus::Cancelled);
    }
#endif
    return true;
}

bool AttachmentImportManager::active() const
{
    return !m_activeOperationId.isEmpty();
}

QString AttachmentImportManager::activeOperationId() const
{
    return m_activeOperationId;
}

void AttachmentImportManager::handleWorkerResult(const AttachmentImportResult &workerResult)
{
    AttachmentImportResult result = workerResult;
#ifdef Q_OS_ANDROID
    clearAndroidCancellation(result.operationId);
#endif
    if (result.operationId != m_activeOperationId) {
        if (!result.finalPath.isEmpty()) {
            QFile::remove(result.finalPath);
        }
        return;
    }
    if (isCancelled(m_cancellation) && result.succeeded()) {
        QFile::remove(result.finalPath);
        result = failure(result.operationId, cancellationStatus(m_cancellation));
    }

    m_activeOperationId.clear();
    m_activeTableId.clear();
    m_activeSourceUrl = QUrl();
    m_cancellation.reset();
    emit importFinished(result);
}

} // namespace amt
