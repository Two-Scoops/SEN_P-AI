#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    QSettings settings;
    ui->logFileEdit->setText(settings.value("EventLogFile",ui->logFileEdit->text()).toString());
    ui->matchSaveEdit->setText(settings.value("MatchAutosaveFile",ui->matchSaveEdit->text()).toString());
    ui->autoSaveCheckbox->setChecked(settings.value("MatchAutosaveEnabled",ui->autoSaveCheckbox->isChecked()).toBool());
    ui->updateRate->setValue(settings.value("UpdateRate",ui->updateRate->value()).toInt());
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
    settings.setValue("MatchAutosaveEnabled",ui->autoSaveCheckbox->isChecked());
    settings.setValue("UpdateRate",ui->updateRate->value());
}
