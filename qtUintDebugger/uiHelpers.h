/*
 * 	This file is part of uintDebugger.
 *
 *    uintDebugger is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    uintDebugger is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with uintDebugger.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QTableWidget>
#include <QTableWidgetItem>

inline void SetPlaceholderRow(QTableWidget *table, const QString &message)
{
	table->clearSpans();
	table->clearContents();
	table->setRowCount(1);

	QTableWidgetItem *placeholder = new QTableWidgetItem(message);
	placeholder->setFlags(Qt::ItemIsEnabled);
	placeholder->setTextAlignment(Qt::AlignCenter);
	table->setItem(0, 0, placeholder);
	table->setSpan(0, 0, 1, table->columnCount());
}
