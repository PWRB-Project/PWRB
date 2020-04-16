// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2019 The CryptoDev developers
// Copyright (c) 2019 The powerbalt developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BET_H
#define BET_H

#include <QWidget>
#include <QPushButton>

#include "qt/pwrb/pwidget.h"
#include "qt/pwrb/contactsdropdown.h"
#include "qt/pwrb/betmultirow.h"
#include "qt/pwrb/sendcustomfeedialog.h"
#include "walletmodel.h"
#include "coincontroldialog.h"
#include "zpwrbcontroldialog.h"
#include "qt/pwrb/tooltipmenu.h"

static const int MAX_BET_POPUP_ENTRIES = 8;

class PWRBGUI;
class ClientModel;
class WalletModel;
class WalletModelTransaction;

namespace Ui {
class bet;
class QPushButton;
}

class BetWidget : public PWidget
{
    Q_OBJECT

public:
    explicit BetWidget(PWRBGUI* parent);
    ~BetWidget();

    void addEntry();

    void loadClientModel() override;
    void loadWalletModel() override;

Q_SIGNALS:
    /** Signal raised when a URI was entered or dragged to the GUI */
    void receivedURI(const QString& uri);

public Q_SLOTS:
    void onChangeAddressClicked();
    void onChangeCustomFeeClicked();
    void onCoinControlClicked();
    void onOpenUriClicked();
    void onValueChanged();
    void refreshAmounts();
    void changeTheme(bool isLightTheme, QString &theme) override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private Q_SLOTS:
    void onPWRBSelected(bool _isPWRB);
    void onSendClicked();
    void onContactsClicked(BetMultiRow* entry);
    void onMenuClicked(BetMultiRow* entry);
    void onAddEntryClicked();
    void clearEntries();
    void clearAll(bool fClearSettings = true);
    void refreshView();
    void onCheckBoxChanged();
    void onContactMultiClicked();
    void onDeleteClicked();
    void onResetCustomOptions(bool fRefreshAmounts);
    void onResetSettings();

private:
    Ui::bet *ui;
    QPushButton *coinIcon;
    QPushButton *btnContacts;

    SendCustomFeeDialog* customFeeDialog = nullptr;
    bool isCustomFeeSelected = false;
    bool fDelegationsChecked = false;
    CAmount cachedDelegatedBalance{0};

    int nDisplayUnit;
    QList<BetMultiRow*> entries;
    CoinControlDialog *coinControlDialog = nullptr;

    ContactsDropdown *menuContacts = nullptr;
    TooltipMenu *menu = nullptr;
    // Current focus entry
    BetMultiRow* focusedEntry = nullptr;

    bool isPWRB = true;
    void resizeMenu();
    QString recipientsToString(QList<SendCoinsRecipient> recipients);
    BetMultiRow* createEntry();
    bool send(QList<SendCoinsRecipient> recipients);
    bool sendZpwrb(QList<SendCoinsRecipient> recipients);
    void setFocusOnLastEntry();
    void showHideCheckBoxDelegations();
    void updateEntryLabels(QList<SendCoinsRecipient> recipients);
    void setCustomFeeSelected(bool isSelected, const CAmount& customFee = DEFAULT_TRANSACTION_FEE);

};

#endif // BET_H
