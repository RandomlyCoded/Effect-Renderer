#include "previewwindow.h"

#include "renderer.h"

#include <QApplication>

namespace randomly {

PreviewWindow::PreviewWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_recorder(new Recorder(this))
    , m_video(new QVideoWidget(this))
    , m_progress(new QProgressBar(nullptr)) // can't have it as a child since QVideoWidgets forces itself into the foreground
{
    resize(1920, 1080);
    m_recorder->setPreviewOutput(m_video);

    m_video->show();

    m_progress->setRange(0, m_recorder->renderer()->targetFrames());
    m_progress->setValue(0);
    m_progress->setFormat("%v / %m frames");
    m_progress->show();

    connect(m_recorder->renderer(), &Renderer::frameRendered, this, &PreviewWindow::updateProgress);
}

PreviewWindow::~PreviewWindow() {}

void PreviewWindow::updateProgress()
{
    m_progress->setValue(m_recorder->renderer()->framesRendered());
}

void PreviewWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event); // default handling

    m_video->resize(size());
}

void PreviewWindow::closeEvent(QCloseEvent *event)
{
    QApplication::exit(0);
}

} // namespace randomly
