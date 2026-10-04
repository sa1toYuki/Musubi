// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

enum class ChapterMode
{
    AutomaticFromAss,
    TextFile,
    None
};

enum class ChapterInputKind
{
    Ass,
    TextFile,
    None
};

struct ChapterEntry
{
    qint64 startMilliseconds = 0;
    QString timestamp;
    QString name;
};

struct AssChapterParseResult
{
    QVector<ChapterEntry> chapters;
    QStringList warnings;
    QString error;
};

AssChapterParseResult parseAssChapters(const QByteArray &utf8Contents);
QByteArray serializeMkvmergeChapters(const QVector<ChapterEntry> &chapters);
ChapterInputKind selectChapterInput(ChapterMode mode, bool hasAssChapters,
                                    bool textFileProvided);
