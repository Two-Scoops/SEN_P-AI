#ifndef STATSELECTIONDIALOG_H
#define STATSELECTIONDIALOG_H

#include <QDialog>
#include "StatsInfo.h"

namespace Ui {
class statSelectionDialog;
}

class statSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit statSelectionDialog(statsInfo &_info, QWidget *parent = 0);
    ~statSelectionDialog();

private slots:
    void on_showHiddenStats_toggled(bool checked);

    void on_buttonBox_accepted();

private:
    Ui::statSelectionDialog *ui;
    statsInfo &info;
};

#endif // STATSELECTIONDIALOG_H
