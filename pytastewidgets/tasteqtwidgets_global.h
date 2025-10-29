#pragma once

#define QT_ANNOTATE_ACCESS_SPECIFIER(a) __attribute__((annotate(#a)))

// #include <QGitlabAPI_global.h>
// #include <checkedfilterproxymodel.h>
#include <qgitlabapi/issue.h>
#include <qgitlabapi/label.h>
#include <qgitlabapi/qgitlabclient.h>
#include <requirements/requirement.h>
#include <requirements/requirementsmanager.h>
#include <requirements/requirementsmodelcommon.h>
#include <requirements/requirementsmodelbase.h>
#include <requirements/requirementswidget.h>
#include <reviews/review.h>
#include <reviews/reviewsmanager.h>
#include <reviews/reviewsmodelbase.h>
#include <reviews/reviewswidget.h>
#include <reviews/componentreviewsproxymodel.h>
#include <tracecommon/issuesmanager.h>
#include <tracecommon/tracecommonlibrary.h>
