#include "StatSelectionDialog.h"
#include "ui_StatSelectionDialog.h"
#include <QSettings>
#include <QBitArray>

statSelectionDialog::statSelectionDialog(statsInfo &_info, QWidget *parent) :
    QDialog(parent),
    info(_info),
    ui(new Ui::statSelectionDialog)
{
    ui->setupUi(this);
    QHeaderView *header = ui->statsTable->horizontalHeader();
    header->resizeSection(0,37);
    header->resizeSection(1,28);
    header->setSectionResizeMode(QHeaderView::Fixed);

    QSettings settings;
    QList<QVariant> visualIndicies = settings.value("StatDisplayOrder",QList<QVariant>()).toList();
    QList<QVariant> isShown = settings.value("StatIsShown",QList<QVariant>()).toList();

    if(isShown.size() == info.count())
        for(int i = 0; i < info.count(); ++i)
            info.getStat(i).shown = isShown[i].toBool();
    if(visualIndicies.size() == info.count())
        for(int i = 0; i < info.count(); ++i)
            info.getStat(i).visualIndex = visualIndicies[i].toInt();


    for(int i = 0; i < info.count(); ++i){
        PESstat &stat = info.getStat(i);
        QString name = stat.name;
        if(name.startsWith("?"))
            name.remove(0,1);
        ui->statsTable->insertRow(i);
        ui->statsTable->setVerticalHeaderItem(i,new QTableWidgetItem(name));
        QTableWidgetItem *show = new QTableWidgetItem();
        QTableWidgetItem *log = new QTableWidgetItem();
        show->setCheckState(stat.shown ? Qt::Checked : Qt::Unchecked);
        log->setCheckState(stat.logged ? Qt::Checked : Qt::Unchecked);
        ui->statsTable->setItem(i,0,show);
        ui->statsTable->setItem(i,1,log);
        if(stat.name.startsWith("?") && !ui->showHiddenStats->isChecked())
            ui->statsTable->setRowHidden(i,true);
    }
    header = ui->statsTable->verticalHeader();
    for(int i = 0; i < info.count(); ++i){
        header->moveSection(header->visualIndex(i), info[i].visualIndex);
    }
    header->setSectionsMovable(true);
    header->setSectionResizeMode(QHeaderView::Fixed);
}

statSelectionDialog::~statSelectionDialog()
{
    delete ui;
}

void statSelectionDialog::on_showHiddenStats_toggled(bool checked)
{
    if(checked){
        for(int i = 0; i < info.count(); ++i)
            ui->statsTable->setRowHidden(i,false);
    } else {
        for(int i = 0; i < info.count(); ++i){
            if(info[i].name.startsWith("?"))
                ui->statsTable->setRowHidden(i,true);
        }
    }
}

void statSelectionDialog::on_statSelectionDialog_accepted()
{
    QList<QVariant> visualIndicies;
    QList<QVariant> isShown;
    for(int i = 0; i < info.count(); ++i){
        info[i].visualIndex = ui->statsTable->visualRow(i);
        info[i].shown = ui->statsTable->item(i,0)->checkState() == Qt::Checked;
        info[i].logged = ui->statsTable->item(i,1)->checkState() == Qt::Checked;
        visualIndicies.append(info[i].visualIndex);
        isShown.append(info[i].shown);
    }
    QSettings settings;
    settings.setValue("StatDisplayOrder",visualIndicies);
    settings.setValue("StatIsShown",isShown);

}
