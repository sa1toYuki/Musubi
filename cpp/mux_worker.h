// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "chapter_parser.h"

#include <QThread>
#include <QString>

#include <atomic>

struct MuxJob
{
    QString videoPath;
    QString subtitlePath;
    QString chaptersPath;
    ChapterMode chapterMode = ChapterMode::AutomaticFromAss;
    QString fontsDirectory;
    QString outputDirectory;
    QString tag;
    QString anime;
    QString episode;
    QString episodeName;
    QString source;
    QString thumbnailTimestamp;
    bool generateJson = true;
    bool generateThumbnail = false;
};

class MuxWorker final : public QThread
{
    Q_OBJECT
public:
    explicit MuxWorker(MuxJob job, QObject *parent = nullptr);
    void cancel();

signals:
    void logLine(const QString &message);
    void statusChanged(const QString &message);
    void mediaInfoFound(const QString &summary);
    void chaptersSummaryFound(const QString &summary);
    void crcProgress(qint64 completed, qint64 total);
    void succeeded(const QString &mkvPath, const QString &jsonPath,
                   const QString &thumbnailPath, const QString &crc,
                   int elapsedSeconds);
    void failed(const QString &message);
    void cancelled();

protected:
    void run() override;

private:
    MuxJob job_;
    std::atomic_bool cancelRequested_{false};
};
