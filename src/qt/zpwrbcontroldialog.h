// Copyright (c) 2017-2020 The PIVX developers
// Copyright (c) 2020 The PWRDev developers
// Copyright (c) 2020 The powerbalt developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZPWRBCONTROLDIALOG_H
#define ZPWRBCONTROLDIALOG_H

#include <QDialog>
#include <QTreeWidgetItem>
#include "zpwrb/zerocoin.h"

class CZerocoinMint;
class WalletModel;

namespace Ui {
class ZPwrbControlDialog;
}

class CZPwrbControlWidgetItem : public QTreeWidgetItem
{
public:
    explicit CZPwrbControlWidgetItem(QTreeWidget *parent, int type = Type) : QTreeWidgetItem(parent, type) {}
    explicit CZPwrbControlWidgetItem(int type = Type) : QTreeWidgetItem(type) {}
    explicit CZPwrbControlWidgetItem(QTreeWidgetItem *parent, int type = Type) : QTreeWidgetItem(parent, type) {}

    bool operator<(const QTreeWidgetItem &other) const;
};

class ZPwrbControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ZPwrbControlDialog(QWidget *parent);
    ~ZPwrbControlDialog();

    void setModel(WalletModel* model);

    static std::set<std::string> setSelectedMints;
    static std::set<CMintMeta> setMints;
    static std::vector<CMintMeta> GetSelectedMints();

private:
    Ui::ZPwrbControlDialog *ui;
    WalletModel* model;

    void updateList();
    void updateLabels();

    enum {
        COLUMN_CHECKBOX,
        COLUMN_DENOMINATION,
        COLUMN_PUBCOIN,
        COLUMN_VERSION,
        COLUMN_CONFIRMATIONS,
        COLUMN_ISSPENDABLE
    };
    friend class CZPwrbControlWidgetItem;

private Q_SLOTS:
    void updateSelection(QTreeWidgetItem* item, int column);
    void ButtonAllClicked();
};

#endif // ZPWRBCONTROLDIALOG_H
