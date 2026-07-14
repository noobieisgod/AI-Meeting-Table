#pragma once

#include <memory>
#include <QObject>
#include <QVector>

#include "core/event_bus.h"
#include "core/session_runner.h"
#include "core/workflow_engine.h"
#include "persistence/database_manager.h"
#include "providers/provider_gateway.h"
#include "services/artifact_manager.h"
#include "services/budget_manager.h"
#include "services/credential_store.h"
#include "services/model_catalog_manager.h"
#include "services/upload_manager.h"

namespace amt {

class ApplicationContext final : public QObject
{
    Q_OBJECT

public:
    using SessionHandle = std::shared_ptr<SessionState>;

    explicit ApplicationContext(QObject *parent = nullptr);

    bool initialize();
    QVector<SessionHandle> &tables();
    const QVector<SessionHandle> &tables() const;
    SessionState createSampleTable() const;
    bool save(const SessionState &state);
    bool saveExisting(const QString &tableId);
    bool removeTable(const QString &tableId);
    bool cleanupAttachmentFileIfUnreferenced(const QString &filePath) const;
    void saveAppSettings() const;
    void applyEffectiveBudgetPolicy(SessionState &state) const;
    void applyTheme() const;
    SessionHandle tableHandle(const QString &tableId) const;

    SessionRunner *sessionRunner();
    BudgetManager *budgetManager();
    const BudgetManager *budgetManager() const;
    CredentialStore *credentialStore();
    const CredentialStore *credentialStore() const;
    UploadManager *uploadManager();
    const UploadManager *uploadManager() const;
    ModelCatalogManager *modelCatalogManager();
    const ModelCatalogManager *modelCatalogManager() const;
    AppSettings &appSettings();
    const AppSettings &appSettings() const;

private:
    bool validateRestoredArtifacts(SessionState &state) const;
    void cleanupUnownedAttachmentFiles() const;

    QVector<SessionHandle> m_tables;
    DatabaseManager m_databaseManager;
    EventBus m_eventBus;
    WorkflowEngine m_workflowEngine;
    ProviderGateway m_providerGateway;
    BudgetManager m_budgetManager;
    CredentialStore m_credentialStore;
    ModelCatalogManager m_modelCatalogManager;
    ArtifactManager m_artifactManager;
    UploadManager m_uploadManager;
    SessionRunner m_sessionRunner;
    AppSettings m_appSettings;
};

} // namespace amt
