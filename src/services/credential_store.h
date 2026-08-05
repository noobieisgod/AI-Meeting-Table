#pragma once

#include <QObject>

#include "domain/models.h"

namespace amt {

class CredentialStore final : public QObject
{
    Q_OBJECT

public:
    explicit CredentialStore(QObject *parent = nullptr);

    bool saveApiKey(ProviderKind provider, const QString &apiKey, QString *errorMessage = nullptr, bool allowDelete = false) const;
    QString loadApiKey(ProviderKind provider, QString *errorMessage = nullptr) const;
};

}
