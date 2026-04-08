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

#include "requirementswidget.h"

#include "addnewrequirementdialog.h"
#include "editrequirementdialog.h"
#include "selectsourcedialog.h"
#include "requirementsmanager.h"
#include "requirementsmodelcommon.h"
#include "virtualassistantdialog.h"
#include "ui_requirementswidget.h"
#include "widgetbar.h"

#include <QCursor>
#include <QDesktopServices>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableView>
#include <QToolButton>
#include <QtConcurrent>
#include <QFuture>
#include <QtCore>
#include <QInputDialog>
#include <QDomDocument>
#include <QCloseEvent>
#include "xlsxdocument.h"
#include "xlsxrichstring.h"
#include "xlsxworkbook.h"
#include "xlsxformat.h"


#include "requirementsmodelbase.h"

namespace requirement {
const int kIconSize = 16;

RequirementsWidget::RequirementsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RequirementsWidget)
    , m_widgetBar(new tracecommon::WidgetBar(this))
    , m_checkingServer(false)
    , m_originalHandler(nullptr)
    , m_embedded(false)
    , m_apply(false)
    , m_first(true)
{
    ui->setupUi(this);
    m_textFilterModel.setDynamicSortFilter(true);
    m_textFilterModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_textFilterModel.setFilterKeyColumn(-1);

    m_tagFilterModel.setDynamicSortFilter(true);
    m_tagFilterModel.setSourceModel(&m_textFilterModel);

    m_checkedModel.setFilterKeyColumn(RequirementsModelCommon::CHECKED);
    m_checkedModel.setSourceModel(&m_tagFilterModel);

    ui->allRequirements->setModel(&m_tagFilterModel);
 
    ui->allRequirements->horizontalHeader()->setStretchLastSection(false);
    ui->allRequirements->setSortingEnabled(true);

    ui->removeRequirementButton->setEnabled(false);

    connect(ui->allRequirements->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &RequirementsWidget::modelSelectionChanged);
    connect(ui->allRequirements, &QTableView::doubleClicked, this, &RequirementsWidget::showEditRequirementDialog);
    connect(ui->clearButton, &QPushButton::clicked, this, &RequirementsWidget::onClear);
    connect(ui->vaButton, &QPushButton::clicked, this, &RequirementsWidget::showVirtualAssistantDialog);
    connect(ui->createRequirementButton, &QPushButton::clicked, this, &RequirementsWidget::showNewRequirementDialog);
    connect(ui->exportRequirementsButton, &QPushButton::clicked, this, &RequirementsWidget::showExportRequirementsDialog);
    connect(ui->importRequirementsButton, &QPushButton::clicked, this, &RequirementsWidget::showImportRequirementsDialog);
    connect(ui->removeRequirementButton, &QPushButton::clicked, this, &RequirementsWidget::removeRequirement);
    connect(ui->applyPushButton, &QPushButton::clicked, this, &RequirementsWidget::applyEdits);
    connect(ui->filterLineEdit, &QLineEdit::textChanged, &m_textFilterModel,
            &QSortFilterProxyModel::setFilterFixedString);

    connect(this, &RequirementsWidget::loadGitLab, this, &RequirementsWidget::setLoginData);

//    m_originalHandler = qInstallMessageHandler(displayProgress);

    ui->verticalLayout->insertWidget(0, m_widgetBar);
    ui->textEdit->hide();
}

RequirementsWidget::~RequirementsWidget()
{
    delete ui;
}



void RequirementsWidget::setManager(RequirementsManager *manager)
{
    qDebug() << "Set Manager";
    m_reqManager = manager;
    connect(m_reqManager, &RequirementsManager::projectIDChanged, this, &RequirementsWidget::updateServerStatus);
    connect(m_reqManager, &RequirementsManager::listOfTags, this, &RequirementsWidget::fillTagBar);
    connect(m_reqManager, &RequirementsManager::reportProgress, this, &RequirementsWidget::progress);
    connect(m_reqManager, &RequirementsManager::connectionError, this, [this](const QString &error) {
        updateServerStatus();
        QMessageBox::warning(this, tr("Connection error"), tr("Connection failed for this error:\n%1").arg(error));
        ui->allRequirements->show();
        ui->textEdit->hide();
    });
    connect(m_reqManager, &RequirementsManager::fetchingRequirementsEnded, m_reqManager,
            &RequirementsManager::requestTags);

    if (!m_reqManager->projectUrl().isEmpty()) {
        m_requirementsUrl = m_reqManager->projectUrl();
    }

    if (!m_reqManager->token().isEmpty()) {
        m_requirementsToken = m_reqManager->token();
    }

}

void RequirementsWidget::setModel(RequirementsModelBase *model)
{
    qDebug() << "Set Model";
    m_model = model;
    m_textFilterModel.setSourceModel(m_model);

    // Make the id column size itself to contents, the title column STRETCH to fill
    // the available width and keep the checkbox column a small fixed width so
    // clicking it does not cause the view to shift or a horizontal scrollbar to appear.
    //
    // Use ResizeToContents for column 0 so long IDs don't unnecessarily waste space,
    // Stretch for the main text column so it absorbs remaining space, and Fixed
    // for the checkbox column to keep it narrow.
    QHeaderView *h = ui->allRequirements->horizontalHeader();
    h->setStretchLastSection(false);
    h->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(1, QHeaderView::Stretch);
    h->setSectionResizeMode(2, QHeaderView::Fixed);

    // Small fixed width for checkbox column (enough for a checkbox + padding)
    ui->allRequirements->setColumnWidth(2, 75);

    // Avoid horizontal scrollbar; columns will resize to fit the viewport.
    ui->allRequirements->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Make the table adjust column sizes when the viewport changes
    ui->allRequirements->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    // Prefer per-pixel scrolling for smoother behaviour (optional)
    ui->allRequirements->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    // Avoid wrapping text in cells (helps the stretch behaviour)
    ui->allRequirements->setWordWrap(false);

    connect(m_model, &requirement::RequirementsModelBase::exportCompleted, this, &RequirementsWidget::workingCompleted);
 
    // When a requirement is selected/deselected, emit a signal (useful for Python backends)
    connect(m_model, &requirement::RequirementsModelBase::dataChanged, [this](const QModelIndex &index) {
        if (index.column() == RequirementsModelBase::CHECKED) {
            bool isChecked = m_model->data(index, Qt::CheckStateRole).toBool();
            QModelIndex reqID_index = m_model->index(index.row(), RequirementsModelBase::REQUIREMENT_ID);
            QString ReqID = m_model->data(reqID_index, Qt::DisplayRole).toString();
            Q_EMIT requirementSelected(ReqID, isChecked);
        }
    });


}

QUrl RequirementsWidget::url() const
{
    return m_requirementsUrl;
}

void RequirementsWidget::setUrl(const QUrl &url)
{
    qDebug() << "Set Url";
    m_targetUrl = url.toString();
    if (m_currentDialog) {
        m_currentDialog->setUrl(url);
    }
    onChangeOfCredentials(url.toString(), m_requirementsToken);
}

QString RequirementsWidget::token() const
{
    return m_requirementsToken;
}

void RequirementsWidget::setToken(const QString &token)
{
    qDebug() << "Set Token";
    m_targetToken = token;
    if (m_currentDialog) {
        m_currentDialog->setToken(token);
    }
    onChangeOfCredentials(m_requirementsUrl, token);
}

QHeaderView *RequirementsWidget::horizontalTableHeader() const
{
    return ui->allRequirements->horizontalHeader();
}

void RequirementsWidget::onChangeOfCredentials(const QString url, const QString token)
{
    if (!m_reqManager) {
        return;
    }

    qDebug() << "Set Change of credentials called";

    m_checkingServer = true;

    m_requirementsUrl = url;
    m_requirementsToken = token;

//    qputenv("ESA_LOCAL_GITLAB_PROJECT", m_requirementsUrl.toUtf8());
//    qputenv("ESA_LOCAL_GITLAB_TOKEN", m_requirementsToken.toUtf8());

    if (m_requirementsUrl.isEmpty()) {
        return;
    }

    m_reqManager->setRequirementsCredentials(m_requirementsUrl, m_requirementsToken);
    Q_EMIT requirementsCredentialsChanged(m_requirementsUrl, m_requirementsToken); // $$$
}

void RequirementsWidget::applyEdits()
{
    if (!m_reqManager || !m_model) {
        return;
    }

    if (m_embedded)
    {
        m_reqManager->setRequirementsCredentials(m_targetUrl, m_targetToken);

        QApplication::processEvents();

        while (m_reqManager->isBusy()) {
            QApplication::processEvents();
        }
    }

    bool allowDelete = false;

    if (ui->sourceLineEdit->text().compare(m_targetUrl)==0) {
        allowDelete = true;
    }

 
    if (m_reqManager->hasValidProjectID()) {
        m_apply = true;
        ui->textEdit->clear();
        ui->textEdit->show();
        ui->allRequirements->hide();
        m_model->applyGitLabEdits(allowDelete);
    }
}

void RequirementsWidget::progress(const QString &text)
{
    QString str(ui->textEdit->toPlainText());

    str.append(QString("%1\n").arg(text));
    ui->textEdit->setText(str);
}

void RequirementsWidget::requestRequirements()
{
    if (!m_reqManager || !m_model) {
        return;
    }

    if (m_reqManager->hasValidProjectID()) {
        m_model->clearRequirements();
        m_reqManager->requestAllRequirements("");
    }
}

void RequirementsWidget::workingCompleted()
{
    if(m_embedded && m_apply) {
        QApplication::quit();
    } else {
        ui->allRequirements->show();
        ui->textEdit->hide();
        m_apply = false;
    }
}

void RequirementsWidget::setLoginData()
{
    bool before = false;

    if (!m_reqManager) {
         return;
    }

    while (m_reqManager->isBusy()) {
        QApplication::processEvents();
    }

    if (m_requirementsUrl.isEmpty() || m_requirementsToken.isEmpty()) {
        qDebug() << "login empty";
        return;
    }

    m_model->clearRequirements();

    if (m_requirementsUrl == m_reqManager->projectUrl() && m_requirementsToken == m_reqManager->token()) {
  
        m_reqManager->requestAllRequirements("");
        ui->sourceLineEdit->setText(m_requirementsUrl);
        ui->applyPushButton->setEnabled(true);
        qDebug() << "login matches";
        return;
    }

    ui->sourceLineEdit->setText("");
    ui->applyPushButton->setEnabled(false);
    qDebug() << "login doesn't match";
    m_reqManager->setRequirementsCredentials(m_requirementsUrl, m_requirementsToken);
}

void RequirementsWidget::updateServerStatus()
{
    qDebug() << "Widget Checking server ";
    if (!m_reqManager) {
        return;
    }

    m_checkingServer = false;

    const QCursor busyCursor(Qt::WaitCursor);
    if (m_reqManager->isBusy()) {
        if (ui->allRequirements->cursor() != busyCursor) {
            ui->allRequirements->setCursor(busyCursor);
        }
    } else {
        if (ui->allRequirements->cursor() == busyCursor) {
            ui->allRequirements->unsetCursor();
        }
    }

    if(m_first) {
        setLoginData();
        m_first = false;
    }
}

void RequirementsWidget::openIssueLink(const QModelIndex &index)
{
    QDesktopServices::openUrl(index.data(tracecommon::TraceCommonModelBase::IssueLinkRole).toString());
}


void RequirementsWidget::showVirtualAssistantDialog()
{
    QScopedPointer<VirtualAssistantDialog> dialog(new VirtualAssistantDialog(m_model));
    dialog->setModal(true);
    const auto ret = dialog->exec();
}


void RequirementsWidget::showNewRequirementDialog()
{
    if (!m_reqManager || !m_model) {
        return;
    }

    Requirement requirement;

    QScopedPointer<AddNewRequirementDialog> dialog(new AddNewRequirementDialog(m_model.get(), &requirement));
    dialog->setModal(true);
    const auto ret = dialog->exec();
    if (ret == QDialog::Accepted) {
        dialog->updateRequirement();
        setModelTypeLabel(m_model->getState());
        requirement.updateIssue(m_model->getRequirements());
        m_model->createModelRequirement(requirement);
    }
}

void RequirementsWidget::showEditRequirementDialog(const QModelIndex &index)
{
    if (!m_reqManager || !m_model) {
        return;
    }

    const auto &currentIndex = ui->allRequirements->selectionModel()->currentIndex();
    if (currentIndex.isValid()) {
        const QString reqIfID = currentIndex.data(RequirementsModelCommon::ReqIfIdRole).toString();
        Requirement requirement = m_model->requirementFromId(reqIfID);

        if (requirement.isValid()) {
            QScopedPointer<EditRequirementDialog> dialog(new EditRequirementDialog(m_model.get(), &requirement));
            dialog->setModal(true);
            const auto ret = dialog->exec();
            if (ret == QDialog::Accepted) {
                dialog->updateRequirement();
                requirement.updateIssue(m_model->getRequirements());
                m_model->editModelRequirement(requirement);
            }
        }
    }
    else {
        QMessageBox::warning(this, tr("Index Error"), tr("Invalid entry %1 selected").arg(index.row()));
    }
}

void RequirementsWidget::fillTagBar(const QStringList &tags)
{
    auto it = std::remove_if(m_tagButtons.begin(), m_tagButtons.end(), [this, tags](QToolButton *button) {
        if (!tags.contains(button->text())) {
            m_tagFilterModel.removeTag(button->text());
            button->deleteLater();
            return true;
        }
        return false;
    });
    m_tagButtons.erase(it, m_tagButtons.end());
//    tags.sort();

    for (const QString &tag : tags) {
        if (!tagButtonExists(tag)) {
            auto button = new QToolButton(m_widgetBar);

            button->setText(tag);
            button->setCheckable(true);
            connect(button, &QToolButton::toggled, this, [this](bool checked) {
                auto button = dynamic_cast<QToolButton *>(sender());
                if (!button) {
                    return;
                }
                if (checked) {
                    m_tagFilterModel.addTag(button->text());
                } else {
                    m_tagFilterModel.removeTag(button->text());
                }
            });

            m_tagButtons.append(button);
            m_widgetBar->addWidget(button);
        }
    }
}

bool RequirementsWidget::tagButtonExists(const QString &tag) const
{
    return std::any_of(
            m_tagButtons.begin(), m_tagButtons.end(), [&tag](const auto *btn) { return btn->text() == tag; });
}

void RequirementsWidget::showExportRequirementsDialog()
{
    if (!m_reqManager || !m_model) {
        return;
    }

    QScopedPointer<SelectSourceDialog> dialog(new SelectSourceDialog(exportDialogTitle, exportDialogLabel, m_requirementsUrl, m_requirementsToken, m_model->getState()));
    m_currentDialog = dialog.data();
    connect(m_currentDialog, &SelectSourceDialog::requirementsUrlChanged, this, [this](const QUrl &url) {
        Q_EMIT requirementsUrlChanged(url.toString());
    });
    dialog->setModal(true);

    const auto ret = dialog->exec();

    m_currentDialog = nullptr;

    if (ret == QDialog::Accepted) {
//        m_model->setExportType(dialog->type());

        switch(dialog->result()) {
        case SelectSourceDialog::GitLabSelected:

            qDebug() << "GitLab Export " << dialog->getToken() << dialog->getUrl();
            ui->textEdit->clear();
            ui->textEdit->show();
            ui->allRequirements->hide();
            m_model->exportGitLabRequirements(dialog->getUrl(), dialog->getToken(), dialog->type());
            break;

        case SelectSourceDialog::ReqIfSelected:
            showExportReqIfFileRequirementsDialog(dialog->type());
            break;

        case SelectSourceDialog::ExcelSelected:
            showExportExcelFileRequirementsDialog(dialog->type());
            break;
        }
    }
}

void RequirementsWidget::showExportReqIfFileRequirementsDialog(enum RequirementsModelBase::modelType type) const
{
    QFileDialog::Options options;
    options |= QFileDialog::DontUseNativeDialog;
    QString selectedFilter;
    QString dialogHeader;

    if(m_model->getState() == RequirementsModelBase::SSS) {
        dialogHeader = tr("Export ReqIf SSS Requirements");
    } else {
        dialogHeader = tr("Export ReqIf SRS Requirements");
    }

    QFileDialog dialog;

    dialog.setDefaultSuffix(QString(".reqif"));

    QString fileName = dialog.getSaveFileName(nullptr,
                            dialogHeader,
                            QDir::homePath(),
                            tr("ReqIf (*.reqif)"),
                            &selectedFilter,
                            options);

    if (!fileName.isEmpty()) {
        QFileInfo fileinfo(fileName);
        QString suffix = fileinfo.completeSuffix();

        if (suffix.isEmpty()) {
            fileName.append(".reqif");
        }

        int rowCount = m_model->rowCount(QModelIndex());

        if (rowCount) {
            ReqIf reqIf(fileName, m_model, type);
            reqIf.createReqIf();
            reqIf.writeReqIfFile();
        }
    }
}

void RequirementsWidget::showExportExcelFileRequirementsDialog(enum RequirementsModelBase::modelType type) const
{
    QFileDialog::Options options;
    options |= QFileDialog::DontUseNativeDialog | QFileDialog::DontConfirmOverwrite;
    QString selectedFilter;
    QString dialogHeader;

    if(type == RequirementsModelBase::SSS) {
        dialogHeader = tr("Export Excel SSS Requirements");
    } else {
        dialogHeader = tr("Export Excel SRS Requirements");
    }

    QFileDialog dialog;

    dialog.setDefaultSuffix(QString(".xlsx"));

    QString fileName = dialog.getSaveFileName(nullptr,
                            dialogHeader,
                            QDir::homePath(),
                            tr("Excel (*.xlsx)"),
                            &selectedFilter,
                            options);

    if (!fileName.isEmpty()) {

        QFileInfo fileinfo(fileName);
        QString suffix = fileinfo.completeSuffix();

        if (suffix.isEmpty()) {
            fileName.append(".xlsx");
        }

        fileinfo.setFile(fileName);

        // check if file exists and if yes: Is it really a file and not a directory?
        if (fileinfo.exists() && fileinfo.isFile()) {
            if(type == RequirementsModelBase::SSS) {
                dialogHeader = tr("Update Existing SSS Requirements");
            } else {
                dialogHeader = tr("Update Existing SRS Requirements");
            }

            QString dialogBody;
            dialogBody = tr("Are you sure you want to update the existing requirements?    ");
            dialogBody.append(QString("\n\n%1").arg(fileinfo.fileName()));

            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(nullptr, dialogHeader, dialogBody, QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No) {
                return;
            }
        } else {
            Excel::createDefault(fileName, type);
        }

        int rowCount = m_model->rowCount(QModelIndex());

        if (rowCount) {
            Excel excel(fileName, m_model, type);
            excel.exportExcel();
        }
    }
}

void RequirementsWidget::setModelTypeLabel(enum RequirementsModelBase::modelType type)
{
    switch(type) {
    case RequirementsModelBase::Empty:
        ui->modelTypeLabel->setText("Empty");
        break;
    case RequirementsModelBase::SRS:
        ui->modelTypeLabel->setText(k_SRSLabel);
        break;
    case RequirementsModelBase::SSS:
        ui->modelTypeLabel->setText(k_SSSLabel);
        break;
    case RequirementsModelBase::Both:
        ui->modelTypeLabel->setText("Both");
        break;
    default:
        ui->modelTypeLabel->setText("Empty");
        break;
    }
}

void RequirementsWidget::showImportRequirementsDialog()
{
    if (!m_reqManager || !m_model) {
        return;
    }

    QScopedPointer<SelectSourceDialog> dialog(new SelectSourceDialog(importDialogTitle, importDialogLabel, m_requirementsUrl, m_requirementsToken, RequirementsModelBase::Both));
    m_currentDialog = dialog.data();
    connect(m_currentDialog, &SelectSourceDialog::requirementsUrlChanged, this, [this](const QUrl &url) {
        Q_EMIT requirementsUrlChanged(url.toString());
    });
    dialog->setModal(true);

    const auto ret = dialog->exec();

    m_currentDialog = nullptr;

    if (ret == QDialog::Accepted) {
        switch(dialog->result())
        {
            case SelectSourceDialog::GitLabSelected:
                m_model->changeModelState(dialog->type());
                setModelTypeLabel(m_model->getState());
                onChangeOfCredentials(dialog->getUrl(), dialog->getToken());
                setLoginData();
                break;

            case SelectSourceDialog::ReqIfSelected:
                showImportReqIfFileRequirementsDialog(dialog->type());
                break;

            case SelectSourceDialog::ExcelSelected:
                showImportExcelFileRequirementsDialog(dialog->type());
                break;
        }
    }
}
void RequirementsWidget::showImportReqIfFileRequirementsDialog(enum RequirementsModelBase::modelType type)
{
    QFileDialog::Options options;
    options |= QFileDialog::DontUseNativeDialog;
    QString selectedFilter;
    QString dialogHeader;

    if(type == RequirementsModelBase::SSS) {
        dialogHeader = tr("Import ReqIf SSS Requirements");
    } else {
        dialogHeader = tr("Import ReqIf SRS Requirements");
    }

    QString fileName = QFileDialog::getOpenFileName(nullptr,
                            dialogHeader,
                            QDir::homePath(),
                            tr("ReqIf (*.reqif)"),
                            &selectedFilter,
                            options);

     if (!fileName.isEmpty()) {

         ReqIf requif(fileName, m_model);
         requif.readReqIfFile();
         requif.importReqIf();

         setModelTypeLabel(m_model->getState());
         ui->sourceLineEdit->setText(fileName);
         // Imported requirements are local until exported to GitLab -> mark pending edits
         if (m_model) {
             m_model->setPendingEdits(true);
         }
         if(!m_embedded)
         {
             ui->applyPushButton->setEnabled(false);
         }
     }
}

void RequirementsWidget::showImportExcelFileRequirementsDialog(enum RequirementsModelBase::modelType type)
{
    QFileDialog::Options options;
    options |= QFileDialog::DontUseNativeDialog;
    QString selectedFilter;
    QString dialogHeader;

    if(type == RequirementsModelBase::SSS) {
        dialogHeader = tr("Import Excel SSS Requirements");
    } else {
        dialogHeader = tr("Import Excel SRS Requirements");
    }

    QString fileName = QFileDialog::getOpenFileName(nullptr,
                            dialogHeader,
                            QDir::homePath(),
                            tr("Excel (*.xlsx)"),
                            &selectedFilter,
                            options);

     if (!fileName.isEmpty()) {
         Excel excel(fileName, m_model, type);
         excel.importExcel();
         setModelTypeLabel(m_model->getState());
         ui->sourceLineEdit->setText(fileName);
         if(!m_embedded)
         {
             ui->applyPushButton->setEnabled(false);
         }
     }
}

/*!
 * \brief removeRequirement takes a look in the selectionModel.
 *        If more than one row is selected returns, otherwise
 *        calls Requirements Manager to remove it (aka close the issue)
 */
void RequirementsWidget::removeRequirement()
{
    if (!m_reqManager || !m_model) {
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Remove requirement"),
            tr("Are you sure you want to remove the selected requirement?"), QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        auto selectionModel = ui->allRequirements->selectionModel();
        if (!selectionModel) {
            return;
        }

        // Collect unique selected rows (one index per row).
        const QModelIndexList rows = selectionModel->selectedRows();
        if (rows.isEmpty()) {
            return;
        }

        // Build a list of unique ReqIf IDs to remove.
        QStringList reqIfIDs;
        reqIfIDs.reserve(rows.size());
        for (const QModelIndex &rowIndex : rows) {
            const QString reqIfID = rowIndex.data(RequirementsModelCommon::ReqIfIdRole).toString();
            if (!reqIfID.isEmpty() && !reqIfIDs.contains(reqIfID)) {
                reqIfIDs.append(reqIfID);
            }
        }

        // Remove each selected requirement. Use the same immediate-vs-queued logic as before.
        for (const QString &reqIfID : reqIfIDs) {
            const Requirement requirement = m_model->requirementFromId(reqIfID);
            if (!requirement.isValid()) {
                continue;
            }

            if (requirement.m_issueIID && m_reqManager->hasValidProjectID()) {
                // Remove on GitLab immediately, wait for completion, then remove locally without queuing.
                m_reqManager->removeRequirement(requirement);
                while (m_reqManager->isBusy()) {
                    QApplication::processEvents();
                }
                m_model->deleteModelRequirementDirect(requirement);
            } else {
                // Local-only requirement: mark for deletion to be applied later.
                m_model->deleteModelRequirement(requirement);
            }
        }
    }
}

void RequirementsWidget::modelSelectionChanged(const QItemSelection &selected, const QItemSelection & /*unused*/)
{
    const bool enabled(selected.indexes().count() > 0);
    ui->removeRequirementButton->setEnabled(enabled);
}

void RequirementsWidget::onClear()
{
    m_model->clearRequirements();
    m_model->changeModelState(RequirementsModelBase::Empty);
    setModelTypeLabel(m_model->getState());
    ui->sourceLineEdit->setText("");
    ui->applyPushButton->setEnabled(false);
}

void RequirementsWidget::closeEvent(QCloseEvent *event)
{
qDebug() << "RequirementsWidget::closeEvent ";
    // If there are no pending edits queued in the model, allow close without warning.
    if (m_model && !m_model->hasPendingEdits()) {
        event->accept();
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Close "), tr("You may have requirement changes not yet committed to Gitlab. Press 'Cancel' and 'Apply Edits' to commit them or 'Close' to Exit."), QMessageBox::Close | QMessageBox::Cancel);
    if (reply == QMessageBox::Close) {
        event->accept();
    } else {
        event->ignore();
    }
}
}
