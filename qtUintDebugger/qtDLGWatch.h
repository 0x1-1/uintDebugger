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

#include <QDockWidget>
#include <QList>
#include <QString>

#include "ui_qtDLGWatch.h"

class qtDLGWatch : public QDockWidget, public Ui_qtDLGWatchClass
{
	Q_OBJECT

public:
	struct WatchEntry
	{
		QString expression;
		int     byteSize;  // 1, 2, 4, or 8
		QString note;
	};

	qtDLGWatch(QWidget *parent = nullptr);
	~qtDLGWatch();

	void AddExpression(const QString &expr, int byteSize = 4, const QString &note = QString());

	const QList<WatchEntry> &entries() const { return m_entries; }

public slots:
	void Refresh();

private slots:
	void OnAdd();
	void OnRemove();
	void OnCellChanged(int row, int col);

private:
	QList<WatchEntry> m_entries;

	void RebuildTable();
	quint64 EvaluateEntry(const WatchEntry &e, bool &ok) const;
};
