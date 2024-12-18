/* Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AccountListPage.h"
#include "ui_AccountListPage.h"

#include <QItemSelectionModel>
#include <QMenu>

#include <QDebug>

#include "net/NetJob.h"

#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/BsLoginDialog.h"
#include "ui/dialogs/MSALoginDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/SkinUploadDialog.h"

#include "tasks/Task.h"
#include "minecraft/auth/AccountTask.h"
#include "minecraft/services/SkinDelete.h"

#include "Application.h"

#include "BuildConfig.h"

AccountListPage::AccountListPage(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::AccountListPage)
{
    ui->setupUi(this);
    ui->listView->setEmptyString(tr(
        "Welcome!\n"
        "If you're new here, you can click the \"Add\" button to add your yggdrasil or Minecraft account."
    ));
    ui->listView->setEmptyMode(VersionListView::String);
    ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);

    m_accounts = APPLICATION->accounts();

    ui->listView->setModel(m_accounts.get());
    ui->listView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->listView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->listView->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->actionAddMojang->setVisible(false);

    // Expand the account column

    QItemSelectionModel *selectionModel = ui->listView->selectionModel();

    connect(selectionModel, &QItemSelectionModel::selectionChanged, [this](const QItemSelection &sel, const QItemSelection &dsel) {
        updateButtonStates();
    });
    connect(ui->listView, &VersionListView::customContextMenuRequested, this, &AccountListPage::ShowContextMenu);

    connect(m_accounts.get(), &AccountList::listChanged, this, &AccountListPage::listChanged);
    connect(m_accounts.get(), &AccountList::listActivityChanged, this, &AccountListPage::listChanged);
    connect(m_accounts.get(), &AccountList::defaultAccountChanged, this, &AccountListPage::listChanged);

    updateButtonStates();

    // Xbox authentication won't work without a client identifier, so disable the button if it is missing
    ui->actionAddMicrosoft->setVisible(!BuildConfig.MSA_CLIENT_ID.isEmpty());
}

AccountListPage::~AccountListPage()
{
    delete ui;
}

void AccountListPage::ShowContextMenu(const QPoint& pos)
{
    auto menu = ui->toolBar->createContextMenu(this, tr("Context menu"));
    menu->exec(ui->listView->mapToGlobal(pos));
    delete menu;
}

void AccountListPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
    QMainWindow::changeEvent(event);
}

QMenu * AccountListPage::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->toolBar->toggleViewAction() );
    return filteredMenu;
}


void AccountListPage::listChanged()
{
    updateButtonStates();
}

void AccountListPage::on_actionAddMicrosoft_triggered()
{
    MinecraftAccountPtr account = MSALoginDialog::newAccount(this);
    if (account)
    {
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1) {
            m_accounts->setDefaultAccount(account);
        }
    }
}

void AccountListPage::on_actionRemove_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0)
    {
        QModelIndex selected = selection.first();
        m_accounts->removeAccount(selected);
    }
}

void AccountListPage::on_actionRefresh_triggered() {
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        m_accounts->requestRefresh(account->internalId());
    }
}


void AccountListPage::on_actionSetDefault_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0)
    {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        m_accounts->setDefaultAccount(account);
    }
}

void AccountListPage::on_actionNoDefault_triggered()
{
    m_accounts->setDefaultAccount(nullptr);
}

void AccountListPage::updateButtonStates()
{
    // If there is no selection, disable buttons that require something selected.
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    bool hasSelection = selection.size() > 0;
    bool accountIsReady = false;
    bool UploadSkin = false;
    if (hasSelection)
    {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        accountIsReady = !account->isActive();
        UploadSkin = accountIsReady;
        if(account->typeString() == "bs"){
            UploadSkin = false;
        }
    }
    ui->actionRemove->setEnabled(accountIsReady);
    ui->actionSetDefault->setEnabled(accountIsReady);
    ui->actionUploadSkin->setEnabled(UploadSkin);
    ui->actionDeleteSkin->setEnabled(UploadSkin);
    ui->actionRefresh->setEnabled(accountIsReady);

    if(m_accounts->defaultAccount().get() == nullptr) {
        ui->actionNoDefault->setEnabled(false);
        ui->actionNoDefault->setChecked(true);
    }
    else {
        ui->actionNoDefault->setEnabled(true);
        ui->actionNoDefault->setChecked(false);
    }
}

void AccountListPage::on_actionUploadSkin_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0)
    {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        SkinUploadDialog dialog(account, this);
        dialog.exec();
    }
}

void AccountListPage::on_actionDeleteSkin_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() <= 0)
        return;

    QModelIndex selected = selection.first();
    MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
    ProgressDialog prog(this);
    auto deleteSkinTask = std::make_shared<SkinDelete>(this, account->accessToken());
    if (prog.execWithTask((Task*)deleteSkinTask.get()) != QDialog::Accepted) {
        CustomMessageBox::selectable(this, tr("Skin Delete"), tr("Failed to delete current skin!"), QMessageBox::Warning)->exec();
        return;
    }
}

void AccountListPage::on_actionAddBs_triggered()
{
    MinecraftAccountPtr account = BsLoginDialog::newAccount(
                this,
                tr("Please enter your skin site account email and password to add your account."));

    if (account)
    {
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1)
        {
            m_accounts->setDefaultAccount(account);
        }
    }
}

