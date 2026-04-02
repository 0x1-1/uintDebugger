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
#include "qtDLGOpenNewFile.h"

#include "clsMemManager.h"

qtDLGOpenNewFile::qtDLGOpenNewFile(QWidget *parent)
	: QDialog(parent),
	m_filePath(""),
	m_commandLine("")
{
	setWindowTitle("Open Target");
	setModal(true);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	QHBoxLayout *fileLayout = new QHBoxLayout();
	QHBoxLayout *argumentLayout = new QHBoxLayout();

	QLabel *fileLabel = new QLabel("Target:", this);
	m_filePathLine = new QLineEdit(this);
	m_browseButton = new QPushButton("Browse...", this);
	QLabel *argumentLabel = new QLabel("Arguments:", this);
	m_argumentLine = new QLineEdit(this);
	m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);

	fileLayout->addWidget(fileLabel);
	fileLayout->addWidget(m_filePathLine);
	fileLayout->addWidget(m_browseButton);

	argumentLayout->addWidget(argumentLabel);
	argumentLayout->addWidget(m_argumentLine);

	mainLayout->addLayout(fileLayout);
	mainLayout->addLayout(argumentLayout);
	mainLayout->addWidget(m_buttonBox);

	connect(m_browseButton, SIGNAL(clicked()), this, SLOT(BrowseForTarget()));
	connect(m_buttonBox, SIGNAL(accepted()), this, SLOT(AcceptSelection()));
	connect(m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

qtDLGOpenNewFile::~qtDLGOpenNewFile()
{
}

void qtDLGOpenNewFile::BrowseForTarget()
{
	QString filePath = QFileDialog::getOpenFileName(this,
		"Please select a Target",
		QDir::currentPath(),
		"Executables (*.exe)");

	if(!filePath.isEmpty())
		m_filePathLine->setText(QDir::toNativeSeparators(filePath));
}

void qtDLGOpenNewFile::AcceptSelection()
{
	m_filePath = m_filePathLine->text().trimmed();
	m_commandLine = m_argumentLine->text();

	if(m_filePath.isEmpty())
	{
		QMessageBox::warning(this, "uintDebugger", "Please select a target executable.", QMessageBox::Ok, QMessageBox::Ok);
		return;
	}

	accept();
}

void qtDLGOpenNewFile::GetFilePathAndCommandLine(QString &filePath, QString &commandLine)
{
	filePath = QDir::toNativeSeparators(m_filePath);
	commandLine = m_commandLine;
}
