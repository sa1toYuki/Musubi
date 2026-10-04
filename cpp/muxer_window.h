// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mux_worker.h"

#include <QMainWindow>
#include <QPointer>
#include <QStringList>

class QCheckBox;
class QHBoxLayout;
class QComboBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTextEdit;
class QLabel;

class MuxerWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MuxerWindow(QWidget *parent = nullptr);
    ~MuxerWindow() override;

private slots:
    void startOrCancel();
    void onSuccess(const QString &mkvPath, const QString &jsonPath,
                   const QString &thumbnailPath, const QString &crc,
                   int elapsedSeconds);
    void onFailure(const QString &message);
    void onCancelled();

private:
    QWidget *makeFileRow(const QString &label, QLineEdit *&edit,
                         const QString &dialogTitle, const QString &filter,
                         bool directory = false);
    void appendLog(const QString &message);
    void setBusy(bool busy);
    void checkDependencies();
    void refreshLog();
    void saveLog();

    QLineEdit *videoEdit_ = nullptr;
    QLineEdit *subtitleEdit_ = nullptr;
    QLineEdit *chaptersEdit_ = nullptr;
    QWidget *chaptersRowWidget_ = nullptr;
    QComboBox *chapterSourceCombo_ = nullptr;
    QLineEdit *fontsEdit_ = nullptr;
    QLineEdit *outputEdit_ = nullptr;
    QLineEdit *tagEdit_ = nullptr;
    QLineEdit *animeEdit_ = nullptr;
    QLineEdit *episodeEdit_ = nullptr;
    QLineEdit *episodeNameEdit_ = nullptr;
    QLineEdit *thumbnailTimestampEdit_ = nullptr;
    QComboBox *sourceCombo_ = nullptr;
    QCheckBox *jsonCheck_ = nullptr;
    QCheckBox *thumbnailCheck_ = nullptr;
    QHBoxLayout *mediaBadgesLayout_ = nullptr;
    QLabel *chaptersSummaryLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QTextEdit *logEdit_ = nullptr;
    QLineEdit *logFilterEdit_ = nullptr;
    QCheckBox *pauseLogCheck_ = nullptr;
    QStringList logLines_;
    QPushButton *runButton_ = nullptr;
    QPointer<MuxWorker> worker_;
    bool busy_ = false;
};
