#pragma once

#include <QDialog>

#include "domain/models.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;

namespace amt {

class ModelCatalogManager;

class SeatEditorDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SeatEditorDialog(const SeatConfig &seat, int seatIndex, ModelCatalogManager *modelCatalogManager, QWidget *parent = nullptr);

    SeatConfig editedSeat() const;

private slots:
    void syncEnabledState();

private:
    void loadFromSeat(const SeatConfig &seat);
    void syncModelChoices(bool preserveSelection);

    int m_seatIndex;
    ModelCatalogManager *m_modelCatalogManager;
    QCheckBox *m_enabledCheck;
    QLineEdit *m_nameEdit;
    QComboBox *m_providerCombo;
    QComboBox *m_modelCombo;
    QComboBox *m_effortCombo;
    QComboBox *m_roleCombo;
    QLabel *m_applyHint;
    QLabel *m_effortHint;
};

}
