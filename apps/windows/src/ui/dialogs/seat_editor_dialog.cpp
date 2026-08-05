#include "ui/dialogs/seat_editor_dialog.h"

#include "services/model_catalog_manager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace amt {

SeatEditorDialog::SeatEditorDialog(const SeatConfig &seat, int seatIndex, ModelCatalogManager *modelCatalogManager, QWidget *parent)
    : QDialog(parent),
      m_seatIndex(seatIndex),
      m_modelCatalogManager(modelCatalogManager),
      m_enabledCheck(new QCheckBox(this)),
      m_nameEdit(new QLineEdit(this)),
      m_providerCombo(new QComboBox(this)),
      m_modelCombo(new QComboBox(this)),
      m_effortCombo(new QComboBox(this)),
      m_roleCombo(new QComboBox(this)),
      m_applyHint(new QLabel(this)),
      m_effortHint(new QLabel(this))
{
    setWindowTitle(QString("Edit Seat %1").arg(seatIndex + 1));
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_enabledCheck->setText("Seat is occupied");
    m_providerCombo->addItems({"ChatGPT", "Gemini", "Claude"});
    m_effortCombo->addItems({"Auto", "Light", "Balanced", "Deep"});
    m_roleCombo->addItems({"Participant", "Final Decision Maker", "Lead Planner", "Lead Executioner", "Lead Quality Control"});
    m_applyHint->setWordWrap(true);
    m_effortHint->setWordWrap(true);

    form->addRow(m_enabledCheck);
    form->addRow("Display Name", m_nameEdit);
    form->addRow("Provider", m_providerCombo);
    form->addRow("Model", m_modelCombo);
    form->addRow("Effort", m_effortCombo);
    form->addRow("Role", m_roleCombo);
    layout->addLayout(form);
    layout->addWidget(m_effortHint);
    layout->addWidget(m_applyHint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_providerCombo, &QComboBox::currentIndexChanged, this, [this](int) { syncModelChoices(false); });
    connect(m_enabledCheck, &QCheckBox::toggled, this, &SeatEditorDialog::syncEnabledState);

    loadFromSeat(seat);
}

SeatConfig SeatEditorDialog::editedSeat() const
{
    SeatConfig seat;
    seat.seatId = QString("seat-%1").arg(m_seatIndex + 1);
    seat.provider = providerKindFromIndex(m_providerCombo->currentIndex());
    seat.modelId = m_modelCombo->currentData().toString();
    seat.modelPreset = seat.modelId.isEmpty() ? QString() : m_modelCombo->currentText().trimmed();
    seat.effort = effortFromEditorIndex(m_effortCombo->currentIndex());
    seat.role = roleFromEditorIndex(m_roleCombo->currentIndex());
    seat.displayName = m_nameEdit->text().trimmed();
    if (seat.displayName.isEmpty()) {
        seat.displayName = QString("Seat %1").arg(m_seatIndex + 1);
    }
    seat.occupied = m_enabledCheck->isChecked();
    seat.enabled = m_enabledCheck->isChecked();
    if (!seat.occupied) {
        seat.role = Role::None;
        seat.effort = ModelEffort::Auto;
        seat.displayName = QString("Seat %1").arg(m_seatIndex + 1);
    }
    return seat;
}

void SeatEditorDialog::syncModelChoices(bool preserveSelection)
{
    const auto provider = providerKindFromIndex(m_providerCombo->currentIndex());
    const QString currentModelId = m_modelCombo->currentData().toString();
    const QString currentDisplay = m_modelCombo->currentText();
    m_modelCombo->clear();
    m_modelCombo->addItem("None", QString());
    for (const auto &entry : m_modelCatalogManager->catalogForProvider(provider)) {
        m_modelCombo->addItem(entry.displayName, entry.id);
    }

    int idx = 0;
    if (preserveSelection) {
        idx = m_modelCombo->findData(currentModelId);
        if (idx < 0) {
            idx = m_modelCombo->findText(currentDisplay);
        }
        if (idx < 0) {
            const auto entry = findModelCatalogEntry(provider, currentDisplay);
            if (!entry.id.isEmpty()) {
                idx = m_modelCombo->findData(entry.id);
            }
        }
        if (idx < 0 && !currentDisplay.trimmed().isEmpty()) {
            m_modelCombo->addItem(currentDisplay, currentModelId.isEmpty() ? currentDisplay : currentModelId);
            idx = m_modelCombo->count() - 1;
        }
    }
    m_modelCombo->setCurrentIndex(qMax(0, idx));

    const SeatConfig previewSeat = editedSeat();
    const bool supportsEffort = seatSupportsEffort(previewSeat);
    if (!supportsEffort) {
        m_effortCombo->setCurrentIndex(0);
    }
    m_effortCombo->setEnabled(supportsEffort && m_enabledCheck->isChecked());
    m_effortHint->setText(supportsEffort
                              ? "Effort is available for this model."
                              : "Effort is not available for this model or custom legacy model.");
}

void SeatEditorDialog::syncEnabledState()
{
    const bool enabled = m_enabledCheck->isChecked();
    m_nameEdit->setEnabled(enabled);
    m_providerCombo->setEnabled(enabled);
    m_modelCombo->setEnabled(enabled);
    m_effortCombo->setEnabled(enabled && seatSupportsEffort(editedSeat()));
    m_roleCombo->setEnabled(enabled);
}

void SeatEditorDialog::loadFromSeat(const SeatConfig &seat)
{
    m_enabledCheck->setChecked(seat.occupied);
    m_nameEdit->setText(seat.displayName);
    m_providerCombo->setCurrentIndex(indexFromProviderKind(seat.provider));
    syncModelChoices(true);
    int idx = m_modelCombo->findData(seat.modelId);
    if (idx < 0) {
        idx = m_modelCombo->findText(effectiveModelName(seat));
    }
    const QString model = effectiveModelName(seat);
    if (idx < 0 && !model.isEmpty()) {
        m_modelCombo->addItem(model, effectiveModelId(seat));
        idx = m_modelCombo->count() - 1;
    }
    if (idx >= 0) {
        m_modelCombo->setCurrentIndex(idx);
    }
    m_effortCombo->setCurrentIndex(indexFromEffort(seat.effort));
    m_roleCombo->setCurrentIndex(indexFromRole(seat.role));
    m_applyHint->setText("If a phase is currently running, changes from this dialog will apply after the phase ends.");
    syncEnabledState();
}

}
