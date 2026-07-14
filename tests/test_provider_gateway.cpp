#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QThread>
#include <QUrlQuery>

#include "providers/provider_gateway.h"

using namespace amt;

namespace {

constexpr auto unknownOutcomeMessage =
    "The provider may have completed the request, but the app did not receive a confirmed result. Trying again could duplicate provider work or usage.";
constexpr auto syntheticKey = "synthetic-unit-marker";

enum ProviderMode {
    OpenAiMode,
    GeminiMode,
    GeminiSearchMode,
    AnthropicMode
};

enum FailureScenario {
    TimeoutScenario,
    ConnectionLossScenario,
    TlsFailureScenario,
    RemoteCloseScenario,
    CancellationScenario,
    MalformedResponseScenario,
    DelayedSuccessScenario,
    SuccessScenario
};

ProviderRequest requestFor(int mode)
{
    ProviderRequest request;
    request.requestId = "logical-operation";
    request.sessionId = "session";
    request.seatId = "seat";
    request.apiKey = syntheticKey;
    request.phase = mode == GeminiSearchMode ? Phase::Research : Phase::Planning;
    request.prompt.insert("instruction", "Synthetic provider transport test");
    switch (mode) {
    case OpenAiMode:
        request.provider = ProviderKind::OpenAI;
        request.model = "gpt-5-mini";
        break;
    case GeminiMode:
    case GeminiSearchMode:
        request.provider = ProviderKind::Gemini;
        request.model = "gemini-2.5-flash";
        break;
    case AnthropicMode:
        request.provider = ProviderKind::Anthropic;
        request.model = "claude-sonnet-4";
        break;
    }
    return request;
}

QByteArray successBody(int mode)
{
    switch (mode) {
    case OpenAiMode:
        return QJsonDocument(QJsonObject{
            {"output", QJsonArray{QJsonObject{{"type", "output_text"}, {"text", "confirmed"}}}},
            {"usage", QJsonObject{{"input_tokens", 10}, {"output_tokens", 5}, {"total_tokens", 15}}}
        }).toJson(QJsonDocument::Compact);
    case GeminiMode:
    case GeminiSearchMode:
        return QJsonDocument(QJsonObject{
            {"candidates", QJsonArray{QJsonObject{{"content", QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", "confirmed"}}}}}}}}},
            {"usageMetadata", QJsonObject{{"promptTokenCount", 10}, {"candidatesTokenCount", 5}, {"totalTokenCount", 15}}}
        }).toJson(QJsonDocument::Compact);
    case AnthropicMode:
        return QJsonDocument(QJsonObject{
            {"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", "confirmed"}}}},
            {"usage", QJsonObject{{"input_tokens", 10}, {"output_tokens", 5}}}
        }).toJson(QJsonDocument::Compact);
    }
    return {};
}

QByteArray headerValue(const ProviderTestRequest &request, const QByteArray &name)
{
    for (const auto &header : request.headers) {
        if (header.first.compare(name, Qt::CaseInsensitive) == 0) {
            return header.second;
        }
    }
    return {};
}

void verifyRequestShape(const ProviderTestRequest &exchange, int mode)
{
    QCOMPARE(exchange.method, QByteArray("POST"));
    QCOMPARE(headerValue(exchange, "Content-Type"), QByteArray("application/json"));
    QVERIFY(headerValue(exchange, "Idempotency-Key").isEmpty());
    QVERIFY(!exchange.body.contains("logical-operation"));

    const QJsonObject body = QJsonDocument::fromJson(exchange.body).object();
    QVERIFY(!body.isEmpty());
    switch (mode) {
    case OpenAiMode:
        QCOMPARE(exchange.url, QUrl("https://api.openai.com/v1/responses"));
        QCOMPARE(headerValue(exchange, "Authorization"),
                 QByteArray("Bearer ") + syntheticKey);
        QCOMPARE(body.value("model").toString(), QString("gpt-5-mini"));
        QVERIFY(!body.value("input").toArray().isEmpty());
        break;
    case GeminiMode:
    case GeminiSearchMode: {
        QCOMPARE(exchange.url.scheme(), QString("https"));
        QCOMPARE(exchange.url.host(), QString("generativelanguage.googleapis.com"));
        QCOMPARE(exchange.url.path(), QString("/v1beta/models/gemini-2.5-flash:generateContent"));
        QCOMPARE(QUrlQuery(exchange.url).queryItemValue("key"), QString(syntheticKey));
        QVERIFY(!body.value("contents").toArray().isEmpty());
        QCOMPARE(body.contains("tools"), mode == GeminiSearchMode);
        if (mode == GeminiSearchMode) {
            const QJsonObject tool = body.value("tools").toArray().first().toObject();
            QVERIFY(tool.contains("google_search"));
        }
        break;
    }
    case AnthropicMode:
        QCOMPARE(exchange.url, QUrl("https://api.anthropic.com/v1/messages"));
        QCOMPARE(headerValue(exchange, "x-api-key"), QByteArray(syntheticKey));
        QCOMPARE(headerValue(exchange, "anthropic-version"), QByteArray("2023-06-01"));
        QCOMPARE(headerValue(exchange, "anthropic-beta"), QByteArray("files-api-2025-04-14"));
        QCOMPARE(body.value("model").toString(), QString("claude-sonnet-4"));
        QVERIFY(!body.value("messages").toArray().isEmpty());
        break;
    }
}

ProviderTestNetworkResult resultFor(int mode, int scenario)
{
    ProviderTestNetworkResult result;
    switch (scenario) {
    case TimeoutScenario:
        result.transportError = "timeout";
        result.networkErrorCode = QNetworkReply::TimeoutError;
        break;
    case ConnectionLossScenario:
        result.transportError = "network";
        result.networkErrorCode = QNetworkReply::TemporaryNetworkFailureError;
        break;
    case TlsFailureScenario:
        result.transportError = "network";
        result.networkErrorCode = QNetworkReply::SslHandshakeFailedError;
        result.sslErrorCount = 1;
        break;
    case RemoteCloseScenario:
        result.transportError = "network";
        result.networkErrorCode = QNetworkReply::RemoteHostClosedError;
        break;
    case CancellationScenario:
        result.transportError = "network";
        result.networkErrorCode = QNetworkReply::OperationCanceledError;
        result.cancelled = true;
        break;
    case MalformedResponseScenario:
        result.statusCode = 200;
        result.body = "{\"unexpected\":true}";
        break;
    case DelayedSuccessScenario:
        QThread::msleep(15);
        [[fallthrough]];
    case SuccessScenario:
        result.statusCode = 200;
        result.body = successBody(mode);
        break;
    }
    return result;
}

} // namespace

class ProviderGatewayTests final : public QObject
{
    Q_OBJECT

private slots:
    void generationPostIsNotReplayed_data();
    void generationPostIsNotReplayed();
    void definitePreTransmissionFailureDoesNotSend_data();
    void definitePreTransmissionFailureDoesNotSend();
    void definiteTransientRetryPolicyRemainsAvailable();
    void geminiRejectedReusableHandleIsNotReplayed();
};

void ProviderGatewayTests::generationPostIsNotReplayed_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("scenario");
    const QStringList modes{"openai", "gemini", "gemini-search", "anthropic"};
    const QStringList scenarios{
        "timeout", "connection-loss", "tls-failure", "remote-close",
        "cancellation", "malformed", "delayed-success", "success"
    };
    for (int mode = OpenAiMode; mode <= AnthropicMode; ++mode) {
        for (int scenario = TimeoutScenario; scenario <= SuccessScenario; ++scenario) {
            const QByteArray rowName = QString("%1-%2")
                                           .arg(modes.at(mode), scenarios.at(scenario))
                                           .toLatin1();
            QTest::newRow(rowName.constData()) << mode << scenario;
        }
    }
}

void ProviderGatewayTests::generationPostIsNotReplayed()
{
    QFETCH(int, mode);
    QFETCH(int, scenario);

    const ProviderRequest request = requestFor(mode);
    QVector<ProviderTestRequest> exchanges;
    const ProviderResponse response = ProviderGateway::processForTesting(
        request,
        [&](const ProviderTestRequest &exchange) {
            exchanges.append(exchange);
            return resultFor(mode, scenario);
        });

    QCOMPARE(exchanges.size(), 1);
    verifyRequestShape(exchanges.first(), mode);
    QVERIFY(!response.errorMessage.contains(syntheticKey));
    QVERIFY(!response.errorMessage.contains("https://"));
    QVERIFY(!response.errorMessage.contains("networkCode"));
    QVERIFY(!response.errorMessage.contains("ssl", Qt::CaseInsensitive));

    if (scenario == SuccessScenario || scenario == DelayedSuccessScenario) {
        QVERIFY(response.success);
        QCOMPARE(static_cast<int>(response.deliveryOutcome),
                 static_cast<int>(ProviderDeliveryOutcome::Succeeded));
        QCOMPARE(response.content, QString("confirmed"));
        QCOMPARE(response.usedTokens, 15);
    } else if (scenario == CancellationScenario) {
        QVERIFY(!response.success);
        QCOMPARE(static_cast<int>(response.deliveryOutcome),
                 static_cast<int>(ProviderDeliveryOutcome::Cancelled));
        QVERIFY(response.errorMessage.contains("cancelled"));
    } else {
        QVERIFY(!response.success);
        QCOMPARE(static_cast<int>(response.deliveryOutcome),
                 static_cast<int>(ProviderDeliveryOutcome::OutcomeUnknown));
        QCOMPARE(response.errorMessage, QString(unknownOutcomeMessage));
    }
}

void ProviderGatewayTests::definitePreTransmissionFailureDoesNotSend_data()
{
    QTest::addColumn<int>("mode");
    QTest::newRow("openai") << int(OpenAiMode);
    QTest::newRow("gemini") << int(GeminiMode);
    QTest::newRow("gemini-search") << int(GeminiSearchMode);
    QTest::newRow("anthropic") << int(AnthropicMode);
}

void ProviderGatewayTests::definitePreTransmissionFailureDoesNotSend()
{
    QFETCH(int, mode);
    ProviderRequest request = requestFor(mode);
    request.apiKey.clear();
    int invocations = 0;
    const ProviderResponse response = ProviderGateway::processForTesting(
        request,
        [&](const ProviderTestRequest &) {
            ++invocations;
            return ProviderTestNetworkResult{};
        });

    QCOMPARE(invocations, 0);
    QVERIFY(!response.success);
    QCOMPARE(static_cast<int>(response.deliveryOutcome),
             static_cast<int>(ProviderDeliveryOutcome::DefiniteFailure));
    QVERIFY(response.errorMessage.contains("blocked before sending"));
}

void ProviderGatewayTests::definiteTransientRetryPolicyRemainsAvailable()
{
    ProviderResponse response;
    response.success = false;
    response.deliveryOutcome = ProviderDeliveryOutcome::DefiniteFailure;
    response.errorMessage = "Synthetic upload failure http=503";
    QVERIFY(ProviderGateway::shouldRetryForTesting(response));

    response.errorMessage = "Synthetic non-transient failure";
    QVERIFY(!ProviderGateway::shouldRetryForTesting(response));

    response.errorMessage = "Synthetic failure http=503";
    response.deliveryOutcome = ProviderDeliveryOutcome::OutcomeUnknown;
    QVERIFY(!ProviderGateway::shouldRetryForTesting(response));
    response.deliveryOutcome = ProviderDeliveryOutcome::Cancelled;
    QVERIFY(!ProviderGateway::shouldRetryForTesting(response));
}

void ProviderGatewayTests::geminiRejectedReusableHandleIsNotReplayed()
{
    ProviderRequest request = requestFor(GeminiMode);
    request.prompt.insert("attachments", QJsonArray{QJsonObject{
        {"attachmentId", "attachment"},
        {"displayName", "synthetic.pdf"},
        {"filePath", "unused-synthetic-path"},
        {"providerHandle", "files/old|https://generativelanguage.googleapis.com/v1beta/files/old|application/pdf"}
    }});

    QVector<ProviderTestRequest> exchanges;
    const ProviderResponse response = ProviderGateway::processForTesting(
        request,
        [&](const ProviderTestRequest &exchange) {
            exchanges.append(exchange);
            ProviderTestNetworkResult result;
            result.statusCode = 400;
            result.body = "{\"error\":{\"message\":\"synthetic rejection\"}}";
            return result;
        });

    QCOMPARE(exchanges.size(), 1);
    verifyRequestShape(exchanges.first(), GeminiMode);
    QVERIFY(!response.success);
    QCOMPARE(static_cast<int>(response.deliveryOutcome),
             static_cast<int>(ProviderDeliveryOutcome::OutcomeUnknown));
    QCOMPARE(response.errorMessage, QString(unknownOutcomeMessage));
}

QTEST_GUILESS_MAIN(ProviderGatewayTests)

#include "test_provider_gateway.moc"
