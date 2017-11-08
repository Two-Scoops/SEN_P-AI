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
    connect(reader,&StatTableReader::teamsChanged, server,&PipeServer::teamsChanged);

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
    if(currentMatch == nullptr)
        return;
    currentMatch->endMatch();
    disconnect(currentMatch, 0,0,0);
    disconnect(reader, 0, currentMatch, 0);
    disconnect(updateTimer, 0, currentMatch, 0);

    currentMatch = nullptr;
}

void MainWindow::teams_changed(qint64, teamInfo home, teamInfo away){
    if(currentMatch != nullptr){
        ui->matchTabs->removeTab(ui->matchTabs->indexOf(currentMatch));
    }
    if(home.name[0] == '\0' || away.name[0] == '\0'){
        currentMatch = nullptr;
        return;
    }
    currentMatch = new Match(reader,home,away,this);
    connect(this, &MainWindow::statsDisplayChanged, currentMatch, &Match::statsDisplayChanged);
    connect(currentMatch,&Match::table_found, server,&PipeServer::table_found);
    connect(currentMatch,&Match::table_lost, server,&PipeServer::table_lost);
    connect(currentMatch,&Match::newEvent, server,&PipeServer::newEvent);

    int currentTab = ui->matchTabs->currentIndex();
    ui->matchTabs->insertTab(0,currentMatch,QStringLiteral("%1 vs %2").arg(home.name).arg(away.name));
    if(currentTab == 0)
        ui->matchTabs->setCurrentIndex(0);
    emit statsDisplayChanged(info);
}

void MainWindow::table_found(uint8_t *data){
    if(!currentMatch){
        currentMatch = new Match(reader,currHome,currAway,this);
        connect(this, &MainWindow::statsDisplayChanged, currentMatch, &Match::statsDisplayChanged);

        int currentTab = ui->matchTabs->currentIndex();
        ui->matchTabs->insertTab(0,currentMatch,QStringLiteral("%1 vs %2").arg(currHome.name).arg(currAway.name));
        if(currentTab == 0)
            ui->matchTabs->setCurrentIndex(0);
        emit statsDisplayChanged(info);
    }
    currentMatch->stats_found(data);
    currentMatch->showBenched(ui->showBenched->isChecked());
    connect(ui->showBenched,&QCheckBox::toggled, currentMatch, &Match::showBenched);
    connect(reader,&StatTableReader::statTableUpdate, currentMatch, &Match::update);
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
