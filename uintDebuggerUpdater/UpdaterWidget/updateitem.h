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
#ifndef UPDATEITEM_H
#define UPDATEITEM_H

#include <QString>

class UpdateItem
{
public:
    UpdateItem();

    QString m_size; // f.e.: 500 Kb
    QString m_package;
    QString m_uri;
    QString m_sha256; // expected hex-encoded SHA-256 of the downloaded file
};

#endif // UPDATEITEM_H
