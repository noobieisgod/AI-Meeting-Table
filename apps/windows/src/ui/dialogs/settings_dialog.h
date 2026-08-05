#pragma once

#include <memory>
#include <QDialog>

#include "domain/models.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTabWidget;
class QCheckBox;
class QPushButton;

namespace amt {

class CredentialStore;
class ModelCatalogManager;

class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(CredentialStore *credentialStore,
                            ModelCatalogManager *modelCatalogManager,
                            AppSettings *appSettings,
                            QVector<std::shared_ptr<SessionState>> *tables,
                            const QString &currentTableId,
                            QWidget *parent = nullptr);
    QString selectedTableId() const;

private slots:
    void saveKeys();
    void handleSelectedTableChanged();
    void onRefreshModels();

private:
    void loadSelectedTableSettings();
    void saveActiveTableSettings();
    SessionState *selectedTable() const;

    CredentialStore *m_credentialStore;
    ModelCatalogManager *m_modelCatalogManager;
    AppSettings *m_appSettings;
    QVector<std::shared_ptr<SessionState>> *m_tables;
    QString m_currentTableId;
    QTabWidget *m_tabWidget;
    QLineEdit *m_openAiEdit;
    QLineEdit *m_geminiEdit;
    QLineEdit *m_anthropicEdit;
    QPushButton *m_refreshModelsButton;
    QSpinBox *m_globalTokensPerPhaseSpin;
    QSpinBox *m_globalTotalTokensSpin;
    QSpinBox *m_globalMaxRoundsSpin;
    QSpinBox *m_globalPhaseSecondsSpin;
    QSpinBox *m_globalSessionSecondsSpin;
    QComboBox *m_tableSelector;
    QCheckBox *m_useTableOverridesCheck;
    QSpinBox *m_tableTokensPerPhaseSpin;
    QSpinBox *m_tableTotalTokensSpin;
    QSpinBox *m_tableMaxRoundsSpin;
    QSpinBox *m_tableExecQcLoopsSpin;
    QSpinBox *m_tablePhaseSecondsSpin;
    QSpinBox *m_tableSessionSecondsSpin;
    QCheckBox *m_stopOnBudgetCheck;
    QCheckBox *m_stopOnSessionTimeoutCheck;
    QCheckBox *m_stopOnPhaseTimeoutCheck;
    QCheckBox *m_allowEarlyStopCheck;
    QComboBox *m_themeCombo;
    QLabel *m_statusLabel;
    QString m_activeTableId;
};

}
