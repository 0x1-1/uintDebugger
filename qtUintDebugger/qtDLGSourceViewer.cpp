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
#include "qtDLGSourceViewer.h"

#include <QFile>
#include <QShortcut>

#include <Windows.h>

qtDLGSourceViewer::qtDLGSourceViewer(QWidget *parent, Qt::WindowFlags flags)
	: QWidget(parent, flags),
	IsSourceAvailable(false)
{
	setupUi(this);
	this->setLayout(verticalLayout);

	connect(new QShortcut(Qt::Key_Escape,this),&QShortcut::activated,this,&qtDLGSourceViewer::close);
}

qtDLGSourceViewer::~qtDLGSourceViewer()
{

}

void qtDLGSourceViewer::OnDisplaySource(QString sourceFile,int LineNumber)
{
	QFile file(sourceFile);
	listSource->clear();
	
	if(file.open(QIODevice::ReadOnly))
	{
		int LineCounter = 1;
		while(file.bytesAvailable() > 0)
		{
			QListWidgetItem *pItem = new QListWidgetItem();
			pItem->setText(file.readLine().replace("\r","").replace("\n",""));
			if(LineCounter == LineNumber)
			{
				pItem->setBackground(QBrush(QColor("Blue"),Qt::SolidPattern));
				pItem->setForeground(QBrush(QColor("White"),Qt::SolidPattern));
			}

			listSource->addItem(pItem);
			LineCounter++;
		}
		IsSourceAvailable = true;
		file.close();
	}
	else
	{
		IsSourceAvailable = false;
		listSource->addItem(QStringLiteral("Source file could not be loaded for this address."));
	}
	return;
}
