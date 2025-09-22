#pragma once

#include "QGitlabAPI_global.h"
#include "issue.h"
#include "label.h"
#include "urlcomposer.h"

#include <QList>
#include <QNetworkAccessManager>
#include <QString>

namespace gitlab {

class IssueRequestOptions;
class LabelsRequestOptions;
class RequestOptions;

/**
 * @brief The QGitlabClient class is the main class to start requests on the Gitlab server
 */
class QGITLABAPI_EXPORT QGitlabClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyStateChanged)

public:
    enum ReqType
    {
        GET = 0,
        POST = 1,
        PUT = 2,
    };

    QGitlabClient();
    /*!
     * \brief Sets the url and token to operate with the GitlabAPI
     * \param The url parameter is stripped of the project name and set with the path "/api/v4" so it can
     *        be used with the different Gitlab API Endpoints
     * \param This the personal token used on any call to the Gitlab API.
     */
    void setCredentials(const QString &url, const QString &token);
    /*!
     * \brief Makes one or more request to the gitlab api to retrieve all the requirements
     * \param the options are used to filter the requirements search
     * \return Returns true if there's a pending request otherwise false.
     */
    bool requestIssues(const IssueRequestOptions &options);
    /*!
     * \brief Edits any Issue (requirement or review)
     * \param projectID used for the query
     * \param issueID used for the query
     * \param newIssue the issue with the edits that need to be changed.
     * \return Returns true if there's a pending request otherwise false.
     */
    bool editIssue(const int &projectID, const int &issueID, const Issue &newIssue);
    /*!
     * \brief Creates a new issue (requirement or review)
     * \param projectID used for the query
     * \param title that corresponds to the gitlab issue title
     * \param description that corresponds to the gitlab issue description
     * \param labels that will be included in the issue
     * \return Returns true if there's a pending request otherwise false.
     */
    bool createIssue(const int &projectID, const QString &title, const QString &description, const QStringList &labels);
    /*!
     * \brief Closes an issue (removes a requirement or review)
     * \param projectID used for the query
     * \param issueID used for the query
     * \return Returns true if there's a pending request otherwise false.
     */
    bool closeIssue(const int &projectID, const int &issueID);
    /*!
     * \brief Makes one or more request to the gitlab api to retrieve all the labels
     * \param the options are used to filter the labels search
     * \return Returns true if there's a pending request otherwise false.
     */

    bool requestListofLabels(const LabelsRequestOptions &options);
    /*!
     * \brief request the project ID to be used on any of the queries to the gitlab API
     * \param projectID retrieveed using the project name included in the url
     * \return Returns true if there's a pending request otherwise false.
     */

    bool requestProjectId(const QUrl &projectUrl);
    /*!
     * \brief Helper function that returns true if there is any ongoing request to the gitlab server
     * \return true if busy
     */
    bool isBusy() const;

    bool requestGroupID(const QString &groupName);

    bool createProject(const QString &projectName, const QString &groupID);

    bool requestGroupProjects(const QString &groupID);
    /*!
     * \brief token retrieves the token used by this class
     * \return
     */
    QString token();
Q_SIGNALS:
    /*!
     * Provides a block/page of issues
     */
    void listOfIssues(QList<Issue>);
    /*!
     * Is send either when fetching data is started. Or when the fething of data ended.
     * The busy property is true, while the fetching is active
     */
    void busyStateChanged(bool);
    /*!
     * Sent after successfully fetching issues
     */
    void issueFetchingDone();
    /*!
     * Sent after successfully fetching labels
     */
    void labelsFetchingDone();
    /*!
     * \brief This is the list of labels retrieved from the gitlab project.
     * If there are too many labels to be fetched, the signal will be emitted
     * once per successful network request
     */
    void listOfLabels(QList<Label>);
    /*!
     * \brief This is the project ID obtained after calling `requestProjectId`.
     * The signal will be emitted if there's a response and the url supplied matches
     * the one contained in the reply's json data.
     */
    void requestedProjectID(int);
    /*!
     * \brief This is the group ID obtained after calling `requestGroupId`.
     * The signal will be emitted if there's a response and the url supplied matches
     * the one contained in the reply's json data.
     */
    void requestedGroupID(QString groupID);

    /*!
     * \brief This signal is emitted for any failing request of this class
     * \param errorString returns a text message corresponding to request error.
     */
    void connectionError(QString errorString);
    /*!
     * \brief This signal is emitted if the server return a sucessful reply
     * on issue creation request (requirement/review).
     * \param issue An Issue object is created to convert it to a requirement or a review.
     */
    void issueCreated(const Issue &issue);
    /*!
     * \brief This signal is emitted if an issue is Closed. At the moment is used to refresh the list of requirements.
     */
    void issueClosed();
    /*!
     * \brief projectCreated signal is emitted if the project was successfully created
     * \param projectName
     */
    void projectCreated(const QString &projectName);
    /*!
     * \brief listOfGroupProjectsURLs This signal is emitted if the list of projects for a gitlab group is succefully
     * retrieved \param projectUrls
     */
    void listOfGroupProjectsURLs(QStringList projectUrls);

protected:
    QNetworkReply *sendRequest(ReqType reqType, const QUrl &url);
    /*!
     * \brief requestNextPage makes a request for next page (if any ) for issues or labels
     * \param reply
     * \param options which can be any derived class from RequestOptions
     * \return true if there was another requestable page
     */
    bool requestNextPage(QNetworkReply *reply, const RequestOptions &options);
    int pageNumberFromHeader(QNetworkReply *reply) const;
    int totalPagesFromHeader(QNetworkReply *reply) const;
    int numberHeaderAttribute(QNetworkReply *reply, const QString &headername) const;
    void setBusy(bool busy);
    bool isIssueRequest(QNetworkReply *reply) const;

private:
    QString mUsername;
    UrlComposer mUrlComposer;
    QString mToken;
    QNetworkAccessManager mManager;
    bool m_busy = false;

    void notifyError(QNetworkReply *reply, const QString &text = QString());
};
}
