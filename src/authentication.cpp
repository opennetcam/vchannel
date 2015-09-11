/**
 * FILE:		authentication.cpp
 *
 * DESCRIPTION:
 * This is the HTTP authentication dialog.
 * -----------------------------------------------------------------------
 *    Copyright (C) 2010-2015 OpenNetcam Project.
 *
 *   This  software is released under the following license:
 *        - GNU General Public License (GPL) version 3 for use with the
 *          Qt Open Source Edition (http://www.qt.io)
 *
 *    Permission to use, copy, modify, and distribute this software and its
 *    documentation for any purpose and without fee is hereby granted
 *    in accordance with the provisions of the GPLv3 which is available at:
 *    http://www.gnu.org/licenses/gpl.html
 *
 *    This software is provided "as is" without express or implied warranty.
 *
 * -----------------------------------------------------------------------
 */

#include <QtGui>

#include "authentication.h"

Authentication::Authentication(QWidget *parent) : QDialog(parent)
{
    ui.setupUi(this);
}

void Authentication::setValues(QString user, QString pass, bool remember)
{
    ui.lineEdit->setText(user);
    ui.lineEdit_2->setText(pass);
    ui.checkBox->setChecked(remember);
}
