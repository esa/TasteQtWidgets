#include "issue.h"

#include <QJsonArray>
#include <QString>

#include <iostream>

using namespace gitlab;

Issue::Issue()
{
    mUrl = QUrl();
    mIssueID = 0;
    mIssueIID = 0;
    mTitle = QString();
    mDescription = QString();
    mAuthor = QString();
    mAssignee = QString();
    mState = QString();
    mLabels.clear();
    mCreatedAt = QDateTime::currentDateTime();
    mUpdatedAt = QDateTime::currentDateTime();
    mNotesCount = 0;
}

Issue::Issue(const Issue &issue) :
    mUrl(issue.mUrl),
    mIssueID(issue.mIssueID),
    mIssueIID(issue.mIssueIID),
    mTitle(issue.mTitle),
    mDescription(issue.mDescription),
    mAuthor(issue.mAuthor),
    mAssignee(issue.mAssignee),
    mState(issue.mState),
    mLabels(issue.mLabels),
    mCreatedAt(issue.mCreatedAt),
    mUpdatedAt(issue.mUpdatedAt),
    mNotesCount(issue.mNotesCount)
{
}


Issue::Issue(const QJsonObject &issue)
{
    mUrl = issue["web_url"].toString();
    mIssueID = issue["id"].toInteger();
    mIssueIID = issue["iid"].toInteger();
    mTitle = issue["title"].toString();
    mDescription = issue["description"].toString();
    mAuthor = issue["author"]["name"].toString();
    mAssignee = issue["assignee"]["name"].toString();
    mState = issue["state"].toString();
    QVariant v = issue["labels"].toString();
    QString s = v.toString();
    mLabels.clear();
    for (const QJsonValueRef &value : issue["labels"].toArray()) {
        QString label = value.toString();
        if (!label.isEmpty()) {
            mLabels.append(label);
        }
    }
    mIssueType = issue["issue_type"].toString();
    mCreatedAt = QDateTime::fromString(issue["created_at"].toString(), Qt::ISODate);
    mUpdatedAt = QDateTime::fromString(issue["updated_at"].toString(), Qt::ISODate);
    mNotesCount = issue["user_notes_count"].toInt();
}
