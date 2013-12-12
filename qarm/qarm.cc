/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * uARM
 *
 * Copyright (C) 2013 Marco Melletti
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef QARM_QARM_CC
#define QARM_QARM_CC

#include "qarm/qarm.h"
#include "armProc/aout.h"
#include "qarm/machine_config_dialog.h"
#include "services/error.h"

#include <QFileDialog>
#include <QFile>
#include <QScrollArea>


qarm::qarm(QApplication *app):
    application(app)
{
    // INFO: machine config init
    //configView = NULL;
    std::string error;
    std::string defaultFName = DEFAULT_CONFIG_FILE;
    MC_Holder::getInstance()->setConfig(MachineConfig::LoadFromFile(defaultFName, error));
    if(MC_Holder::getInstance()->getConfig() == NULL)
        MC_Holder::getInstance()->setConfig(MachineConfig::Create(defaultFName));

    mac = new machine;

    mainWidget = new QWidget;
    toolbar = new mainBar;
    display = new procDisplay(this);

    toolbar->setSpeed(MC_Holder::getInstance()->getConfig()->getClockRate());

    centralLayout = new QVBoxLayout;

    centralLayout->addWidget(new QFLine(false));
    centralLayout->addWidget(toolbar);
    centralLayout->addWidget(new QFLine(false));
    centralLayout->addWidget(display);

    mainWidget->setLayout(centralLayout);

    clock = new QTimer(this);

    connect(toolbar, SIGNAL(play(int)), this, SLOT(start(int)));
    connect(toolbar, SIGNAL(speedChanged(int)), this, SLOT(speedChanged(int)));
    connect(toolbar, SIGNAL(pause()), clock, SLOT(stop()));
    connect(toolbar, SIGNAL(reset()), this, SLOT(softReset()));
    connect(toolbar, SIGNAL(showRam()), this, SLOT(showRam()));
    connect(toolbar, SIGNAL(step()), this, SLOT(step()));
    //UNUSED: no longer needed with MachineConfig
    //connect(toolbar, SIGNAL(openRAM()), this, SLOT(selectCore()));
    //connect(toolbar, SIGNAL(openBIOS()), this, SLOT(selectBios()));
    connect(toolbar, SIGNAL(showConfig()), this, SLOT(showConfigDialog()));

    connect(clock, SIGNAL(timeout()), this, SLOT(step()));

    connect(this, SIGNAL(resetMachine(ulong)), mac, SLOT(reset(ulong)));
    connect(this, SIGNAL(resetMachine(ulong)), this, SIGNAL(resetDisplay()));
    connect(mac, SIGNAL(dataReady(Word*,Word*,Word*,QString)), display, SLOT(updateVals(Word*,Word*,Word*,QString)));
    connect(this, SIGNAL(resetDisplay()), display, SLOT(reset()));

    connect(this, SIGNAL(stop()), clock, SLOT(stop()));
    connect(this, SIGNAL(stop()), toolbar, SLOT(stop()));

    connect(mac, SIGNAL(updateStatus(QString)), toolbar, SLOT(updateStatus(QString)));

    setCentralWidget(mainWidget);
}

void qarm::softReset(){
    clock->stop();
    initialized = false;
    emit resetMachine(MC_Holder::getInstance()->getConfig()->getRamSize() * BYTES_PER_MEGABYTE);
    toolbar->setSpeed(MC_Holder::getInstance()->getConfig()->getClockRate());
    doReset = false;
    if(dataLoaded && biosLoaded)
        initialize();
}

bool qarm::initialize(){
    initialized = true;
    initialized &= openBIOS();
    initialized &= openRAM();
    return initialized;
}

void qarm::step(){
    if(doReset){
        emit resetMachine(MC_Holder::getInstance()->getConfig()->getRamSize() * BYTES_PER_MEGABYTE);
        doReset = false;
    }
    if(!initialized){
        if(!initialize()){
            emit stop();
            return;
        }
    }
    mac->step();
}

void qarm::start(int speed){
    int time;
    if(speed <= IPSTRESH)
        time = 1000 / speed;
    else
        time = 1;
    clock->start(time);
}

void qarm::speedChanged(int speed){
    if(clock->isActive()){
        clock->stop();
        start(speed);
    }
}

void qarm::showRam(){
    if(initialized){
        ramView *ramWindow = new ramView(mac, this);
        connect(this, SIGNAL(resetMachine(ulong)), ramWindow, SLOT(update()));
        connect(mac, SIGNAL(dataReady(Word*,Word*,Word*,QString)), ramWindow, SLOT(update()));
        ramWindow->show();
    } else {
        QMessageBox::warning(this, "Warning", "Machine not initialized,\ncannot display memory contents.", QMessageBox::Ok);
    }
}

void qarm::selectCore(){
    if(dataLoaded){
        QMessageBox::StandardButton reply = QMessageBox::question(this,"Caution",
                                                                  "Program File already loaded..\nReset machine and load new Program?",
                                                                  QMessageBox::Yes|QMessageBox::No);
        if(reply == QMessageBox::No)
            return;
        else {
            initialized = false;
            doReset = true;
        }
    }
    QString fileName = QFileDialog::getOpenFileName(this, "Open Program File", "", "Program Files (*.core.uarm);;Binary Files (*.bin);;All Files (*.*)");
    if(fileName != ""){
        coreF = fileName;
        dataLoaded = true;
    }
}

void qarm::selectBios(){
    if(biosLoaded){
        QMessageBox::StandardButton reply = QMessageBox::question(this,"Caution",
                                                                  "BIOS ROM already loaded..\nReset machine and load new BIOS?",
                                                                  QMessageBox::Yes|QMessageBox::No);
        if(reply == QMessageBox::No)
            return;
        else {
            initialized = false;
            doReset = true;
        }
    }
    QString fileName = QFileDialog::getOpenFileName(this, "Open BIOS File", "", "BIOS Files (*.rom.uarm);;Binary Files (*.bin);;All Files (*.*)");
    if(fileName != ""){
        biosF = fileName;
        biosLoaded = true;
    }
}

bool qarm::openRAM(){
    coreF = QString::fromStdString(MC_Holder::getInstance()->getConfig()->getROM(ROM_TYPE_CORE));
    if(coreF != ""){
        QFile f (coreF);
        if(!f.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Error", "Could not open Core file");
            return false;
        }
        QDataStream in(&f);
        ramMemory *ram = mac->getBus()->getRam();
        if(ram != NULL){
            Word len = (Word) f.size();
            char *buffer = new char[len];
            int sz = in.readRawData(buffer, len);
            if(sz <= 0 || (buffer[0] | buffer[1]<<8 | buffer[2]<<16 | buffer[3]<<24) != COREFILEID){
                QMessageBox::critical(this, "Error", "Irregular Core file");
                return false;
            }
            sz -= 4;
            if(sz <= 0 || !mac->getBus()->loadRAM(buffer+4, (Word) sz, true)){
                QMessageBox::critical(this, "Error", "Problems while loading Core file");
                return false;
            }
            delete [] buffer;
        }
        f.close();
        mac->refreshData();
    }
    return true;
}

bool qarm::openBIOS(){
    biosF = QString::fromStdString(MC_Holder::getInstance()->getConfig()->getROM(ROM_TYPE_BIOS));
    if(biosF != ""){
        QFile f (biosF);
        if(!f.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Error", "Could not open BIOS file");
            return false;
        }
        QDataStream in(&f);
        Word len = (Word) f.size();
        char *buffer = new char[len];
        Word sz = in.readRawData(buffer, len);
        if(sz <= 0 || (buffer[0] | buffer[1]<<8 | buffer[2]<<16 | buffer[3]<<24) != BIOSFILEID){
            QMessageBox::critical(this, "Error", "Irregular BIOS file");
            return false;
        }
        sz -= 8;
        if(sz <= 0 || !mac->getBus()->loadBIOS(buffer+8, (Word) sz)){
            QMessageBox::critical(this, "Error", "Problems while flashing BIOS ROM");
            return false;
        }
        delete [] buffer;
        f.close();
    }
    return true;
}

void qarm::showConfigDialog(){
    assert(MC_Holder::getInstance()->getConfig());

    MachineConfigDialog dialog(MC_Holder::getInstance()->getConfig(), this);
    if (dialog.exec() == QDialog::Accepted) {
        try {
            MC_Holder::getInstance()->getConfig()->Save();
        } catch (FileError& e) {
            QMessageBox::critical(this, QString("%1: Error").arg(application->applicationName()), e.what());
            return;
        }
        // EDIT: no config view for now..
        //configView->Update();
    }
}

#endif //QARM_QARM_CC
