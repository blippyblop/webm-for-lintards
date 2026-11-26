#include "ui/mainwindow.h"

#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WebM for Lintards"));
    QApplication::setApplicationDisplayName(QStringLiteral("WebM for Lintards"));
    QApplication::setOrganizationName(QStringLiteral("webmforlazies"));
    QApplication::setApplicationVersion(QStringLiteral("0.5.0"));

    wc::MainWindow window;
    window.show();
    return app.exec();
}
