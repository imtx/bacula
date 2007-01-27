/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

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

/*
 *  Main Window control for qt-console
 *
 *   Kern Sibbald, January MMVI
 *
 */ 

#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
   QTreeWidgetItem *item, *topItem;
   setupUi(this);                     /* Setup UI defined by main.ui (designer) */
   stackedWidget->setCurrentIndex(0);
   /* Dummy message ***FIXME*** remove a bit later */
   textEdit->setPlainText("Hello Baculites\nThis is the main console window.");
   lineEdit->setFocus();
   /* Connect command line edit to input_line */
   connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(input_line()));
   connect(actionAbout_qt_console, SIGNAL(triggered()), this, SLOT(about()));

   connect(treeWidget, SIGNAL(itemActivated(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemPressed(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));


   /* Dummy setup of treeWidget */
   treeWidget->clear();
   treeWidget->setColumnCount(1);
   treeWidget->setHeaderLabel("Selection");
   topItem = new QTreeWidgetItem(treeWidget);
   topItem->setText(0, "Rufus");
   item = new QTreeWidgetItem(topItem);
   item->setText(0, "Console");
   item->setText(1, "0");
   item = new QTreeWidgetItem(topItem);
   item->setText(0, "Restore");
   item->setText(1, "1");
   treeWidget->expandItem(topItem);
}

void MainWindow::treeItemClicked(QTreeWidgetItem *item, int column)
{
   (void)column;
   int index = item->text(1).toInt();
   if (index >= 0 && index < 2) {
      stackedWidget->setCurrentIndex(index);
   }
}


/*
 * The user just finished typing a line in the command line edit box
 */
void MainWindow::input_line()
{
   QString cmdStr = lineEdit->text();    /* Get the text */
   lineEdit->clear();                    /* clear the lineEdit box */
   textEdit->append(cmdStr);             /* append text on screen */
}
void MainWindow::about()
{
   QMessageBox::about(this, tr("About qt-console"),
            tr("<h2>Qt_console 0.1</h2>"
            "<p>Copyright &copy; 2007 Free Software Foundation Europe e.V."
            "<p>The <b>qt-console</b> is an administrative"
               " interface to the Director."));
}

void set_textf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   int len;
   va_start(arg_ptr, fmt);
   len = bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   mainWin->textEdit->append(buf);
}

void set_text(const char *buf)
{
   mainWin->textEdit->append(buf);
}

void set_statusf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   int len;
   va_start(arg_ptr, fmt);
   len = bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
// gtk_label_set_text(GTK_LABEL(status1), buf);
// set_scroll_bar_to_end();
// ready = false;
}

void set_status_ready()
{
   mainWin->statusBar()->showMessage("Ready");
// ready = true;
// set_scroll_bar_to_end();
}

void set_status(const char *buf)
{
   mainWin->statusBar()->showMessage(buf);
// ready = false;
}
