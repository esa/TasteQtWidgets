/*
   Copyright (C) 2024 European Space Agency - <maxime.perrotin@esa.int>

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

#include "reviewswidget.h"

#include "addnewreviewdialog.h"
#include "review.h"
#include "reviewsmanager.h"
#include "reviewsmodelbase.h"
#include "ui_reviewswidget.h"
#include "widgetbar.h"

#include <QDesktopServices>
#include <QLineEdit>
#include <QMessageBox>

using namespace tracecommon;

namespace reviews {
const int kIconSize = 16;

ReviewsWidget::ReviewsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ReviewsWidget)
    , m_widgetBar(new tracecommon::WidgetBar(this))
{
    ui->setupUi(this);
    ui->removeReviewButton->setEnabled(false);

    m_textFilterModel.setDynamicSortFilter(true);
    m_textFilterModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_textFilterModel.setFilterKeyColumn(-1);

    m_tagFilterModel.setDynamicSortFilter(true);
    m_tagFilterModel.setSourceModel(&m_textFilterModel);



    connect(ui->refreshButton, &QPushButton::clicked, this, &ReviewsWidget::setLoginData);
    connect(ui->credentialWidget, &tracecommon::CredentialWidget::urlChanged, this,
            &ReviewsWidget::onChangeOfCredentials);
    connect(ui->credentialWidget, &tracecommon::CredentialWidget::tokenChanged, this,
            &ReviewsWidget::onChangeOfCredentials);
    connect(ui->allReviews, &QTableView::doubleClicked, this, &ReviewsWidget::openIssueLink);
    connect(ui->createReviewButton, &QPushButton::clicked, this, &ReviewsWidget::showNewReviewDialog);
    connect(ui->removeReviewButton, &QPushButton::clicked, this, &ReviewsWidget::removeReview);
    connect(ui->filterLineEdit, &QLineEdit::textChanged, &m_textFilterModel,
            &QSortFilterProxyModel::setFilterFixedString);

    ui->verticalLayout->insertWidget(0, m_widgetBar);
}

ReviewsWidget::~ReviewsWidget()
{
    delete ui;
}

void ReviewsWidget::showButtons(bool show)
{
    ui->createReviewButton->setVisible(show);
    ui->removeReviewButton->setVisible(show);
}

void ReviewsWidget::setManager(ReviewsManager *manager)
{
    m_reviewsManager = manager;
    connect(m_reviewsManager, &ReviewsManager::projectIDChanged, this, &ReviewsWidget::updateProjectReady);
    connect(m_reviewsManager, &ReviewsManager::busyChanged, this, &ReviewsWidget::updateServerStatus);
    connect(m_reviewsManager, &ReviewsManager::reviewAdded, this, &ReviewsWidget::reviewAdded);
    connect(m_reviewsManager, &ReviewsManager::reviewAdded, this, &ReviewsWidget::requestReviews);
    connect(m_reviewsManager, &ReviewsManager::fetchingReviewsEnded, m_reviewsManager, &ReviewsManager::requestTags);
    connect(m_reviewsManager, &ReviewsManager::listOfTags, this, &ReviewsWidget::fillTagBar);
    connect(m_reviewsManager, &tracecommon::IssuesManager::projectUrlChanged, ui->credentialWidget,
            &tracecommon::CredentialWidget::setUrl);
    connect(m_reviewsManager, &tracecommon::IssuesManager::tokenChanged, ui->credentialWidget,
            &tracecommon::CredentialWidget::setToken);

    if (!m_reviewsManager->projectUrl().isEmpty()) {
        ui->credentialWidget->setUrl(m_reviewsManager->projectUrl());
    }
    if (!m_reviewsManager->token().isEmpty()) {
        ui->credentialWidget->setToken(m_reviewsManager->token());
    }

    m_reviewsModel = new ComponentReviewsProxyModel(manager, this);
    setModel(m_reviewsModel);
}

void ReviewsWidget::setModel(ReviewsModelBase *model)
{
    m_model = model;
    m_textFilterModel.setSourceModel(m_model);
    // Note: m_tagFilterModel uses m_textFilterModel as source model
    ui->allReviews->setModel(&m_tagFilterModel);
}

void ReviewsWidget::setAcceptableIds(const QStringList &ids)
{
    if (m_reviewsModel)
    {
        m_reviewsModel->setAcceptableIds(ids);
    }
}

QUrl ReviewsWidget::url() const
{
    return ui->credentialWidget->url();
}

void ReviewsWidget::setUrl(const QUrl &url)
{
    if (ui->credentialWidget->url() == url) {
        return;
    }
    ui->credentialWidget->setUrl(url.toString());
}

QString ReviewsWidget::token() const
{
    return ui->credentialWidget->token();
}

void ReviewsWidget::setToken(const QString &token)
{
    if (ui->credentialWidget->token() == token) {
        return;
    }
    ui->credentialWidget->setToken(token);
}

QHeaderView *ReviewsWidget::horizontalTableHeader() const
{
    return ui->allReviews->horizontalHeader();
}

void ReviewsWidget::setLoginData()
{
    if (m_reviewsManager->isBusy()) {
        return;
    }

    ui->serverStatusLabel->setPixmap({});

    const QUrl currUrl(ui->credentialWidget->url());
    const QString currToken(ui->credentialWidget->token());

    if (currUrl.isEmpty() || currToken.isEmpty()) {
        ui->serverStatusLabel->setPixmap({});
        return;
    }

    if (currUrl == m_reviewsManager->projectUrl() && currToken == m_reviewsManager->token()) {
        m_reviewsManager->requestAllReviews();
        return;
    }

    ui->serverStatusLabel->setToolTip(tr("Checking connection to the server"));
    m_reviewsManager->setCredentials(currUrl.toString(), currToken);
}

void ReviewsWidget::updateServerStatus()
{
    if (!m_reviewsManager) {
        return;
    }

    ui->refreshButton->setEnabled(!m_reviewsManager->isBusy());
    const QCursor busyCursor(Qt::WaitCursor);
    if (m_reviewsManager->isBusy()) {
        if (ui->allReviews->cursor() != busyCursor) {
            ui->allReviews->setCursor(busyCursor);
        }
    } else {
        if (ui->allReviews->cursor() == busyCursor) {
            ui->allReviews->unsetCursor();
        }
    }

    const bool connectionOk = (m_reviewsManager->hasValidProjectID());
    if (connectionOk) {
        ui->serverStatusLabel->setPixmap(
                QPixmap(":/tracecommonresources/icons/check_icon.svg").scaled(kIconSize, kIconSize));
        ui->serverStatusLabel->setToolTip(tr("Connection to the server is ok"));
    } else {
        ui->serverStatusLabel->setPixmap(
                QPixmap(":/tracecommonresources/icons/uncheck_icon.svg").scaled(kIconSize, kIconSize));
        ui->serverStatusLabel->setToolTip(tr("Connection to the server failed"));
    }
}

void ReviewsWidget::updateProjectReady()
{
    qDebug() << "ReviewsWidget::updateProjectReady called";
    if (!m_reviewsManager) {
        qDebug() << "ReviewsWidget::updateProjectReady - No manager";
        ui->createReviewButton->setEnabled(false);
        return;
    }

    const bool hasValidID = m_reviewsManager->hasValidProjectID();
    qDebug() << "ReviewsWidget::updateProjectReady - Valid project ID:" << hasValidID;
    ui->createReviewButton->setEnabled(hasValidID);
    if (hasValidID) {
        requestReviews();
    }
}

void ReviewsWidget::addReviews(const QList<Review> &reviews)
{
    m_reviewsModel->addReviews(reviews);
}

void ReviewsWidget::onChangeOfCredentials()
{
    const QUrl newUrl(ui->credentialWidget->url());
    Q_EMIT reviewsUrlChanged(newUrl);
    setUrl(newUrl);
    const QString newToken(ui->credentialWidget->token());
    if (!newUrl.isValid() || newToken.isEmpty()) {
        return;
    }
    m_reviewsManager->setCredentials(newUrl.toString(), newToken);
    Q_EMIT reviewsCredentialsChanged(newUrl, newToken);
}

void ReviewsWidget::requestReviews()
{
    qDebug() << "ReviewsWidget::requestReviews called";
    if (m_reviewsManager && m_reviewsManager->hasValidProjectID()) {
        qDebug() << "ReviewsWidget::requestReviews - Valid manager and project ID, requesting reviews";
        m_reviewsManager->requestAllReviews();
    } else {
        qDebug() << "ReviewsWidget::requestReviews - Missing manager or project ID is invalid";
    }
}

void ReviewsWidget::openIssueLink(const QModelIndex &index)
{
    const QString data = index.data(TraceCommonModelBase::IssueLinkRole).toString();
    const QUrl issueUrl(data);
    bool ok = QDesktopServices::openUrl(issueUrl);
    if (!ok) {
        QMessageBox::warning(this, tr("Url error"), tr("Error opening the url\n%1").arg(issueUrl.toString()));
    }
}

void ReviewsWidget::showNewReviewDialog() const
{
    QScopedPointer<AddNewReviewDialog> dialog(new AddNewReviewDialog(m_model.get()));
    dialog->setModal(true);
    const auto ret = dialog->exec();
    if (ret == QDialog::Accepted) {
        m_reviewsManager->createReview(
                dialog->title(), dialog->reviewId(), dialog->description(), dialog->testMethod());
    }
}

void ReviewsWidget::removeReview()
{
    const auto &currentIndex = ui->allReviews->selectionModel()->currentIndex();
    if (!currentIndex.isValid()) {
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Remove review"), tr("Are you sure you want to remove the selected review?"),
            QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        const Review &review = m_model->reviewFromIndex(currentIndex);
        const bool ok = m_reviewsManager->removeReview(review);
        if (ok) {
            Q_EMIT reviewRemoved(review.m_id);
        }
    }
}

void ReviewsWidget::fillTagBar(const QStringList &tags)
{
    auto it = std::remove_if(m_tagButtons.begin(), m_tagButtons.end(), [this, tags](QToolButton *button) {
        if (!tags.contains(button->text())) {
            m_tagFilterModel.removeTag(button->text());
            button->deleteLater();
            return true;
        }
        return false;
    });
    if (it != m_tagButtons.end()) {
        m_tagButtons.erase(it, m_tagButtons.end());
    }

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

bool ReviewsWidget::tagButtonExists(const QString &tag) const
{
    return std::any_of(m_tagButtons.begin(), m_tagButtons.end(),
            [&tag](const auto *btn) { return btn->text().compare(tag) == 0; });
}

} // namespace reviews
