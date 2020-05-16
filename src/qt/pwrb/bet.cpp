// Copyright (c) 2020 The CryptoDev developers
// Copyright (c) 2020 The powerbalt developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pwrb/bet.h"
#include "qt/pwrb/forms/ui_bet.h"
#include "qt/pwrb/addnewcontactdialog.h"
#include "qt/pwrb/qtutils.h"
#include "qt/pwrb/sendchangeaddressdialog.h"
#include "qt/pwrb/optionbutton.h"
#include "qt/pwrb/sendconfirmdialog.h"
#include "qt/pwrb/myaddressrow.h"
#include "qt/pwrb/guitransactionsutils.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "coincontrol.h"
#include "script/standard.h"
#include "zpwrb/deterministicmint.h"
#include "openuridialog.h"
#include "zpwrbcontroldialog.h"

BetWidget::BetWidget(PWRBGUI* parent) :
    PWidget(parent),
    ui(new Ui::bet),
    coinIcon(new QPushButton()),
    btnContacts(new QPushButton())
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    setCssProperty(ui->left, "container");
    ui->left->setContentsMargins(0,20,0,20);
    setCssProperty(ui->right, "container-right");
    ui->right->setContentsMargins(20,10,20,20);

    /* Light Font */
    QFont fontLight;
    fontLight.setWeight(QFont::Light);

    /* Title */
    ui->labelTitle->setText(tr("Bet"));
    setCssProperty(ui->labelTitle, "text-title-screen");
    ui->labelTitle->setFont(fontLight);

    /* Button Group */
    ui->pushLeft->setText("PWRB");
    setCssProperty(ui->pushLeft, "btn-check-left");
    ui->pushLeft->setChecked(true);
    ui->pushRight->setVisible(false);
    setCssProperty(ui->pushRight, "btn-check-right");

    /* Subtitle */
    ui->labelSubtitle1->setVisible(false);
    setCssProperty(ui->labelSubtitle1, "text-subtitle");

    ui->labelSubtitle2->setVisible(false);
    setCssProperty(ui->labelSubtitle2, "text-subtitle");

    /* Address */
    ui->labelSubtitleAddress->setText(tr("Enter up to three numbers (1-69)"));
    setCssProperty(ui->labelSubtitleAddress, "text-title");


    /* Amount */
    ui->labelSubtitleAmount->setText(tr("Amount"));
    setCssProperty(ui->labelSubtitleAmount, "text-title");

    /* Buttons */
    ui->pushButtonFee->setText(tr("Customize fee"));
    setCssBtnSecondary(ui->pushButtonFee);

    ui->pushButtonClear->setText(tr("Clear all"));
    setCssProperty(ui->pushButtonClear, "btn-secundary-clear");

    setCssBtnPrimary(ui->pushButtonSave);
    ui->pushButtonReset->setText(tr("Reset to default"));
    setCssBtnSecondary(ui->pushButtonReset);

    // Coin control
    ui->btnCoinControl->setTitleClassAndText("btn-title-grey", "Coin Control");
    ui->btnCoinControl->setSubTitleClassAndText("text-subtitle", "Select the source of the coins.");

    // Change address option
    ui->btnChangeAddress->setTitleClassAndText("btn-title-grey", "Change Address");
    ui->btnChangeAddress->setSubTitleClassAndText("text-subtitle", "Customize the change address.");

    connect(ui->pushButtonFee, &QPushButton::clicked, this, &BetWidget::onChangeCustomFeeClicked);
    connect(ui->btnCoinControl, &OptionButton::clicked, this, &BetWidget::onCoinControlClicked);
    connect(ui->btnChangeAddress, &OptionButton::clicked, this, &BetWidget::onChangeAddressClicked);
    connect(ui->pushButtonReset, &QPushButton::clicked, [this](){ onResetCustomOptions(true); });
    connect(ui->checkBoxDelegations, &QCheckBox::stateChanged, this, &BetWidget::onCheckBoxChanged);

    setCssProperty(ui->coinWidget, "container-coin-type");
    setCssProperty(ui->labelLine, "container-divider");


    // Total Bet
    ui->labelTitleTotalBet->setText(tr("Total to bet"));
    setCssProperty(ui->labelTitleTotalBet, "text-title");

    ui->labelAmountBet->setText("0.00 PWRB");
    setCssProperty(ui->labelAmountBet, "text-body1");

    // Total Remaining
    setCssProperty(ui->labelTitleTotalRemaining, "text-title");

    setCssProperty(ui->labelAmountRemaining, "text-body1");

    // Icon Send
    ui->stackedWidget->addWidget(coinIcon);
    coinIcon->show();
    coinIcon->raise();

    setCssProperty(coinIcon, "coin-icon-pwrb");

    QSize BUTTON_SIZE = QSize(24, 24);
    coinIcon->setMinimumSize(BUTTON_SIZE);
    coinIcon->setMaximumSize(BUTTON_SIZE);

    int posX = 0;
    int posY = 20;
    coinIcon->move(posX, posY);

    // Entry
    addEntry();

    // Init custom fee false (updated in loadWalletModel)
    setCustomFeeSelected(false);

    // Connect
    connect(ui->pushLeft, &QPushButton::clicked, [this](){onPWRBSelected(true);});
    connect(ui->pushRight,  &QPushButton::clicked, [this](){onPWRBSelected(false);});
    connect(ui->pushButtonSave, &QPushButton::clicked, this, &BetWidget::onSendClicked);
    connect(ui->pushButtonClear, &QPushButton::clicked, [this](){clearAll(true);});
}

void BetWidget::refreshView()
{
    const bool isChecked = ui->pushLeft->isChecked();
    ui->pushButtonSave->setText(isChecked ? tr("Place Bet") : tr("Send zPWRB"));
    refreshAmounts();
}

void BetWidget::refreshAmounts()
{

    CAmount total = 0;
    QMutableListIterator<BetMultiRow*> it(entries);
    while (it.hasNext()) {
        BetMultiRow* entry = it.next();
        CAmount amount = entry->getAmountValue();
        if (amount > 0)
            total += amount;
    }

    bool isZpwrb = ui->pushRight->isChecked();
    nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();

    ui->labelAmountBet->setText(GUIUtil::formatBalance(total, nDisplayUnit, isZpwrb));

    CAmount totalAmount = 0;
    if (CoinControlDialog::coinControl->HasSelected()) {
        // Set remaining balance to the sum of the coinControl selected inputs
        totalAmount = walletModel->getBalance(CoinControlDialog::coinControl) - total;
        ui->labelTitleTotalRemaining->setText(tr("Total remaining from the selected UTXO"));
    } else {
        // Wallet's balance
        totalAmount = (isZpwrb ?
                walletModel->getZerocoinBalance() :
                walletModel->getBalance(nullptr, fDelegationsChecked)) - total;
        ui->labelTitleTotalRemaining->setText(tr("Total remaining"));
    }
    ui->labelAmountRemaining->setText(
            GUIUtil::formatBalance(
                    totalAmount,
                    nDisplayUnit,
                    isZpwrb
                    )
    );
    // show or hide delegations checkbox if need be
    showHideCheckBoxDelegations();
}

void BetWidget::loadClientModel()
{
    if (clientModel) {
        connect(clientModel, &ClientModel::numBlocksChanged, [this](){
            if (customFeeDialog) customFeeDialog->updateFee();
        });
    }
}

void BetWidget::loadWalletModel()
{
    if (walletModel) {
        if (walletModel->getOptionsModel()) {
            // display unit
            nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        }

        // set walletModel for entries
        for (BetMultiRow *entry : entries) {
            if( entry ){
                entry->setWalletModel(walletModel);
            }
        }

        // Restore custom fee from wallet Settings
        CAmount nCustomFee;
        if (walletModel->getWalletCustomFee(nCustomFee)) {
            setCustomFeeSelected(true, nCustomFee);
        }

        // Refresh view
        refreshView();

        // TODO: This only happen when the coin control features are modified in other screen, check before do this if the wallet has another screen modifying it.
        // Coin Control
        //connect(walletModel->getOptionsModel(), &OptionsModel::coinControlFeaturesChanged, [this](){});
        //ui->frameCoinControl->setVisible(model->getOptionsModel()->getCoinControlFeatures());
        //coinControlUpdateLabels();
    }
}

void BetWidget::clearAll(bool fClearSettings)
{
    onResetCustomOptions(false);
    if (fClearSettings) onResetSettings();
    clearEntries();
    refreshAmounts();
}

void BetWidget::onResetSettings()
{
    if (customFeeDialog) customFeeDialog->clear();
    setCustomFeeSelected(false);
    if (walletModel) walletModel->setWalletCustomFee(false, DEFAULT_TRANSACTION_FEE);
}

void BetWidget::onResetCustomOptions(bool fRefreshAmounts){
    CoinControlDialog::coinControl->SetNull();
    ui->btnChangeAddress->setActive(false);
    ui->btnCoinControl->setActive(false);
    if (ui->checkBoxDelegations->isChecked()) ui->checkBoxDelegations->setChecked(false);
    if (fRefreshAmounts) {
        refreshAmounts();
    }
}

void BetWidget::clearEntries(){
    int num = entries.length();
    for (int i = 0; i < num; ++i) {
        ui->scrollAreaWidgetContents->layout()->takeAt(0)->widget()->deleteLater();
    }
    entries.clear();

    addEntry();
}

void BetWidget::addEntry(){
    if(entries.isEmpty()){
        createEntry();
    } else {
        if (entries.length() == 1) {
            BetMultiRow *entry = entries.at(0);
            entry->hideLabels();
            entry->setNumber(1);
        }else if(entries.length() == MAX_BET_POPUP_ENTRIES){
            inform(tr("Maximum amount of outputs reached"));
            return;
        }

        BetMultiRow *betMultiRow = createEntry();
        betMultiRow->setNumber(entries.length());
        betMultiRow->hideLabels();
    }
    setFocusOnLastEntry();
}

BetMultiRow* BetWidget::createEntry(){
    BetMultiRow *betMultiRow = new BetMultiRow(this);
    if(this->walletModel) betMultiRow->setWalletModel(this->walletModel);
    entries.append(betMultiRow);
    ui->scrollAreaWidgetContents->layout()->addWidget(betMultiRow);
    connect(betMultiRow, &BetMultiRow::onContactsClicked, this, &BetWidget::onContactsClicked);
    connect(betMultiRow, &BetMultiRow::onMenuClicked, this, &BetWidget::onMenuClicked);
    connect(betMultiRow, &BetMultiRow::onValueChanged, this, &BetWidget::onValueChanged);
    return betMultiRow;
}

void BetWidget::onAddEntryClicked(){
    // Check prev valid entries before add a new one.
    for (BetMultiRow* entry : entries){
        if(!entry || !entry->validate()) {
            inform(tr("Invalid entry, previous entries must be valid before add a new one"));
            return;
        }
    }
    addEntry();
}

void BetWidget::resizeEvent(QResizeEvent *event){
    resizeMenu();
    QWidget::resizeEvent(event);
}

void BetWidget::showEvent(QShowEvent *event)
{
    // Set focus on last recipient address when Send-window is displayed
    setFocusOnLastEntry();

    // Update cached delegated balance
    CAmount cachedDelegatedBalance_new = walletModel->getDelegatedBalance();
    if (cachedDelegatedBalance != cachedDelegatedBalance_new) {
        cachedDelegatedBalance = cachedDelegatedBalance_new;
        refreshAmounts();
    }
}

void BetWidget::setFocusOnLastEntry()
{
    if (!entries.isEmpty()) entries.last()->setFocus();
}

void BetWidget::showHideCheckBoxDelegations()
{
    // Show checkbox only when there is any available owned delegation,
    // coincontrol is not selected, and we are trying to bet PWRB (not zPWRB)
    const bool isZpwrb = ui->pushRight->isChecked();
    const bool isCControl = CoinControlDialog::coinControl->HasSelected();
    const bool hasDel = cachedDelegatedBalance > 0;

    const bool showCheckBox = !isZpwrb && !isCControl && hasDel;
    ui->checkBoxDelegations->setVisible(showCheckBox);
    if (showCheckBox)
        ui->checkBoxDelegations->setToolTip(
                tr("Possibly bet coins delegated for cold-staking (currently available: %1").arg(
                        GUIUtil::formatBalance(cachedDelegatedBalance, nDisplayUnit, isZpwrb))
        );
}

void BetWidget::onSendClicked(){

    if (!walletModel || !walletModel->getOptionsModel())
        return;

    QList<SendCoinsRecipient> recipients;

    for (BetMultiRow* entry : entries){
        // TODO: Check UTXO splitter here..
        // Validate send..
        if(entry && entry->validate()) {
            recipients.append(entry->getValue());
        }else{
            inform(tr("Invalid entry - Check numbers and that potential win doesn't exceed 10,000 PWRB"));
            return;
        }
    }

    if (recipients.isEmpty()) {
        inform(tr("No set recipients"));
        return;
    }

    bool sendPwrb = ui->pushLeft->isChecked();

    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        // Unlock wallet was cancelled
        inform(tr("Cannot bet, wallet locked"));
        return;
    }

    if((sendPwrb) ? send(recipients) : sendZpwrb(recipients)) {
        // Just return on success
        return;
    }
}

bool BetWidget::send(QList<SendCoinsRecipient> recipients){
    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    prepareStatus = walletModel->prepareTransaction(currentTransaction, CoinControlDialog::coinControl, fDelegationsChecked);

    // process prepareStatus and on error generate message shown to user
    GuiTransactionsUtils::ProcessSendCoinsReturnAndInform(
            this,
            prepareStatus,
            walletModel,
            BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                         currentTransaction.getTransactionFee()),
            true
    );

    if (prepareStatus.status != WalletModel::OK) {
        inform(tr("Cannot create transaction."));
        return false;
    }

    showHideOp(true);
    const bool fStakeDelegationVoided = currentTransaction.getTransaction()->fStakeDelegationVoided;
    QString warningStr = QString();
    if (fStakeDelegationVoided)
        warningStr = tr("WARNING:\nThis bet spends a cold-stake delegation, voiding it.\n"
                     "These coins will no longer be cold-staked.");
    TxDetailDialog* dialog = new TxDetailDialog(window, true, warningStr);
    dialog->setDisplayUnit(walletModel->getOptionsModel()->getDisplayUnit());
    dialog->setData(walletModel, currentTransaction);
    dialog->adjustSize();
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);

    if(dialog->isConfirm()){
        // now send the prepared transaction
        WalletModel::SendCoinsReturn sendStatus = dialog->getStatus();
        // process sendStatus and on error generate message shown to user
        GuiTransactionsUtils::ProcessSendCoinsReturnAndInform(
                this,
                sendStatus,
                walletModel
        );
        if (sendStatus.status == WalletModel::OK) {
            // if delegations were spent, update cachedBalance
            if (fStakeDelegationVoided)
                cachedDelegatedBalance = walletModel->getDelegatedBalance();
            clearAll(false);
            inform(tr("Bet Placed"));
            dialog->deleteLater();
            return true;
        }
    }

    dialog->deleteLater();
    return false;
}

bool BetWidget::sendZpwrb(QList<SendCoinsRecipient> recipients){
    if (!walletModel || !walletModel->getOptionsModel())
        return false;

    if(sporkManager.IsSporkActive(SPORK_16_ZEROCOIN_MAINTENANCE_MODE)) {
        Q_EMIT message(tr("Spend Zerocoin"), tr("zPWRB is currently undergoing maintenance."), CClientUIInterface::MSG_ERROR);
        return false;
    }

    std::list<std::pair<CBitcoinAddress*, CAmount>> outputs;
    CAmount total = 0;
    for (SendCoinsRecipient rec : recipients){
        total += rec.amount;
        outputs.push_back(std::pair<CBitcoinAddress*, CAmount>(new CBitcoinAddress(rec.address.toStdString()),rec.amount));
    }

    // use mints from zPWRB selector if applicable
    std::vector<CMintMeta> vMintsToFetch;
    std::vector<CZerocoinMint> vMintsSelected;
    if (!ZPwrbControlDialog::setSelectedMints.empty()) {
        vMintsToFetch = ZPwrbControlDialog::GetSelectedMints();

        for (auto& meta : vMintsToFetch) {
            CZerocoinMint mint;
            if (!walletModel->getMint(meta.hashSerial, mint)){
                inform(tr("Coin control mint not found"));
                return false;
            }
            vMintsSelected.emplace_back(mint);
        }
    }

    QString sendBody = outputs.size() == 1 ?
            tr("Sending %1 to address %2\n")
            .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), total, false, BitcoinUnits::separatorAlways))
            .arg(recipients.first().address)
            :
           tr("Sending %1 to addresses:\n%2")
           .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), total, false, BitcoinUnits::separatorAlways))
           .arg(recipientsToString(recipients));

    bool ret = false;
    Q_EMIT message(
            tr("Spend Zerocoin"),
            sendBody,
            CClientUIInterface::MSG_INFORMATION | CClientUIInterface::BTN_MASK | CClientUIInterface::MODAL,
            &ret);

    if(!ret) return false;

    CZerocoinSpendReceipt receipt;

    std::string changeAddress = "";
    if(!boost::get<CNoDestination>(&CoinControlDialog::coinControl->destChange)){
        changeAddress = CBitcoinAddress(CoinControlDialog::coinControl->destChange).ToString();
    }else{
        changeAddress = walletModel->getAddressTableModel()->getAddressToShow().toStdString();
    }

    if (walletModel->sendZpwrb(
            vMintsSelected,
            receipt,
            outputs,
            changeAddress
    )
            ) {
        inform(tr("zPWRB transaction sent!"));
        ZPwrbControlDialog::setSelectedMints.clear();
        clearAll(false);
        return true;
    } else {
        QString body;
        if (receipt.GetStatus() == ZPWRB_SPEND_V1_SEC_LEVEL) {
            body = tr("Version 1 zPWRB require a security level of 100 to successfully spend.");
        } else {
            int nNeededSpends = receipt.GetNeededSpends(); // Number of spends we would need for this transaction
            const int nMaxSpends = Params().GetConsensus().ZC_MaxSpendsPerTx; // Maximum possible spends for one zPWRB transaction
            if (nNeededSpends > nMaxSpends) {
                body = tr("Too much inputs (") + QString::number(nNeededSpends, 10) +
                       tr(") needed.\nMaximum allowed: ") + QString::number(nMaxSpends, 10);
                body += tr(
                        "\nEither mint higher denominations (so fewer inputs are needed) or reduce the amount to spend.");
            } else {
                body = QString::fromStdString(receipt.GetStatusMessage());
            }
        }
        Q_EMIT message("zPWRB transaction failed", body, CClientUIInterface::MSG_ERROR);
        return false;
    }
}

QString BetWidget::recipientsToString(QList<SendCoinsRecipient> recipients){
    QString s = "";
    for (SendCoinsRecipient rec : recipients){
        s += rec.address + " -> " + BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), rec.amount, false, BitcoinUnits::separatorAlways) + "\n";
    }
    return s;
}

void BetWidget::updateEntryLabels(QList<SendCoinsRecipient> recipients){
    for (SendCoinsRecipient rec : recipients){
        QString label = rec.label;
        if(!label.isNull()) {
            QString labelOld = walletModel->getAddressTableModel()->labelForAddress(rec.address);
            if(label.compare(labelOld) != 0) {
                CTxDestination dest = CBitcoinAddress(rec.address.toStdString()).Get();
                if (!walletModel->updateAddressBookLabels(dest, label.toStdString(),
                                                          this->walletModel->isMine(dest) ?
                                                                  AddressBook::AddressBookPurpose::RECEIVE :
                                                                  AddressBook::AddressBookPurpose::SEND)) {
                    // Label update failed
                    Q_EMIT message("", tr("Address label update failed for address: %1").arg(rec.address), CClientUIInterface::MSG_ERROR);
                    return;
                }
            }
        }

    }
}


void BetWidget::onChangeAddressClicked(){
    showHideOp(true);
    SendChangeAddressDialog* dialog = new SendChangeAddressDialog(window, walletModel);
    if (!boost::get<CNoDestination>(&CoinControlDialog::coinControl->destChange)) {
        dialog->setAddress(QString::fromStdString(CBitcoinAddress(CoinControlDialog::coinControl->destChange).ToString()));
    }
    if (openDialogWithOpaqueBackgroundY(dialog, window, 3, 5)) {
        CBitcoinAddress address(dialog->getAddress().toStdString());

        // Ask if it's what the user really wants
        if (!walletModel->isMine(address) &&
            !ask(tr("Warning!"), tr("The change address doesn't belong to this wallet.\n\nDo you want to continue?"))) {
            return;
        }
        CoinControlDialog::coinControl->destChange = address.Get();
        ui->btnChangeAddress->setActive(true);
    }
    // check if changeAddress has been reset to NoDestination (or wasn't set at all)
    if (boost::get<CNoDestination>(&CoinControlDialog::coinControl->destChange))
        ui->btnChangeAddress->setActive(false);
    dialog->deleteLater();
}

void BetWidget::onOpenUriClicked(){
    showHideOp(true);
    OpenURIDialog *dlg = new OpenURIDialog(window);
    if (openDialogWithOpaqueBackgroundY(dlg, window, 3, 5)) {

        SendCoinsRecipient rcp;
        if (!GUIUtil::parseBitcoinURI(dlg->getURI(), &rcp)) {
            inform(tr("Invalid URI"));
            return;
        }
        if (!walletModel->validateAddress(rcp.address)) {
            inform(tr("Invalid address in URI"));
            return;
        }

        int listSize = entries.size();
        if (listSize == 1) {
            BetMultiRow *entry = entries[0];
            entry->setAddressAndLabelOrDescription(rcp.address, rcp.message);
            entry->setAmount(BitcoinUnits::format(nDisplayUnit, rcp.amount, false));
        } else {
            // Use the last one if it's invalid or add a new one
            BetMultiRow *entry = entries[listSize - 1];
            if (!entry->validate()) {
                addEntry();
                entry = entries[listSize];
            }
            entry->setAddressAndLabelOrDescription(rcp.address, rcp.message);
            entry->setAmount(BitcoinUnits::format(nDisplayUnit, rcp.amount, false));
        }
        Q_EMIT receivedURI(dlg->getURI());
    }
    dlg->deleteLater();
}

void BetWidget::onChangeCustomFeeClicked(){
    showHideOp(true);
    if (!customFeeDialog) {
        customFeeDialog = new SendCustomFeeDialog(window, walletModel);
    }
    if (openDialogWithOpaqueBackgroundY(customFeeDialog, window, 3, 5)){
        const CAmount& nFeePerKb = customFeeDialog->getFeeRate().GetFeePerK();
        setCustomFeeSelected(customFeeDialog->isCustomFeeChecked(), nFeePerKb);
    }
}

void BetWidget::onCoinControlClicked(){
    if(isPWRB){
        if (walletModel->getBalance() > 0) {
            if (!coinControlDialog) {
                coinControlDialog = new CoinControlDialog();
                coinControlDialog->setModel(walletModel);
            } else {
                coinControlDialog->refreshDialog();
            }
            coinControlDialog->exec();
            ui->btnCoinControl->setActive(CoinControlDialog::coinControl->HasSelected());
            refreshAmounts();
        } else {
            inform(tr("You don't have any PWRB to select."));
        }
    }else{
        if (walletModel->getZerocoinBalance() > 0) {
            ZPwrbControlDialog *zPwrbControl = new ZPwrbControlDialog(this);
            zPwrbControl->setModel(walletModel);
            zPwrbControl->exec();
            ui->btnCoinControl->setActive(!ZPwrbControlDialog::setSelectedMints.empty());
            zPwrbControl->deleteLater();
        } else {
            inform(tr("You don't have any zPWRB in your balance to select."));
        }
    }
}

void BetWidget::onValueChanged() {
    refreshAmounts();
}

void BetWidget::onCheckBoxChanged()
{
    const bool checked = ui->checkBoxDelegations->isChecked();
    if (checked != fDelegationsChecked) {
        fDelegationsChecked = checked;
        refreshAmounts();
    }
}

void BetWidget::onPWRBSelected(bool _isPWRB){
    isPWRB = _isPWRB;
    setCssProperty(coinIcon, _isPWRB ? "coin-icon-pwrb" : "coin-icon-zpwrb");
    refreshView();
    updateStyle(coinIcon);
}

void BetWidget::onContactsClicked(BetMultiRow* entry){
    focusedEntry = entry;
    if(menu && menu->isVisible()){
        menu->hide();
    }

    int contactsSize = walletModel->getAddressTableModel()->sizeSend();
    if(contactsSize == 0) {
        inform(tr("No contacts available, you can go to the contacts screen and add some there!"));
        return;
    }

    int height = (contactsSize <= 2) ? entry->getEditHeight() * ( 2 * (contactsSize + 1 )) : entry->getEditHeight() * 6;
    int width = entry->getEditWidth();

    if(!menuContacts){
        menuContacts = new ContactsDropdown(
                    width,
                    height,
                    this
        );
        menuContacts->setWalletModel(walletModel, AddressTableModel::Send);
        connect(menuContacts, &ContactsDropdown::contactSelected, [this](QString address, QString label){
            if(focusedEntry){
                if (label != "(no label)")
                    focusedEntry->setLabel(label);
                focusedEntry->setAddress(address);
            }
        });

    }

    if(menuContacts->isVisible()){
        menuContacts->hide();
        return;
    }

    menuContacts->resizeList(width, height);
    menuContacts->setStyleSheet(this->styleSheet());
    menuContacts->adjustSize();

    QPoint pos;
    if (entries.size() > 1){
        pos = entry->pos();
        pos.setY((pos.y() + (focusedEntry->getEditHeight() - 12) * 4));
    } else {
        pos = focusedEntry->getEditLineRect().bottomLeft();
        pos.setY((pos.y() + (focusedEntry->getEditHeight() - 12) * 3));
    }
    pos.setX(pos.x() + 20);
    menuContacts->move(pos);
    menuContacts->show();
}

void BetWidget::onMenuClicked(BetMultiRow* entry){
    focusedEntry = entry;
    if(menuContacts && menuContacts->isVisible()){
        menuContacts->hide();
    }
    QPoint pos = entry->pos();
    pos.setX(pos.x() + (entry->width() - entry->getMenuBtnWidth()));
    pos.setY(pos.y() + entry->height() + (entry->getMenuBtnWidth()));

    if(!this->menu){
        this->menu = new TooltipMenu(window, this);
        this->menu->setCopyBtnVisible(false);
        this->menu->setEditBtnText(tr("Save contact"));
        this->menu->setMinimumSize(this->menu->width() + 30,this->menu->height());
        connect(this->menu, &TooltipMenu::message, this, &AddressesWidget::message);
        connect(this->menu, &TooltipMenu::onEditClicked, this, &BetWidget::onContactMultiClicked);
        connect(this->menu, &TooltipMenu::onDeleteClicked, this, &BetWidget::onDeleteClicked);
    }else {
        this->menu->hide();
    }
    menu->move(pos);
    menu->show();
}

void BetWidget::onContactMultiClicked(){
    if(focusedEntry) {
        QString address = focusedEntry->getAddress();
        if (address.isEmpty()) {
            inform(tr("Address field is empty"));
            return;
        }
        if (!walletModel->validateAddress(address)) {
            inform(tr("Invalid address"));
            return;
        }
        CBitcoinAddress pwrbAdd = CBitcoinAddress(address.toStdString());
        if (walletModel->isMine(pwrbAdd)) {
            inform(tr("Cannot store your own address as contact"));
            return;
        }

        showHideOp(true);
        AddNewContactDialog *dialog = new AddNewContactDialog(window);
        QString label = walletModel->getAddressTableModel()->labelForAddress(address);
        if (!label.isNull()){
            dialog->setTexts(tr("Update Contact"), "Edit label for the selected address:\n%1");
            dialog->setData(address, label);
        } else {
            dialog->setTexts(tr("Create New Contact"), "Save label for the selected address:\n%1");
            dialog->setData(address, "");
        }
        openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);
        if (dialog->res) {
            if (label == dialog->getLabel()) {
                return;
            }
            if (walletModel->updateAddressBookLabels(pwrbAdd.Get(), dialog->getLabel().toStdString(),
                    AddressBook::AddressBookPurpose::SEND)) {
                inform(tr("New Contact Stored"));
            } else {
                inform(tr("Error Storing Contact"));
            }
        }
        dialog->deleteLater();
    }

}

void BetWidget::onDeleteClicked(){
    if (focusedEntry) {
        focusedEntry->hide();
        focusedEntry->deleteLater();
        int entryNumber = focusedEntry->getNumber();

        // remove selected entry and update row number for the others
        QMutableListIterator<BetMultiRow*> it(entries);
        while (it.hasNext()) {
            BetMultiRow* entry = it.next();
            if (focusedEntry == entry){
                it.remove();
            } else if (focusedEntry && entry->getNumber() > entryNumber){
                entry->setNumber(entry->getNumber() - 1);
            }
        }

        if (entries.size() == 1) {
            BetMultiRow* betMultiRow = QMutableListIterator<BetMultiRow*>(entries).next();
            betMultiRow->setNumber(entries.length());
            betMultiRow->showLabels();
        }

        focusedEntry = nullptr;

        // Update total amounts
        refreshAmounts();
        setFocusOnLastEntry();
    }
}

void BetWidget::resizeMenu(){
    if(menuContacts && menuContacts->isVisible() && focusedEntry){
        int width = focusedEntry->getEditWidth();
        menuContacts->resizeList(width, menuContacts->height());
        menuContacts->resize(width, menuContacts->height());
        QPoint pos = focusedEntry->getEditLineRect().bottomLeft();
        pos.setX(pos.x() + 20);
        pos.setY(pos.y() + ((focusedEntry->getEditHeight() - 12)  * 3));
        menuContacts->move(pos);
    }
}

void BetWidget::setCustomFeeSelected(bool isSelected, const CAmount& customFee)
{
    isCustomFeeSelected = isSelected;
    ui->pushButtonFee->setText(isCustomFeeSelected ?
                    tr("Custom Fee %1").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, customFee) + "/kB") :
                    tr("Customize Fee"));
    if (walletModel)
        walletModel->setWalletDefaultFee(customFee);
}

void BetWidget::changeTheme(bool isLightTheme, QString& theme){
    if (coinControlDialog) coinControlDialog->setStyleSheet(theme);
}

BetWidget::~BetWidget(){
    delete ui;
}
