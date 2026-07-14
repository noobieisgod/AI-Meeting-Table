#include "services/upload_manager.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace amt {

UploadManager::UploadManager(QObject *parent)
    : QObject(parent)
{
}

AttachmentRecord UploadManager::createAttachment(const QString &filePath, QString *error) const
{
    AttachmentRecord attachment;
    QFileInfo info(filePath);
    attachment.attachmentId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    attachment.displayName = info.fileName();
    attachment.filePath = info.absoluteFilePath();
    attachment.fileHash = computeFileHash(attachment.filePath, error);
    if (attachment.fileHash.isEmpty()) {
        attachment.attachmentId.clear();
        attachment.filePath.clear();
        attachment.displayName.clear();
        return attachment;
    }
    attachment.addedAt = QDateTime::currentDateTimeUtc();
    return attachment;
}

QString UploadManager::computeFileHash(const QString &filePath, QString *error) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QString("Failed to read attachment: %1").arg(filePath);
        }
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(8192));
    }
    return QString::fromUtf8(hash.result().toHex());
}

} // namespace amt
