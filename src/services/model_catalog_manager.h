#pragma once

#include <QMap>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVector>
#include <functional>

#include "domain/models.h"
#include "services/credential_store.h"

class QNetworkAccessManager;
class QNetworkReply;

namespace amt {

class ModelCatalogManager final : public QObject {
  Q_OBJECT

public:
  using CredentialLoader = std::function<QString(ProviderKind)>;

  explicit ModelCatalogManager(CredentialStore *credentialStore,
                               QObject *parent = nullptr);
  ModelCatalogManager(CredentialLoader credentialLoader,
                      QNetworkAccessManager *networkManager,
                      int requestTimeoutMs, QObject *parent = nullptr);

  void fetchModelsAsync();
  QVector<ModelCatalogEntry> catalogForProvider(ProviderKind provider) const;
  QVariantList fetchStatuses() const;

signals:
  void fetchCompleted();

private:
  void fetchOpenAI(const QString &apiKey, quint64 generation);
  void fetchGemini(const QString &apiKey, quint64 generation);
  void fetchAnthropic(const QString &apiKey, quint64 generation);
  void setStatus(ProviderKind provider, const QString &message, bool success,
                 bool fallback = false);
  void setFailureStatus(ProviderKind provider, const QString &providerName,
                        const QString &reason);
  void checkCompletion(quint64 generation);
  void cancelActiveReplies();

  CredentialLoader m_credentialLoader;
  QNetworkAccessManager *m_ownedNetworkManager = nullptr;
  QNetworkAccessManager *m_networkManager = nullptr;
  QSet<QNetworkReply *> m_activeReplies;
  QMap<ProviderKind, QVector<ModelCatalogEntry>> m_dynamicCatalogs;
  QMap<ProviderKind, QString> m_statusMessages;
  QMap<ProviderKind, bool> m_statusSuccess;
  QMap<ProviderKind, bool> m_statusFallback;
  int m_pendingRequests = 0;
  int m_requestTimeoutMs = 90000;
  quint64 m_generation = 0;
};

} // namespace amt
