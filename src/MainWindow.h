#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <deque>
#include "Common.h"
#include "CustomTableStyle.h"
#include "MatchReader.h"

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
    void newMatch(Match *match);
    void doneMatch(QString tabText);

    void on_openButton_clicked();
    void on_saveButton_clicked();
    void on_saveasButton_clicked();
    void saveCurrentMatch();
    void currentMatchUpdated();

    void on_matchTabs_tabCloseRequested(int index);

signals:
    void statsDisplayChanged(statsInfo &info);
    void settingsChanged();

private:
    Ui::MainWindow *ui = nullptr;
    statsInfo info;
    statSelectionDialog *statsSelect = nullptr;
    SettingsDialog *settingsDialog = nullptr;
    MatchReader *reader;
    QTimer *autosaveTimer;
    std::deque<QString> loadedFilenames;
    QSettings settings;
    void saveMatch(int tab, QString name = QString());
};





#endif // MAINWINDOW_H
