#include "renderer.h"
#include "recorder.h"

#include <QElapsedTimer>
#include <QLoggingCategory>

namespace randomly {

Q_LOGGING_CATEGORY(lcRenderer, "randomly.Renderer")

Renderer::Renderer(QObject *parent, Recorder *recorder, QSize size, quint64 _framesToRender, uint seed)
    : QObject{parent}
    , m_size(size)
    , m_recorder(recorder)
    , m_noise(PerlinNoise(seed))
    , framesToRender(_framesToRender)
{
    m_renderTimer.start();
}

void Renderer::render()
{
    if (currentFrame == framesToRender) {
        m_recorder->stop();
        qCInfo(lcRenderer) << "Rendering done!" << m_renderTimer.elapsed() << "ms total";
        return;
    }

    qCInfo(lcRenderer) << "rendering" << currentFrame << "/" << framesToRender;

    QElapsedTimer timing;
    timing.start();

    QImage img(m_size.width(), m_size.height(), QImage::Format::Format_ARGB32);

    // fill each pixel with perlin noise
    for (int x = 0; x < img.width(); ++x) {
        for (int y = 0; y < img.height(); ++y) {
            auto v = m_noise.noise(x * scale, y * scale, z);

            v *= 256;
            auto clr = qRgb(v, v, v);

            img.setPixel(x, y, clr);
        }
    }

    // properly init the frame
    m_vframe = QVideoFrame(img);
    m_vframe.setStartTime(frameTime);
    m_vframe.setEndTime(frameTime + frameDelay);

    // update tracking variables
    frameTime += frameDelay;
    ++currentFrame;
    z += scale;

    qCInfo(lcRenderer) << "rendering done in" << timing.elapsed() << "ms";

    emit frameRendered(m_vframe);
}

} // namespace randomly
