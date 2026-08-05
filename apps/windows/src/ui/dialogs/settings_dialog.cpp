#include "ui/dialogs/settings_dialog.h"

#include "services/credential_store.h"
#include "services/model_catalog_manager.h"

#include <algorithm>

#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QProgressDialog>
#include <QEventLoop>
#include <QTimer>
#include <QVariant>

namespace amt {

namespace {

QSpinBox *makeSpinBox(QWidget *parent, int min, int max, int value)
{
    auto *spin = new QSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(value);
    return spin;
}

void fillBudgetEditors(const BudgetPolicy &policy,
                       QSpinBox *tokensPerPhase,
                       QSpinBox *totalTokens,
                       QSpinBox *maxRounds,
                       QSpinBox *phaseSeconds,
                       QSpinBox *sessionSeconds)
{
    tokensPerPhase->setValue(policy.maxTokensPerPhase);
    totalTokens->setValue(policy.maxTotalTokens);
    maxRounds->setValue(policy.maxRounds);
    phaseSeconds->setValue(policy.maxPhaseSeconds);
    sessionSeconds->setValue(policy.maxSessionSeconds);
}

BudgetPolicy readBudgetEditors(const BudgetPolicy &existing,
                               QSpinBox *tokensPerPhase,
                               QSpinBox *totalTokens,
                               QSpinBox *maxRounds,
                               QSpinBox *phaseSeconds,
                               QSpinBox *sessionSeconds)
{
    BudgetPolicy policy = existing;
    policy.maxTokensPerPhase = tokensPerPhase->value();
    policy.maxTotalTokens = totalTokens->value();
    policy.maxRounds = maxRounds->value();
    policy.maxPhaseSeconds = phaseSeconds->value();
    policy.maxSessionSeconds = sessionSeconds->value();
    return policy;
}

QVector<int> sortedTableIndexes(const QVector<std::shared_ptr<SessionState>> &tables)
{
    QVector<int> indexes;
    indexes.reserve(tables.size());
    for (int i = 0; i < tables.size(); ++i) {
        indexes.append(i);
    }
    std::sort(indexes.begin(), indexes.end(), [&tables](int lhs, int rhs) {
        const auto &a = *tables.at(lhs);
        const auto &b = *tables.at(rhs);
        if (a.updatedAt == b.updatedAt) {
            return a.title.toLower() < b.title.toLower();
        }
        return a.updatedAt > b.updatedAt;
    });
    return indexes;
}

void applyStopPolicyEditors(const StopPolicy &policy,
                            QCheckBox *stopOnBudgetCheck,
                            QCheckBox *stopOnSessionTimeoutCheck,
                            QCheckBox *stopOnPhaseTimeoutCheck,
                            QCheckBox *allowEarlyStopCheck)
{
    stopOnBudgetCheck->setChecked(policy.stopOnBudgetExceeded);
    stopOnSessionTimeoutCheck->setChecked(policy.stopOnSessionTimeout);
    stopOnPhaseTimeoutCheck->setChecked(policy.stopOnPhaseTimeout);
    allowEarlyStopCheck->setChecked(policy.allowEarlyStopByDecisionMaker);
}

void storeStopPolicyEditors(StopPolicy &policy,
                            const QCheckBox *stopOnBudgetCheck,
                            const QCheckBox *stopOnSessionTimeoutCheck,
                            const QCheckBox *stopOnPhaseTimeoutCheck,
                            const QCheckBox *allowEarlyStopCheck)
{
    policy.stopOnBudgetExceeded = stopOnBudgetCheck->isChecked();
    policy.stopOnSessionTimeout = stopOnSessionTimeoutCheck->isChecked();
    policy.stopOnPhaseTimeout = stopOnPhaseTimeoutCheck->isChecked();
    policy.allowEarlyStopByDecisionMaker = allowEarlyStopCheck->isChecked();
}

void setTableEditorsEnabled(bool enabled,
                            QSpinBox *tokensPerPhase,
                            QSpinBox *totalTokens,
                            QSpinBox *maxRounds,
                            QSpinBox *execQcLoops,
                            QSpinBox *phaseSeconds,
                            QSpinBox *sessionSeconds)
{
    tokensPerPhase->setEnabled(enabled);
    totalTokens->setEnabled(enabled);
    maxRounds->setEnabled(enabled);
    execQcLoops->setEnabled(enabled);
    phaseSeconds->setEnabled(enabled);
    sessionSeconds->setEnabled(enabled);
}

void storeTableSettings(SessionState &table,
                        bool useOverrides,
                        QSpinBox *tokensPerPhase,
                        QSpinBox *totalTokens,
                        QSpinBox *maxRounds,
                        QSpinBox *phaseSeconds,
                        QSpinBox *sessionSeconds,
                        QSpinBox *execQcLoops,
                        const QCheckBox *stopOnBudgetCheck,
                        const QCheckBox *stopOnSessionTimeoutCheck,
                        const QCheckBox *stopOnPhaseTimeoutCheck,
                        const QCheckBox *allowEarlyStopCheck)
{
    table.useBudgetOverrides = useOverrides;
    table.budgetOverrides = readBudgetEditors(
        table.budgetOverrides,
        tokensPerPhase,
        totalTokens,
        maxRounds,
        phaseSeconds,
        sessionSeconds);
    table.budgetOverrides.maxExecQcLoops = execQcLoops->value();
    storeStopPolicyEditors(
        table.stopPolicy,
        stopOnBudgetCheck,
        stopOnSessionTimeoutCheck,
        stopOnPhaseTimeoutCheck,
        allowEarlyStopCheck);
}

}

SettingsDialog::SettingsDialog(CredentialStore *credentialStore,
                               ModelCatalogManager *modelCatalogManager,
                               AppSettings *appSettings,
                               QVector<std::shared_ptr<SessionState>> *tables,
                               const QString &currentTableId,
                               QWidget *parent)
    : QDialog(parent),
      m_credentialStore(credentialStore),
      m_modelCatalogManager(modelCatalogManager),
      m_appSettings(appSettings),
      m_tables(tables),
      m_currentTableId(currentTableId),
      m_tabWidget(new QTabWidget(this)),
      m_openAiEdit(new QLineEdit(this)),
      m_geminiEdit(new QLineEdit(this)),
      m_anthropicEdit(new QLineEdit(this)),
      m_refreshModelsButton(new QPushButton("Refresh Models", this)),
      m_globalTokensPerPhaseSpin(makeSpinBox(this, 100, 1000000, appSettings->globalBudgetDefaults.maxTokensPerPhase)),
      m_globalTotalTokensSpin(makeSpinBox(this, 100, 2000000, appSettings->globalBudgetDefaults.maxTotalTokens)),
      m_globalMaxRoundsSpin(makeSpinBox(this, 1, 1000, appSettings->globalBudgetDefaults.maxRounds)),
      m_globalPhaseSecondsSpin(makeSpinBox(this, 1, 86400, appSettings->globalBudgetDefaults.maxPhaseSeconds)),
      m_globalSessionSecondsSpin(makeSpinBox(this, 1, 604800, appSettings->globalBudgetDefaults.maxSessionSeconds)),
      m_tableSelector(new QComboBox(this)),
      m_useTableOverridesCheck(new QCheckBox("Use custom hard stops for this table", this)),
      m_tableTokensPerPhaseSpin(makeSpinBox(this, 100, 1000000, appSettings->globalBudgetDefaults.maxTokensPerPhase)),
      m_tableTotalTokensSpin(makeSpinBox(this, 100, 2000000, appSettings->globalBudgetDefaults.maxTotalTokens)),
      m_tableMaxRoundsSpin(makeSpinBox(this, 1, 1000, appSettings->globalBudgetDefaults.maxRounds)),
      m_tableExecQcLoopsSpin(makeSpinBox(this, 1, 100, appSettings->globalBudgetDefaults.maxExecQcLoops)),
      m_tablePhaseSecondsSpin(makeSpinBox(this, 1, 86400, appSettings->globalBudgetDefaults.maxPhaseSeconds)),
      m_tableSessionSecondsSpin(makeSpinBox(this, 1, 604800, appSettings->globalBudgetDefaults.maxSessionSeconds)),
      m_stopOnBudgetCheck(new QCheckBox("Stop on budget exceeded", this)),
      m_stopOnSessionTimeoutCheck(new QCheckBox("Stop on session timeout", this)),
      m_stopOnPhaseTimeoutCheck(new QCheckBox("Stop on phase timeout", this)),
      m_allowEarlyStopCheck(new QCheckBox("Allow early stop by decision maker", this)),
      m_themeCombo(new QComboBox(this)),
      m_statusLabel(new QLabel(this))
{
    setWindowTitle("Settings");
    resize(760, 700);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabWidget, 1);

    auto *apiPage = new QWidget(this);
    auto *apiLayout = new QVBoxLayout(apiPage);
    auto *providerGroup = new QGroupBox("Provider Credentials", apiPage);
    auto *providerForm = new QFormLayout(providerGroup);
    m_openAiEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_geminiEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_anthropicEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_openAiEdit->setText(m_credentialStore->loadApiKey(ProviderKind::OpenAI));
    m_geminiEdit->setText(m_credentialStore->loadApiKey(ProviderKind::Gemini));
    m_anthropicEdit->setText(m_credentialStore->loadApiKey(ProviderKind::Anthropic));
    providerForm->addRow("OpenAI API Key", m_openAiEdit);
    providerForm->addRow("Gemini API Key", m_geminiEdit);
    providerForm->addRow("Anthropic API Key", m_anthropicEdit);
    apiLayout->addWidget(providerGroup);

    auto *refreshGroup = new QGroupBox("Dynamic Models", apiPage);
    auto *refreshLayout = new QVBoxLayout(refreshGroup);
    auto *refreshDesc = new QLabel("Fetch the latest available models from your configured providers.", refreshGroup);
    refreshDesc->setWordWrap(true);
    refreshLayout->addWidget(refreshDesc);
    refreshLayout->addWidget(m_refreshModelsButton);
    connect(m_refreshModelsButton, &QPushButton::clicked, this, &SettingsDialog::onRefreshModels);
    apiLayout->addWidget(refreshGroup);

    apiLayout->addStretch(1);
    m_tabWidget->addTab(apiPage, "API Keys");

    auto *hardStopsPage = new QWidget(this);
    auto *hardStopsLayout = new QVBoxLayout(hardStopsPage);
    auto *globalGroup = new QGroupBox("Global Hard Stops", hardStopsPage);
    auto *globalForm = new QFormLayout(globalGroup);
    globalForm->addRow("Max tokens per phase", m_globalTokensPerPhaseSpin);
    globalForm->addRow("Max total tokens", m_globalTotalTokensSpin);
    globalForm->addRow("Max rounds (per phase)", m_globalMaxRoundsSpin);
    globalForm->addRow("Phase time limit (s)", m_globalPhaseSecondsSpin);
    globalForm->addRow("Session time limit (s)", m_globalSessionSecondsSpin);
    hardStopsLayout->addWidget(globalGroup);

    auto *tableGroup = new QGroupBox("Table Hard Stops", hardStopsPage);
    auto *tableForm = new QFormLayout(tableGroup);
    tableForm->addRow("Meeting Table", m_tableSelector);
    tableForm->addRow(QString(), m_useTableOverridesCheck);
    tableForm->addRow("Max tokens per phase", m_tableTokensPerPhaseSpin);
    tableForm->addRow("Max total tokens", m_tableTotalTokensSpin);
    tableForm->addRow("Max rounds (per phase)", m_tableMaxRoundsSpin);
    tableForm->addRow("Max Exec/QC loops", m_tableExecQcLoopsSpin);
    tableForm->addRow("Phase time limit (s)", m_tablePhaseSecondsSpin);
    tableForm->addRow("Session time limit (s)", m_tableSessionSecondsSpin);

    auto *stopPolicyGroup = new QGroupBox("Stop Policy", hardStopsPage);
    auto *stopPolicyForm = new QFormLayout(stopPolicyGroup);
    stopPolicyForm->addRow(m_stopOnBudgetCheck);
    stopPolicyForm->addRow(m_stopOnSessionTimeoutCheck);
    stopPolicyForm->addRow(m_stopOnPhaseTimeoutCheck);
    stopPolicyForm->addRow(m_allowEarlyStopCheck);
    m_stopOnBudgetCheck->setChecked(true);
    m_stopOnSessionTimeoutCheck->setChecked(true);
    m_stopOnPhaseTimeoutCheck->setChecked(true);
    m_allowEarlyStopCheck->setChecked(true);
    hardStopsLayout->addWidget(tableGroup);
    hardStopsLayout->addWidget(stopPolicyGroup);
    hardStopsLayout->addStretch(1);
    m_tabWidget->addTab(hardStopsPage, "Hard Stops");

    auto *visualPage = new QWidget(this);
    auto *visualLayout = new QVBoxLayout(visualPage);
    auto *visualGroup = new QGroupBox("Appearance", visualPage);
    auto *visualForm = new QFormLayout(visualGroup);
    m_themeCombo->addItems({"Light", "Dark"});
    m_themeCombo->setCurrentText(toString(m_appSettings->theme));
    visualForm->addRow("Theme", m_themeCombo);
    visualLayout->addWidget(visualGroup);
    visualLayout->addStretch(1);
    m_tabWidget->addTab(visualPage, "Visual");

    if (m_tables) {
        const auto indexes = sortedTableIndexes(*m_tables);
        for (int index : indexes) {
            const auto &table = m_tables->at(index);
            m_tableSelector->addItem(table->title, table->tableId);
        }
    }
    const int currentIndex = m_tableSelector->findData(m_currentTableId);
    if (currentIndex >= 0) {
        m_tableSelector->setCurrentIndex(currentIndex);
    }
    connect(m_tableSelector, &QComboBox::currentIndexChanged, this, &SettingsDialog::handleSelectedTableChanged);
    connect(m_useTableOverridesCheck, &QCheckBox::toggled, this, &SettingsDialog::loadSelectedTableSettings);
    m_activeTableId = m_currentTableId;
    loadSelectedTableSettings();

    layout->addWidget(m_statusLabel);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::saveKeys);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString SettingsDialog::selectedTableId() const
{
    return m_tableSelector->currentData().toString();
}

void SettingsDialog::onRefreshModels()
{
    QString error;
    m_credentialStore->saveApiKey(ProviderKind::OpenAI, m_openAiEdit->text(), &error, false);
    m_credentialStore->saveApiKey(ProviderKind::Gemini, m_geminiEdit->text(), &error, false);
    m_credentialStore->saveApiKey(ProviderKind::Anthropic, m_anthropicEdit->text(), &error, false);

    QProgressDialog progress("Fetching available models...", "Cancel", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.show();

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(m_modelCatalogManager, &ModelCatalogManager::fetchCompleted, &loop, &QEventLoop::quit);

    m_modelCatalogManager->fetchModelsAsync();
    timeoutTimer.start(5000);
    loop.exec();

    QStringList statusLines;
    const QVariantList statuses = m_modelCatalogManager->fetchStatuses();
    for (const QVariant &statusValue : statuses) {
        const QVariantMap status = statusValue.toMap();
        statusLines.append(status.value("message").toString());
    }
    m_statusLabel->setText(statusLines.isEmpty()
                               ? "Model refresh completed."
                               : statusLines.join("\n"));
}

void SettingsDialog::saveKeys()
{
    QStringList errors;
    QString error;
    if (!m_credentialStore->saveApiKey(ProviderKind::OpenAI, m_openAiEdit->text(), &error, true)) {
        errors.append(error);
    }
    error.clear();
    if (!m_credentialStore->saveApiKey(ProviderKind::Gemini, m_geminiEdit->text(), &error, true)) {
        errors.append(error);
    }
    error.clear();
    if (!m_credentialStore->saveApiKey(ProviderKind::Anthropic, m_anthropicEdit->text(), &error, true)) {
        errors.append(error);
    }

    m_appSettings->globalBudgetDefaults = readBudgetEditors(
        m_appSettings->globalBudgetDefaults,
        m_globalTokensPerPhaseSpin,
        m_globalTotalTokensSpin,
        m_globalMaxRoundsSpin,
        m_globalPhaseSecondsSpin,
        m_globalSessionSecondsSpin);
    m_appSettings->theme = themeModeFromString(m_themeCombo->currentText());

    if (auto *table = selectedTable()) {
        storeTableSettings(
            *table,
            m_useTableOverridesCheck->isChecked(),
            m_tableTokensPerPhaseSpin,
            m_tableTotalTokensSpin,
            m_tableMaxRoundsSpin,
            m_tablePhaseSecondsSpin,
            m_tableSessionSecondsSpin,
            m_tableExecQcLoopsSpin,
            m_stopOnBudgetCheck,
            m_stopOnSessionTimeoutCheck,
            m_stopOnPhaseTimeoutCheck,
            m_allowEarlyStopCheck);
    }

    m_statusLabel->setText(errors.join("\n"));
    accept();
}

void SettingsDialog::handleSelectedTableChanged()
{
    saveActiveTableSettings();
    m_activeTableId = m_tableSelector->currentData().toString();
    loadSelectedTableSettings();
}

void SettingsDialog::saveActiveTableSettings()
{
    if (!m_tables || m_activeTableId.isEmpty()) {
        return;
    }
    for (auto &table : *m_tables) {
        if (!table || table->tableId != m_activeTableId) {
            continue;
        }
        storeTableSettings(
            *table,
            m_useTableOverridesCheck->isChecked(),
            m_tableTokensPerPhaseSpin,
            m_tableTotalTokensSpin,
            m_tableMaxRoundsSpin,
            m_tablePhaseSecondsSpin,
            m_tableSessionSecondsSpin,
            m_tableExecQcLoopsSpin,
            m_stopOnBudgetCheck,
            m_stopOnSessionTimeoutCheck,
            m_stopOnPhaseTimeoutCheck,
            m_allowEarlyStopCheck);
        break;
    }
}

void SettingsDialog::loadSelectedTableSettings()
{
    auto *table = selectedTable();
    const bool hasTable = table != nullptr;
    m_useTableOverridesCheck->setEnabled(hasTable);

    if (!hasTable) {
        m_useTableOverridesCheck->setChecked(false);
        setTableEditorsEnabled(
            false,
            m_tableTokensPerPhaseSpin,
            m_tableTotalTokensSpin,
            m_tableMaxRoundsSpin,
            m_tableExecQcLoopsSpin,
            m_tablePhaseSecondsSpin,
            m_tableSessionSecondsSpin);
        m_stopOnBudgetCheck->setEnabled(false);
        m_stopOnSessionTimeoutCheck->setEnabled(false);
        m_stopOnPhaseTimeoutCheck->setEnabled(false);
        m_allowEarlyStopCheck->setEnabled(false);
        fillBudgetEditors(
            m_appSettings->globalBudgetDefaults,
            m_tableTokensPerPhaseSpin,
            m_tableTotalTokensSpin,
            m_tableMaxRoundsSpin,
            m_tablePhaseSecondsSpin,
            m_tableSessionSecondsSpin);
        m_tableExecQcLoopsSpin->setValue(m_appSettings->globalBudgetDefaults.maxExecQcLoops);
        return;
    }

    const QSignalBlocker blocker(m_useTableOverridesCheck);
    m_useTableOverridesCheck->setChecked(table->useBudgetOverrides);
    const bool useOverrides = m_useTableOverridesCheck->isChecked();
    setTableEditorsEnabled(
        useOverrides,
        m_tableTokensPerPhaseSpin,
        m_tableTotalTokensSpin,
        m_tableMaxRoundsSpin,
        m_tableExecQcLoopsSpin,
        m_tablePhaseSecondsSpin,
        m_tableSessionSecondsSpin);
    m_stopOnBudgetCheck->setEnabled(hasTable);
    m_stopOnSessionTimeoutCheck->setEnabled(hasTable);
    m_stopOnPhaseTimeoutCheck->setEnabled(hasTable);
    m_allowEarlyStopCheck->setEnabled(hasTable);

    const BudgetPolicy &effectivePolicy = useOverrides ? table->budgetOverrides : m_appSettings->globalBudgetDefaults;
    fillBudgetEditors(
        effectivePolicy,
        m_tableTokensPerPhaseSpin,
        m_tableTotalTokensSpin,
        m_tableMaxRoundsSpin,
        m_tablePhaseSecondsSpin,
        m_tableSessionSecondsSpin);
    m_tableExecQcLoopsSpin->setValue(effectivePolicy.maxExecQcLoops);

    applyStopPolicyEditors(
        table->stopPolicy,
        m_stopOnBudgetCheck,
        m_stopOnSessionTimeoutCheck,
        m_stopOnPhaseTimeoutCheck,
        m_allowEarlyStopCheck);
}

SessionState *SettingsDialog::selectedTable() const
{
    if (!m_tables) {
        return nullptr;
    }
    const QString tableId = m_tableSelector->currentData().toString();
    for (auto &table : *m_tables) {
        if (table && table->tableId == tableId) {
            return table.get();
        }
    }
    return nullptr;
}

}
