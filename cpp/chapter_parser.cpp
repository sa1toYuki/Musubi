// SPDX-License-Identifier: GPL-3.0-or-later
#include "chapter_parser.h"

#include <QRegularExpression>
#include <QStringDecoder>

#include <algorithm>
#include <limits>

namespace {
QStringList splitFormat(const QString &formatText)
{
    QStringList fields;
    for (const QString &field : formatText.split(QChar(u',')))
        fields.append(field.trimmed());
    return fields;
}

QVector<QString> splitEventFields(const QString &eventText, int fieldCount,
                                  int textIndex)
{
    if (fieldCount <= 0 || textIndex < 0 || textIndex >= fieldCount) return {};
    QVector<QString> fields(fieldCount);
    qsizetype left = 0;

    for (int i = 0; i < textIndex; ++i) {
        const qsizetype comma = eventText.indexOf(QChar(u','), left);
        if (comma < 0) return {};
        fields[i] = eventText.mid(left, comma - left);
        left = comma + 1;
    }

    qsizetype right = eventText.size();
    for (int i = fieldCount - 1; i > textIndex; --i) {
        const qsizetype comma = eventText.lastIndexOf(QChar(u','), right - 1);
        if (comma < left) return {};
        fields[i] = eventText.mid(comma + 1, right - comma - 1);
        right = comma;
    }

    fields[textIndex] = eventText.mid(left, right - left);
    return fields;
}

bool parseAssTimestamp(const QString &value, qint64 &milliseconds,
                       QString &formatted)
{
    static const QRegularExpression pattern(
        QStringLiteral("^(\\d+):(\\d{2}):(\\d{2})\\.(\\d{2})$"));
    const QRegularExpressionMatch match = pattern.match(value.trimmed());
    if (!match.hasMatch()) return false;

    bool hoursOk = false;
    const qint64 hours = match.captured(1).toLongLong(&hoursOk);
    const int minutes = match.captured(2).toInt();
    const int seconds = match.captured(3).toInt();
    const int centiseconds = match.captured(4).toInt();
    if (!hoursOk || minutes > 59 || seconds > 59 || centiseconds > 99) return false;

    const qint64 remainder = qint64(minutes) * 60000 + qint64(seconds) * 1000 +
                             qint64(centiseconds) * 10;
    const qint64 max = std::numeric_limits<qint64>::max();
    if (hours > (max - remainder) / 3600000) return false;
    milliseconds = hours * 3600000 + remainder;

    const qint64 totalSeconds = milliseconds / 1000;
    const qint64 outputHours = totalSeconds / 3600;
    const qint64 outputMinutes = (totalSeconds / 60) % 60;
    const qint64 outputSeconds = totalSeconds % 60;
    const qint64 outputMilliseconds = milliseconds % 1000;
    formatted = QStringLiteral("%1:%2:%3.%4")
                    .arg(outputHours, 2, 10, QChar(u'0'))
                    .arg(outputMinutes, 2, 10, QChar(u'0'))
                    .arg(outputSeconds, 2, 10, QChar(u'0'))
                    .arg(outputMilliseconds, 3, 10, QChar(u'0'));
    return true;
}

QString cleanChapterName(QString text)
{
    static const QRegularExpression overrideTags(QStringLiteral(R"(\{\\[^}]*\})"));
    static const QRegularExpression lineBreaks(QStringLiteral(R"([ \t]*\\[Nn][ \t]*)"));
    text.remove(overrideTags);
    text.replace(lineBreaks, QStringLiteral(" "));
    return text.trimmed();
}

bool isChapterEffect(const QString &effect)
{
    static const QStringList accepted = {
        QStringLiteral("CAP"), QStringLiteral("Capítulo"), QStringLiteral("Capitulo"),
        QStringLiteral("Chapter"), QStringLiteral("Chap")};
    const QString candidate = effect.trimmed();
    for (const QString &value : accepted)
        if (candidate.compare(value, Qt::CaseInsensitive) == 0) return true;
    return false;
}
}

AssChapterParseResult parseAssChapters(const QByteArray &utf8Contents)
{
    AssChapterParseResult result;
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString contents = decoder.decode(utf8Contents);
    if (decoder.hasError()) {
        result.error = QStringLiteral("A legenda ASS não está em UTF-8 válido.");
        return result;
    }
    if (contents.startsWith(QChar(0xFEFF))) contents.remove(0, 1);

    bool inEvents = false;
    QStringList fieldNames;
    int startIndex = -1;
    int effectIndex = -1;
    int textIndex = -1;
    bool foundEventsFormat = false;
    static const QRegularExpression commentPrefix(QStringLiteral("^Comment:\\s*(.*)$"),
                                                  QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression timePattern(QStringLiteral("^\\d+:\\d{2}:\\d{2}\\.\\d{2}$"));

    const QStringList lines = contents.split(QChar(u'\n'));
    for (QString line : lines) {
        if (line.endsWith(QChar(u'\r'))) line.chop(1);
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QChar(u'[')) && trimmed.endsWith(QChar(u']'))) {
            inEvents = trimmed.compare(QStringLiteral("[Events]"), Qt::CaseInsensitive) == 0;
            continue;
        }
        if (!inEvents) continue;

        if (trimmed.startsWith(QStringLiteral("Format:"), Qt::CaseInsensitive)) {
            fieldNames = splitFormat(trimmed.mid(7));
            startIndex = effectIndex = textIndex = -1;
            for (int i = 0; i < fieldNames.size(); ++i) {
                const QString name = fieldNames.at(i).trimmed();
                if (name.compare(QStringLiteral("Start"), Qt::CaseInsensitive) == 0) startIndex = i;
                if (name.compare(QStringLiteral("Effect"), Qt::CaseInsensitive) == 0) effectIndex = i;
                if (name.compare(QStringLiteral("Text"), Qt::CaseInsensitive) == 0) textIndex = i;
            }
            foundEventsFormat = true;
            if (startIndex < 0 || effectIndex < 0 || textIndex < 0)
                result.warnings.append(QStringLiteral("Format: da seção [Events] precisa conter Start, Effect e Text."));
            continue;
        }

        const QRegularExpressionMatch comment = commentPrefix.match(trimmed);
        if (!comment.hasMatch() || fieldNames.isEmpty() || startIndex < 0 ||
            effectIndex < 0 || textIndex < 0)
            continue;

        const QVector<QString> fields = splitEventFields(comment.captured(1), fieldNames.size(), textIndex);
        if (fields.size() != fieldNames.size()) {
            result.warnings.append(QStringLiteral("Linha Comment inválida: quantidade de campos não corresponde ao Format."));
            continue;
        }
        if (!isChapterEffect(fields.at(effectIndex))) continue;

        const QString rawTime = fields.at(startIndex).trimmed();
        if (!timePattern.match(rawTime).hasMatch()) {
            result.warnings.append(QStringLiteral("Capítulo ignorado: tempo Start inválido (%1). ").arg(rawTime).trimmed());
            continue;
        }
        ChapterEntry chapter;
        if (!parseAssTimestamp(rawTime, chapter.startMilliseconds, chapter.timestamp)) {
            result.warnings.append(QStringLiteral("Capítulo ignorado: tempo Start fora do intervalo válido (%1). ").arg(rawTime).trimmed());
            continue;
        }
        chapter.name = cleanChapterName(fields.at(textIndex));
        if (chapter.name.isEmpty()) {
            result.warnings.append(QStringLiteral("Capítulo em %1 ignorado: nome vazio.").arg(chapter.timestamp));
            continue;
        }
        result.chapters.append(std::move(chapter));
    }

    if (!foundEventsFormat)
        result.warnings.append(QStringLiteral("Não foi encontrada uma linha Format: na seção [Events]."));

    std::stable_sort(result.chapters.begin(), result.chapters.end(),
                     [](const ChapterEntry &left, const ChapterEntry &right) {
                         return left.startMilliseconds < right.startMilliseconds;
                     });
    for (qsizetype i = 1; i < result.chapters.size(); ++i) {
        if (result.chapters.at(i - 1).startMilliseconds == result.chapters.at(i).startMilliseconds) {
            result.warnings.append(QStringLiteral("Aviso: há capítulos duplicados no tempo %1; ambos serão mantidos.")
                                       .arg(result.chapters.at(i).timestamp));
        }
    }
    if (!result.chapters.isEmpty() && result.chapters.first().startMilliseconds != 0) {
        result.warnings.append(QStringLiteral("Aviso: o primeiro capítulo começa em %1, não em 00:00:00.000; nenhum capítulo será inventado.")
                                   .arg(result.chapters.first().timestamp));
    }
    return result;
}

QByteArray serializeMkvmergeChapters(const QVector<ChapterEntry> &chapters)
{
    QString output;
    for (qsizetype i = 0; i < chapters.size(); ++i) {
        const QString number = QString::number(i + 1).rightJustified(2, QChar(u'0'));
        output += QStringLiteral("CHAPTER%1=%2\nCHAPTER%1NAME=%3\n")
                      .arg(number, chapters.at(i).timestamp, chapters.at(i).name);
    }
    return output.toUtf8();
}

ChapterInputKind selectChapterInput(ChapterMode mode, bool hasAssChapters,
                                    bool textFileProvided)
{
    switch (mode) {
    case ChapterMode::AutomaticFromAss:
        if (hasAssChapters) return ChapterInputKind::Ass;
        return textFileProvided ? ChapterInputKind::TextFile : ChapterInputKind::None;
    case ChapterMode::TextFile:
        return ChapterInputKind::TextFile;
    case ChapterMode::None:
        return ChapterInputKind::None;
    }
    return ChapterInputKind::None;
}
