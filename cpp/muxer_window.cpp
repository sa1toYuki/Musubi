// SPDX-License-Identifier: GPL-3.0-or-later
#include "muxer_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QLabel>
#include <QTextCursor>
#include <QIcon>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QSplitter>
#include <QStyle>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace {
class PathLineEdit final : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QLineEdit::paintEvent(event);
        if (hasFocus() || text().isEmpty()) return;
        QPainter painter(this);
        const QRect textRect = contentsRect().adjusted(8, 1, -8, -1);
        painter.setClipRect(textRect);
        painter.fillRect(textRect, palette().base());
        painter.setPen(palette().text().color());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                         fontMetrics().elidedText(text(), Qt::ElideMiddle, textRect.width()));
        Q_UNUSED(event);
    }
};

QLineEdit *makeLineEdit(const QString &placeholder = {})
{
    auto *edit = new QLineEdit;
    edit->setPlaceholderText(placeholder);
    edit->setMinimumHeight(34);
    return edit;
}
}

MuxerWindow::MuxerWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Musubi"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/musubi-256.png")));
    resize(1120, 820);
    setMinimumSize(920, 760);

    auto *root = new QWidget(this);
    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->setSpacing(16);

    auto *header = new QWidget(root);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    auto *icon = new QLabel(header);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/musubi-48.png")).pixmap(40, 40));
    auto *heading = new QWidget(header);
    auto *headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(QStringLiteral("Musubi"), heading);
    QFont titleFont = title->font();
    titleFont.setPointSize(19);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto *subtitle = new QLabel(QStringLiteral("Multiplexador MKV"), heading);
    subtitle->setProperty("secondary", true);
    headingLayout->addWidget(title);
    headingLayout->addWidget(subtitle);
    headerLayout->addWidget(icon);
    headerLayout->addWidget(heading);
    headerLayout->addStretch();
    outer->addWidget(header);

    auto *panels = new QGridLayout;
    panels->setHorizontalSpacing(16);
    panels->setVerticalSpacing(8);

    auto *filesGroup = new QGroupBox(QStringLiteral("Arquivos de entrada"), root);
    auto *filesLayout = new QGridLayout(filesGroup);
    filesLayout->setColumnStretch(0, 1);
    filesLayout->addWidget(makeFileRow(QStringLiteral("Vídeo (.mkv)"), videoEdit_,
                                      QStringLiteral("Selecione o vídeo MKV"), QStringLiteral("MKV (*.mkv)")), 0, 0);
    filesLayout->addWidget(makeFileRow(QStringLiteral("Legenda (.ass)"), subtitleEdit_,
                                      QStringLiteral("Selecione a legenda ASS"), QStringLiteral("ASS (*.ass)")), 1, 0);
    chaptersRowWidget_ = makeFileRow(QStringLiteral("Capítulos (.txt) · usado só se a legenda não tiver"),
                                     chaptersEdit_, QStringLiteral("Selecione o arquivo de capítulos"),
                                     QStringLiteral("Texto (*.txt)"));
    filesLayout->addWidget(chaptersRowWidget_, 2, 0);
    auto *chapterOptions = new QWidget(filesGroup);
    auto *chapterOptionsLayout = new QVBoxLayout(chapterOptions);
    chapterOptionsLayout->setContentsMargins(0, 4, 0, 4);
    chapterOptionsLayout->setSpacing(8);
    auto *chapterSelectorRow = new QWidget(chapterOptions);
    auto *chapterSelectorLayout = new QHBoxLayout(chapterSelectorRow);
    chapterSelectorLayout->setContentsMargins(0, 0, 0, 0);
    auto *chapterSourceLabel = new QLabel(QStringLiteral("Origem dos capítulos"), chapterOptions);
    chapterSourceLabel->setFixedWidth(140);
    chapterSourceLabel->setProperty("secondary", true);
    chapterSourceCombo_ = new QComboBox(chapterOptions);
    chapterSourceCombo_->addItem(QStringLiteral("Automático pela legenda"),
                                 static_cast<int>(ChapterMode::AutomaticFromAss));
    chapterSourceCombo_->addItem(QStringLiteral("Arquivo .txt"),
                                 static_cast<int>(ChapterMode::TextFile));
    chapterSourceCombo_->addItem(QStringLiteral("Sem capítulos"),
                                 static_cast<int>(ChapterMode::None));
    chapterSourceCombo_->setToolTip(QStringLiteral("No modo automático, a legenda tem prioridade e o .txt é usado como fallback."));
    chaptersSummaryLabel_ = new QLabel(QStringLiteral("A legenda será verificada automaticamente."), chapterOptions);
    chaptersSummaryLabel_->setObjectName(QStringLiteral("chapterBadge"));
    chaptersSummaryLabel_->setWordWrap(true);
    chapterSelectorLayout->addWidget(chapterSourceLabel);
    chapterSelectorLayout->addWidget(chapterSourceCombo_, 1);
    chapterOptionsLayout->addWidget(chapterSelectorRow);
    chapterOptionsLayout->addWidget(chaptersSummaryLabel_);
    filesLayout->addWidget(chapterOptions, 3, 0);
    connect(chapterSourceCombo_, &QComboBox::currentIndexChanged, this, [this] {
        const ChapterMode mode = static_cast<ChapterMode>(chapterSourceCombo_->currentData().toInt());
        chaptersRowWidget_->setVisible(mode != ChapterMode::None);
    });
    filesLayout->addWidget(makeFileRow(QStringLiteral("Fontes (pasta)"), fontsEdit_,
                                      QStringLiteral("Selecione a pasta de fontes"), {}, true), 4, 0);
    filesLayout->addWidget(makeFileRow(QStringLiteral("Pasta de saída"), outputEdit_,
                                      QStringLiteral("Selecione a pasta de saída"), {}, true), 5, 0);
    panels->addWidget(filesGroup, 0, 0);

    auto *metaGroup = new QGroupBox(QStringLiteral("Informações do episódio"), root);
    auto *meta = new QFormLayout(metaGroup);
    tagEdit_ = makeLineEdit();
    tagEdit_->setText(QStringLiteral("CFSB"));
    animeEdit_ = makeLineEdit();
    episodeEdit_ = makeLineEdit();
    episodeNameEdit_ = makeLineEdit();
    meta->addRow(QStringLiteral("Tag da fansub"), tagEdit_);
    meta->addRow(QStringLiteral("Nome do anime"), animeEdit_);
    meta->addRow(QStringLiteral("Número do episódio"), episodeEdit_);
    meta->addRow(QStringLiteral("Nome do episódio"), episodeNameEdit_);
    sourceCombo_ = new QComboBox(metaGroup);
    sourceCombo_->addItems({QStringLiteral("BD"), QStringLiteral("WEB-DL"), QStringLiteral("TV"),
                            QStringLiteral("DVD"), QStringLiteral("HDTV")});
    sourceCombo_->setCurrentText(QStringLiteral("WEB-DL"));
    meta->addRow(QStringLiteral("Source"), sourceCombo_);

    auto *options = new QWidget(metaGroup);
    auto *optionsLayout = new QVBoxLayout(options);
    optionsLayout->setContentsMargins(0, 4, 0, 4);
    jsonCheck_ = new QCheckBox(QStringLiteral("Gerar arquivo .json com metadados"), options);
    jsonCheck_->setChecked(true);
    optionsLayout->addWidget(jsonCheck_);
    auto *thumbRow = new QHBoxLayout;
    thumbnailCheck_ = new QCheckBox(QStringLiteral("Gerar thumbnail .webp"), options);
    thumbnailTimestampEdit_ = makeLineEdit(QStringLiteral("00:00:30"));
    thumbnailTimestampEdit_->setText(QStringLiteral("00:00:30"));
    thumbnailTimestampEdit_->setMaximumWidth(130);
    thumbnailTimestampEdit_->setEnabled(false);
    connect(thumbnailCheck_, &QCheckBox::toggled, thumbnailTimestampEdit_, &QLineEdit::setEnabled);
    thumbRow->addWidget(thumbnailCheck_);
    thumbRow->addStretch();
    thumbRow->addWidget(thumbnailTimestampEdit_);
    optionsLayout->addLayout(thumbRow);
    meta->addRow(QStringLiteral("Opções de saída"), options);
    meta->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    for (QWidget *field : {static_cast<QWidget *>(tagEdit_), static_cast<QWidget *>(animeEdit_),
                           static_cast<QWidget *>(episodeEdit_), static_cast<QWidget *>(episodeNameEdit_),
                           static_cast<QWidget *>(sourceCombo_), static_cast<QWidget *>(options)}) {
        if (auto *label = qobject_cast<QLabel *>(meta->labelForField(field))) {
            label->setFixedWidth(140);
            label->setProperty("secondary", true);
        }
    }
    auto *mediaBadges = new QWidget(metaGroup);
    mediaBadgesLayout_ = new QHBoxLayout(mediaBadges);
    mediaBadgesLayout_->setContentsMargins(0, 0, 0, 0);
    mediaBadgesLayout_->setSpacing(8);
    auto *mediaPlaceholder = new QLabel(QStringLiteral("Mídia ainda não analisada"), mediaBadges);
    mediaPlaceholder->setObjectName(QStringLiteral("mediaBadge"));
    mediaBadgesLayout_->addWidget(mediaPlaceholder);
    mediaBadgesLayout_->addStretch();
    meta->addRow(mediaBadges);
    panels->addWidget(metaGroup, 0, 1);
    panels->setColumnStretch(0, 1);
    panels->setColumnStretch(1, 1);
    auto *panelsWidget = new QWidget(root);
    panelsWidget->setLayout(panels);
    auto *panelsScrollArea = new QScrollArea(root);
    panelsScrollArea->setWidgetResizable(true);
    panelsScrollArea->setFrameShape(QFrame::NoFrame);
    panelsScrollArea->setWidget(panelsWidget);

    auto *logGroup = new QGroupBox(QStringLiteral("Log"), root);
    auto *logLayout = new QVBoxLayout(logGroup);
    auto *logTools = new QHBoxLayout;
    logFilterEdit_ = makeLineEdit(QStringLiteral("Filtrar log…"));
    logFilterEdit_->setMaximumWidth(260);
    pauseLogCheck_ = new QCheckBox(QStringLiteral("Pausar"), logGroup);
    auto *clearLogButton = new QPushButton(QStringLiteral("Limpar"), logGroup);
    auto *saveLogButton = new QPushButton(QStringLiteral("Salvar"), logGroup);
    logTools->addWidget(logFilterEdit_);
    logTools->addWidget(pauseLogCheck_);
    logTools->addStretch();
    logTools->addWidget(clearLogButton);
    logTools->addWidget(saveLogButton);
    logLayout->addLayout(logTools);
    logEdit_ = new QTextEdit(logGroup);
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(130);
    logEdit_->setObjectName(QStringLiteral("logEdit"));
    logEdit_->setFont(QFont(QStringLiteral("monospace"), 9));
    logLayout->addWidget(logEdit_);
    connect(logFilterEdit_, &QLineEdit::textChanged, this, &MuxerWindow::refreshLog);
    connect(pauseLogCheck_, &QCheckBox::toggled, this, [this](bool paused) {
        if (!paused) refreshLog();
    });
    connect(clearLogButton, &QPushButton::clicked, this, [this] {
        logLines_.clear();
        logEdit_->clear();
    });
    connect(saveLogButton, &QPushButton::clicked, this, &MuxerWindow::saveLog);
    auto *contentSplitter = new QSplitter(Qt::Vertical, root);
    contentSplitter->addWidget(panelsScrollArea);
    contentSplitter->addWidget(logGroup);
    contentSplitter->setStretchFactor(0, 3);
    contentSplitter->setStretchFactor(1, 1);
    contentSplitter->setSizes({560, 220});
    outer->addWidget(contentSplitter, 1);

    statusLabel_ = new QLabel(QStringLiteral("Pronto."), root);
    progressBar_ = new QProgressBar(root);
    progressBar_->setRange(0, 1000);
    progressBar_->setValue(0);
    progressBar_->hide();
    runButton_ = new QPushButton(QStringLiteral("Multiplexar"), root);
    runButton_->setObjectName(QStringLiteral("runButton"));
    runButton_->setMinimumHeight(44);
    outer->addWidget(statusLabel_);
    outer->addWidget(progressBar_);
    outer->addWidget(runButton_);
    setCentralWidget(root);

    connect(runButton_, &QPushButton::clicked, this, &MuxerWindow::startOrCancel);
    checkDependencies();
}

MuxerWindow::~MuxerWindow()
{
    if (worker_ && worker_->isRunning()) {
        worker_->cancel();
        worker_->wait();
    }
}

QWidget *MuxerWindow::makeFileRow(const QString &label, QLineEdit *&edit,
                                  const QString &dialogTitle, const QString &filter,
                                  bool directory)
{
    auto *row = new QWidget(this);
    auto *layout = new QGridLayout(row);
    layout->setContentsMargins(0, 4, 0, 4);
    auto *caption = new QLabel(label, row);
    caption->setFixedWidth(140);
    caption->setProperty("secondary", true);
    if (edit == chaptersEdit_) {
        caption->setWordWrap(true);
        row->setMinimumHeight(54);
    }
    edit = new PathLineEdit;
    edit->setPlaceholderText(directory ? QStringLiteral("Opcional") : QStringLiteral("Selecione o arquivo"));
    edit->setMinimumHeight(34);
    connect(edit, &QLineEdit::textChanged, edit, [edit](const QString &path) { edit->setToolTip(path); });
    auto *button = new QPushButton(QStringLiteral("Procurar"), row);
    button->setMinimumWidth(88);
    layout->addWidget(caption, 0, 0);
    layout->addWidget(edit, 0, 1);
    layout->addWidget(button, 0, 2);
    layout->setColumnStretch(1, 1);

    connect(button, &QPushButton::clicked, this, [this, edit, dialogTitle, filter, directory] {
        const QString picked = directory
            ? QFileDialog::getExistingDirectory(this, dialogTitle)
            : QFileDialog::getOpenFileName(this, dialogTitle, {}, filter);
        if (!picked.isEmpty()) {
            edit->setText(picked);
            if (edit == videoEdit_ && outputEdit_->text().trimmed().isEmpty())
                outputEdit_->setText(QFileInfo(picked).absolutePath());
        }
    });
    return row;
}

void MuxerWindow::appendLog(const QString &message)
{
    logLines_.append(message);
    if (!pauseLogCheck_->isChecked()) refreshLog();
}

void MuxerWindow::refreshLog()
{
    if (pauseLogCheck_ && pauseLogCheck_->isChecked()) return;
    logEdit_->clear();
    const QString filter = logFilterEdit_ ? logFilterEdit_->text().trimmed() : QString();
    for (const QString &line : logLines_) {
        if (!filter.isEmpty() && !line.contains(filter, Qt::CaseInsensitive)) continue;
        QString color = QStringLiteral("#E2E8F0");
        if (line.contains(QStringLiteral("ERRO"), Qt::CaseInsensitive) ||
            line.contains(QStringLiteral("Falha"), Qt::CaseInsensitive)) color = QStringLiteral("#EF4444");
        else if (line.contains(QStringLiteral("Aviso"), Qt::CaseInsensitive) ||
                 line.contains(QStringLiteral("WARNING"), Qt::CaseInsensitive)) color = QStringLiteral("#F59E0B");
        else if (line.contains(QStringLiteral("sucesso"), Qt::CaseInsensitive) ||
                 line.contains(QStringLiteral("Concluído"), Qt::CaseInsensitive)) color = QStringLiteral("#22C55E");
        logEdit_->append(QStringLiteral("<span style=\"color:%1\">%2</span>")
                             .arg(color, line.toHtmlEscaped()));
    }
}

void MuxerWindow::saveLog()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Salvar log"),
                                                       QStringLiteral("Musubi.log"),
                                                       QStringLiteral("Log (*.log);;Texto (*.txt)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Não foi possível salvar"), file.errorString());
        return;
    }
    file.write(logLines_.join(QChar(u'\n')).toUtf8());
}

void MuxerWindow::setBusy(bool busy)
{
    busy_ = busy;
    runButton_->setText(busy ? QStringLiteral("Cancelar") : QStringLiteral("Multiplexar"));
    progressBar_->setVisible(busy);
    if (busy) {
        progressBar_->setRange(0, 0);
    } else {
        progressBar_->setRange(0, 1000);
        progressBar_->setValue(0);
    }
    for (QWidget *widget : {static_cast<QWidget *>(videoEdit_), static_cast<QWidget *>(subtitleEdit_),
                            static_cast<QWidget *>(chaptersEdit_), static_cast<QWidget *>(chapterSourceCombo_),
                            static_cast<QWidget *>(fontsEdit_),
                            static_cast<QWidget *>(outputEdit_), static_cast<QWidget *>(tagEdit_),
                            static_cast<QWidget *>(animeEdit_), static_cast<QWidget *>(episodeEdit_),
                            static_cast<QWidget *>(episodeNameEdit_), static_cast<QWidget *>(sourceCombo_),
                            static_cast<QWidget *>(jsonCheck_), static_cast<QWidget *>(thumbnailCheck_),
                            static_cast<QWidget *>(thumbnailTimestampEdit_)})
        widget->setEnabled(!busy);
}

void MuxerWindow::checkDependencies()
{
    appendLog(QStringLiteral("Verificando dependências externas..."));
    const QStringList required = {QStringLiteral("mkvmerge"), QStringLiteral("ffmpeg"), QStringLiteral("ffprobe")};
    QStringList missing;
    for (const QString &name : required) {
        QString program = name;
#ifdef Q_OS_WIN
        program += QStringLiteral(".exe");
#endif
        if (QStandardPaths::findExecutable(program).isEmpty()) missing.append(name);
    }
    QString mkvinfo = QStringLiteral("mkvinfo");
#ifdef Q_OS_WIN
    mkvinfo += QStringLiteral(".exe");
#endif
    if (QStandardPaths::findExecutable(mkvinfo).isEmpty())
        appendLog(QStringLiteral("Aviso: mkvinfo não encontrado (opcional; não é executado)."));
    if (missing.isEmpty()) {
        appendLog(QStringLiteral("Dependências obrigatórias encontradas."));
    } else {
        appendLog(QStringLiteral("ERRO: não encontrados: %1. Instale MKVToolNix e FFmpeg e adicione ao PATH.")
                       .arg(missing.join(QStringLiteral(", "))));
        statusLabel_->setText(QStringLiteral("Dependências obrigatórias faltando."));
    }
}

void MuxerWindow::startOrCancel()
{
    if (busy_) {
        if (worker_) worker_->cancel();
        statusLabel_->setText(QStringLiteral("Cancelando após interromper a tarefa atual..."));
        return;
    }

    const QString video = videoEdit_->text().trimmed();
    const QString subtitle = subtitleEdit_->text().trimmed();
    const QString chapters = chaptersEdit_->text().trimmed();
    if (video.isEmpty() || subtitle.isEmpty() ||
        animeEdit_->text().trimmed().isEmpty() || episodeEdit_->text().trimmed().isEmpty() ||
        episodeNameEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Campos incompletos"),
                             QStringLiteral("Informe vídeo, legenda, anime, episódio e nome do episódio."));
        return;
    }

    MuxJob job;
    job.videoPath = video;
    job.subtitlePath = subtitle;
    job.chaptersPath = chapters;
    job.chapterMode = static_cast<ChapterMode>(chapterSourceCombo_->currentData().toInt());
    job.fontsDirectory = fontsEdit_->text().trimmed();
    job.outputDirectory = outputEdit_->text().trimmed();
    job.tag = tagEdit_->text();
    job.anime = animeEdit_->text();
    job.episode = episodeEdit_->text();
    job.episodeName = episodeNameEdit_->text();
    job.source = sourceCombo_->currentText();
    job.thumbnailTimestamp = thumbnailTimestampEdit_->text().trimmed();
    job.generateJson = jsonCheck_->isChecked();
    job.generateThumbnail = thumbnailCheck_->isChecked();

    worker_ = new MuxWorker(job, this);
    connect(worker_, &MuxWorker::logLine, this, &MuxerWindow::appendLog);
    connect(worker_, &MuxWorker::statusChanged, statusLabel_, &QLabel::setText);
    connect(worker_, &MuxWorker::mediaInfoFound, this, [this](const QString &summary) {
        while (QLayoutItem *item = mediaBadgesLayout_->takeAt(0)) {
            if (QWidget *widget = item->widget()) widget->deleteLater();
            delete item;
        }
        for (const QString &part : summary.split(QStringLiteral(" • "), Qt::SkipEmptyParts)) {
            auto *badge = new QLabel(part, mediaBadgesLayout_->parentWidget());
            badge->setObjectName(QStringLiteral("mediaBadge"));
            mediaBadgesLayout_->addWidget(badge);
        }
        mediaBadgesLayout_->addStretch();
    });
    connect(worker_, &MuxWorker::chaptersSummaryFound, this, [this](const QString &summary) {
        chaptersSummaryLabel_->setText(summary);
        const bool found = summary.contains(QStringLiteral("encontrados"), Qt::CaseInsensitive) ||
                           summary.contains(QStringLiteral("arquivo .txt"), Qt::CaseInsensitive);
        chaptersSummaryLabel_->setProperty("tone", found ? QStringLiteral("success") : QStringLiteral("warning"));
        chaptersSummaryLabel_->style()->unpolish(chaptersSummaryLabel_);
        chaptersSummaryLabel_->style()->polish(chaptersSummaryLabel_);
    });
    connect(worker_, &MuxWorker::crcProgress, this, [this](qint64 completed, qint64 total) {
        progressBar_->setRange(0, 1000);
        progressBar_->setValue(total > 0 ? static_cast<int>((completed * 1000) / total) : 1000);
    });
    connect(worker_, &MuxWorker::succeeded, this, &MuxerWindow::onSuccess);
    connect(worker_, &MuxWorker::failed, this, &MuxerWindow::onFailure);
    connect(worker_, &MuxWorker::cancelled, this, &MuxerWindow::onCancelled);
    connect(worker_, &QThread::finished, worker_, &QObject::deleteLater);
    setBusy(true);
    statusLabel_->setText(QStringLiteral("Iniciando multiplexação..."));
    appendLog(QString(48, QChar(u'─')));
    appendLog(QStringLiteral("Iniciando multiplexação..."));
    worker_->start();
}

void MuxerWindow::onSuccess(const QString &mkvPath, const QString &jsonPath,
                            const QString &thumbnailPath, const QString &crc,
                            int elapsedSeconds)
{
    setBusy(false);
    appendLog(QStringLiteral("MKV gerado: %1").arg(QFileInfo(mkvPath).fileName()));
    if (!jsonPath.isEmpty()) appendLog(QStringLiteral("JSON gerado: %1").arg(QFileInfo(jsonPath).fileName()));
    if (!thumbnailPath.isEmpty()) appendLog(QStringLiteral("Thumbnail gerada: %1").arg(QFileInfo(thumbnailPath).fileName()));
    const int minutes = elapsedSeconds / 60;
    const int seconds = elapsedSeconds % 60;
    appendLog(QStringLiteral("CRC-32: %1").arg(crc));
    appendLog(QStringLiteral("Concluído em %1m %2s").arg(minutes).arg(seconds));
    statusLabel_->setText(QStringLiteral("Episódio gerado com sucesso!"));
    QMessageBox::information(this, QStringLiteral("Sucesso"),
                             QStringLiteral("Arquivo gerado:\n%1").arg(QFileInfo(mkvPath).fileName()));
}

void MuxerWindow::onFailure(const QString &message)
{
    setBusy(false);
    appendLog(QStringLiteral("Falha: %1").arg(message));
    statusLabel_->setText(QStringLiteral("Falha na multiplexação."));
    QMessageBox::critical(this, QStringLiteral("Erro"), message);
}

void MuxerWindow::onCancelled()
{
    setBusy(false);
    appendLog(QStringLiteral("Operação cancelada."));
    statusLabel_->setText(QStringLiteral("Operação cancelada."));
}
