/**
 * FILE:		authentication.h
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

#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <QDialog>
#include "ui_authdialog.h"
#include "../include/common.h"

class Authentication : public QDialog
{
Q_OBJECT

public:
    explicit Authentication(QWidget *parent = 0);
    void setValues(QString user, QString pass, bool remember = false);
    QString getUser() { return ui.lineEdit->text(); }
    QString getPass() { return ui.lineEdit_2->text(); }
    bool getFlag() { return ui.checkBox->isChecked(); }

signals:

public slots:

private:
    Ui::AuthenticationDialog ui;
};

#endif // AUTHENTICATION_H
