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

#pragma once

#include <QSortFilterProxyModel>

namespace requirement {

/**
 * @brief The CheckedFilterProxyModel class is used to filter a Qt view model for rows that are checked
 * Set the colum for the checked state with @see setFilterKeyColumn()
 */
class CheckedFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit CheckedFilterProxyModel(QObject *parent = nullptr);

    /**
     * @brief setFilterChecked if @p on is true, only checked row are shown. if @p on is false, only unchecked rows are
     * shown. @note Default is to show checked row.
     */
    void setFilterChecked(bool on);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool m_filterChecked = true;
};

} // namespace requirement
