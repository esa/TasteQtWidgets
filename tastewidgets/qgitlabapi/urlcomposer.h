#pragma once

#include <QUrl>

namespace gitlab {

class IssueRequestOptions;
class LabelsRequestOptions;

/*!
 * Class to create URLs to fetch data from gitlab
 */
class UrlComposer
{
public:
    enum UrlTypes
    {
        GetIssues,
        CreateIssue,
        EditIssue,
        Projects,
        Groups,
        GroupProjects,
        CreateProject,
        ProjectLabels,
    };

    UrlComposer();

    static QUrlQuery setQuery(const QMap<QString, QVariant> &data);

    QUrl composeGetIssuesUrl(const int &projectID, const IssueRequestOptions &options) const;
    QUrl composeCreateIssueUrl(const int &projectID, const QString &title, const QString &description,
            const QStringList &labels, const QString &assignee) const;
    QUrl composeEditIssueUrl(const int &projectID, const int &issueID, const QString &title = QString(),
            const QString &description = QString(), const QString &assignee = QString(),
            const QString &state_event = QString(), const QStringList &labels = QStringList()) const;

    QUrl composeProjectLabelsUrl(const LabelsRequestOptions &options) const;
    QUrl composeProjectUrl(const QString &projectName) const;
    QUrl composeGroupUrl(const QString &groupName) const;
    QUrl composeCreateProjectUrl(const QString &projectName, const QString &groupID) const;
    QUrl composeGroupProjectsUrl(const QString &groupID) const;

    void setBaseURL(const QUrl &newBaseURL);

    QUrl baseURL() const;

protected:
    QString composeUrl(UrlComposer::UrlTypes target) const;

private:
    QUrl mBaseURL;
};

}
