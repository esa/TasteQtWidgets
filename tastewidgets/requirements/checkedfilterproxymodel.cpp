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

#include "checkedfilterproxymodel.h"

namespace requirement {

CheckedFilterProxyModel::CheckedFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel { parent }
{
}

void CheckedFilterProxyModel::setFilterChecked(bool on)
{
    m_filterChecked = on;
}

bool CheckedFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourcParent) const
{
    QModelIndex idx = sourceModel()->index(sourceRow, filterKeyColumn(), sourcParent);

    const bool checked = idx.data(Qt::CheckStateRole).toBool();
    return m_filterChecked ? checked : !checked;
}

} // namespace requirement
