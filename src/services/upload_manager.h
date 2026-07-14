#pragma once

#include <QObject>

#include "domain/models.h"

namespace amt {

class UploadManager final : public QObject
{
    Q_OBJECT

public:
    explicit UploadManager(QObject *parent = nullptr);

    AttachmentRecord createAttachment(const QString &filePath, QString *error = nullptr) const;
    AttachmentRecord createAttachment(const QString &filePath,
                                      const QString &sha256,
                                      qint64 byteCount,
                                      QString *error = nullptr) const;

private:
    QString computeFileHash(const QString &filePath, QString *error = nullptr) const;
};

} // namespace amt
