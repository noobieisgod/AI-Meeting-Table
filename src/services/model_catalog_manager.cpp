#include "services/model_catalog_manager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

#include <utility>

namespace amt {

namespace {

bool isModelAllowed(const QString &id) {
  const QString lowerId = id.toLower();
  const QStringList exclusions = {
      "vision", "audio",      "realtime", "tts",      "image",
      "api",    "transcribe", "banana",   "robotics", "embedding"};
  for (const QString &exclusion : exclusions) {
    if (lowerId.contains(exclusion)) {
      return false;
    }
  }
  return true;
}

void appendDeduplicated(QVector<ModelCatalogEntry> &target,
                        const QVector<ModelCatalogEntry> &source) {
  QVector<ModelCatalogEntry> deduplicated;
  deduplicated.reserve(target.size() + source.size());
  QSet<QString> existingIds;
  for (const auto &entry : target) {
    if (!existingIds.contains(entry.id)) {
      deduplicated.append(entry);
      existingIds.insert(entry.id);
    }
  }
  for (const auto &entry : source) {
    if (existingIds.contains(entry.id)) {
      continue;
    }
    deduplicated.append(entry);
    existingIds.insert(entry.id);
  }
  target = std::move(deduplicated);
}

QString replyFailureSummary(QNetworkReply *reply) {
  QStringList parts;
  const int statusCode =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (statusCode > 0) {
    parts << QString("HTTP %1").arg(statusCode);
  }
  if (reply->property("amtTimedOut").toBool()) {
    parts << "timeout";
  } else if (reply->error() != QNetworkReply::NoError) {
    parts << QString("network code %1").arg(static_cast<int>(reply->error()));
  }
  return parts.isEmpty() ? "unknown transport error" : parts.join(", ");
}

bool supportsGeminiGenerateContent(const QJsonObject &model) {
  const QJsonArray supportedActions = model.value("supportedActions").toArray();
  const QJsonArray supportedMethods =
      model.value("supportedGenerationMethods").toArray();
  for (const auto &value : supportedActions) {
    if (value.toString().compare("generateContent", Qt::CaseInsensitive) == 0) {
      return true;
    }
  }
  for (const auto &value : supportedMethods) {
    if (value.toString().compare("generateContent", Qt::CaseInsensitive) == 0) {
      return true;
    }
  }
  return supportedActions.isEmpty() && supportedMethods.isEmpty();
}

} // namespace

ModelCatalogManager::ModelCatalogManager(CredentialStore *credentialStore,
                                         QObject *parent)
    : QObject(parent),
      m_credentialLoader([credentialStore](ProviderKind provider) {
        return credentialStore ? credentialStore->loadApiKey(provider)
                               : QString{};
      }),
      m_ownedNetworkManager(new QNetworkAccessManager(this)),
      m_networkManager(m_ownedNetworkManager) {}

ModelCatalogManager::ModelCatalogManager(CredentialLoader credentialLoader,
                                         QNetworkAccessManager *networkManager,
                                         int requestTimeoutMs, QObject *parent)
    : QObject(parent), m_credentialLoader(std::move(credentialLoader)),
      m_networkManager(networkManager),
      m_requestTimeoutMs(qMax(1, requestTimeoutMs)) {
  if (!m_networkManager) {
    m_ownedNetworkManager = new QNetworkAccessManager(this);
    m_networkManager = m_ownedNetworkManager;
  }
}

void ModelCatalogManager::fetchModelsAsync() {
  const quint64 generation = ++m_generation;
  cancelActiveReplies();
  m_statusMessages.clear();
  m_statusSuccess.clear();
  m_statusFallback.clear();
  m_pendingRequests = 0;

  const QString openAiKey =
      m_credentialLoader ? m_credentialLoader(ProviderKind::OpenAI) : QString{};
  const QString geminiKey =
      m_credentialLoader ? m_credentialLoader(ProviderKind::Gemini) : QString{};
  const QString anthropicKey = m_credentialLoader
                                   ? m_credentialLoader(ProviderKind::Anthropic)
                                   : QString{};

  if (!openAiKey.isEmpty()) {
    setStatus(ProviderKind::OpenAI, "OpenAI is refreshing...", false);
    ++m_pendingRequests;
    fetchOpenAI(openAiKey, generation);
  } else {
    setStatus(ProviderKind::OpenAI, "OpenAI skipped: no API key configured.",
              false);
  }

  if (!geminiKey.isEmpty()) {
    setStatus(ProviderKind::Gemini, "Google is refreshing...", false);
    ++m_pendingRequests;
    fetchGemini(geminiKey, generation);
  } else {
    setStatus(ProviderKind::Gemini, "Google skipped: no API key configured.",
              false);
  }

  if (!anthropicKey.isEmpty()) {
    setStatus(ProviderKind::Anthropic, "Anthropic is refreshing...", false);
    ++m_pendingRequests;
    fetchAnthropic(anthropicKey, generation);
  } else {
    setStatus(ProviderKind::Anthropic,
              "Anthropic skipped: no API key configured.", false);
  }

  if (m_pendingRequests == 0) {
    QTimer::singleShot(0, this, [this, generation]() {
      if (generation == m_generation) {
        emit fetchCompleted();
      }
    });
  }
}

QVector<ModelCatalogEntry>
ModelCatalogManager::catalogForProvider(ProviderKind provider) const {
  const auto dynamic = m_dynamicCatalogs.constFind(provider);
  if (dynamic != m_dynamicCatalogs.cend() && !dynamic->isEmpty()) {
    return *dynamic;
  }
  return modelCatalogForProvider(provider);
}

QVariantList ModelCatalogManager::fetchStatuses() const {
  QVariantList rows;
  const QVector<ProviderKind> providers = {
      ProviderKind::OpenAI, ProviderKind::Gemini, ProviderKind::Anthropic};
  for (ProviderKind provider : providers) {
    QVariantMap row;
    row.insert("provider", providerKindToString(provider));
    row.insert("message",
               m_statusMessages.value(
                   provider, QString("%1 has not refreshed yet.")
                                 .arg(providerKindToString(provider))));
    row.insert("success", m_statusSuccess.value(provider, false));
    row.insert("fallback", m_statusFallback.value(provider, false));
    rows.append(row);
  }
  return rows;
}

void ModelCatalogManager::setStatus(ProviderKind provider,
                                    const QString &message, bool success,
                                    bool fallback) {
  m_statusMessages.insert(provider, message);
  m_statusSuccess.insert(provider, success);
  m_statusFallback.insert(provider, fallback);
}

void ModelCatalogManager::setFailureStatus(ProviderKind provider,
                                           const QString &providerName,
                                           const QString &reason) {
  const bool hasPrevious = m_dynamicCatalogs.contains(provider) &&
                           !m_dynamicCatalogs.value(provider).isEmpty();
  setStatus(provider,
            QString("%1 refresh failed (%2); using %3 model list.")
                .arg(providerName, reason,
                     hasPrevious ? "the previous" : "the built-in fallback"),
            false, true);
}

void ModelCatalogManager::checkCompletion(quint64 generation) {
  if (generation != m_generation) {
    return;
  }
  --m_pendingRequests;
  if (m_pendingRequests <= 0) {
    m_pendingRequests = 0;
    emit fetchCompleted();
  }
}

void ModelCatalogManager::cancelActiveReplies() {
  const QSet<QNetworkReply *> replies = m_activeReplies;
  for (QNetworkReply *reply : replies) {
    if (reply && !reply->isFinished()) {
      reply->abort();
    }
  }
}

void ModelCatalogManager::fetchOpenAI(const QString &apiKey,
                                      quint64 generation) {
  QNetworkRequest request{QUrl("https://api.openai.com/v1/models")};
  request.setRawHeader("Authorization",
                       QByteArray("Bearer ") + apiKey.toUtf8());
  QNetworkReply *reply = m_networkManager->get(request);
  m_activeReplies.insert(reply);

  auto *timeout = new QTimer(reply);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, reply, [reply]() {
    reply->setProperty("amtTimedOut", true);
    reply->abort();
  });
  timeout->start(m_requestTimeoutMs);

  connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
    m_activeReplies.remove(reply);
    reply->deleteLater();
    if (generation != m_generation) {
      return;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      setFailureStatus(ProviderKind::OpenAI, "OpenAI",
                       replyFailureSummary(reply));
    } else {
      QJsonParseError parseError;
      const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
      if (parseError.error != QJsonParseError::NoError) {
        setFailureStatus(
            ProviderKind::OpenAI, "OpenAI",
            QString("parse error %1").arg(parseError.errorString()));
      } else {
        QVector<ModelCatalogEntry> models;
        for (const QJsonValue &value :
             document.object().value("data").toArray()) {
          const QString id = value.toObject().value("id").toString();
          if ((id.startsWith("gpt-") || id.startsWith("o1") ||
               id.startsWith("o3") || id.startsWith("o4")) &&
              isModelAllowed(id)) {
            models.append(
                {id, preferredModelDisplayName(ProviderKind::OpenAI, id, id),
                 false, true});
          }
        }
        if (models.isEmpty()) {
          setFailureStatus(ProviderKind::OpenAI, "OpenAI",
                           "no usable text models returned");
        } else {
          appendDeduplicated(models,
                             modelCatalogForProvider(ProviderKind::OpenAI));
          m_dynamicCatalogs.insert(ProviderKind::OpenAI, models);
          setStatus(ProviderKind::OpenAI,
                    QString("Updated just now: %1 OpenAI models.").arg(models.size()),
                    true);
        }
      }
    }
    checkCompletion(generation);
  });
}

void ModelCatalogManager::fetchGemini(const QString &apiKey,
                                      quint64 generation) {
  QUrl url("https://generativelanguage.googleapis.com/v1beta/models");
  QUrlQuery query;
  query.addQueryItem("key", apiKey);
  url.setQuery(query);
  QNetworkReply *reply = m_networkManager->get(QNetworkRequest(url));
  m_activeReplies.insert(reply);

  auto *timeout = new QTimer(reply);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, reply, [reply]() {
    reply->setProperty("amtTimedOut", true);
    reply->abort();
  });
  timeout->start(m_requestTimeoutMs);

  connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
    m_activeReplies.remove(reply);
    reply->deleteLater();
    if (generation != m_generation) {
      return;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      setFailureStatus(ProviderKind::Gemini, "Google",
                       replyFailureSummary(reply));
    } else {
      QJsonParseError parseError;
      const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
      if (parseError.error != QJsonParseError::NoError) {
        setFailureStatus(
            ProviderKind::Gemini, "Google",
            QString("parse error %1").arg(parseError.errorString()));
      } else {
        QVector<ModelCatalogEntry> models;
        for (const QJsonValue &value :
             document.object().value("models").toArray()) {
          const QJsonObject model = value.toObject();
          const QString name = model.value("name").toString();
          const QString displayName = model.value("displayName").toString();
          if (name.startsWith("models/gemini-")) {
            const QString id = name.mid(7);
            if (isModelAllowed(id) && isModelAllowed(displayName) &&
                supportsGeminiGenerateContent(model)) {
              models.append({id,
                             preferredModelDisplayName(ProviderKind::Gemini, id,
                                                       displayName),
                             false, true});
            }
          }
        }
        if (models.isEmpty()) {
          setFailureStatus(ProviderKind::Gemini, "Google",
                           "no generateContent Gemini models returned");
        } else {
          appendDeduplicated(models,
                             modelCatalogForProvider(ProviderKind::Gemini));
          m_dynamicCatalogs.insert(ProviderKind::Gemini, models);
          setStatus(ProviderKind::Gemini,
                    QString("Updated just now: %1 Google models.").arg(models.size()),
                    true);
        }
      }
    }
    checkCompletion(generation);
  });
}

void ModelCatalogManager::fetchAnthropic(const QString &apiKey,
                                         quint64 generation) {
  QNetworkRequest request{QUrl("https://api.anthropic.com/v1/models")};
  request.setRawHeader("x-api-key", apiKey.toUtf8());
  request.setRawHeader("anthropic-version", "2023-06-01");
  QNetworkReply *reply = m_networkManager->get(request);
  m_activeReplies.insert(reply);

  auto *timeout = new QTimer(reply);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, reply, [reply]() {
    reply->setProperty("amtTimedOut", true);
    reply->abort();
  });
  timeout->start(m_requestTimeoutMs);

  connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
    m_activeReplies.remove(reply);
    reply->deleteLater();
    if (generation != m_generation) {
      return;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      setFailureStatus(ProviderKind::Anthropic, "Anthropic",
                       replyFailureSummary(reply));
    } else {
      QJsonParseError parseError;
      const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
      if (parseError.error != QJsonParseError::NoError) {
        setFailureStatus(
            ProviderKind::Anthropic, "Anthropic",
            QString("parse error %1").arg(parseError.errorString()));
      } else {
        QVector<ModelCatalogEntry> models;
        for (const QJsonValue &value :
             document.object().value("data").toArray()) {
          const QJsonObject object = value.toObject();
          const QString id = object.value("id").toString();
          const QString displayName = object.value("display_name").toString();
          if (id.startsWith("claude-") && isModelAllowed(id) &&
              isModelAllowed(displayName)) {
            models.append({id,
                           preferredModelDisplayName(ProviderKind::Anthropic,
                                                     id, displayName),
                           false, true});
          }
        }
        if (models.isEmpty()) {
          setFailureStatus(ProviderKind::Anthropic, "Anthropic",
                           "no usable text models returned");
        } else {
          appendDeduplicated(models,
                             modelCatalogForProvider(ProviderKind::Anthropic));
          m_dynamicCatalogs.insert(ProviderKind::Anthropic, models);
          setStatus(ProviderKind::Anthropic,
                    QString("Updated just now: %1 Anthropic models.").arg(models.size()),
                    true);
        }
      }
    }
    checkCompletion(generation);
  });
}

} // namespace amt
