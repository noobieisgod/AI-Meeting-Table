#pragma once

#include <QDialog>
#include <QVector>

#include "domain/models.h"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QComboBox;

namespace amt {

class ModelCatalogManager;

struct SeatEditorRow {
    QCheckBox *enabledCheck = nullptr;
    QLineEdit *nameEdit = nullptr;
    QComboBox *providerCombo = nullptr;
    QComboBox *modelCombo = nullptr;
    QComboBox *effortCombo = nullptr;
    QComboBox *roleCombo = nullptr;
};

class CreateTableDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CreateTableDialog(ModelCatalogManager *modelCatalogManager, QWidget *parent = nullptr);

    SessionState tableDefinition() const;

private slots:
    void accept() override;

private:
    void syncSeatAvailability() const;
    void syncModelChoices(int row, bool preserveSelection = false) const;

    ModelCatalogManager *m_modelCatalogManager;
    QLineEdit *m_nameEdit;
    QSpinBox *m_seatCountSpin;
    QVector<SeatEditorRow> m_rows;
    mutable bool m_hasAcceptedDefinition = false;
    mutable SessionState m_cachedDefinition;
};

}
