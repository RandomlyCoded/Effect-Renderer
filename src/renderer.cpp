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

constexpr int OFFSET_RED   = 2;
constexpr int OFFSET_GREEN = 1;
constexpr int OFFSET_BLUE  = 0;

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

Renderer::Renderer(QObject *parent, Recorder *recorder, const RenderInfo &info)
    : QObject{parent}
    , m_recorder(recorder)
    , m_size(info.size)
    , m_noise(PerlinNoise(info.seed))
    , framesToRender(info.framesToRender)
    , m_saveFrames(info.saveFrames)
    , m_rng(new QRandomGenerator(info.seed))
{
    m_renderTimer.start();

    m_particles.reserve(info.particleCount);

    for (int i = 0; i < info.particleCount; ++i)
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
    static const auto particleClr = QColor(0xff700080).toHsl();

    img.fill(bg);

    // I believe technically a QByteArray would be correct, but using a raw pointer halves rendering time
    // most likely because the QByteArray spends time checking if it needs to be detached
    auto imgData = img.bits();

    for (auto &p: m_particles) {
        auto positions = p.positions();

        for (int i = positions.length() - 1; i >= 0; --i) {
            int age = getAgeOfPosition(i, p.lifeTime(), p.initialLifeTime());

            const auto f = age / Particle::queueSizeF;
            const auto pos = positions.get(i);

            const auto imgPos = clampPositionToImage(pos, m_size.width(), m_size.height());
            const int index0 = (imgPos.x() + imgPos.y() * img.width()) * 4;

            auto bgClr = QColor {
                uchar(imgData[index0 + OFFSET_RED  ]),
                uchar(imgData[index0 + OFFSET_GREEN]),
                uchar(imgData[index0 + OFFSET_BLUE ])
            }.toHsl();

            const auto clr = QColor::fromHslF(     particleClr.hslHueF(),
                                              lerp(particleClr.hslSaturationF(), bgClr.hslSaturationF(), f),
                                              lerp(particleClr.lightnessF(),     bgClr.lightnessF(),     f)
                             ).toRgb();

            // order is reversed, probably some little/big endian stuff idk
            imgData[index0 + OFFSET_RED  ] = clr.red();
            imgData[index0 + OFFSET_GREEN] = clr.green();
            imgData[index0 + OFFSET_BLUE ] = clr.blue();
        }
    }

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
