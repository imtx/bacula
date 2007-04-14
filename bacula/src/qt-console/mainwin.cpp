/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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
 *   Version $Id$
 *
 *  Main Window control for bat (qt-console)
 *
 *   Kern Sibbald, January MMVII
 *
 */ 

#include "bat.h"

MainWin::MainWin(QWidget *parent) : QMainWindow(parent)
{

   mainWin = this;
   setupUi(this);                     /* Setup UI defined by main.ui (designer) */
   treeWidget->clear();
   treeWidget->setColumnCount(1);
   treeWidget->setHeaderLabel("Select Page");
   treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);

   createPages();

   resetFocus();

   createConnections();

   this->show();

   readSettings();

   m_console->connect();
}

void MainWin::createPages()
{
   DIRRES *dir;
   QTreeWidgetItem *item, *topItem;

   /* Create console tree stacked widget item */
   m_console = new Console(stackedWidget);

   /* Console is special -> needs director*/
   /* Just take the first Director */
   LockRes();
   dir = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   m_console->setDirRes(dir);
   UnlockRes();

   /* The top tree item representing the director */
   topItem = createTopPage(dir->name());
   topItem->setIcon(0, QIcon(QString::fromUtf8("images/server.png")));

   /* Create Tree Widget Item */
   item = createPage("Console", topItem);
   m_console->setTreeItem(item);

   /* Append to pagelist */
   hashInsert(item, m_console);

   /* Set Color of treeWidgetItem for the console
   * It will be set to green in the console class if the connection is made.
   */
   QBrush redBrush(Qt::red);
   item->setForeground(0, redBrush);

   /*
    * Now with the console created, on with the rest, these are easy   
    * All should be
    * 1. create tree widget item
    * 2. create object passing pointer to tree widget item (modified constructors to pass QTreeWidget pointers)
    * 3. append to stackhash
    */

   /* brestore */
   item=createPage("brestore", topItem);
   bRestore* brestore=new bRestore(stackedWidget);
   hashInsert(item, brestore);


   /* lastly for now, the medialist */
   item=createPage("Media", topItem );
   MediaList* medialist=new MediaList(stackedWidget, m_console);
   hashInsert(item, medialist);

   /* Iterate through and add to the stack */
   foreach(Pages *page, m_pagehash)
      page->dockPage();

   treeWidget->expandItem(topItem);
   stackedWidget->setCurrentWidget(m_console);
}

/* Create a root Tree Widget */
QTreeWidgetItem *MainWin::createTopPage(char *name)
{
   QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget);
   item->setText(0, name);
   return item;
}

/* Create A Tree Widget Item which will be associated with a Page in the stacked widget */
QTreeWidgetItem *MainWin::createPage(char *name, QTreeWidgetItem *parent)
{
   QTreeWidgetItem *item = new QTreeWidgetItem(parent);
   item->setText(0, name);
   return item;
}

/*
 * Handle up and down arrow keys for the command line
 *  history.
 */
void MainWin::keyPressEvent(QKeyEvent *event)
{
   if (m_cmd_history.size() == 0) {
      event->ignore();
      return;
   }
   switch (event->key()) {
   case Qt::Key_Down:
      if (m_cmd_last < 0 || m_cmd_last >= (m_cmd_history.size()-1)) {
         event->ignore();
         return;
      }
      m_cmd_last++;
      break;
   case Qt::Key_Up:
      if (m_cmd_last == 0) {
         event->ignore();
         return;
      }
      if (m_cmd_last < 0 || m_cmd_last > (m_cmd_history.size()-1)) {
         m_cmd_last = m_cmd_history.size() - 1;
      } else {
         m_cmd_last--;
      }
      break;
   default:
      event->ignore();
      return;
   }
   lineEdit->setText(m_cmd_history[m_cmd_last]);
}

void MainWin::createConnections()
{
   /* Connect signals to slots */
   connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(input_line()));
   connect(actionAbout_bat, SIGNAL(triggered()), this, SLOT(about()));

#ifdef xxx
     connect(treeWidget, SIGNAL(itemActivated(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemPressed(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));  
#endif
   connect(treeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemDoubleClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(
           currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
           this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   connect(stackedWidget, SIGNAL(currentChanged(int)),
           this, SLOT(stackItemChanged(int)));

   connect(actionQuit, SIGNAL(triggered()), app, SLOT(closeAllWindows()));
   connect(actionConnect, SIGNAL(triggered()), m_console, SLOT(connect()));
   connect(actionStatusDir, SIGNAL(triggered()), m_console, SLOT(status_dir()));
   connect(actionSelectFont, SIGNAL(triggered()), m_console, SLOT(set_font()));
   connect(actionLabel, SIGNAL(triggered()), this,  SLOT(labelDialogClicked()));
   connect(actionRun, SIGNAL(triggered()), this,  SLOT(runDialogClicked()));
   connect(actionRestore, SIGNAL(triggered()), this,  SLOT(restoreDialogClicked()));
   connect(actionUndock, SIGNAL(triggered()), this,  SLOT(undockWindowButton()));
   connect(actionToggleDock, SIGNAL(triggered()), this,  SLOT(toggleDockContextWindow()));
}

/* 
 * Reimplementation of QWidget closeEvent virtual function   
 */
void MainWin::closeEvent(QCloseEvent *event)
{
   writeSettings();
   m_console->writeSettings();
   m_console->terminate();
   event->accept();
   foreach(Pages *page, m_pagehash){
      if( !page->isDocked() )
         page->close();
   }
}

void MainWin::writeSettings()
{
   QSettings settings("bacula.org", "bat");

   settings.beginGroup("MainWin");
   settings.setValue("winSize", size());
   settings.setValue("winPos", pos());
   settings.endGroup();
}

void MainWin::readSettings()
{ 
   QSettings settings("bacula.org", "bat");

   settings.beginGroup("MainWin");
   resize(settings.value("winSize", QSize(1041, 801)).toSize());
   move(settings.value("winPos", QPoint(200, 150)).toPoint());
   settings.endGroup();
}

/*
 * This subroutine is called with an item in the Page Selection window
 *   is clicked 
 */
void MainWin::treeItemClicked(QTreeWidgetItem *item, int /*column*/)
{
   /* Is this a page that has been inserted into the hash  */
   if( getFromHash(item) ){
      Pages* page = getFromHash(item);
      int stackindex=stackedWidget->indexOf(page);

      if( stackindex >= 0 ){
         stackedWidget->setCurrentWidget(page);
      }
      /* run the virtual function in case this class overrides it */
      page->PgSeltreeWidgetClicked();
   }
}

/*
 * This subroutine is called with an item in the Page Selection window
 *   is double clicked
 */
void MainWin::treeItemDoubleClicked(QTreeWidgetItem * /*item*/, int /*column*/)
{
}

/*
 * Called with a change of the highlighed tree widget item in the page selector.
 */
void MainWin::treeItemChanged(QTreeWidgetItem *currentitem, QTreeWidgetItem *previousitem)
{
   /* The Previous item */

   if ( previousitem ){
      /* Is this a page that has been inserted into the hash  */
      if( getFromHash(previousitem) ){
         Pages* page = getFromHash(previousitem);
         treeWidget->removeAction(actionToggleDock);
         foreach( QAction* pageaction, page->m_contextActions ){
            treeWidget->removeAction(pageaction);
         } 
      }
   }

   /* Is this a page that has been inserted into the hash  */
   if( getFromHash(currentitem) ){
      Pages* page = getFromHash(currentitem);
      int stackindex = stackedWidget->indexOf(page);
   
      /* Is this page currently on the stack */
      if( stackindex >= 0 ){
         /* put this page on the top of the stack */
         stackedWidget->setCurrentIndex(stackindex);
      } else {
         /* it is undocked, raise it to the front */
         page->raise();
      }
      setContextMenuDockText(page, currentitem);

      treeWidget->addAction(actionToggleDock);

      /* Add the actions to the Page Selectors tree widget that are part of the
       * current items list of desired actions regardless of whether on top of stack*/
      treeWidget->addActions(page->m_contextActions);
   }
}

void MainWin::labelDialogClicked() 
{
   new labelDialog(m_console);
}

void MainWin::runDialogClicked() 
{
   new runDialog(m_console);
}

void MainWin::restoreDialogClicked() 
{
   new prerestoreDialog(m_console);
}



/*
 * The user just finished typing a line in the command line edit box
 */
void MainWin::input_line()
{
   QString cmdStr = lineEdit->text();    /* Get the text */
   lineEdit->clear();                    /* clear the lineEdit box */
   if (m_console->is_connected()) {
      m_console->display_text(cmdStr + "\n");
      m_console->write_dir(cmdStr.toUtf8().data());         /* send to dir */
   } else {
      set_status("Director not connected. Click on connect button.");
   }
   m_cmd_history.append(cmdStr);
   m_cmd_last = -1;
}


void MainWin::about()
{
   QMessageBox::about(this, tr("About bat"),
            tr("<br><h2>bat 0.2, by Kern Sibbald</h2>"
            "<p>Copyright &copy; " BYEAR " Free Software Foundation Europe e.V."
            "<p>The <b>bat</b> is an administrative console"
               " interface to the Director."));
}

void MainWin::set_statusf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   int len;
   va_start(arg_ptr, fmt);
   len = bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   set_status(buf);
}

void MainWin::set_status_ready()
{
   set_status(" Ready");
}

void MainWin::set_status(const char *buf)
{
   statusBar()->showMessage(buf);
}

/*
 * Function to respond to the button bar button to undock
 */
void MainWin::undockWindowButton()
{
   Pages* page = (Pages*)stackedWidget->currentWidget();
   page->togglePageDocking();
   /* The window has been undocked, lets change the context menu */
   setContextMenuDockText();
}

/*
 * Function to respond to action on page selector context menu to toggle the 
 * dock status of the window associated with the page selectors current
 * tree widget item.
 */
void MainWin::toggleDockContextWindow()
{
   QTreeWidgetItem *currentitem = treeWidget->currentItem();
   
   /* Is this a page that has been inserted into the hash  */
   if( getFromHash(currentitem) ){
      Pages* page = getFromHash(currentitem);
      page->togglePageDocking();
      if ( page->isDocked() ){
         stackedWidget->setCurrentWidget(page);
      }
      /* Toggle the menu item.  The window's dock status has been toggled */
      setContextMenuDockText(page, currentitem);
   }
}

/*
 * Function to set the text of the toggle dock context menu when page and
 * widget item are NOT known.  This is an overoaded funciton.
 * It is called from MainWin::undockWindowButton, it is not intended to change
 * for the top pages tree widget, it is for the currently active tree widget
 * item.  Which is why the page is not passed.
 */
void MainWin::setContextMenuDockText()
{
   QTreeWidgetItem *currentitem = treeWidget->currentItem();
   
   /* Is this a page that has been inserted into the hash  */
   if( getFromHash(currentitem) ){
      Pages* page = getFromHash(currentitem);
      setContextMenuDockText(page, currentitem);
   }
}

/*
 * Function to set the text of the toggle dock context menu when page and
 * widget item are known.  This is the more commonly used.
 */
void MainWin::setContextMenuDockText(Pages* page, QTreeWidgetItem* item)
{
   QString docktext("");
   if( page->isDocked() ){
      docktext += "UnDock ";
   } else {
      docktext += "ReDock ";
   }
   docktext += item->text(0) += " Window";
   
   actionToggleDock->setText(docktext);
   setTreeWidgetItemDockColor(page, item);
}

/*
 * Function to set the color of the tree widget item based on whether it is
 * docked or not.
 */
void MainWin::setTreeWidgetItemDockColor(Pages* page, QTreeWidgetItem* item)
{
   if( item->text(0) != "Console" ){
      if( page->isDocked() ){
      /* Set the brush to blue if undocked */
         QBrush blackBrush(Qt::black);
         item->setForeground(0, blackBrush);
      } else {
      /* Set the brush back to black if docked */
         QBrush blueBrush(Qt::blue);
         item->setForeground(0, blueBrush);
      }
   }
}

/*
 *  Overload of previous function, use treeindex to get item from page
 *  This is called when an undocked window is closed.
 */
void MainWin::setTreeWidgetItemDockColor(Pages* page)
{
   QTreeWidgetItem* item = getFromHash(page);
   if( item ){
     setTreeWidgetItemDockColor(page, item);
   }
}

/*
 * This function is called when the stack item is changed.  Call
 * the virtual function here.  Avoids a window being undocked leaving
 * a window at the top of the stack unpopulated.
 */
void MainWin::stackItemChanged(int)
{
   Pages* page = (Pages*)stackedWidget->currentWidget();
   /* run the virtual function in case this class overrides it */
   page->currentStackItem();
}

/*
 * Function to simplify insertion of QTreeWidgetItem <-> Page association
 * into a double direction hash.
 */
void MainWin::hashInsert(QTreeWidgetItem *item, Pages *page)
{
   m_pagehash.insert(item, page);
   m_widgethash.insert(page, item);
}

/*
 * Function to retrieve a Page* when the item in the page selector's tree is
 * known.
 */
Pages* MainWin::getFromHash(QTreeWidgetItem *item)
{
   return m_pagehash.value(item);
}

/*
 * Function to retrieve the page selectors tree widget item when the page is
 * known.
 */
QTreeWidgetItem* MainWin::getFromHash(Pages *page)
{
   return m_widgethash.value(page);
}
