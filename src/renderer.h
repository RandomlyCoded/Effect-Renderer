#ifndef RENDERER_H
#define RENDERER_H

#include "../PerlinNoise/perlinnoise.h"

#include <QElapsedTimer>
#include <QObject>
#include <QVideoFrame>

namespace randomly {

class Recorder;

class Renderer : public QObject
{
    Q_OBJECT
public:
    explicit Renderer(QObject *parent = nullptr, Recorder *recorder = nullptr, QSize size = {}, quint64 _framesToRender = 60, uint seed = 0);

    void render();

signals:
    void frameRendered(QVideoFrame &frame);

private:
    QSize m_size;
    QVideoFrame m_vframe;
    Recorder *m_recorder;
    PerlinNoise m_noise;
    QElapsedTimer m_renderTimer;

    quint64 currentFrame = 0;
    const quint64 framesToRender;

    quint64 frameTime = 0;
    static constexpr quint64 frameDelay = 16667LL; // microseconds; around 60 FPS

    qreal z = 0;

    static constexpr qreal scale = 0.02;
};

} // namespace randomly

#endif // RENDERER_H
