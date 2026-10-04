// SPDX-License-Identifier: GPL-3.0-or-later
#include "chapter_parser.h"
#include "mux_worker.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTextStream>

namespace {
int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << Qt::endl;
    ++failures;
}

QString eventLine(const QString &effect, const QString &start, const QString &text)
{
    return QStringLiteral("Comment: 0,%1,0:25:00.00,Default,,0,0,0,%2,%3")
        .arg(start, effect, text);
}

int exportChapters(const QString &inputPath, const QString &outputPath)
{
    QFile input(inputPath);
    if (!input.open(QIODevice::ReadOnly)) {
        QTextStream(stderr) << "Não foi possível ler " << inputPath << Qt::endl;
        return 2;
    }
    const AssChapterParseResult parsed = parseAssChapters(input.readAll());
    if (!parsed.error.isEmpty()) {
        QTextStream(stderr) << parsed.error << Qt::endl;
        return 2;
    }
    QFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        output.write(serializeMkvmergeChapters(parsed.chapters)) < 0) {
        QTextStream(stderr) << "Não foi possível gravar " << outputPath << Qt::endl;
        return 2;
    }
    return parsed.chapters.isEmpty() ? 3 : 0;
}

int runMuxIntegration(QCoreApplication &app, const QString &videoPath,
                      const QString &assPath, const QString &outputDirectory)
{
    MuxJob job;
    job.videoPath = videoPath;
    job.subtitlePath = assPath;
    job.chapterMode = ChapterMode::AutomaticFromAss;
    job.fontsDirectory.clear();
    job.outputDirectory = outputDirectory;
    job.tag = QStringLiteral("TEST");
    job.anime = QStringLiteral("Chapter integration");
    job.episode = QStringLiteral("01");
    job.episodeName = QStringLiteral("ASS chapters");
    job.source = QStringLiteral("BD");
    job.generateJson = false;
    job.generateThumbnail = false;

    MuxWorker worker(job);
    QEventLoop loop;
    QString outputPath;
    QString error;
    QString chapterSummary;
    QObject::connect(&worker, &MuxWorker::logLine, &loop,
                     [](const QString &line) { QTextStream(stdout) << line << Qt::endl; },
                     Qt::QueuedConnection);
    QObject::connect(&worker, &MuxWorker::chaptersSummaryFound, &loop,
                     [&chapterSummary](const QString &summary) { chapterSummary = summary; },
                     Qt::QueuedConnection);
    QObject::connect(&worker, &MuxWorker::succeeded, &loop,
                     [&loop, &outputPath](const QString &mkvPath, const QString &,
                                          const QString &, const QString &, int) {
                         outputPath = mkvPath;
                         loop.quit();
                     }, Qt::QueuedConnection);
    QObject::connect(&worker, &MuxWorker::failed, &loop,
                     [&loop, &error](const QString &message) {
                         error = message;
                         loop.quit();
                     }, Qt::QueuedConnection);
    QObject::connect(&worker, &MuxWorker::cancelled, &loop,
                     [&loop, &error] {
                         error = QStringLiteral("mux de integração cancelado");
                         loop.quit();
                     }, Qt::QueuedConnection);

    worker.start();
    loop.exec();
    worker.wait();
    if (!error.isEmpty()) {
        QTextStream(stderr) << error << Qt::endl;
        return 5;
    }
    QTextStream(stdout) << "SUMMARY=" << chapterSummary << Qt::endl;
    QTextStream(stdout) << "MKV=" << outputPath << Qt::endl;
    Q_UNUSED(app);
    return 0;
}

void testExampleVariantsAndCleaning()
{
    QString ass = QStringLiteral(
        "[Script Info]\n"
        "ScriptType: v4.00+\n"
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n")
        + eventLine(QStringLiteral("CAP"), QStringLiteral("0:00:00.00"),
                    QStringLiteral("  {\\i1}Prólogo\\N Abertura   ")) + QChar(u'\n')
        + eventLine(QStringLiteral(" cap "), QStringLiteral("0:00:58.04"),
                    QStringLiteral("OP, primeira parte")) + QChar(u'\n')
        + eventLine(QStringLiteral("Capitulo"), QStringLiteral("0:01:20.00"),
                    QStringLiteral("Sem acento")) + QChar(u'\n')
        + eventLine(QStringLiteral("Capítulo"), QStringLiteral("0:02:28.00"),
                    QStringLiteral("Episódio 03")) + QChar(u'\n')
        + eventLine(QStringLiteral("cHaP"), QStringLiteral("0:04:00.12"),
                    QStringLiteral("Intervalo")) + QChar(u'\n')
        + eventLine(QStringLiteral("Chapter"), QStringLiteral("0:22:27.41"),
                    QStringLiteral("ED\\n Créditos")) + QChar(u'\n')
        + QStringLiteral("Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,CAP,diálogo ignorado\n")
        + eventLine(QStringLiteral("capture"), QStringLiteral("0:00:03.00"),
                    QStringLiteral("Effect parecido ignorado")) + QChar(u'\n')
        + eventLine(QStringLiteral("Chapters"), QStringLiteral("0:00:04.00"),
                    QStringLiteral("Plural ignorado")) + QChar(u'\n')
        + eventLine(QStringLiteral(""), QStringLiteral("0:00:05.00"),
                    QStringLiteral("Sem Effect ignorado")) + QChar(u'\n')
        + eventLine(QStringLiteral("\\pos(20,30)"), QStringLiteral("0:00:06.00"),
                    QStringLiteral("Typeset ignorado"));

    QByteArray bytes = QByteArray::fromHex("efbbbf") + ass.toUtf8();
    bytes.replace("\n", "\r\n");
    const AssChapterParseResult parsed = parseAssChapters(bytes);
    expect(parsed.error.isEmpty(), QStringLiteral("UTF-8 BOM e CRLF devem ser aceitos"));
    expect(parsed.chapters.size() == 6,
           QStringLiteral("CAP, cap, Capitulo, Capítulo, chap e Chapter devem ser aceitos; outros eventos ignorados"));
    const QStringList expectedTimes = {
        QStringLiteral("00:00:00.000"), QStringLiteral("00:00:58.040"),
        QStringLiteral("00:01:20.000"), QStringLiteral("00:02:28.000"),
        QStringLiteral("00:04:00.120"), QStringLiteral("00:22:27.410")};
    if (parsed.chapters.size() == expectedTimes.size()) {
        for (qsizetype i = 0; i < expectedTimes.size(); ++i)
            expect(parsed.chapters.at(i).timestamp == expectedTimes.at(i),
                   QStringLiteral("ordenação e conversão de centésimos no capítulo %1").arg(i + 1));
        expect(parsed.chapters.at(0).name == QStringLiteral("Prólogo Abertura"),
               QStringLiteral("tags de override e \\N devem ser limpos (valor: %1)").arg(parsed.chapters.at(0).name));
        expect(parsed.chapters.at(1).name == QStringLiteral("OP, primeira parte"),
               QStringLiteral("vírgulas do campo Text devem ser preservadas"));
        expect(parsed.chapters.at(2).name == QStringLiteral("Sem acento"),
               QStringLiteral("Effect Capitulo sem acento deve ser aceito"));
        expect(parsed.chapters.at(5).name == QStringLiteral("ED Créditos"),
               QStringLiteral("\\n minúsculo e espaços nas pontas devem ser normalizados"));
    }

    const QByteArray expectedOutput =
        "CHAPTER01=00:00:00.000\nCHAPTER01NAME=Pr\xC3\xB3logo Abertura\n"
        "CHAPTER02=00:00:58.040\nCHAPTER02NAME=OP, primeira parte\n";
    const QByteArray serialized = serializeMkvmergeChapters(parsed.chapters.mid(0, 2));
    expect(serialized == expectedOutput,
           QStringLiteral("saída deve manter o formato CHAPTERxx e precisão em milissegundos (valor: %1)")
               .arg(QString::fromUtf8(serialized)));
}

void testTextColumnCanAppearBeforeTrailingFields()
{
    const QString ass = QStringLiteral(
        "[Events]\n"
        "Format: Text, Layer, Start, End, Style, Effect\n"
        "Comment: título, com vírgulas,0,0:00:00.00,0:00:01.00,Default,CAP\n");
    const AssChapterParseResult parsed = parseAssChapters(ass.toUtf8());
    expect(parsed.chapters.size() == 1,
           QStringLiteral("Format deve localizar os campos por nome, mesmo com Text fora da última coluna"));
    if (parsed.chapters.size() == 1)
        expect(parsed.chapters.first().name == QStringLiteral("título, com vírgulas"),
               QStringLiteral("Text com vírgulas antes de outras colunas deve permanecer intacto"));
}

void testWarningsAndDuplicateRetention()
{
    const QString ass = QStringLiteral(
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n")
        + eventLine(QStringLiteral("CAP"), QStringLiteral("0:00:01.00"), QStringLiteral("Primeiro")) + QChar(u'\n')
        + eventLine(QStringLiteral("CAP"), QStringLiteral("0:00:01.00"), QStringLiteral("Duplicado")) + QChar(u'\n')
        + eventLine(QStringLiteral("CAP"), QStringLiteral("0:00:02.00"), QStringLiteral("   ")) + QChar(u'\n')
        + eventLine(QStringLiteral("CAP"), QStringLiteral("0:99:00.00"), QStringLiteral("Tempo inválido"));
    const AssChapterParseResult parsed = parseAssChapters(ass.toUtf8());
    expect(parsed.chapters.size() == 2, QStringLiteral("tempo duplicado mantém ambos e linhas inválidas/vazias são ignoradas"));
    bool duplicateWarning = false;
    bool firstStartWarning = false;
    bool emptyNameWarning = false;
    bool invalidTimeWarning = false;
    for (const QString &warning : parsed.warnings) {
        duplicateWarning |= warning.contains(QStringLiteral("duplicados"), Qt::CaseInsensitive);
        firstStartWarning |= warning.contains(QStringLiteral("nenhum capítulo será inventado"), Qt::CaseInsensitive);
        emptyNameWarning |= warning.contains(QStringLiteral("nome vazio"), Qt::CaseInsensitive);
        invalidTimeWarning |= warning.contains(QStringLiteral("Start fora do intervalo"), Qt::CaseInsensitive);
    }
    expect(duplicateWarning, QStringLiteral("tempos duplicados devem gerar WARNING"));
    expect(firstStartWarning, QStringLiteral("primeiro tempo diferente de zero deve gerar WARNING sem capítulo inventado"));
    expect(emptyNameWarning, QStringLiteral("nome vazio deve gerar WARNING"));
    expect(invalidTimeWarning, QStringLiteral("tempo inválido deve gerar WARNING"));
}

void testFallbackSelection()
{
    expect(selectChapterInput(ChapterMode::AutomaticFromAss, true, true) == ChapterInputKind::Ass,
           QStringLiteral("legenda tem prioridade quando contém capítulos, mesmo com .txt"));
    expect(selectChapterInput(ChapterMode::AutomaticFromAss, false, true) == ChapterInputKind::TextFile,
           QStringLiteral("modo automático usa .txt como fallback quando ASS não tem capítulos"));
    expect(selectChapterInput(ChapterMode::AutomaticFromAss, false, false) == ChapterInputKind::None,
           QStringLiteral("modo automático segue sem capítulos se ASS e .txt não tiverem capítulos"));
    expect(selectChapterInput(ChapterMode::TextFile, true, true) == ChapterInputKind::TextFile,
           QStringLiteral("modo .txt usa apenas o arquivo selecionado"));
    expect(selectChapterInput(ChapterMode::None, true, true) == ChapterInputKind::None,
           QStringLiteral("modo sem capítulos ignora ASS e .txt"));
}

void testInvalidUtf8()
{
    const AssChapterParseResult parsed = parseAssChapters(QByteArray::fromHex("efbbbf5b4576656e74735d0aff"));
    expect(!parsed.error.isEmpty(), QStringLiteral("bytes que não são UTF-8 devem retornar erro explícito"));
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() == 4 && args.at(1) == QStringLiteral("--export-chapters"))
        return exportChapters(args.at(2), args.at(3));
    if (args.size() == 5 && args.at(1) == QStringLiteral("--mux-integration"))
        return runMuxIntegration(app, args.at(2), args.at(3), args.at(4));

    testExampleVariantsAndCleaning();
    testTextColumnCanAppearBeforeTrailingFields();
    testWarningsAndDuplicateRetention();
    testFallbackSelection();
    testInvalidUtf8();
    if (failures == 0) {
        QTextStream(stdout) << "Todos os testes do parser de capítulos passaram." << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << failures << " teste(s) falharam." << Qt::endl;
    return 1;
}
