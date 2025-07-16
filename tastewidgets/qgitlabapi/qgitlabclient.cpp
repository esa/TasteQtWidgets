#include "qgitlabclient.h"

#include "issuerequestoptions.h"
#include "labelsrequestoptions.h"

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#define WRN qWarning() << Q_FUNC_INFO

namespace gitlab {

const QString kContentType = "application/x-www-form-urlencoded";

QGitlabClient::QGitlabClient() { }

void QGitlabClient::setCredentials(const QString &url, const QString &token)
{
    QUrl api_url(url);
    api_url.setPath("/api/v4");
    mUrlComposer.setBaseURL(api_url.toString());
    mToken = token;
}

bool QGitlabClient::requestIssues(const IssueRequestOptions &options)
{
    if (isBusy()) {
        return true;
    }
    setBusy(true);
    auto reply = sendRequest(QGitlabClient::GET, mUrlComposer.composeGetIssuesUrl(options.mProjectID, options));
    connect(reply, &QNetworkReply::finished, [reply, options, this]() {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonParseError jsonError;
            auto replyContent = QJsonDocument::fromJson(reply->readAll(), &jsonError);
            if (QJsonParseError::NoError != jsonError.error) {
                const QString &errMsg = QString("ERROR: QGitlabClient::requestIssues: Parsing json data: %1, #%2")
                                                .arg(jsonError.errorString())
                                                .arg(jsonError.offset);
                WRN << errMsg;
                notifyError(reply, errMsg);
                // TODO: setBusy(false); ?
            } else {
                QList<Issue> issues;
                for (const QJsonValueRef &value : replyContent.array()) {
                    issues.push_back(value.toObject());
                }
                Q_EMIT listOfIssues(issues);
            }

            if (!requestNextPage(reply, options)) {
                setBusy(false);
                Q_EMIT issueFetchingDone();
            }
        } else {
            WRN << reply->error() << reply->errorString();
            setBusy(false);
            notifyError(reply, "QGitlabClient::requestIssues");
        }
    });
    return false;
}

bool QGitlabClient::editIssue(const int &projectID, const int &issueID, const Issue &newIssue)
{
    if (isBusy()) {
        return true;
    }
    setBusy(true);
    auto reply = sendRequest(QGitlabClient::PUT,
            mUrlComposer.composeEditIssueUrl(projectID, newIssue.mIssueIID, newIssue.mTitle, newIssue.mDescription,
                    newIssue.mAssignee, newIssue.mState_event, newIssue.mLabels));
    connect(reply, &QNetworkReply::finished, [reply, this]() {
        setBusy(false);
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
            WRN << reply->error() << reply->errorString();
            notifyError(reply, "QGitlabClient::editIssue");
        }
    });
    return false;
}

bool QGitlabClient::createIssue(
        const int &projectID, const QString &title, const QString &description, const QStringList &labels)
{
    if (isBusy()) {
        return true;
    }
    setBusy(true);
    auto reply = sendRequest(
            QGitlabClient::POST, mUrlComposer.composeCreateIssueUrl(projectID, title, description, labels, ""));
    connect(reply, &QNetworkReply::finished, [reply, this]() {
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            WRN << reply->error() << reply->errorString();
            notifyError(reply, "QGitlabClient::createIssue");

        } else {
            QJsonDocument replyContent = QJsonDocument::fromJson(reply->readAll());
            QJsonObject jobj = replyContent.object();
            Issue issue(jobj);
            Q_EMIT issueCreated(issue);
        }
    });
    return false;
}

bool QGitlabClient::closeIssue(const int &projectID, const int &issueID)
{
    if (isBusy()) {
        return true;
    }
    setBusy(true);
    static const QString &title = QString();
    static const QString &description = QString();
    static const QString &assignee = QString();
    const QString state_event = "close";
    const QStringList &labels = QStringList();
    auto reply = sendRequest(QGitlabClient::PUT,
            mUrlComposer.composeEditIssueUrl(projectID, issueID, title, description, assignee, state_event, labels));
    connect(reply, &QNetworkReply::finished, [reply, this]() {
        setBusy(false);
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
            WRN << reply->error() << reply->errorString();
            notifyError(reply, "QGitlabClient::closeIssue");

        } else {
            Q_EMIT issueClosed();
        }
    });
    return false;
}

bool QGitlabClient::requestListofLabels(const LabelsRequestOptions &options)
{
    if (isBusy()) {
        return true;
    }
    setBusy(true);
    auto reply = sendRequest(QGitlabClient::GET, mUrlComposer.composeProjectLabelsUrl(options));
    connect(reply, &QNetworkReply::finished, [reply, this, options]() {
        setBusy(false);
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonParseError jsonError;
            auto replyContent = QJsonDocument::fromJson(reply->readAll(), &jsonError);
            if (QJsonParseError::NoError != jsonError.error) {
                WRN << "ERROR: Parsing json data: " << jsonError.errorString();
                notifyError(reply, "QGitlabClient::requestListofLabels");
            } else {
                QList<Label> labels;
                for (const QJsonValueRef &label : replyContent.array()) {
                    labels.push_back(Label(label.toObject()));
                }
                Q_EMIT listOfLabels(labels);
            }
            if (!requestNextPage(reply, options)) {
                setBusy(false);
                Q_EMIT labelsFetchingDone();
            }
        } else {
            WRN << reply->error() << reply->errorString();
            notifyError(reply, "QGitlabClient::requestListofLabels");
        }
    });
    return false;
}

bool QGitlabClient::requestProjectId(const QUrl &projectUrl)
{
    if (isBusy()) {
        return true;
    }
    setBusy(true);
    auto projectName = QDir(QUrl(projectUrl).path()).dirName();
    auto reply = sendRequest(QGitlabClient::GET, mUrlComposer.composeProjectUrl(projectName));
    connect(reply, &QNetworkReply::finished, [reply, projectUrl, this]() {
        setBusy(false);
        int projectID = -1;
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonParseError jsonError;
            auto replyContent = QJsonDocument::fromJson(reply->readAll(), &jsonError);
            if (QJsonParseError::NoError != jsonError.error) {
                WRN << "ERROR: Parsing json data: " << jsonError.errorString();
                const QString &errMsg = QString("ERROR: QGitlabClient::requestProjectId: Parsing json data: %1, #%2")
                                                .arg(jsonError.errorString())
                                                .arg(jsonError.offset);
                WRN << errMsg;
                notifyError(reply, errMsg);
            } else {
                auto content = replyContent.array();
                if (!content.isEmpty()) {
                    static const auto ID = "id";
                    for (const auto item : content) {
                        const QString projectUrlStr(projectUrl.toString());
                        const auto &itemObj = item.toObject();
                        const bool objOk = !itemObj.isEmpty() && itemObj.contains("web_url") && itemObj.contains("id");
                        if (objOk && itemObj.value("web_url").toString() == projectUrlStr) {
                            projectID = itemObj.value("id").toInteger();
                            break;
                        }
                    }
                }
            }
        } else {
            WRN << reply->error() << reply->errorString();
            notifyError(reply,
                    QString("Response %1 != 200")
                            .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
        }
        Q_EMIT requestedProjectID(projectID);
    });
    return false;
}

/*!
 * Returns true, if data is currently fetched from the server
 */
bool QGitlabClient::isBusy() const
{
    return m_busy;
}

bool QGitlabClient::requestGroupID(const QString &groupName)
{
    if (isBusy()) {
        return true;
    }
    setBusy(true);
    auto reply = sendRequest(QGitlabClient::GET, mUrlComposer.composeGroupUrl(groupName));
    connect(reply, &QNetworkReply::finished, [reply, this]() {
        setBusy(false);
        QString groupID = QString("-1");
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonParseError jsonError;
            auto replyContent = QJsonDocument::fromJson(reply->readAll(), &jsonError);
            if (QJsonParseError::NoError != jsonError.error) {
                WRN << "ERROR: Parsing json data: " << jsonError.errorString();
                const QString &errMsg = QString("ERROR: QGitlabClient::requestGroupId: Parsing json data: %1, #%2")
                                                .arg(jsonError.errorString())
                                                .arg(jsonError.offset);
                WRN << errMsg;
                notifyError(reply, errMsg);
            } else {
                auto content = replyContent.array();
                if (!content.isEmpty()) {
                    static const auto ID = "id";
                    for (const auto item : content) {
                        const auto &itemObj = item.toObject();
                        const bool objOk = !itemObj.isEmpty() && itemObj.contains("web_url") && itemObj.contains("id");
                        if (objOk) {
                            groupID = QString::number(itemObj.value("id").toInt());
                            break;
                        }
                    }
                }
            }
        } else {
            WRN << reply->error() << reply->errorString();
            notifyError(reply,
                    QString("Response %1 != 200")
                            .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
        }
        Q_EMIT requestedGroupID(groupID);
    });
    return false;
}

bool QGitlabClient::createProject(const QString &projectName, const QString &groupID)
{
    if (isBusy()) {
        return true;
    }
    setBusy(true);
    auto reply = sendRequest(QGitlabClient::POST, mUrlComposer.composeCreateProjectUrl(projectName, groupID));
    connect(reply, &QNetworkReply::finished, [reply, this, &projectName]() {
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            WRN << reply->error() << reply->errorString();
            notifyError(reply, "QGitlabClient::createProject");

        } else {
            QJsonDocument replyContent = QJsonDocument::fromJson(reply->readAll());
            QJsonObject jobj = replyContent.object();
            Issue issue(jobj);
            Q_EMIT projectCreated(projectName);
        }
    });
    return false;
}

bool QGitlabClient::requestGroupProjects(const QString &groupID)
{
    if (isBusy()) {
        return true;
    }
    setBusy(true);

    auto reply = sendRequest(QGitlabClient::GET, mUrlComposer.composeGroupProjectsUrl(groupID));
    connect(reply, &QNetworkReply::finished, [reply, this]() {
        setBusy(false);

        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonParseError jsonError;
            auto replyContent = QJsonDocument::fromJson(reply->readAll(), &jsonError);
            if (QJsonParseError::NoError != jsonError.error) {
                WRN << "ERROR: Parsing json data: " << jsonError.errorString();
                const QString &errMsg =
                        QString("ERROR: QGitlabClient::requestGroupProjects: Parsing json data: %1, #%2")
                                .arg(jsonError.errorString())
                                .arg(jsonError.offset);
                WRN << errMsg;
                notifyError(reply, errMsg);
            } else {
                auto content = replyContent.array();
                if (!content.isEmpty()) {
                    QStringList projectHttpUrls;
                    for (const auto item : content) {
                        const auto &itemObj = item.toObject();
                        const bool objOk = !itemObj.isEmpty() && itemObj.contains("http_url_to_repo");
                        if (objOk) {
                            projectHttpUrls << itemObj.value("http_url_to_repo").toString();
                        }
                    }
                    if (!projectHttpUrls.isEmpty()) {
                        Q_EMIT listOfGroupProjectsURLs(projectHttpUrls);
                    }
                }
            }
        } else {
            WRN << reply->error() << reply->errorString();
            notifyError(reply,
                    QString("Response %1 != 200")
                            .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
        }
    });

    return false;
}

QNetworkReply *QGitlabClient::sendRequest(QGitlabClient::ReqType reqType, const QUrl &uri)
{
    QNetworkRequest request(uri);
    request.setRawHeader("PRIVATE-TOKEN", mToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, kContentType);

    QNetworkReply *reply;
    switch (reqType) {
    case QGitlabClient::GET: {
        reply = mManager.get(request);
        break;
    }
    case QGitlabClient::POST: {
        reply = mManager.post(request, uri.query(QUrl::FullyEncoded).toUtf8());
        break;
    }
    case QGitlabClient::PUT: {
        reply = mManager.sendCustomRequest(request, "PUT");
        break;
    }
    default: {
        WRN << "Unknown request";
    }
    }

    return reply;
}

bool QGitlabClient::requestNextPage(QNetworkReply *reply, const RequestOptions &options)
{
    int page = pageNumberFromHeader(reply);
    const int totalPages = totalPagesFromHeader(reply);
    if (page >= 0 && totalPages >= 0) {
        if (page < totalPages) {
            if (isIssueRequest(reply)) {
                IssueRequestOptions nextPage = dynamic_cast<const IssueRequestOptions &>(options);
                nextPage.mPage = page + 1;
                m_busy = false; // workaround to enable request for the next page
                requestIssues(nextPage);
            } else {
                LabelsRequestOptions nextPage = dynamic_cast<const LabelsRequestOptions &>(options);
                nextPage.mPage = page + 1;
                m_busy = false; // workaround to enable request for the next page
                requestListofLabels(nextPage);
            }
            return true;
        }
    }
    return false;
}

int QGitlabClient::pageNumberFromHeader(QNetworkReply *reply) const
{
    return numberHeaderAttribute(reply, "x-page");
}

int QGitlabClient::totalPagesFromHeader(QNetworkReply *reply) const
{
    return numberHeaderAttribute(reply, "x-total-pages");
}

int QGitlabClient::numberHeaderAttribute(QNetworkReply *reply, const QString &headername) const
{
    if (!reply) {
        return -1;
    }

    const QString pageAttribute = reply->rawHeader(headername.toUtf8());
    if (pageAttribute.isEmpty()) {
        return -1;
    }

    return pageAttribute.toInt();
}

void QGitlabClient::setBusy(bool busy)
{
    if (busy == m_busy) {
        return;
    }

    m_busy = busy;
    Q_EMIT busyStateChanged(m_busy);
}
}

bool gitlab::QGitlabClient::isIssueRequest(QNetworkReply *reply) const
{
    return reply->request().url().query().contains("api/v4/issues");
}

void gitlab::QGitlabClient::notifyError(QNetworkReply *reply, const QString &text)
{
    const QStringList fields {
        reply ? reply->errorString() : QString(),
        text,
    };

    const QString &msg = fields.join("\n").simplified();
    if (msg.isEmpty()) {
        auto ptr = reinterpret_cast<quintptr>(reply);
        const QString &wrnMsg = QString("Error message is empty, args were: reply='0x%1', text='%2'")
                                        .arg(ptr, QT_POINTER_SIZE * 2, 16, QChar('0'))
                                        .arg(text);
        WRN << wrnMsg;
    }

    Q_EMIT connectionError(msg);
}
