// SPDX-License-Identifier: GPL-3.0-or-later
#include "muxer_window.h"

#include <QApplication>
#include <QFile>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Musubi"));
    app.setOrganizationName(QStringLiteral("Crystal FanSub"));
    app.setStyle(QStringLiteral("Fusion"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/musubi-256.png")));
    QFile stylesheet(QStringLiteral(":/styles/musubi.qss"));
    if (stylesheet.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));

    MuxerWindow window;
    window.show();
    return app.exec();
}
