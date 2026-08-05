#pragma once

#include <QObject>

#include "domain/models.h"

namespace amt {

class ArtifactManager final : public QObject
{
    Q_OBJECT

public:
    explicit ArtifactManager(QObject *parent = nullptr);

    ArtifactVersion createVersion(SessionState &state, Phase phase, int round, const QString &summary, const QString &content);
    QString artifactContentPath(const QString &versionId) const;
    QString lastError() const;

private:
    mutable QString m_lastError;
};

} // namespace amt
