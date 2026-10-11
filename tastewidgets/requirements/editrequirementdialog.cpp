/*
   Copyright (C) 2025 European Space Agency - <maxime.perrotin@esa.int>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this program. If not, see <https://www.gnu.org/licenses/lgpl-2.1.html>.
*/
#include "editrequirementdialog.h"

#include "orbitrequirementcontext.h"

#include <QDir>

namespace requirement {

EditRequirementDialog::EditRequirementDialog(RequirementsModelBase *model, Requirement *requirement, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
    , m_requirement(requirement)
    , ui(new Ui::BasicRequirementDialog)
{
    ui->setupUi(this);
    const bool isSSS = (m_requirement->m_reqType.compare(k_SSSLabel) == 0);
    // SRS and SSS can contain different types of requirement.
    // Set the list of requirement types according to the specification type.
    const QStringList* typeListPtr = nullptr; 
    if(isSSS)
    {
        typeListPtr = &TypeListSSS;
    }
    else
    {
        typeListPtr = &TypeListSRS;
    }
    auto typeList = *typeListPtr;
    connect(ui->titleLineEdit, &QLineEdit::textChanged, this, &EditRequirementDialog::updateOkButton);
    connect(ui->browserButton, &QPushButton::clicked, this, &EditRequirementDialog::openIssueLink);
    connect(ui->descriptionTextEdit, &QTextEdit::textChanged, this, &EditRequirementDialog::updateOkButton);

    connect(ui->addRefButton, &QPushButton::clicked, this, &EditRequirementDialog::addParent);
    connect(ui->removeRefButton, &QPushButton::clicked, this, &EditRequirementDialog::removeParent);
    connect(ui->virtualassistantButton, &QPushButton::clicked, this, &EditRequirementDialog::showOrbitChat);

    ui->titleLineEdit->setText(m_requirement->m_longName);
    ui->idLineEdit->setText(m_requirement->m_id);
    ui->descriptionTextEdit->setText(m_requirement->m_description);
    ui->noteTextEdit->setText(m_requirement->m_note);
    ui->typeComboBox->addItems(typeList); // Add requirement types from requirement list.
    ui->statusComboBox->addItems(StatusList);
    ui->justificationTextEdit->setText(m_requirement->m_justification);
    ui->versionLineEdit->setText(m_requirement->m_valVersion);
    ui->valDescriptionTextEdit->setText(m_requirement->m_valDescription);
    ui->valStatusComboBox->addItems(StatusList);
    ui->valEvidenceTextEdit->setText(m_requirement->m_valEvidence);
    ui->complianceComboBox->addItems(ComplianceList);
    ui->complianceStatusComboBox->addItems(StatusList);

    auto typeIndex = typeList.indexOf(m_requirement->m_type);
    auto statusIndex = StatusList.indexOf(m_requirement->m_status);
    auto validationIndex = StatusList.indexOf(m_requirement->m_valStatus);
    auto complianceIndex = ComplianceList.indexOf(m_requirement->m_compliance);
    auto complianceStatusIndex = StatusList.indexOf(m_requirement->m_complianceStatus);

    if(typeIndex == -1) typeIndex = 0;
    if(statusIndex == -1) statusIndex = 0;
    if(validationIndex == -1) validationIndex = 0;
    if(complianceIndex == -1) complianceIndex = 0;
    if(complianceStatusIndex == -1) complianceStatusIndex = 0;

    ui->typeComboBox->setCurrentIndex(typeIndex);

    ui->statusComboBox->setCurrentIndex(statusIndex);
    ui->valStatusComboBox->setCurrentIndex(validationIndex);
    ui->complianceComboBox->setCurrentIndex(complianceIndex);
    ui->complianceStatusComboBox->setCurrentIndex(complianceStatusIndex);

    // Display specification combo box according to specification type.
    if (isSSS) {
        ui->specificationComboBox->setCurrentIndex(1);
        ui->priorityComboBox->setEnabled(false);
        ui->addRefButton->setEnabled(false);
        ui->removeRefButton->setEnabled(false);
    }
    else {
        ui->specificationComboBox->setCurrentIndex(0);
        ui->priorityComboBox->addItems(PriorityList);
        auto priorityIndex = PriorityList.indexOf(m_requirement->m_priority);
        if(priorityIndex == -1) priorityIndex = 0;
        ui->priorityComboBox->setCurrentIndex(priorityIndex);
        ui->versionLabel->hide();
        ui->versionLineEdit->hide();
    }
    ui->specificationComboBox->setEnabled(false);

    for (const QString &val : m_requirement->m_validation) {
        if(val.compare(k_validationTestLabel) == 0) ui->testCheckBox->setCheckState(Qt::Checked);
        if(val.compare(k_validationAnalysisLabel) == 0) ui->analysisCheckBox->setCheckState(Qt::Checked);
        if(val.compare(k_validationInspectionLabel) == 0) ui->inspectionCheckBox->setCheckState(Qt::Checked);
        if(val.compare(k_validationDesignLabel) == 0) ui->designCheckBox->setCheckState(Qt::Checked);
    }

    ui->notUniqueIDLabel->hide();
    ui->browserButton->show();
    ui->idLineEdit->setReadOnly(true);

    ui->availableListWidget->addItems(model->unreferencedFromId(m_requirement->m_id));
    ui->availableListWidget->sortItems();
    ui->childrenListWidget->addItems(model->childrenFromId(m_requirement->m_id, m_requirement->m_children));
    ui->childrenListWidget->sortItems();
    ui->parentsListWidget->addItems(model->parentsFromId(m_requirement->m_id, m_requirement->m_parents));
    ui->parentsListWidget->sortItems();
}

EditRequirementDialog::~EditRequirementDialog()
{
    delete ui;
}

void EditRequirementDialog::showOrbitChat()
{
    updateRequirement();

    // Ask about this one requirement, with the whole set alongside it so Orbit
    // can judge traceability and duplication against the rest.
    QList<Requirement> requirements = m_model ? *m_model->getRequirements() : QList<Requirement>();
    const QString specType =
            m_requirement->m_reqType.compare(k_SSSLabel) == 0 ? QStringLiteral("SSS") : QStringLiteral("SRS");
    const OrbitRequirementContext::Prepared prepared = OrbitRequirementContext::prepare(requirements, specType);

    auto *dialog = new OrbitChatDialog(prepared.directory, this);
    dialog->setBriefing(prepared.briefing);
    const QString id = m_requirement->m_id.trimmed();
    dialog->addHelper(tr("Review this requirement"),
            tr("Review requirement %1 in requirements.md against ECSS-E-ST-10-06C: compliance score 0-100, "
               "the wording (shall/should/may/can, forbidden terms 8.3.3), singularity (8.2.7), unambiguity "
               "(8.2.4), quantifiability (8.2.1) and verifiability (8.2.9), then a reworded version.")
                    .arg(id));
    dialog->addHelper(tr("Suggest a type"),
            tr("For requirement %1 in requirements.md, give the single ECSS requirement type from SS6 "
               "(functional, mission, interface, environmental, operational, human factor, logistics "
               "support, physical, product assurance, configuration, design, verification) and one sentence "
               "of justification.")
                    .arg(id));
    dialog->addHelper(tr("Find duplicates of this one"),
            tr("Find every requirement in requirements.md that duplicates or overlaps requirement %1 "
               "(ECSS 8.2.5). Give the ids, why they overlap, and how to resolve it.")
                    .arg(id));
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void EditRequirementDialog::openIssueLink()
{
    QDesktopServices::openUrl(m_requirement->m_link);
    reject();
}

void EditRequirementDialog::updateRequirement()
{
    m_requirement->m_longName = ui->titleLineEdit->text();
    m_requirement->m_type = ui->typeComboBox->currentText();
    m_requirement->m_description = ui->descriptionTextEdit->toPlainText();
    m_requirement->m_priority = ui->priorityComboBox->currentText();
    m_requirement->m_status = ui->statusComboBox->currentText();
    m_requirement->m_justification = ui->justificationTextEdit->toPlainText();
    m_requirement->m_valVersion = ui->versionLineEdit->text();
    m_requirement->m_valDescription = ui->valDescriptionTextEdit->toPlainText();
    m_requirement->m_valStatus = ui->valStatusComboBox->currentText();
    m_requirement->m_valEvidence = ui->valEvidenceTextEdit->toPlainText();
    m_requirement->m_compliance = ui->complianceComboBox->currentText();
    m_requirement->m_complianceStatus = ui->complianceStatusComboBox->currentText();
    m_requirement->m_note = ui->noteTextEdit->toPlainText();

    QStringList val;
    if (ui->testCheckBox->isChecked()) val << k_validationTestLabel;
    if (ui->analysisCheckBox->isChecked()) val << k_validationAnalysisLabel;
    if (ui->inspectionCheckBox->isChecked()) val << k_validationInspectionLabel;
    if (ui->designCheckBox->isChecked()) val << k_validationDesignLabel;
    m_requirement->m_validation = val;
    m_requirement->m_reqType = k_SRSLabel;

    if (ui->specificationComboBox->currentIndex() == 1) {
        m_requirement->m_reqType = k_SSSLabel;
    }
    else {
        m_requirement->m_reqType = k_SRSLabel;
    }

    m_requirement->m_parents = QStringList();

    int count = ui->parentsListWidget->count();
    for ( int index = 0; index < count; index++ ) {
        m_requirement->m_parents << ui->parentsListWidget->item(index)->text();
    }
}

void EditRequirementDialog::addParent()
{
    if(ui->availableListWidget->count() && ui->availableListWidget->currentItem()) {
        ui->parentsListWidget->addItem(ui->availableListWidget->takeItem(ui->availableListWidget->currentRow())->text());
        ui->parentsListWidget->sortItems();
    }
}

void EditRequirementDialog::removeParent()
{
     if(ui->parentsListWidget->count() && ui->parentsListWidget->currentItem()) {
         ui->availableListWidget->addItem(ui->parentsListWidget->takeItem(ui->parentsListWidget->currentRow())->text());
         ui->availableListWidget->sortItems();
     }
}

void EditRequirementDialog::updateOkButton()
{
    QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (!okButton) {
        return;
    }

    okButton->setDisabled(ui->titleLineEdit->text().isEmpty() || ui->descriptionTextEdit->toPlainText().isEmpty());
}

}
