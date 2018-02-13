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
    settings.beginGroup("MainWindow");
    resize(settings.value("size", size()).toSize());
    move(settings.value("pos",pos()).toPoint());
    settings.endGroup();

    ui->matchTabs->setStyle(new MyStyle(style(),ui->matchTabs));

    statsSelect = new statSelectionDialog(info,this);
    settingsDialog = new SettingsDialog(this);

    reader = ui->reader;
    connect(this,&MainWindow::settingsChanged, reader,&MatchReader::settingsChanged);
    connect(reader,&MatchReader::new_match, this,&MainWindow::newMatch);
    connect(reader,&MatchReader::done_match, this,&MainWindow::doneMatch);
    connect(reader,&MatchReader::invalidate_match, this,&MainWindow::invalidateMatch);
    connect(reader,&MatchReader::matchUpdated, this,&MainWindow::currentMatchUpdated);
    connect(ui->showBenched,&QPushButton::toggled, reader,&MatchReader::show_benced);

    autosaveTimer = new QTimer(this);
    autosaveTimer->setInterval(settings.value("MatchAutosaveInterval",10000).toInt());
    connect(autosaveTimer,&QTimer::timeout, this,&MainWindow::saveCurrentMatch);
    if(settings.value("MatchAutosaveTimed",false).toBool())
        autosaveTimer->start();


    settingsDialog->setModal(true);
    connect(ui->settingsButton,&QPushButton::clicked, settingsDialog,&SettingsDialog::exec);
    connect(settingsDialog,&SettingsDialog::accepted, this, [this]{
        emit settingsChanged();
        autosaveTimer->setInterval(settings.value("MatchAutosaveInterval",10000).toInt());
        if(settings.value("MatchAutosaveTimed",false).toBool())
            autosaveTimer->start();
        else
            autosaveTimer->stop();
    });

    statsSelect->setModal(true);
    connect(ui->columnsButton,&QPushButton::clicked, statsSelect,&statSelectionDialog::exec);
    connect(statsSelect,&statSelectionDialog::accepted, this, [this]{
        emit statsDisplayChanged(info);
    });

}

MainWindow::~MainWindow(){
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    delete ui;
}

void MainWindow::newMatch(Match *match){
    qDebug("Function Entered");
    int currentTab = ui->matchTabs->currentIndex();
    ui->matchTabs->insertTab(0,match,QStringLiteral("*Current Match"));
    if(currentTab == 0)
        ui->matchTabs->setCurrentIndex(0);

    match->showBenched(ui->showBenched->isChecked());
    match->statsDisplayChanged(info);
    connect(this,&MainWindow::statsDisplayChanged, match,&Match::statsDisplayChanged);
    connect(ui->showBenched,  &QCheckBox::toggled, match,&Match::showBenched);

    loadedFilenames.push_front(QString());
    if(settings.value("MatchAutosaveOnEnd",false).toBool() || settings.value("MatchAutosaveTimed",false).toBool()){
        loadedFilenames.front() = match->applyFilenameFormat(settings.value("MatchAutosaveFile",QString()).toString(),match->timestamp());
    }
    dbgReturn(return);
}

void MainWindow::doneMatch(QString tabText){
    qDebug("Function Entered");
    Match* match = qobject_cast<Match*>(ui->matchTabs->widget(0));
    if(ui->matchTabs->tabText(0).startsWith('*')){
        ui->matchTabs->setTabText(0,"*"+tabText);
        if(settings.value("MatchAutosaveOnEnd",false).toBool())
            saveMatch(0,match->applyFilenameFormat(settings.value("MatchAutosaveFile",QString()).toString(),match->timestamp()));
    } else
        ui->matchTabs->setTabText(0,tabText);
    dbgReturn(return);
}

void MainWindow::invalidateMatch(){
    ui->matchTabs->removeTab(0);
}


#include <QFileDialog>
void MainWindow::on_openButton_clicked(){
    qDebug("Function Entered");
    int tab = 0;
    if(ui->matchTabs->count() > 0 && ui->matchTabs->tabText(0).endsWith("Current Match"))
        tab = 1;
    QStringList names = QFileDialog::getOpenFileNames(this,"Load Matches",QString(),"SEN:P-AI files (*.sen)");
    for(QString &name: names){
        if(names.isEmpty())
            continue;
        Match *match = new Match();
        if(!Match::file<load>(match,name))
            continue;
        ui->matchTabs->insertTab(tab,match,name.mid(name.lastIndexOf(QRegExp("[\\\\/]"))+1));
        loadedFilenames.insert(loadedFilenames.begin()+tab,name);
        match->showBenched(ui->showBenched->isChecked());
        match->statsDisplayChanged(info);
        connect(this,&MainWindow::statsDisplayChanged, match,&Match::statsDisplayChanged);
        connect(ui->showBenched,  &QCheckBox::toggled, match,&Match::showBenched);
    }
    dbgReturn(return);
}
void MainWindow::saveMatch(int tab, QString name){
    qDebug("Function Entered");
    Match *match = qobject_cast<Match*>(ui->matchTabs->widget(tab));
    if(!match) dbgReturn(return);
    if(name.isEmpty()){
        name = QFileDialog::getSaveFileName(this,"Save Match As",settings.value("MatchSaveDir",QString()).toString(),"SEN:P-AI files (*.sen)");
        if(name.isEmpty())
            dbgReturn(return);
        int dirIdx = name.lastIndexOf(QRegExp("[\\\\/]"));
        settings.setValue("MatchSaveDir",name.left(dirIdx));
    }
    if(!Match::file<save>(match,name))
        dbgReturn(return);
    QString text = ui->matchTabs->tabText(tab);
    if(text.startsWith(QChar('*')))
        ui->matchTabs->setTabText(tab,text.mid(1));
    loadedFilenames[ui->matchTabs->currentIndex()] = name;
    dbgReturn(return);
}

void MainWindow::on_saveButton_clicked(){
    qDebug("Function Entered");
    int tab = ui->matchTabs->currentIndex();
    if(tab >= 0)
        saveMatch(tab,loadedFilenames[tab]);
    dbgReturn(return);
}

void MainWindow::on_saveasButton_clicked(){
    qDebug("Function Entered");
    saveMatch(ui->matchTabs->currentIndex());
    dbgReturn(return);
}

void MainWindow::saveCurrentMatch(){
    qDebug("Function Entered");
    Match *match = qobject_cast<Match*>(ui->matchTabs->widget(0));
    if(ui->matchTabs->count() && ui->matchTabs->tabText(0).endsWith("Current Match"))
        saveMatch(0,match->applyFilenameFormat(settings.value("MatchAutosaveFile",QString()).toString(),match->timestamp()));
    dbgReturn(return);
}

void MainWindow::currentMatchUpdated(){
    qDebug("Function Entered");
    ui->matchTabs->setTabText(0,"*Current Match");
    dbgReturn(return);
}

void MainWindow::on_matchTabs_tabCloseRequested(int index){
    qDebug("Function Entered");
    //Don't close the current match
    if(!ui->matchTabs->tabText(index).endsWith("Current Match"))
        ui->matchTabs->removeTab(index);
    dbgReturn(return);
}
