// Copyright (c) 2019 The CryptoDev developers
// Copyright (c) 2019 The powerbalt developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pwrb/betmultirow.h"
#include "qt/pwrb/forms/ui_betmultirow.h"

#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "bitcoinunits.h"
#include "qt/pwrb/qtutils.h"

BetMultiRow::BetMultiRow(PWidget *parent) :
    PWidget(parent),
    ui(new Ui::BetMultiRow)
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());

    ui->lineEditAddress->setPlaceholderText(tr("Enter numbers"));
    setCssProperty(ui->lineEditAddress, "edit-primary-multi-book");
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->stackedAddress);

    ui->lineEditAmount->setPlaceholderText("0.00 PWR ");
    initCssEditLine(ui->lineEditAmount);
    GUIUtil::setupAmountWidget(ui->lineEditAmount, this);

    /* Description */
    ui->labelSubtitleDescription->setText("Potential Win");
    setCssProperty(ui->labelSubtitleDescription, "text-title");
    ui->lineEditDescription->setPlaceholderText(tr("Enter numbers and bet amount above"));
    initCssEditLine(ui->lineEditDescription);

    connect(ui->lineEditAmount, &QLineEdit::textChanged, this, &BetMultiRow::amountChanged);
    connect(ui->lineEditAddress, &QLineEdit::textChanged, [this](){addressChanged(ui->lineEditAddress->text());});
}

void BetMultiRow::amountChanged(const QString& amount){
    if(!amount.isEmpty()) {
        QString amountStr = amount;
        CAmount value = getAmountValue(amountStr);
        if (value > 0) {
            QString selections = getAddress();
            QStringList numbers = selections.split(' ',QString::SkipEmptyParts);
            numbers.removeDuplicates();
            int numSelections = numbers.size();
            double odds = 0.0;

            if (numSelections == 1){
                odds = 12;
            } else if (numSelections == 2){
                odds = 200;
            } else if (numSelections == 3){
                odds = 3500;
            } else {
                odds = 0.0;
            }
            CAmount potentialWin = value * odds;

            setCssProperty(ui->lineEditAddress, "edit-primary-multi-book");
            if (potentialWin > 0){
                QString strWin = BitcoinUnits::formatWithUnit(0, potentialWin, false, BitcoinUnits::separatorAlways);
                ui->lineEditDescription->setText(strWin);
            }

            GUIUtil::updateWidgetTextAndCursorPosition(ui->lineEditAmount, amountStr);
            setCssEditLine(ui->lineEditAmount, true, true);
        }
    }
    Q_EMIT onValueChanged();
}

/**
 * Returns -1 if the value is invalid
 */
CAmount BetMultiRow::getAmountValue(QString amount){
    bool isValid = false;
    CAmount value = GUIUtil::parseValue(amount, displayUnit, &isValid);
    return isValid ? value : -1;
}

bool BetMultiRow::addressChanged(const QString& str, bool fOnlyValidate){
    if(!str.isEmpty()) {
        QString trimmedStr = str.trimmed();
        bool validBet = walletModel->validateBet(trimmedStr);

        if (!validBet) {
            // check URI
            SendCoinsRecipient rcp;
            if (GUIUtil::parseBitcoinURI(trimmedStr, &rcp)) {
                ui->lineEditAddress->setText(rcp.address);
                ui->lineEditAmount->setText(BitcoinUnits::format(displayUnit, rcp.amount, false));

                QString label = walletModel->getAddressTableModel()->labelForAddress(rcp.address);
                if (!label.isNull() && !label.isEmpty()){
                    ui->lineEditDescription->setText(label);
                } else if(!rcp.message.isEmpty())
                    ui->lineEditDescription->setText(rcp.message);

                Q_EMIT onUriParsed(rcp);
            } else {
                setCssProperty(ui->lineEditAddress, "edit-primary-multi-book-error");
            }
        } else {
            CAmount bet = getAmountValue(ui->lineEditAmount->text());
            QStringList numbers = trimmedStr.split(' ',QString::SkipEmptyParts);
            numbers.removeDuplicates();
            int numSelections = numbers.size();
            double odds = 0.0;

            if (numSelections == 1){
                odds = 12;
            } else if (numSelections == 2){
                odds = 200;
            } else if (numSelections == 3){
                odds = 3500;
            } else {
                odds = 0.0;
            }
            CAmount potentialWin = bet * odds;

            setCssProperty(ui->lineEditAddress, "edit-primary-multi-book");
            if (potentialWin > 0){
                QString strWin = BitcoinUnits::formatWithUnit(0, potentialWin, false, BitcoinUnits::separatorAlways);
                ui->lineEditDescription->setText(strWin);
            }
        }
        updateStyle(ui->lineEditAddress);
        return validBet;
    }
    return false;
}

void BetMultiRow::loadWalletModel() {
    if (walletModel && walletModel->getOptionsModel()) {
        displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BetMultiRow::updateDisplayUnit);
    }
    clear();
}

void BetMultiRow::updateDisplayUnit(){
    // Update edit text..
    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
}

void BetMultiRow::deleteClicked() {
    Q_EMIT removeEntry(this);
}

void BetMultiRow::clear() {
    ui->lineEditAddress->clear();
    ui->lineEditAmount->clear();
    ui->lineEditDescription->clear();
    setCssProperty(ui->lineEditAddress, "edit-primary-multi-book", true);
}

bool BetMultiRow::validate()
{
    if (!walletModel)
        return false;

    // Check input validity
    bool retval = true;

    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized())
        return retval;

    // Check address validity, returns false if it's invalid
    QString address = ui->lineEditAddress->text();
    if (address.isEmpty()){
        retval = false;
        setCssProperty(ui->lineEditAddress, "edit-primary-multi-book-error", true);
    } else
        retval = addressChanged(address, true);

    CAmount value = getAmountValue(ui->lineEditAmount->text());

    // Betting an amount outside 0.05 - 833 is invalid
    if (value < 5000000 || value > 83300000000) {
        setCssEditLine(ui->lineEditAmount, false, true);
        retval = false;
    }

    // Betting with potential winnings in excess of 10000 invalid
    QString selections = getAddress();
    QStringList numbers = selections.split(' ',QString::SkipEmptyParts);
    numbers.removeDuplicates();
    int numSelections = numbers.size();
    double odds = 0.0;

    if (numSelections == 1){
        odds = 12;
    } else if (numSelections == 2){
        odds = 200;
    } else if (numSelections == 3){
        odds = 3500;
    } else {
        odds = 0.0;
    }
    CAmount potentialWin = value * odds;

    if (potentialWin > 1000000000000){
        retval = false;
    }

    // Reject dust outputs:
    if (retval && GUIUtil::isDust(address, value)) {
        setCssEditLine(ui->lineEditAmount, false, true);
        retval = false;
    }

    return retval;
}

SendCoinsRecipient BetMultiRow::getValue() {
    // Payment request
    if (recipient.paymentRequest.IsInitialized())
        return recipient;

    // Normal payment
    recipient.address = getAddress();
    recipient.scriptPubKey = walletModel->parseBet(ui->lineEditAddress->text().trimmed());
    recipient.label = ui->lineEditDescription->text();
    recipient.amount = getAmountValue();;
    return recipient;
}

QString BetMultiRow::getAddress() {
    return ui->lineEditAddress->text().trimmed();
}

CAmount BetMultiRow::getAmountValue() {
    return getAmountValue(ui->lineEditAmount->text());
}

QRect BetMultiRow::getEditLineRect(){
    return ui->lineEditAddress->rect();
}

int BetMultiRow::getEditHeight(){
    return ui->stackedAddress->height();
}

int BetMultiRow::getEditWidth(){
    return ui->lineEditAddress->width();
}

int BetMultiRow::getNumber(){
    return number;
}

void BetMultiRow::setAddress(const QString& address) {
    ui->lineEditAddress->setText(address);
    ui->lineEditAmount->setFocus();
}

void BetMultiRow::setAmount(const QString& amount){
    ui->lineEditAmount->setText(amount);
}

void BetMultiRow::setAddressAndLabelOrDescription(const QString& address, const QString& message){
    QString label = walletModel->getAddressTableModel()->labelForAddress(address);
    if (!label.isNull() && !label.isEmpty()){
        ui->lineEditDescription->setText(label);
    } else if(!message.isEmpty())
        ui->lineEditDescription->setText(message);
    setAddress(address);
}

void BetMultiRow::setLabel(const QString& label){
    ui->lineEditDescription->setText(label);
}

bool BetMultiRow::isClear(){
    return ui->lineEditAddress->text().isEmpty();
}

void BetMultiRow::setFocus(){
    ui->lineEditAddress->setFocus();
}

void BetMultiRow::setOnlyStakingAddressAccepted(bool onlyStakingAddress) {
    this->onlyStakingAddressAccepted = onlyStakingAddress;
}


void BetMultiRow::setNumber(int _number){
    number = _number;
}

void BetMultiRow::hideLabels(){
    ui->layoutLabel->setVisible(false);
}

void BetMultiRow::showLabels(){
    ui->layoutLabel->setVisible(true);
}

void BetMultiRow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
}

void BetMultiRow::enterEvent(QEvent *) {
    if(!this->isExpanded){
        isExpanded = true;
    }
}

void BetMultiRow::leaveEvent(QEvent *) {
    if(isExpanded){
        isExpanded = false;
    }
}

int BetMultiRow::getMenuBtnWidth(){
    return 1;
}

BetMultiRow::~BetMultiRow(){
    delete ui;
}
