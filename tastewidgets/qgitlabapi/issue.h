#pragma once

#include "QGitlabAPI_global.h"

#include <QDateTime>
#include <QJsonObject>
#include <QStringList>

namespace gitlab {

/**
 * @brief The Issue class holds data for a single issue
 */
class QGITLABAPI_EXPORT Issue
{
public:
    Issue();
    Issue(const Issue &issue);
    Issue(const QJsonObject &issue);

    QUrl mUrl; // Web page of the issue
    int mIssueID; /// unique ID for the whole server
    int mIssueIID; /// unique ID within it's project
    QString mTitle;
    QString mDescription;
    QString mAuthor;
    QString mAssignee;
    QString mState;
    QString mState_event; /// @note should that be part of this class?
    QStringList mLabels;
    QString mIssueType;
    QDateTime mCreatedAt;
    QDateTime mUpdatedAt;
    int mNotesCount;
};

}
