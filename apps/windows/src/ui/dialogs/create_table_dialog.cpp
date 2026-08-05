#include "ui/dialogs/create_table_dialog.h"

#include "services/model_catalog_manager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QUuid>

namespace amt {

CreateTableDialog::CreateTableDialog(ModelCatalogManager *modelCatalogManager, QWidget *parent)
    : QDialog(parent),
      m_modelCatalogManager(modelCatalogManager),
      m_nameEdit(new QLineEdit(this)),
      m_seatCountSpin(new QSpinBox(this))
{
    setWindowTitle("Create Meeting Table");
    resize(900, 520);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    m_nameEdit->setText("New Meeting Table");
    m_seatCountSpin->setRange(1, 8);
    m_seatCountSpin->setValue(4);
    form->addRow("Table Name", m_nameEdit);
    form->addRow("Seat Count", m_seatCountSpin);
    layout->addLayout(form);

    auto *group = new QGroupBox("Seat Assignments", this);
    auto *grid = new QGridLayout(group);
    grid->addWidget(new QLabel("Use"), 0, 0);
    grid->addWidget(new QLabel("Name"), 0, 1);
    grid->addWidget(new QLabel("Provider"), 0, 2);
    grid->addWidget(new QLabel("Model"), 0, 3);
    grid->addWidget(new QLabel("Effort"), 0, 4);
    grid->addWidget(new QLabel("Role"), 0, 5);

    for (int row = 0; row < 8; ++row) {
        SeatEditorRow seatRow;
        seatRow.enabledCheck = new QCheckBox(group);
        seatRow.enabledCheck->setChecked(false);
        seatRow.nameEdit = new QLineEdit(group);
        seatRow.providerCombo = new QComboBox(group);
        seatRow.providerCombo->addItems({"ChatGPT", "Gemini", "Claude"});
        seatRow.modelCombo = new QComboBox(group);
        seatRow.effortCombo = new QComboBox(group);
        seatRow.effortCombo->addItems({"Auto", "Light", "Balanced", "Deep"});
        seatRow.roleCombo = new QComboBox(group);
        seatRow.roleCombo->addItems({"Participant", "Final Decision Maker", "Lead Planner", "Lead Executioner", "Lead Quality Control"});

        seatRow.nameEdit->setText(QString("Seat %1").arg(row + 1));

        grid->addWidget(seatRow.enabledCheck, row + 1, 0);
        grid->addWidget(seatRow.nameEdit, row + 1, 1);
        grid->addWidget(seatRow.providerCombo, row + 1, 2);
        grid->addWidget(seatRow.modelCombo, row + 1, 3);
        grid->addWidget(seatRow.effortCombo, row + 1, 4);
        grid->addWidget(seatRow.roleCombo, row + 1, 5);
        m_rows.append(seatRow);
    }

    for (int row = 0; row < m_rows.size(); ++row) {
        syncModelChoices(row, true);
        m_rows[row].modelCombo->setCurrentIndex(0);
        m_rows[row].roleCombo->setCurrentIndex(0);
        connect(m_rows[row].providerCombo, &QComboBox::currentIndexChanged, this, [this, row](int) { syncModelChoices(row, false); });
    }

    layout->addWidget(group);

    connect(m_seatCountSpin, &QSpinBox::valueChanged, this, [this](int) { syncSeatAvailability(); });
    syncSeatAvailability();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &CreateTableDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

SessionState CreateTableDialog::tableDefinition() const
{
    if (m_hasAcceptedDefinition) {
        return m_cachedDefinition;
    }

    SessionState state;
    state.tableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    state.title = m_nameEdit->text().trimmed().isEmpty() ? "New Meeting Table" : m_nameEdit->text().trimmed();
    state.updatedAt = QDateTime::currentDateTimeUtc();
    state.phase = Phase::Idle;
    state.round = 1;

    for (int i = 0; i < m_rows.size(); ++i) {
        const auto &row = m_rows.at(i);
        if (!row.enabledCheck->isChecked()) {
            continue;
        }
        SeatConfig seat;
        seat.seatId = QString("seat-%1").arg(i + 1);
        seat.displayName = row.nameEdit->text().trimmed().isEmpty() ? QString("Seat %1").arg(i + 1) : row.nameEdit->text().trimmed();
        seat.provider = providerKindFromIndex(row.providerCombo->currentIndex());
        seat.modelId = row.modelCombo->currentData().toString();
        seat.modelPreset = seat.modelId.isEmpty() ? QString() : row.modelCombo->currentText().trimmed();
        seat.effort = effortFromEditorIndex(row.effortCombo->currentIndex());
        seat.role = roleFromEditorIndex(row.roleCombo->currentIndex());
        seat.occupied = true;
        seat.enabled = true;
        normalizeSeatModel(seat);
        state.seats.append(seat);
    }
    state.finalDecisionMakerSeatId = findFinalDecisionMakerSeatId(state.seats);

    return state;
}

void CreateTableDialog::accept()
{
    const SessionState state = tableDefinition();
    const QString validationError = validateSeatRoleAssignments(state.seats);
    if (!validationError.isEmpty()) {
        QMessageBox::warning(this, "Create Meeting Table", validationError);
        return;
    }
    m_cachedDefinition = state;
    m_hasAcceptedDefinition = true;
    QDialog::accept();
}

void CreateTableDialog::syncSeatAvailability() const
{
    const int count = m_seatCountSpin->value();
    for (int i = 0; i < m_rows.size(); ++i) {
        const bool active = i < count;
        SeatConfig seat;
        seat.provider = providerKindFromIndex(m_rows[i].providerCombo->currentIndex());
        seat.modelId = m_rows[i].modelCombo->currentData().toString();
        seat.modelPreset = m_rows[i].modelCombo->currentText();
        m_rows[i].enabledCheck->setEnabled(active);
        m_rows[i].nameEdit->setEnabled(active);
        m_rows[i].providerCombo->setEnabled(active);
        m_rows[i].modelCombo->setEnabled(active);
        m_rows[i].effortCombo->setEnabled(active && seatSupportsEffort(seat));
        m_rows[i].roleCombo->setEnabled(active);
        if (!active) {
            m_rows[i].enabledCheck->setChecked(false);
        }
    }
}

void CreateTableDialog::syncModelChoices(int row, bool preserveSelection) const
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    const auto provider = providerKindFromIndex(m_rows[row].providerCombo->currentIndex());
    const QString currentId = m_rows[row].modelCombo->currentData().toString();
    const QString currentText = m_rows[row].modelCombo->currentText();
    m_rows[row].modelCombo->clear();
    m_rows[row].modelCombo->addItem("None", QString());
    for (const auto &entry : m_modelCatalogManager->catalogForProvider(provider)) {
        m_rows[row].modelCombo->addItem(entry.displayName, entry.id);
    }

    int idx = 0;
    if (preserveSelection) {
        idx = m_rows[row].modelCombo->findData(currentId);
        if (idx < 0) {
            idx = m_rows[row].modelCombo->findText(currentText);
        }
        if (idx < 0) {
            const auto legacy = findModelCatalogEntry(provider, currentText);
            if (!legacy.id.isEmpty()) {
                idx = m_rows[row].modelCombo->findData(legacy.id);
            }
        }
        if (idx < 0 && !currentText.trimmed().isEmpty()) {
            m_rows[row].modelCombo->addItem(currentText, currentId.isEmpty() ? currentText : currentId);
            idx = m_rows[row].modelCombo->count() - 1;
        }
    }
    m_rows[row].modelCombo->setCurrentIndex(qMax(0, idx));

    SeatConfig seat;
    seat.provider = provider;
    seat.modelId = m_rows[row].modelCombo->currentData().toString();
    seat.modelPreset = m_rows[row].modelCombo->currentText();
    const bool supportsEffort = seatSupportsEffort(seat);
    if (!supportsEffort) {
        m_rows[row].effortCombo->setCurrentIndex(0);
    }
    m_rows[row].effortCombo->setEnabled(supportsEffort && m_rows[row].enabledCheck->isChecked());
}

}
