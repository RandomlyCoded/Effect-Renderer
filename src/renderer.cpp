#include "renderer.h"
#include "recorder.h"

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QPainter>
#include <QRandomGenerator>
#include <qthread.h>

namespace randomly {

using namespace std::chrono_literals;

namespace
{

inline qreal lerp(qreal a, qreal b, qreal t)
{
    return (b - a) * t + a;
}

// 0 is max foreground, Particle::queueSize is all background
int getAgeInOldTrail(int i, int offset, int len)
{
    // if the current position is within the first half (len >> 1 ~ len/2)
    if (i - offset < (len >> 1)) {
        return Particle::queueSize - (i - offset);
    }

    // otherwise, use normal fading
    return i;
}

inline QPoint clampPositionToImage(QPointF p, int w, int h)
{
    return {
        std::clamp(int(p.x()), 0, w - 1),
        std::clamp(int(p.y()), 0, h - 1)
    };
}

int getAgeOfPosition(int i, int lifeTime, int initialLifeTime)
{
    if (lifeTime + i > initialLifeTime) { // previous generation
        // Particle::queueSize - (initialLifeTime - lifeTime) old generation elements
        const auto offset = initialLifeTime - lifeTime;
        const int len = Particle::queueSize - offset;

        return getAgeInOldTrail(i, offset, len);
    }

    // current generation
    if ((lifeTime + i) < (Particle::queueSize >> 1)) { // nearing the end of the life cycle
        // Particle::queueSize - lifeTime elements in this segment
        const int len = Particle::queueSize - lifeTime;
        // if (len < Particle::queueSize >> 1) // less than half the elements are fading
        // return Particle::queueSize - i - lifeTime;
        // return i + len;
        return len - i - lifeTime;
    }

    return i;
}

} // namespace

Q_LOGGING_CATEGORY(lcRenderer, "randomly.Renderer")

Renderer::Renderer(QObject *parent, Recorder *recorder, QSize size, quint64 _framesToRender, bool saveFrames, uint seed, int particleCount)
    : QObject{parent}
    , m_size(size)
    , m_recorder(recorder)
    , m_noise(PerlinNoise(seed))
    , framesToRender(_framesToRender)
    , m_saveFrames(saveFrames)
    , m_rng(new QRandomGenerator(seed))
{
    m_renderTimer.start();

    m_particles.reserve(particleCount);

    for (int i = 0; i < particleCount; ++i)
        m_particles.emplaceBack(makeParticle());

    qCInfo(lcRenderer) << "particles initialized in" << m_renderTimer.elapsed() << "ms";

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

    static const QColor bg(0xff2d2d2d);

    img.fill(bg);

    QPainter paint;

    paint.begin(&img);
    static const auto clr = QColor(0xff700080);

    auto pen = QPen();
    pen.setWidth(1);

    paint.setPen(pen);

    for (auto &p: m_particles) {
        auto positions = p.positions();

        for (int i = positions.length() - 1; i >= 0; --i) {
            int age = getAgeOfPosition(i, p.lifeTime(), p.initialLifeTime());

            const auto f = (age / qreal(Particle::queueSize));
            auto hsl = clr.toHsl();
            const auto pos = positions.get(i);

            auto bgClr = img.pixelColor(clampPositionToImage(pos, m_size.width(), m_size.height())).toHsl();

            hsl.setHslF(     hsl.hslHueF(),
                        lerp(hsl.hslSaturationF(), bgClr.hslSaturationF(), f),
                        lerp(hsl.lightnessF(),     bgClr.lightnessF(),     f));
            pen.setColor(hsl);
            paint.setPen(pen);

            paint.drawPoint(pos);
        }
    }

    paint.end();

    updateParticles();

    // properly init the frame
    m_vframe = QVideoFrame(img);
    m_vframe.setStartTime(frameTime);
    m_vframe.setEndTime(frameTime + frameDelay);

    // update tracking variables
    frameTime += frameDelay;
    ++currentFrame;

    m_z += scale;

    if (m_saveFrames)
        img.save(QString("data/frame_%1.png").arg(currentFrame, 3, 10, QChar('0')));

    qCInfo(lcRenderer) << "rendering done in" << timing.elapsed() << "ms (" << (qreal(1000) / timing.elapsed()) << "FPS)";

    emit frameRendered(m_vframe);
}

QPair<QPointF, int> Renderer::makeParticle()
{
    auto pos = QPointF{m_rng->generateDouble() * width(), m_rng->generateDouble() * height()};
    auto lifetime = m_rng->bounded(2 * Particle::queueSize, Particle::maxLifetime);
    return {pos, lifetime};
}

void Renderer::updateParticles()
{
    for (auto &p: m_particles) {
        // just keep reusing the same particles
        if (p.lifeTime() == 0) {
            auto newP = makeParticle();
            p.reset(newP.first, newP.second);
            continue;
        }

        auto noise = m_noise.noise(p.pos().x() * scale, p.pos().y() * scale, m_z);
        p.tick(noise * Particle::pStep, width(), height());
    }
}

template<typename T, int Size>
Queue<T, Size>::Queue(T fill)
    : m_step(0)
{
    for (int i = 0; i < Size; ++i)
        m_data[i] = fill;
}

template<typename T, int Size>
T &Queue<T, Size>::get(int idx)
{
    return m_data[(idx + m_step) % Size];
}

Particle::Particle(QPointF pos, int lifetime)
    : m_positions(pos)
    , m_lifeTime(lifetime)
    , m_initialLifeTime(lifetime)
{}

void Particle::tick(qreal direction, int w, int h)
{
    const auto p = pos();
    auto aX = p.x() + cos(direction);
    auto aY = p.y() + sin(direction);

    if (aX > w)
        aX = 0;
    if (aX < 0)
        aX = w;

    if (aY > h)
        aY = 0;
    if (aY < 0)
        aY = h;

    changePos({aX, aY});

    --m_lifeTime;
}

void Particle::reset(QPointF newPos, int newLifeTime)
{
    changePos(newPos);
    m_initialLifeTime = newLifeTime;
    m_lifeTime = newLifeTime;
}

void Particle::changePos(QPointF newPos)
{
    m_positions.next();
    m_positions.get(0) = newPos;
}

} // namespace randomly
