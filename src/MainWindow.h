#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include "Common.h"
#include "CustomTableStyle.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:

    void showTime();
    void table_lost();
    void table_found(uint8_t *data);
    void teams_changed(qint64, teamInfo home, teamInfo away);

    void on_columnsButton_clicked();
    void on_settingsButton_clicked();

signals:
    void statsDisplayChanged(statsInfo &info);

private:
    Ui::MainWindow *ui = nullptr;
    QDateTime *clock = nullptr;
    statsInfo info;
    StatTableReader *reader = nullptr;
    QTimer *updateTimer = nullptr;
    statSelectionDialog *statsSelect = nullptr;
    SettingsDialog *settingsDialog = nullptr;
    PipeServer *server = nullptr;

    Match *currMatch = nullptr;
    teamInfo currHome, currAway;
};





#endif // MAINWINDOW_H
