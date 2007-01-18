/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/



#include "ui_main.h"
#include <QtGui>

int main(int argc, char *argv[])
{
   QApplication app(argc, argv);        
   app.setQuitOnLastWindowClosed(true);
   
   QMainWindow *main = new QMainWindow;
   Ui::MainWindow ui;
   ui.setupUi(main);

   ui.textEdit->setPlainText("Hello Baculites\nThis is the main console window.");
   main->show();
   return app.exec();
}
