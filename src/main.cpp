#include "ui/mainwindow.h"

#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WebM for Lazys"));
    QApplication::setApplicationDisplayName(QStringLiteral("WebM for Lazys"));
    QApplication::setOrganizationName(QStringLiteral("webmforlazies"));
    QApplication::setApplicationVersion(QStringLiteral("0.2.0"));

    wc::MainWindow window;
    window.show();
    return app.exec();
}
