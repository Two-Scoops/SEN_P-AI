#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QScrollBar>
#include <QHeaderView>
#include <algorithm>
#include "QrTimestamp.h"
//#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("SEN:P-AI v" + QCoreApplication::applicationVersion());
    QSettings settings;
    settings.beginGroup("MainWindow");
    resize(settings.value("size", size()).toSize());
    move(settings.value("pos",pos()).toPoint());
    settings.endGroup();

    ui->matchTabs->setStyle(new MyStyle(style(),ui->matchTabs));


    statsSelect = new statSelectionDialog(info,this);
    settingsDialog = new SettingsDialog(this);


    QTimer *timer = new QTimer(this);
    QrTimestamp *qrcode = new QrTimestamp(ui->qrCode,this);
    connect(timer,&QTimer::timeout, qrcode,&QrTimestamp::update);
    connect(timer,&QTimer::timeout, this,&MainWindow::showTime);
    timer->start(33);


    reader = new StatTableReader(L"PES2017.exe",this);
    server = new PipeServer(this);

    updateTimer = new QTimer(this);
    connect(updateTimer,&QTimer::timeout,               reader,&StatTableReader::update);
    connect(reader,&StatTableReader::status_changed,   ui->statusBar,&QStatusBar::showMessage);
    connect(reader,&StatTableReader::table_lost,       this,&MainWindow::table_lost);
    connect(reader,&StatTableReader::table_found,      this,&MainWindow::table_found);
    connect(reader,&StatTableReader::teamsChanged,      this,&MainWindow::teams_changed);
    updateTimer->start(settings.value("UpdateRate",100).toInt());

}

void MainWindow::showTime(){
    ui->timeLabel->setText(QDateTime::currentDateTimeUtc().toString("MMM dd, hh:mm:ss.zzz t"));
}

void MainWindow::table_lost(){
    if(currMatch == nullptr)
        return;
    currMatch->endMatch();
    disconnect(currMatch, 0,0,0);
    disconnect(reader, 0, currMatch, 0);
    disconnect(updateTimer, 0, currMatch, 0);

    currMatch = nullptr;
}

void MainWindow::teams_changed(qint64, teamInfo home, teamInfo away){
    if(currMatch != nullptr){
        ui->matchTabs->removeTab(ui->matchTabs->indexOf(currMatch));
    }
    if(home.name[0] == '\0' || away.name[0] == '\0'){
        currMatch = nullptr;
        return;
    }
    currHome = home; currAway = away;
    currMatch = new Match(reader,home,away,this);
    connect(this, &MainWindow::statsDisplayChanged, currMatch, &Match::statsDisplayChanged);
    connect(currMatch,&Match::table_found, server,&PipeServer::table_found);
    connect(currMatch,&Match::table_lost, server,&PipeServer::table_lost);
    connect(currMatch,&Match::newEvent, server,&PipeServer::newEvent);

    int currentTab = ui->matchTabs->currentIndex();
    ui->matchTabs->insertTab(0,currMatch,QStringLiteral("%1 vs %2").arg(home.name).arg(away.name));
    if(currentTab == 0)
        ui->matchTabs->setCurrentIndex(0);
    server->teamsChanged(currMatch);
    emit statsDisplayChanged(info);
}

void MainWindow::table_found(uint8_t *data){
    if(!currMatch){
        currMatch = new Match(reader,currHome,currAway,this);
        connect(this, &MainWindow::statsDisplayChanged, currMatch, &Match::statsDisplayChanged);

        int currentTab = ui->matchTabs->currentIndex();
        ui->matchTabs->insertTab(0,currMatch,QStringLiteral("%1 vs %2").arg(currHome.name).arg(currAway.name));
        if(currentTab == 0)
            ui->matchTabs->setCurrentIndex(0);
        emit statsDisplayChanged(info);
    }
    currMatch->stats_found(data);
    currMatch->showBenched(ui->showBenched->isChecked());
    connect(ui->showBenched,&QCheckBox::toggled, currMatch, &Match::showBenched);
    connect(reader,&StatTableReader::statTableUpdate, currMatch, &Match::update);
}

MainWindow::~MainWindow()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    delete ui;
}

void MainWindow::on_columnsButton_clicked()
{
    statsSelect->setModal(true);
    statsSelect->exec();
    emit statsDisplayChanged(info);
}


void MainWindow::on_settingsButton_clicked(){
    settingsDialog->setModal(true);
    settingsDialog->exec();
    updateTimer->setInterval(QSettings().value("UpdateRate",100).toInt());
}
