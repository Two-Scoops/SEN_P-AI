#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    on_SettingsDialog_rejected();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::on_SettingsDialog_accepted()
{
    QSettings settings;
    settings.setValue("EventLogFile",ui->logFileEdit->text());
    settings.setValue("MatchAutosaveFile",ui->matchSaveEdit->text());
    settings.setValue("MatchAutosaveOnEnd",ui->autosaveOnEnd->isChecked());
    settings.setValue("MatchAutosaveTimed",ui->autosaveTimed->isChecked());
    settings.setValue("MatchAutosaveInterval",ui->autosaveInterval->value());
    settings.setValue("UpdateRate",ui->updateRate->value());
}

void SettingsDialog::on_SettingsDialog_rejected()
{
    QSettings settings;
    ui->logFileEdit->setText(settings.value("EventLogFile",ui->logFileEdit->text()).toString());
    ui->matchSaveEdit->setText(settings.value("MatchAutosaveFile",ui->matchSaveEdit->text()).toString());
    ui->autosaveOnEnd->setChecked(settings.value("MatchAutosaveOnEnd",ui->autosaveOnEnd->isChecked()).toBool());
    ui->autosaveTimed->setChecked(settings.value("MatchAutosaveTimed",ui->autosaveTimed->isChecked()).toBool());
    ui->autosaveInterval->setValue(settings.value("MatchAutosaveInterval",ui->autosaveInterval->value()).toInt());
    ui->updateRate->setValue(settings.value("UpdateRate",ui->updateRate->value()).toInt());
}

void SettingsDialog::on_autosaveOnEnd_toggled(bool checked) {
    if(checked)
         ui->matchSaveEdit->setEnabled(true);
    else ui->matchSaveEdit->setEnabled(ui->autosaveTimed->isChecked());
}

void SettingsDialog::on_autosaveTimed_toggled(bool checked) {
    if(checked)
         ui->matchSaveEdit->setEnabled(true);
    else ui->matchSaveEdit->setEnabled(ui->autosaveOnEnd->isChecked());
}
