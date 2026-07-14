#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QSignalSpy>
#include <QTimer>

#include <algorithm>
#include <cstring>

#include "services/model_catalog_manager.h"

using namespace amt;

namespace {

struct ReplyPlan {
  QByteArray body;
  int statusCode = 200;
  QNetworkReply::NetworkError error = QNetworkReply::NoError;
  int delayMs = 0;
};

class FakeReply final : public QNetworkReply {
public:
  FakeReply(const QNetworkRequest &request, const ReplyPlan &plan,
            QObject *parent)
      : QNetworkReply(parent), m_plan(plan) {
    setRequest(request);
    setUrl(request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    if (m_plan.delayMs >= 0) {
      QTimer::singleShot(m_plan.delayMs, this, [this]() { complete(); });
    }
  }

  void abort() override {
    if (m_completed) {
      return;
    }
    m_plan.error = QNetworkReply::OperationCanceledError;
    complete();
  }

  qint64 bytesAvailable() const override {
    return (m_plan.body.size() - m_offset) + QNetworkReply::bytesAvailable();
  }

protected:
  qint64 readData(char *data, qint64 maxSize) override {
    if (m_offset >= m_plan.body.size()) {
      return -1;
    }
    const qint64 length =
        qMin(maxSize, static_cast<qint64>(m_plan.body.size() - m_offset));
    std::memcpy(data, m_plan.body.constData() + m_offset,
                static_cast<size_t>(length));
    m_offset += length;
    return length;
  }

private:
  void complete() {
    if (m_completed) {
      return;
    }
    m_completed = true;
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, m_plan.statusCode);
    if (m_plan.error != QNetworkReply::NoError) {
      setError(m_plan.error, "sensitive transport detail must not escape");
    }
    setFinished(true);
    if (!m_plan.body.isEmpty()) {
      emit readyRead();
    }
    emit finished();
  }

  ReplyPlan m_plan;
  qint64 m_offset = 0;
  bool m_completed = false;
};

class FakeNetworkAccessManager final : public QNetworkAccessManager {
public:
  void enqueue(const ReplyPlan &plan) { m_plans.enqueue(plan); }

protected:
  QNetworkReply *createRequest(Operation, const QNetworkRequest &request,
                               QIODevice *) override {
    ReplyPlan plan;
    if (!m_plans.isEmpty()) {
      plan = m_plans.dequeue();
    } else {
      plan.statusCode = 500;
      plan.error = QNetworkReply::UnknownNetworkError;
    }
    return new FakeReply(request, plan, this);
  }

private:
  QQueue<ReplyPlan> m_plans;
};

QByteArray openAiModels(std::initializer_list<const char *> ids) {
  QJsonArray data;
  for (const char *id : ids) {
    data.append(QJsonObject{{"id", id}});
  }
  return QJsonDocument(QJsonObject{{"data", data}})
      .toJson(QJsonDocument::Compact);
}

QVariantMap openAiStatus(const ModelCatalogManager &manager) {
  for (const QVariant &value : manager.fetchStatuses()) {
    const QVariantMap row = value.toMap();
    if (row.value("provider").toString() == "OpenAI") {
      return row;
    }
  }
  return {};
}

int modelCount(const QVector<ModelCatalogEntry> &catalog, const QString &id) {
  return static_cast<int>(std::count_if(
      catalog.cbegin(), catalog.cend(),
      [&id](const ModelCatalogEntry &entry) { return entry.id == id; }));
}

} // namespace

class ModelCatalogTests final : public QObject {
  Q_OBJECT

private slots:
  void overlappingRefreshUsesNewestGenerationAndRetainsLastGood();
  void credentialsLoadOncePerProviderAndTimeoutCompletes();
};

void ModelCatalogTests::
    overlappingRefreshUsesNewestGenerationAndRetainsLastGood() {
  FakeNetworkAccessManager network;
  network.enqueue(
      {openAiModels({"gpt-old"}), 200, QNetworkReply::NoError, 100});
  network.enqueue(
      {openAiModels({"gpt-new", "gpt-new"}), 200, QNetworkReply::NoError, 0});
  network.enqueue({QByteArray("secret response body"), 500,
                   QNetworkReply::ContentAccessDenied, 0});

  ModelCatalogManager manager(
      [](ProviderKind provider) {
        return provider == ProviderKind::OpenAI ? QString("test-key")
                                                : QString{};
      },
      &network, 1000);
  QSignalSpy completed(&manager, &ModelCatalogManager::fetchCompleted);

  manager.fetchModelsAsync();
  manager.fetchModelsAsync();
  QTRY_COMPARE(completed.count(), 1);
  QVector<ModelCatalogEntry> catalog =
      manager.catalogForProvider(ProviderKind::OpenAI);
  QCOMPARE(modelCount(catalog, "gpt-new"), 1);
  QCOMPARE(modelCount(catalog, "gpt-old"), 0);

  manager.fetchModelsAsync();
  QTRY_COMPARE(completed.count(), 2);
  catalog = manager.catalogForProvider(ProviderKind::OpenAI);
  QCOMPARE(modelCount(catalog, "gpt-new"), 1);
  const QVariantMap status = openAiStatus(manager);
  QVERIFY(status.value("fallback").toBool());
  QVERIFY(status.value("success").toBool());
  QVERIFY(!status.value("message").toString().contains("secret response body"));
  QVERIFY(!status.value("message").toString().contains(
      "sensitive transport detail"));
}

void ModelCatalogTests::credentialsLoadOncePerProviderAndTimeoutCompletes() {
  FakeNetworkAccessManager network;
  ReplyPlan timeoutPlan;
  timeoutPlan.delayMs = -1;
  network.enqueue(timeoutPlan);

  QMap<ProviderKind, int> loads;
  ModelCatalogManager manager(
      [&loads](ProviderKind provider) {
        loads[provider] += 1;
        return provider == ProviderKind::OpenAI ? QString("test-key")
                                                : QString{};
      },
      &network, 10);
  QSignalSpy completed(&manager, &ModelCatalogManager::fetchCompleted);
  manager.fetchModelsAsync();
  QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 1000);
  QCOMPARE(loads.value(ProviderKind::OpenAI), 1);
  QCOMPARE(loads.value(ProviderKind::Gemini), 1);
  QCOMPARE(loads.value(ProviderKind::Anthropic), 1);
  QVERIFY(
      openAiStatus(manager).value("message").toString().contains("timeout"));
}

QTEST_GUILESS_MAIN(ModelCatalogTests)

#include "test_model_catalog.moc"
