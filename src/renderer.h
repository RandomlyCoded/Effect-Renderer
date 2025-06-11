#ifndef RENDERER_H
#define RENDERER_H

#include "../PerlinNoise/perlinnoise.h"

#include <QElapsedTimer>
#include <QObject>
#include <QRandomGenerator>
#include <QVideoFrame>

namespace randomly {

class Recorder;

template <typename T, int Size>
class Queue
{
public:
    Queue(T fill);

    constexpr auto length() const { return Size; }

    void next() { m_step = m_step ? (m_step - 1) : Size - 1; }
    T &get(int idx);

private:
    T m_data[Size];
    int m_step;
};

class Particle
{
public:
    static constexpr int maxLifetime = 8 * 60; // 8 seconds
    static constexpr qreal pStep = 4 * M_PI;

    static constexpr int queueSize = 128;

    Particle(QPointF pos, int lifetime);
    Particle(QPair<QPointF, int> data) : Particle(data.first, data.second) {}
    
    void tick(qreal direction, int w, int h);
    void reset(QPointF newPos, int newLifeTime);
    int lifeTime() const { return m_lifeTime; }
    QPointF pos() { return m_positions.get(0); }
    auto positions() { return m_positions; }
    int initialLifeTime() { return m_initialLifeTime; }

private:
    void changePos(QPointF newPos);

    Queue<QPointF, queueSize> m_positions;
    int m_lifeTime;
    int m_initialLifeTime;
};

struct RenderInfo
{
    QSize size = {1920, 1080};
    quint64 framesToRender = 60;
    bool saveFrames = false;
    uint seed = 0;
    int particleCount = 5000;
};

class Renderer : public QObject
{
    Q_OBJECT
public:
    explicit Renderer(QObject *parent = nullptr, Recorder *recorder = nullptr, const RenderInfo &info = {});

    void render();

    int width()  { return m_size.width();  }
    int height() { return m_size.height(); }

    int framesRendered() { return currentFrame; }
    int targetFrames() { return framesToRender; }

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

    bool m_saveFrames;
    QRandomGenerator *m_rng;

    qreal m_z = 0;

    static constexpr qreal scale = 0.002;

    QPair<QPointF, int> makeParticle();

    void updateParticles();
    QList<Particle> m_particles;
};

} // namespace randomly

#endif // RENDERER_H
