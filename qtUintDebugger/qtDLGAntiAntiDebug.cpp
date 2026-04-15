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
#include "qtDLGAntiAntiDebug.h"
#include "clsDebugger/clsDebugger.h"
#include "clsHelperClass.h"
#include "clsAntiAntiDebug.h"

#include <QShortcut>

qtDLGAntiAntiDebug::qtDLGAntiAntiDebug(QWidget *parent, int iPID)
    : QWidget(parent, Qt::Window)
    , m_iPID(iPID)
{
    setupUi(this);
    setStyleSheet(clsHelperClass::LoadStyleSheet());
    setAttribute(Qt::WA_DeleteOnClose, true);

    new QShortcut(Qt::Key_Escape, this, SLOT(close()));

    connect(btnApply,  &QPushButton::clicked, this, &qtDLGAntiAntiDebug::OnApply);
    connect(btnRevert, &QPushButton::clicked, this, &qtDLGAntiAntiDebug::OnRevert);

    refreshUI();
}

qtDLGAntiAntiDebug::~qtDLGAntiAntiDebug()
{
}

void qtDLGAntiAntiDebug::refreshUI()
{
    clsDebugger *pDbg = clsDebugger::GetInstance();
    clsAntiAntiDebug &aad = pDbg->m_antiAntiDebug;

    const bool attached = aad.isSetup();
    btnApply->setEnabled(attached);
    btnRevert->setEnabled(attached);

    if (!attached)
    {
        lblStatus->setText("No process attached.");
        lblPatchCount->setText("0 / 4 patches applied");
        return;
    }

    // Sync checkboxes with current patch state.
    const QVector<AntiAntiPatchEntry> &patches = aad.patches();
    int appliedCount = 0;
    for (const AntiAntiPatchEntry &e : patches)
        if (e.applied) ++appliedCount;

    auto findApplied = [&](const QString &name) -> bool {
        for (const AntiAntiPatchEntry &e : patches)
            if (e.name == name) return e.applied;
        return false;
    };

    cbIsDebuggerPresent->setChecked(findApplied("IsDebuggerPresent"));
    cbCheckRemoteDebuggerPresent->setChecked(findApplied("CheckRemoteDebuggerPresent"));
    cbNtQueryInformationProcess->setChecked(findApplied("NtQueryInformationProcess"));
    cbNtSetInformationThread->setChecked(findApplied("NtSetInformationThread"));

    lblPatchCount->setText(QString("%1 / 4 patches applied").arg(appliedCount));
    lblStatus->setText("Ready.");
}

void qtDLGAntiAntiDebug::OnApply()
{
    clsDebugger *pDbg = clsDebugger::GetInstance();
    clsAntiAntiDebug &aad = pDbg->m_antiAntiDebug;

    if (!aad.isSetup())
        return;

    QString result;
    int ok = 0, fail = 0;

    auto tryPatch = [&](bool checked, bool (clsAntiAntiDebug::*fn)(),
                         const QString &name)
    {
        if (!checked) return;
        bool success = (aad.*fn)();
        if (success) ++ok;
        else {
            ++fail;
            result += QString("  [!] %1 failed\n").arg(name);
        }
    };

    tryPatch(cbIsDebuggerPresent->isChecked(),
             &clsAntiAntiDebug::PatchIsDebuggerPresent,
             "IsDebuggerPresent");
    tryPatch(cbCheckRemoteDebuggerPresent->isChecked(),
             &clsAntiAntiDebug::PatchCheckRemoteDebuggerPresent,
             "CheckRemoteDebuggerPresent");
    tryPatch(cbNtQueryInformationProcess->isChecked(),
             &clsAntiAntiDebug::PatchNtQueryInformationProcess,
             "NtQueryInformationProcess");
    tryPatch(cbNtSetInformationThread->isChecked(),
             &clsAntiAntiDebug::PatchNtSetInformationThread,
             "NtSetInformationThread");

    if (fail > 0)
        lblStatus->setText(QString("Applied %1, failed %2 — check function addresses.").arg(ok).arg(fail));
    else
        lblStatus->setText(QString("Applied %1 patch(es) successfully.").arg(ok));

    refreshUI();
}

void qtDLGAntiAntiDebug::OnRevert()
{
    clsDebugger *pDbg = clsDebugger::GetInstance();
    clsAntiAntiDebug &aad = pDbg->m_antiAntiDebug;

    if (!aad.isSetup())
        return;

    bool ok = aad.UnpatchAll();
    lblStatus->setText(ok ? "All patches reverted." : "Revert failed for one or more patches.");
    refreshUI();
}
