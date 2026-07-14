#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QUrl>

class QThread;

namespace amt {

enum class AttachmentImportStatus : int {
    Success = 0,
    TooLarge = 1,
    InsufficientStorage = 2,
    ProviderFailure = 3,
    DestinationFailure = 4,
    Cancelled = 5,
    Timeout = 6,
    HashFailure = 7,
    RenameFailure = 8
};

struct AttachmentImportResult {
    QString operationId;
    AttachmentImportStatus status = AttachmentImportStatus::ProviderFailure;
    QString finalPath;
    qint64 byteCount = 0;
    QString sha256;

    bool succeeded() const { return status == AttachmentImportStatus::Success; }
};

struct AttachmentImportCancellationState;

class AttachmentImportManager final : public QObject
{
    Q_OBJECT

public:
    static constexpr qint64 maximumAttachmentBytes = 25LL * 1024LL * 1024LL;
    static constexpr qint64 freeSpaceReserveBytes = 64LL * 1024LL * 1024LL;
    static constexpr int noProgressTimeoutMs = 60 * 1000;
    static constexpr qsizetype copyBufferBytes = 64 * 1024;

    explicit AttachmentImportManager(QObject *parent = nullptr);
    ~AttachmentImportManager() override;

    QString startImport(const QString &tableId, const QUrl &sourceUrl, const QString &targetDirectory);
    bool cancelActive();
    bool active() const;
    QString activeOperationId() const;

signals:
    void importFinished(const amt::AttachmentImportResult &result);

private:
    void handleWorkerResult(const AttachmentImportResult &result);

    QString m_activeOperationId;
    QString m_activeTableId;
    QUrl m_activeSourceUrl;
    std::shared_ptr<AttachmentImportCancellationState> m_cancellation;
    QThread *m_thread = nullptr;
};

} // namespace amt

Q_DECLARE_METATYPE(amt::AttachmentImportResult)
