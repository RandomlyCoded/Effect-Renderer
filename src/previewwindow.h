#ifndef PREVIEWWINDOW_H
#define PREVIEWWINDOW_H

#include "recorder.h"

#include <QMainWindow>
#include <QVideoWidget>

namespace randomly {

class PreviewWindow : public QMainWindow
{
    Q_OBJECT

public:
    PreviewWindow(QWidget *parent = nullptr);
    ~PreviewWindow();

private:
    QVideoWidget *m_video;

    Recorder *m_renderer;

    // QWidget interface
protected:
    void resizeEvent(QResizeEvent *event);
};

} // namespace randomly
#endif // PREVIEWWINDOW_H
