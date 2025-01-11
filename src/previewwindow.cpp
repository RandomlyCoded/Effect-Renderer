#include "previewwindow.h"

namespace randomly {

PreviewWindow::PreviewWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_renderer(new Recorder(this))
    , m_video(new QVideoWidget(this))
{
    resize(1920, 1080);
    m_renderer->setPreviewOutput(m_video);

    m_video->show();
}

PreviewWindow::~PreviewWindow() {}

void PreviewWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event); // default handling

    m_video->resize(size());
}

} // namespace randomly
