// SPDX-License-Identifier: GPL-3.0-or-later
#include "mux_worker.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QStringDecoder>
#include <QTemporaryFile>

#include <array>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
class JobFailure final : public std::runtime_error
{
public:
    explicit JobFailure(const QString &message)
        : std::runtime_error(message.toUtf8().constData()), message_(message) {}
    QString message() const { return message_; }
private:
    QString message_;
};

class JobCancelled final {};

QString resolveTool(const QString &stem)
{
    QStringList names{stem};
#ifdef Q_OS_WIN
    names.prepend(stem + QStringLiteral(".exe"));
#endif
    const QDir appDir(QCoreApplication::applicationDirPath());
    for (const QString &name : names) {
        const QString local = appDir.filePath(name);
        if (QFileInfo(local).isFile()) return QFileInfo(local).absoluteFilePath();
        const QString bundled = appDir.filePath(QStringLiteral("tools/") + name);
        if (QFileInfo(bundled).isFile()) return QFileInfo(bundled).absoluteFilePath();
    }
    for (const QString &name : names) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty()) return found;
    }
#ifdef Q_OS_WIN
    const QStringList extraDirs = {
        QStringLiteral("C:/Program Files/MKVToolNix"),
        QStringLiteral("C:/Program Files (x86)/MKVToolNix"),
        QStringLiteral("C:/ffmpeg/bin"), QStringLiteral("C:/Program Files/ffmpeg/bin")};
    for (const QString &directory : extraDirs) {
        for (const QString &name : names) {
            const QString candidate = QDir(directory).filePath(name);
            if (QFileInfo(candidate).isFile()) return QFileInfo(candidate).absoluteFilePath();
        }
    }
#endif
    return {};
}

QString runCommand(const QString &program, const QStringList &arguments,
                   const QString &description, const std::atomic_bool &cancelRequested)
{
    if (cancelRequested.load()) throw JobCancelled{};
    if (program.isEmpty()) throw JobFailure(QStringLiteral("Executável obrigatório não encontrado: %1").arg(description));

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(QIODevice::ReadOnly);
    if (!process.waitForStarted(30000)) {
        throw JobFailure(QStringLiteral("Falha ao iniciar %1: %2").arg(description, process.errorString()));
    }
    while (!process.waitForFinished(100)) {
        if (cancelRequested.load()) {
            process.terminate();
            if (!process.waitForFinished(1500)) {
                process.kill();
                process.waitForFinished();
            }
            throw JobCancelled{};
        }
    }
    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(process.readAllStandardError());
    if (cancelRequested.load()) throw JobCancelled{};
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString detail = (stderrText.trimmed().isEmpty() ? stdoutText : stderrText).trimmed();
        throw JobFailure(QStringLiteral("%1 falhou (código %2): %3")
                             .arg(description).arg(process.exitCode()).arg(detail));
    }
    return stdoutText;
}

QString sanitizeFilenamePart(QString value)
{
    value = value.trimmed();
    const QString invalid = QStringLiteral("<>:\"/\\|?*");
    for (const QChar c : invalid) value.replace(c, QChar(u'_'));
    while (!value.isEmpty() && (value.endsWith(QChar(u'.')) || value.endsWith(QChar(u' '))))
        value.chop(1);
    return value;
}

QString sanitizeTag(QString value)
{
    value = value.trimmed();
    while (value.startsWith(QChar(u'['))) value.remove(0, 1);
    while (value.endsWith(QChar(u']'))) value.chop(1);
    const QString invalid = QStringLiteral("<>:\"/\\|?*");
    for (const QChar c : invalid) value.remove(c);
    value.remove(QChar(u' '));
    if (value.isEmpty()) throw JobFailure(QStringLiteral("Tag da fansub inválida."));
    if (value.size() > 32) value.truncate(32);
    return value;
}

QString jsonString(const QString &value)
{
    QJsonArray array;
    array.append(value);
    const QByteArray encoded = QJsonDocument(array).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(encoded.mid(1, encoded.size() - 2));
}

QString formatFileSize(qint64 bytes)
{
    const bool gigabytes = bytes >= (qint64(1024) * 1024 * 1024);
    const QString label = gigabytes
        ? QLocale::c().toString(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 'f', 2)
        : QString::number(bytes / (1024 * 1024));
    return label + (gigabytes ? QStringLiteral(" GB") : QStringLiteral(" MB"));
}

QString formatDuration(int totalSeconds)
{
    int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    if (seconds > 35) ++minutes;
    if (minutes >= 60) {
        int hours = minutes / 60;
        const int remainder = minutes % 60;
        if (remainder > 35) ++hours;
        return QString::number(hours) + QStringLiteral("hr");
    }
    return QString::number(minutes) + QStringLiteral("m");
}

bool isChapterFileValid(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw JobFailure(QStringLiteral("Não foi possível ler capítulos: %1").arg(path));
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString content = decoder.decode(file.readAll());
    if (decoder.hasError())
        throw JobFailure(QStringLiteral("Não foi possível ler capítulos como UTF-8: %1").arg(path));
    if (content.startsWith(QChar(0xFEFF))) content.remove(0, 1);
    static const QRegularExpression chapterPattern(QStringLiteral("^CHAPTER\\d+="),
                                                    QRegularExpression::MultilineOption);
    return chapterPattern.match(content).hasMatch();
}

QString detectVideoCodec(const QString &identifyOutput)
{
    QStringList lines;
    for (const QString &line : identifyOutput.split(QChar(u'\n')))
        if (line.contains(QStringLiteral("video"), Qt::CaseInsensitive)) lines.append(line);
    const QString blob = lines.join(QChar(u'\n'));
    if (blob.contains(QStringLiteral("HEVC")) || blob.contains(QStringLiteral("H.265"))) return QStringLiteral("HEVC");
    if (blob.contains(QStringLiteral("AVC")) || blob.contains(QStringLiteral("H.264"))) return QStringLiteral("AVC");
    if (blob.contains(QStringLiteral("AV1"))) return QStringLiteral("AV1");
    return QStringLiteral("Desconhecido");
}

QString detectAudioCodec(const QString &identifyOutput)
{
    for (const QString &line : identifyOutput.split(QChar(u'\n'))) {
        if (!line.contains(QStringLiteral("audio"), Qt::CaseInsensitive)) continue;
        if (line.contains(QStringLiteral("AAC"))) return QStringLiteral("AAC");
        if (line.contains(QStringLiteral("FLAC"))) return QStringLiteral("FLAC");
        if (line.contains(QStringLiteral("Opus"))) return QStringLiteral("Opus");
    }
    return QStringLiteral("Desconhecido");
}

int countTracks(const QString &identifyOutput, const QString &type)
{
    const QRegularExpression trackLine(QStringLiteral("^Track ID \\d+: (video|audio) "),
                                       QRegularExpression::CaseInsensitiveOption);
    int count = 0;
    for (const QString &line : identifyOutput.split(QChar(u'\n'))) {
        if (trackLine.match(line).hasMatch() && line.contains(QStringLiteral(": ") + type + QChar(u' ')))
            ++count;
    }
    return count;
}

QString heightFromDimensions(const QString &dimensions)
{
    const int x = dimensions.indexOf(QChar(u'x'));
    if (x < 0) return {};
    bool ok = false;
    const int height = dimensions.mid(x + 1).toInt(&ok);
    return ok && height > 0 ? QString::number(height) + QStringLiteral("p") : QString();
}

QString detectResolution(const QString &jsonText)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(jsonText.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw JobFailure(QStringLiteral("mkvmerge -J retornou JSON inválido"));
    const QJsonArray tracks = document.object().value(QStringLiteral("tracks")).toArray();
    for (const QJsonValue &trackValue : tracks) {
        const QJsonObject track = trackValue.toObject();
        if (track.value(QStringLiteral("type")).toString() != QStringLiteral("video")) continue;
        const QJsonObject properties = track.value(QStringLiteral("properties")).toObject();
        QString resolution = heightFromDimensions(properties.value(QStringLiteral("display_dimensions")).toString());
        if (resolution.isEmpty())
            resolution = heightFromDimensions(properties.value(QStringLiteral("pixel_dimensions")).toString());
        if (!resolution.isEmpty()) return resolution;
    }
    return QStringLiteral("Desconhecida");
}

QString calculateCrc32(const QString &path, const std::atomic_bool &cancelRequested,
                       const std::function<void(qint64, qint64)> &progress)
{
    static const std::array<quint32, 256> table = [] {
        std::array<quint32, 256> values{};
        for (quint32 i = 0; i < values.size(); ++i) {
            quint32 crc = i;
            for (int bit = 0; bit < 8; ++bit)
                crc = (crc & 1U) ? ((crc >> 1U) ^ 0xEDB88320U) : (crc >> 1U);
            values[i] = crc;
        }
        return values;
    }();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw JobFailure(QStringLiteral("Não foi possível ler o arquivo para calcular CRC-32: %1").arg(path));
    const qint64 total = QFileInfo(file).size();
    qint64 completed = 0;
    quint32 crc = 0xFFFFFFFFU;
    progress(0, total);
    while (!file.atEnd()) {
        if (cancelRequested.load()) throw JobCancelled{};
        const QByteArray block = file.read(1024 * 1024);
        if (block.isEmpty() && file.error() != QFileDevice::NoError)
            throw JobFailure(QStringLiteral("Erro ao ler arquivo durante CRC-32: %1").arg(path));
        for (const unsigned char byte : block)
            crc = table[(crc ^ byte) & 0xFFU] ^ (crc >> 8U);
        completed += block.size();
        progress(completed, total);
    }
    crc ^= 0xFFFFFFFFU;
    return QStringLiteral("%1").arg(crc, 8, 16, QChar(u'0')).toUpper();
}

QStringList collectFonts(const QString &directory)
{
    QStringList files;
    if (directory.isEmpty() || !QFileInfo(directory).isDir()) return files;
    QDirIterator iterator(directory, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == QStringLiteral("ttf") || suffix == QStringLiteral("otf")) files.append(path);
    }
    files.sort(Qt::CaseSensitive);
    return files;
}

QString writeMetadata(const QString &jsonPath, const QString &anime, const QString &episode,
                      const QString &episodeName, const QString &duration,
                      const QString &crc, const QString &sizeLabel)
{
    const QString json = QStringLiteral("{\n")
        + QStringLiteral("  \"anime\": %1,\n").arg(jsonString(anime))
        + QStringLiteral("  \"episodio\": %1,\n").arg(jsonString(episode))
        + QStringLiteral("  \"episode_name\": %1,\n").arg(jsonString(episodeName))
        + QStringLiteral("  \"duracao\": %1,\n").arg(jsonString(duration))
        + QStringLiteral("  \"crc\": %1,\n").arg(jsonString(crc))
        + QStringLiteral("  \"tamanho\": %1\n}\n").arg(jsonString(sizeLabel));
    QFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw JobFailure(QStringLiteral("Não foi possível gravar JSON: %1").arg(jsonPath));
    const QByteArray utf8 = json.toUtf8();
    if (file.write(utf8) != utf8.size())
        throw JobFailure(QStringLiteral("Falha ao gravar JSON: %1").arg(jsonPath));
    return jsonPath;
}
} // namespace

MuxWorker::MuxWorker(MuxJob job, QObject *parent) : QThread(parent), job_(std::move(job)) {}

void MuxWorker::cancel()
{
    cancelRequested_.store(true);
}

void MuxWorker::run()
{
    QString tempOutput;
    std::unique_ptr<QTemporaryFile> generatedChapterFile;
    try {
        QElapsedTimer elapsed;
        elapsed.start();
        const QFileInfo videoInfo(job_.videoPath);
        const QFileInfo subtitleInfo(job_.subtitlePath);
        if (!videoInfo.isFile()) throw JobFailure(QStringLiteral("Arquivo de vídeo não encontrado: %1").arg(job_.videoPath));
        if (!subtitleInfo.isFile()) throw JobFailure(QStringLiteral("Arquivo de legenda não encontrado: %1").arg(job_.subtitlePath));

        const QString textChaptersPath = job_.chaptersPath.trimmed();
        QString chaptersPath;
        ChapterInputKind chapterInput = ChapterInputKind::None;
        AssChapterParseResult assChapters;
        if (job_.chapterMode == ChapterMode::AutomaticFromAss) {
            QFile assFile(subtitleInfo.absoluteFilePath());
            if (!assFile.open(QIODevice::ReadOnly))
                throw JobFailure(QStringLiteral("Não foi possível ler a legenda ASS: %1").arg(job_.subtitlePath));
            assChapters = parseAssChapters(assFile.readAll());
            if (!assChapters.error.isEmpty()) throw JobFailure(assChapters.error);
            for (const QString &warning : assChapters.warnings)
                emit logLine(QStringLiteral("WARNING: %1").arg(warning));
            chapterInput = selectChapterInput(job_.chapterMode, !assChapters.chapters.isEmpty(),
                                              !textChaptersPath.isEmpty());
        } else {
            chapterInput = selectChapterInput(job_.chapterMode, false,
                                              !textChaptersPath.isEmpty());
        }

        if (chapterInput == ChapterInputKind::Ass) {
            const QString summary = QStringLiteral("Capítulos: %1 encontrados na legenda")
                                        .arg(assChapters.chapters.size());
            emit chaptersSummaryFound(summary);
            emit logLine(QStringLiteral("INFO: %1 capítulo(s) encontrado(s) na legenda ASS.")
                             .arg(assChapters.chapters.size()));
            for (qsizetype i = 0; i < assChapters.chapters.size(); ++i) {
                const QString number = QString::number(i + 1).rightJustified(2, QChar(u'0'));
                const ChapterEntry &chapter = assChapters.chapters.at(i);
                emit logLine(QStringLiteral("INFO: Capítulo %1 — %2 — %3")
                                 .arg(number, chapter.timestamp, chapter.name));
            }
            if (!textChaptersPath.isEmpty())
                emit logLine(QStringLiteral("INFO: o arquivo .txt informado foi ignorado porque a legenda tem prioridade."));

            generatedChapterFile = std::make_unique<QTemporaryFile>(
                QDir::temp().filePath(QStringLiteral("Musubi-chapters-XXXXXX.txt")));
            generatedChapterFile->setAutoRemove(true);
            if (!generatedChapterFile->open())
                throw JobFailure(QStringLiteral("Não foi possível criar o arquivo temporário de capítulos."));
            const QByteArray chapterData = serializeMkvmergeChapters(assChapters.chapters);
            if (generatedChapterFile->write(chapterData) != chapterData.size() ||
                !generatedChapterFile->flush())
                throw JobFailure(QStringLiteral("Não foi possível gravar o arquivo temporário de capítulos."));
            chaptersPath = generatedChapterFile->fileName();
            generatedChapterFile->close();
        } else if (chapterInput == ChapterInputKind::TextFile) {
            if (textChaptersPath.isEmpty())
                throw JobFailure(QStringLiteral("Selecione um arquivo de capítulos .txt."));
            const QFileInfo chaptersInfo(textChaptersPath);
            if (!chaptersInfo.isFile())
                throw JobFailure(QStringLiteral("Arquivo de capítulos não encontrado: %1").arg(textChaptersPath));
            if (!isChapterFileValid(textChaptersPath))
                throw JobFailure(QStringLiteral("Arquivo de capítulos inválido: %1").arg(textChaptersPath));
            chaptersPath = chaptersInfo.absoluteFilePath();
            emit chaptersSummaryFound(QStringLiteral("Capítulos: arquivo .txt"));
            if (job_.chapterMode == ChapterMode::AutomaticFromAss) {
                emit logLine(QStringLiteral("INFO: nenhum capítulo válido foi encontrado na legenda; usando o .txt informado como fallback."));
            } else {
                emit logLine(QStringLiteral("INFO: usando apenas o arquivo .txt selecionado."));
            }
        } else {
            const QString summary = job_.chapterMode == ChapterMode::None
                ? QStringLiteral("Capítulos: desativados")
                : QStringLiteral("Capítulos: nenhum na legenda; mux sem capítulos");
            emit chaptersSummaryFound(summary);
            if (job_.chapterMode == ChapterMode::None)
                emit logLine(QStringLiteral("INFO: capítulos desativados pelo usuário."));
            else
                emit logLine(QStringLiteral("WARNING: nenhum capítulo válido foi encontrado na legenda e nenhum .txt foi informado; o mux seguirá sem capítulos."));
        }

        const QString outputDirectory = job_.outputDirectory.isEmpty()
            ? videoInfo.absolutePath() : QFileInfo(job_.outputDirectory).absoluteFilePath();
        if (!QFileInfo(outputDirectory).isDir())
            throw JobFailure(QStringLiteral("Pasta de saída não encontrada: %1").arg(outputDirectory));
        const QStorageInfo storage(outputDirectory);
        if (storage.isValid() && storage.bytesAvailable() <= videoInfo.size()) {
            throw JobFailure(QStringLiteral("Espaço em disco insuficiente em: %1 (necessário aproximadamente o tamanho do vídeo de origem).")
                                 .arg(outputDirectory));
        }

        const QString mkvmerge = resolveTool(QStringLiteral("mkvmerge"));
        const QString mkvinfo = resolveTool(QStringLiteral("mkvinfo"));
        const QString ffmpeg = resolveTool(QStringLiteral("ffmpeg"));
        const QString ffprobe = resolveTool(QStringLiteral("ffprobe"));
        QStringList missing;
        if (mkvmerge.isEmpty()) missing.append(QStringLiteral("mkvmerge (MKVToolNix)"));
        if (ffmpeg.isEmpty()) missing.append(QStringLiteral("ffmpeg"));
        if (ffprobe.isEmpty()) missing.append(QStringLiteral("ffprobe"));
        if (!missing.isEmpty()) throw JobFailure(QStringLiteral("Dependências faltando: %1. Instale MKVToolNix e FFmpeg e adicione-os ao PATH.")
                                                    .arg(missing.join(QStringLiteral(", "))));
        if (mkvinfo.isEmpty()) emit logLine(QStringLiteral("Aviso: mkvinfo não encontrado; a ferramenta é opcional e não será executada."));

        emit statusChanged(QStringLiteral("Analisando faixas de mídia..."));
        emit logLine(QStringLiteral("Analisando vídeo de origem..."));
        const QString identify = runCommand(mkvmerge, {QStringLiteral("--identify"), videoInfo.absoluteFilePath()},
                                            QStringLiteral("mkvmerge --identify"), cancelRequested_);
        const QString resolutionJson = runCommand(mkvmerge, {QStringLiteral("-J"), videoInfo.absoluteFilePath()},
                                                  QStringLiteral("mkvmerge -J"), cancelRequested_);
        const QString videoCodec = detectVideoCodec(identify);
        const QString audioCodec = detectAudioCodec(identify);
        const QString resolution = detectResolution(resolutionJson);
        const int videoCount = countTracks(identify, QStringLiteral("video"));
        const int audioCount = countTracks(identify, QStringLiteral("audio"));
        const int totalTracks = videoCount + audioCount;
        QStringList trackOrder;
        for (int i = 0; i < totalTracks; ++i) trackOrder.append(QStringLiteral("0:%1").arg(i));
        trackOrder.append(QStringLiteral("1:0"));
        const QString summary = QStringLiteral("Vídeo: %1  •  Áudio: %2  •  Resolução: %3")
                                    .arg(videoCodec, audioCodec, resolution);
        emit mediaInfoFound(summary);
        emit logLine(summary);

        const QString tag = sanitizeTag(job_.tag.isEmpty() ? QStringLiteral("CFSB") : job_.tag);
        const QString anime = sanitizeFilenamePart(job_.anime);
        const QString episode = sanitizeFilenamePart(job_.episode);
        const QString episodeName = sanitizeFilenamePart(job_.episodeName);
        const QString baseName = QStringLiteral("[%1] %2 - %3 [%4][%5][%6][%7]")
                                     .arg(tag, anime, episode, resolution, job_.source, videoCodec, audioCodec);
        tempOutput = QDir(outputDirectory).filePath(baseName + QStringLiteral("_TEMP.mkv"));
        QFile::remove(tempOutput);

        QStringList muxArguments = {
            QStringLiteral("--ui-language"), QStringLiteral("pt_BR"),
            QStringLiteral("--priority"), QStringLiteral("lower"),
            QStringLiteral("--output"), tempOutput,
            QStringLiteral("--title"), QStringLiteral("%1 - %2 - %3").arg(anime, episode, episodeName),
            QStringLiteral("--no-subtitles"), QStringLiteral("--no-chapters"), QStringLiteral("--no-global-tags"),
            QStringLiteral("--language"), QStringLiteral("1:ja-JP"),
            QStringLiteral("--track-name"), QStringLiteral("1:Japonês"),
            QStringLiteral("--original-flag"), QStringLiteral("1:yes"),
            QStringLiteral("--audio-tracks"), QStringLiteral("1"),
            QStringLiteral("("), videoInfo.absoluteFilePath(), QStringLiteral(")"),
            QStringLiteral("--language"), QStringLiteral("0:pt-BR"),
            QStringLiteral("--track-name"), QStringLiteral("0:Português do Brasil"),
            QStringLiteral("("), subtitleInfo.absoluteFilePath(), QStringLiteral(")"),
            QStringLiteral("--track-order"), trackOrder.join(QChar(u','))};

        if (!chaptersPath.isEmpty()) {
            const int trackOrderIndex = muxArguments.indexOf(QStringLiteral("--track-order"));
            muxArguments.insert(trackOrderIndex, QStringLiteral("Capítulo <NUM:2>"));
            muxArguments.insert(trackOrderIndex, QStringLiteral("--generate-chapters-name-template"));
            muxArguments.insert(trackOrderIndex, chaptersPath);
            muxArguments.insert(trackOrderIndex, QStringLiteral("--chapters"));
        }

        const QStringList fonts = collectFonts(job_.fontsDirectory);
        if (!fonts.isEmpty()) {
            const int insertAt = muxArguments.indexOf(QStringLiteral("--no-global-tags"));
            QStringList fontArguments{QStringLiteral("--no-attachments")};
            for (const QString &font : fonts) {
                fontArguments.append(QStringLiteral("--attach-file"));
                fontArguments.append(font);
            }
            for (int i = fontArguments.size() - 1; i >= 0; --i) muxArguments.insert(insertAt, fontArguments.at(i));
        }

        emit statusChanged(QStringLiteral("Multiplexando faixas..."));
        emit logLine(QStringLiteral("Executando mkvmerge..."));
        runCommand(mkvmerge, muxArguments, QStringLiteral("mkvmerge"), cancelRequested_);
        if (generatedChapterFile) {
            if (!generatedChapterFile->remove())
                throw JobFailure(QStringLiteral("Não foi possível remover o arquivo temporário de capítulos."));
            generatedChapterFile.reset();
        }
        if (!QFileInfo(tempOutput).isFile()) throw JobFailure(QStringLiteral("mkvmerge terminou sem gerar o arquivo de saída"));

        emit statusChanged(QStringLiteral("Calculando CRC-32..."));
        emit logLine(QStringLiteral("Calculando CRC-32 em blocos..."));
        const QString crc = calculateCrc32(tempOutput, cancelRequested_, [this](qint64 done, qint64 total) {
            emit crcProgress(done, total);
        });
        const QString finalPath = QDir(outputDirectory).filePath(baseName + QStringLiteral("[%1].mkv").arg(crc));
        QFile::remove(finalPath);
        if (!QFile::rename(tempOutput, finalPath))
            throw JobFailure(QStringLiteral("Não foi possível renomear o MKV temporário para: %1").arg(finalPath));
        tempOutput.clear();

        QString thumbnailPath;
        if (job_.generateThumbnail) {
            thumbnailPath = QDir(outputDirectory).filePath(baseName + QStringLiteral("[%1].webp").arg(crc));
            emit statusChanged(QStringLiteral("Gerando thumbnail..."));
            emit logLine(QStringLiteral("Extraindo thumbnail WebP..."));
            runCommand(ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
                                QStringLiteral("-ss"), job_.thumbnailTimestamp.isEmpty() ? QStringLiteral("00:00:30") : job_.thumbnailTimestamp,
                                QStringLiteral("-i"), finalPath, QStringLiteral("-vf"), QStringLiteral("thumbnail,setsar=1"),
                                QStringLiteral("-vframes"), QStringLiteral("1"), QStringLiteral("-y"), thumbnailPath},
                       QStringLiteral("ffmpeg"), cancelRequested_);
        }

        emit statusChanged(QStringLiteral("Obtendo duração do vídeo..."));
        const QString durationOutput = runCommand(ffprobe,
            {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_entries"),
             QStringLiteral("format=duration"), QStringLiteral("-of"),
             QStringLiteral("default=noprint_wrappers=1:nokey=1"), finalPath},
            QStringLiteral("ffprobe"), cancelRequested_).trimmed();
        bool durationOk = false;
        const double secondsValue = QLocale::c().toDouble(durationOutput, &durationOk);
        if (!durationOk) throw JobFailure(QStringLiteral("ffprobe retornou duração inválida"));
        const QString duration = formatDuration(static_cast<int>(secondsValue));

        QString jsonPath;
        if (job_.generateJson) {
            jsonPath = QFileInfo(finalPath).absolutePath() + QChar(u'/') + QFileInfo(finalPath).completeBaseName() + QStringLiteral(".json");
            writeMetadata(jsonPath, anime, episode, episodeName, duration, crc,
                          formatFileSize(QFileInfo(finalPath).size()));
        }
        const int elapsedSeconds = static_cast<int>(elapsed.elapsed() / 1000);
        emit succeeded(finalPath, jsonPath, thumbnailPath, crc, elapsedSeconds);
    } catch (const JobCancelled &) {
        if (!tempOutput.isEmpty()) QFile::remove(tempOutput);
        emit cancelled();
    } catch (const JobFailure &failure) {
        if (!tempOutput.isEmpty()) QFile::remove(tempOutput);
        emit failed(failure.message());
    } catch (const std::exception &error) {
        if (!tempOutput.isEmpty()) QFile::remove(tempOutput);
        emit failed(QString::fromUtf8(error.what()));
    }
}
