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
#include "addnewrequirementdialog.h"

#include "ui_basicrequirementdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include "requirementsmodelbase.h"

namespace requirement {

AddNewRequirementDialog::AddNewRequirementDialog(RequirementsModelBase *model, Requirement *requirement, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
    , m_requirement(requirement)
    , ui(new Ui::BasicRequirementDialog)
{
    ui->setupUi(this);
    auto sizePolicy = ui->notUniqueIDLabel->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    ui->notUniqueIDLabel->setSizePolicy(sizePolicy);
    ui->notUniqueIDLabel->setVisible(false);
    connect(ui->titleLineEdit, &QLineEdit::textChanged, this, &AddNewRequirementDialog::updateOkButton);
    connect(ui->idLineEdit, &QLineEdit::textChanged, this, &AddNewRequirementDialog::updateOkButton);
    connect(ui->descriptionTextEdit, &QTextEdit::textChanged, this, &AddNewRequirementDialog::updateOkButton);
    connect(ui->specificationComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),[=](int index){ 
        qDebug() << "requirement specification changed " << index;
        ui->typeComboBox->clear();
        if(index == 0)
        {
            ui->typeComboBox->addItems(TypeListSRS);
            ui->versionLabel->hide();
            ui->versionLineEdit->hide();
        }
     });
    connect(ui->addRefButton, &QPushButton::clicked, this, &AddNewRequirementDialog::addParent);
    connect(ui->removeRefButton, &QPushButton::clicked, this, &AddNewRequirementDialog::removeParent);


    ui->priorityComboBox->addItems(PriorityList);
    ui->statusComboBox->addItems(StatusList);
    ui->valStatusComboBox->addItems(StatusList);
    ui->complianceComboBox->addItems(ComplianceList);
    ui->complianceStatusComboBox->addItems(StatusList);

    ui->virtualassistantButton->hide();
    ui->browserButton->hide();
    ui->idLineEdit->setReadOnly(false);

    ui->availableListWidget->addItems(model->unreferencedFromId(QString("")));
    ui->availableListWidget->sortItems();

    enum RequirementsModelBase::modelType type = RequirementsModelBase::SRS; 

    if(type == RequirementsModelBase::SRS)
    {
        ui->typeComboBox->addItems(TypeListSRS); // Add requirement types from requirement list for SRS.
        ui->specificationComboBox->setCurrentIndex(0);
        ui->versionLabel->hide();
        ui->versionLineEdit->hide();
    }
 
    updateOkButton();
}

AddNewRequirementDialog::~AddNewRequirementDialog()
{
    delete ui;
}


void AddNewRequirementDialog::updateRequirement()
{
    m_requirement->m_id = ui->idLineEdit->text();
    m_requirement->m_type = ui->typeComboBox->currentText();
    m_requirement->m_longName = ui->titleLineEdit->text();
    m_requirement->m_description = ui->descriptionTextEdit->toPlainText();
    m_requirement->m_priority = ui->priorityComboBox->currentText();
    m_requirement->m_status = ui->statusComboBox->currentText();
    m_requirement->m_justification = ui->justificationTextEdit->toPlainText();
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

    if (ui->specificationComboBox->currentIndex() == 1)
    {
        m_requirement->m_reqType = k_SSSLabel;
        m_requirement->m_valVersion = ui->versionLineEdit->text();
        switch(m_model->getState())
        {
        case RequirementsModelBase::SRS:
            m_model->changeModelState(RequirementsModelBase::Both);
            break;
        case RequirementsModelBase::Empty:
            m_model->changeModelState(RequirementsModelBase::SSS);
            break;
        }
    }
    else
    {
        m_requirement->m_reqType = k_SRSLabel;
        switch(m_model->getState())
        {
        case RequirementsModelBase::SSS:
            m_model->changeModelState(RequirementsModelBase::Both);
            break;
        case RequirementsModelBase::Empty:
            m_model->changeModelState(RequirementsModelBase::SRS);
            break;
        }
    }

    m_requirement->m_parents = QStringList();

    int count = ui->parentsListWidget->count();
    for ( int index = 0; index < count; index++ ) {
        m_requirement->m_parents << ui->parentsListWidget->item(index)->text();
    }

}

void AddNewRequirementDialog::addParent()
{
    if(ui->availableListWidget->count() && ui->availableListWidget->currentItem())
    {
        ui->parentsListWidget->addItem(ui->availableListWidget->takeItem(ui->availableListWidget->currentRow())->text());
        ui->parentsListWidget->sortItems();
    }
}

void AddNewRequirementDialog::removeParent()
{
     if(ui->parentsListWidget->count() && ui->parentsListWidget->currentItem())
     {
         ui->availableListWidget->addItem(ui->parentsListWidget->takeItem(ui->parentsListWidget->currentRow())->text());
         ui->availableListWidget->sortItems();
     }
}

void AddNewRequirementDialog::updateOkButton()
{
    QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (!okButton) {
        return;
    }

    bool reqIfExists = m_model->reqIfIDExists((const QString)QString(ui->idLineEdit->text()));
    ui->notUniqueIDLabel->setVisible(reqIfExists);
    okButton->setDisabled(ui->titleLineEdit->text().isEmpty() || ui->idLineEdit->text().isEmpty() || reqIfExists);
}

}
