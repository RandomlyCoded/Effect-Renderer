#include "previewwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    randomly::PreviewWindow w;
    w.show();
    return a.exec();
}
