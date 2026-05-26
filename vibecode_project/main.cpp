#include "ai_project.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ai_project window;
    window.show();

    return app.exec();
}