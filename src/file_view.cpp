/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2011 ~ 2017 Deepin, Inc.
 *               2011 ~ 2017 Wang Yong
 *
 * Author:     Wang Yong <wangyong@deepin.com>
 * Maintainer: Wang Yong <wangyong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QProcess>
#include <QTimer>

#include "file_item.h"
#include "file_view.h"
#include "utils.h"

FileView::FileView(QWidget *parent) : QListWidget(parent)
{
    setMouseTracking(true);   // make MouseMove can response

    setFixedSize(433, 305);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(this, SIGNAL(rightClick(QPoint)), this, SLOT(onRightClick(QPoint)));
    
    rightMenu = new QMenu();
    renameAction = new QAction(tr("Rename"), this);
    connect(renameAction, &QAction::triggered, this, &FileView::renameItem);
    displayAction = new QAction(tr("Display in file manager"), this);
    connect(displayAction, &QAction::triggered, this, &FileView::displayItem);
    deleteAction = new QAction(tr("Delete"), this);
    connect(deleteAction, &QAction::triggered, this, &FileView::deleteItem);
    rightMenu->addAction(renameAction);
    rightMenu->addAction(displayAction);
    rightMenu->addAction(deleteAction);

    fileWatcher = new QFileSystemWatcher();
    connect(fileWatcher, &QFileSystemWatcher::fileChanged, this, &FileView::monitorFileChanged);
    
    QFileInfoList fileInfoList = Utils::getRecordingFileinfos();

    foreach (auto fileInfo, fileInfoList) {
        fileWatcher->addPath(fileInfo.absoluteFilePath());
        
        FileItem *fileItem = new FileItem();
        fileItem->setFileInfo(fileInfo);
        connect(fileItem, SIGNAL(play()), this, SLOT(handlePlay()));
        connect(fileItem, SIGNAL(pause()), this, SLOT(handlePause()));
        connect(fileItem, SIGNAL(resume()), this, SLOT(handleResume()));
        connect(fileItem, SIGNAL(stop()), this, SLOT(handleStop()));

        addItem(fileItem->getItem());
        fileItem->getItem()->setSizeHint(QSize(100, 60));
        setItemWidget(fileItem->getItem(), fileItem);
    }
}

void FileView::monitorList()
{
    if (count() == 0) {
        emit listClear();
    }
}

void FileView::monitorFileChanged(QString filepath)
{
    for(int i = 0; i < count(); i++) {
        QListWidgetItem* matchItem = item(i);
        FileItem *fileItem = static_cast<FileItem *>(itemWidget(matchItem));

        if (fileItem->getRecodingFilepath() == filepath) {
            if (!Utils::fileExists(filepath)) {
                emit stop(fileItem->getRecodingFilepath());
                delete takeItem(row(matchItem));
            }
            
            monitorList();
            
            break;
        }
    }
}

void FileView::mousePressEvent(QMouseEvent *event)
{
    QListWidget::mousePressEvent(event);

    if(event->button() == Qt::RightButton){
        emit rightClick(event->pos());
    }
}

void FileView::onRightClick(QPoint pos)
{
    rightSelectItem = itemAt(pos);
    if (rightSelectItem != 0) {
        rightMenu->exec(this->mapToGlobal(pos));
    }
}

void FileView::renameItem()
{
    if (rightSelectItem != 0) {
        FileItem *fileItem = static_cast<FileItem *>(itemWidget(rightSelectItem));
        fileItem->switchStatus(FileItem::STATUS_RENAME);
    }
}

void FileView::displayItem()
{
    if (rightSelectItem != 0) {
        FileItem *fileItem = static_cast<FileItem *>(itemWidget(rightSelectItem));
        auto dirUrl = QUrl::fromLocalFile(fileItem->getFileInfo().absoluteDir().absolutePath());
        QFileInfo ddefilemanger("/usr/bin/dde-file-manager");
        if (ddefilemanger.exists()) {
            auto dirFile = QUrl::fromLocalFile(fileItem->getFileInfo().absoluteFilePath());
            auto url = QString("%1?selectUrl=%2").arg(dirUrl.toString()).arg(dirFile.toString());
            QProcess::startDetached("dde-file-manager" , QStringList() << url);
        } else {
            QProcess::startDetached("gvfs-open" , QStringList() << dirUrl.toString());
        }
    }
}

void FileView::deleteItem()
{
    if (rightSelectItem != 0) {
        FileItem *fileItem = static_cast<FileItem *>(itemWidget(rightSelectItem));
        emit stop(fileItem->getRecodingFilepath());

        QFile(fileItem->getRecodingFilepath()).remove();
        delete takeItem(row(rightSelectItem));
        
        monitorList();
    }
}

void FileView::handlePlay()
{
    emit play(((FileItem*) sender())->getRecodingFilepath());
}

void FileView::handlePause()
{
    emit pause(((FileItem*) sender())->getRecodingFilepath());
}

void FileView::handleResume()
{
    emit resume(((FileItem*) sender())->getRecodingFilepath());
}

void FileView::handleStop()
{
    emit stop(((FileItem*) sender())->getRecodingFilepath());
}

void FileView::handlePlayFinish(QString filepath)
{
    for(int i = 0; i < count(); i++) {
        QListWidgetItem* matchItem = item(i);
        FileItem *fileItem = static_cast<FileItem *>(itemWidget(matchItem));

        if (fileItem->getRecodingFilepath() == filepath) {
            fileItem->switchStatus(FileItem::STATUS_NORMAL);
            break;
        }
    }
}

void FileView::selectItemWithPath(QString path)
{
    for(int i = 0; i < count(); i++) {
        QListWidgetItem* matchItem = item(i);
        FileItem *fileItem = static_cast<FileItem *>(itemWidget(matchItem));

        if (fileItem->getRecodingFilepath() == path) {
            setCurrentItem(matchItem);
            fileItem->switchStatus(FileItem::STATUS_PLAY);
            
            // ListPage will got item's duration after recording.
            // Update duration after 1 seoncd, avoid get wrong duration when wav file not flush to disk.
            QTimer::singleShot(1000, fileItem, SLOT(updateDurationLabel()));
            
            break;
        }
    }
}
