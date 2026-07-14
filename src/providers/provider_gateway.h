#pragma once

#include <QObject>

#include "domain/models.h"

namespace amt {

class CredentialStore;

struct ProviderRequest {
    QString requestId;
    QString sessionId;
    QString seatId;
    ProviderKind provider = ProviderKind::OpenAI;
    QString model;
    Phase phase = Phase::Idle;
    QJsonObject prompt;
    QString apiKey;
    quint64 runGeneration = 0;
};

struct ProviderResponse {
    QString requestId;
    QString sessionId;
    QString seatId;
    bool success = true;
    QString content;
    bool skipped = false;
    QString decisionOutcome;
    bool multipleDecisionRulings = false;
    int usedTokens = 0;
    double estimatedCost = 0.0;
    QString errorMessage;
    QJsonObject attachmentProviderHandles;
    quint64 runGeneration = 0;
};

class ProviderGateway : public QObject
{
    Q_OBJECT

public:
    explicit ProviderGateway(QObject *parent = nullptr);
    ~ProviderGateway() override;
    void setCredentialStore(CredentialStore *credentialStore);

    virtual void sendAsync(const ProviderRequest &request);

signals:
    void responseReady(const amt::ProviderResponse &response);

private:
    CredentialStore *m_credentialStore = nullptr;
};

} // namespace amt

Q_DECLARE_METATYPE(amt::ProviderRequest)
Q_DECLARE_METATYPE(amt::ProviderResponse)
