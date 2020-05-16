// Copyright (c) 2019 The CryptoDev developers
// Copyright (c) 2019 The powerbalt developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BETMULTIROW_H
#define BETMULTIROW_H

#include <QWidget>
#include <QPushButton>
#include <QAction>
#include "walletmodel.h"
#include "amount.h"
#include "qt/pwrb/pwidget.h"

class WalletModel;
class SendCoinsRecipient;

namespace Ui {
class BetMultiRow;
class QPushButton;
}

class BetMultiRow : public PWidget
{
    Q_OBJECT

public:
    explicit BetMultiRow(PWidget *parent = nullptr);
    ~BetMultiRow();

    void hideLabels();
    void showLabels();
    void setNumber(int number);
    int getNumber();

    void loadWalletModel() override;
    bool validate();
    SendCoinsRecipient getValue();
    QString getAddress();
    CAmount getAmountValue();

    /** Return whether the entry is still empty and unedited */
    bool isClear();
    void setOnlyStakingAddressAccepted(bool onlyStakingAddress);
    CAmount getAmountValue(QString str);

    void setAddress(const QString& address);
    void setLabel(const QString& label);
    void setAmount(const QString& amount);
    void setAddressAndLabelOrDescription(const QString& address, const QString& message);
    void setFocus();

    QRect getEditLineRect();
    int getEditHeight();
    int getEditWidth();
    int getMenuBtnWidth();

public Q_SLOTS:
    void clear();
    void updateDisplayUnit();

Q_SIGNALS:
    void removeEntry(BetMultiRow* entry);
    void onContactsClicked(BetMultiRow* entry);
    void onMenuClicked(BetMultiRow* entry);
    void onValueChanged();
    void onUriParsed(SendCoinsRecipient rcp);

protected:
    void resizeEvent(QResizeEvent *event) override;
    virtual void enterEvent(QEvent *) override ;
    virtual void leaveEvent(QEvent *) override ;

private Q_SLOTS:
    void amountChanged(const QString&);
    bool addressChanged(const QString&, bool fOnlyValidate = false);
    void deleteClicked();
    //void on_payTo_textChanged(const QString& address);
    //void on_addressBookButton_clicked();

private:
    Ui::BetMultiRow *ui;

    int displayUnit;
    int number = 0;
    bool isExpanded = false;
    bool onlyStakingAddressAccepted = false;

    SendCoinsRecipient recipient;

};

#endif // BETMULTIROW_H
