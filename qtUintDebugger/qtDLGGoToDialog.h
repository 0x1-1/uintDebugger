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
#ifndef QTDLGGOTODIALOG_H
#define QTDLGGOTODIALOG_H

#include "ui_qtDLGGoToDialog.h"

class qtDLGGoToDialog : public QDialog, private Ui_qtDLGGoToDialogClass
{
	Q_OBJECT

public:
	qtDLGGoToDialog(QString *pData, QStringList completerList, QWidget *parent = 0, Qt::WindowFlags flags = {});
	~qtDLGGoToDialog();

private:
	QString *m_pDataString;
	QCompleter *m_pLineCompleter;

private slots:
	void OpenNewData();
};

#endif
