/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this program. If not, see
<https://www.gnu.org/licenses/lgpl-2.1.html>.
*/

#pragma once

// #include "commandsstackbase.h"
#include "requirementsmodelcommon.h"

#include <QAbstractTableModel>
#include <QList>

// namespace shared {
// class PropertyTemplateConfig;
// class VEObject;



namespace requirement {
class RequirementsModelCommon;

/*!
 * \brief Model to hold requirements for a Qt view .
 */
class RequirementsModelBase : public requirement::RequirementsModelCommon
{
    Q_OBJECT

public:
    RequirementsModelBase(requirement::RequirementsManager *manager, QObject *parent = nullptr);

    static QStringList addInternalRefs(const QStringList &refIfIDs, QList<Requirement> *requirements);
    static Requirement requirementFromIdExtended(const QString &reqIfID, QList<Requirement> *requirements);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    void addRequirements(const QList<Requirement> &requirements) override;
    void clearRequirements() override;
    void createModelRequirement(Requirement &requirement);
    void editModelRequirement(Requirement &requirement);
    void deleteModelRequirement(const Requirement &requirement);
    /* Remove a requirement from the local model without adding it to m_deleted.
       Use this when the deletion has already been applied on the server. */
    void deleteModelRequirementDirect(const Requirement &requirement);
    void applyGitLabEdits(bool allowDelete);
    /*! Mark/unmark that the model has pending local edits that need export */
    void setPendingEdits(bool pending);
    /*! Returns true when there are pending deletions/edits that still need Apply Edits */
    bool hasPendingEdits() const;
    QList<Requirement>* getRequirements() { return &m_requirements; };
    bool syncRequirements();
    QStringList unreferencedFromId(const QString &reqIfID) const;
    QStringList parentsFromId(const QString &reqIfID, const QStringList parents) const;
    QStringList childrenFromId(const QString &reqIfID, const QStringList parents) const;
    void setLocal();

    enum modelType
    {
        Empty,
        SRS,
        SSS,
        Both
    };

    void exportGitLabRequirements(QString url, QString token, enum RequirementsModelBase::modelType type);

    void changeModelState(enum RequirementsModelBase::modelType selection);
    enum modelType getState() {return m_type;};

    void setExportType(enum RequirementsModelBase::modelType type) {m_exportType = type;};
    enum modelType getExportType() {return m_exportType;};

    enum RoleNames
    {
        IssueLinkRole = Qt::UserRole + 1,
        IssueIdRole,
        TagsRole,
        TitleRole,
        DetailDescriptionRole,
        AuthorRole,
        UserRole,
        ReqIfIdRole,
        TypeRole,
        PriorityRole,
        StatusRole,
        NoteRole,
        ParentsRole,
        ChildrenRole,
        JustificationRole,
        ValidationRole,
        ValVersionRole,
        ValDescriptionRole,
        ValStatusRole,
        ValEvidenceRole,
        ComplianceRole,
        ComplianceStatusRole,
        END
    };

public Q_SLOTS:
    void fetchingFinished();

Q_SIGNALS:
    void exportCompleted() const;
    void newRequirement(const Requirement &requirement) const;
    void updateRequirement(const Requirement &requirement) const;
    void deleteRequirement(const Requirement &requirement) const;

private:
    Issue remoteMatch(QString reqIfId);
    void updateServerStatus();
    void pushRequirements();
    bool reqIfIDExistsExtended(const QString &reqIfID, QList<Requirement> *requirements) const;

    QString m_url;
    QString m_token;

    enum modelType m_type;
    enum modelType m_exportType;

    volatile bool m_newTarget;
    volatile bool m_edit;
    volatile bool m_local;
    volatile bool m_syncRefs;
    volatile bool m_pendingEdits;
    volatile bool m_checkingServer;
    QList<Requirement> m_export;
    QList<Requirement> m_remote;
    QList<Requirement> m_deleted;
};

} // namespace requirement
