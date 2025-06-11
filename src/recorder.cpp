#include "recorder.h"

#include "renderer.h"

#include <QCommandLineParser>
#include <QFile>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMediaCaptureSession>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QMessageBox>
#include <QThread>
#include <QVideoFrameInput>
#include <QVideoWidget>

namespace randomly {

namespace
{

Q_LOGGING_CATEGORY(lcRecorder, "randomly.Recorder");

inline int operator""_kbps(quint64 v)
{
    return v * 1000;
}

inline int tryConvertInt(const QString &v, const char *name)
{
    bool ok = true;

    const auto i = v.toInt(&ok);
    if (!ok) {
        qCWarning(lcRecorder).nospace() << "Invalid integer \"" << name << "\": " << v;
        exit(1);
    }

    return i;
}

QDebug operator<<(QDebug dbg, RenderInfo &info)
{
    const QDebugStateSaver saver(dbg);
    dbg.nospace().noquote();

    return dbg << info.framesToRender << " frames@" << info.size.width() << "x" << info.size.height() << "/" << info.seed << ", " << info.particleCount << "(" << info.saveFrames << ")";
}

QSize tryParseSize(QString str)
{
    if (!str.contains('x')) {
        qCWarning(lcRecorder) << "Invalid resolution provided! Expected format: <width>x<height>";
        exit(1);
    }

    auto dims = str.split('x');

    return {tryConvertInt(dims[0], "width"), tryConvertInt(dims[1], "height")};
}

} // namespace

Recorder::Recorder(QObject *parent)
    : QObject{parent}
    , m_input(new QVideoFrameInput(this))
    , m_session(new QMediaCaptureSession(this))
    , m_recorder(new QMediaRecorder(this))
{
    QCoreApplication::setApplicationName(RANDOMLY_EXECUTABLE_NAME);
    QCoreApplication::setApplicationVersion(RANDOMLY_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("Renderer for an effect by RandomlyCoded");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption framesOption({"f", "frames"}, "Number of frames to render\t(default: 60).", "count", "60");
    parser.addOption(framesOption);

    QCommandLineOption resolutionOption({"r", "resolution"}, "Video resolution\t\t(default: 1920x1080).", "width>x<height", "1920x1080");
    parser.addOption(resolutionOption);

    QCommandLineOption seedOption({"s", "seed"}, "Seed for perlin noise\t(default: 0).", "seed", "0");
    parser.addOption(seedOption);

    QCommandLineOption particleOption({"p", "particles"}, "Number of particles\t(default: 5000).", "count", "5000");
    parser.addOption(particleOption);

    QCommandLineOption outputOption({"o", "output"}, "Output file (default: output.mp4).", "file", "output.mp4");
    parser.addOption(outputOption);

    QCommandLineOption saveFramesOption("save-frames", "Save individual frames to ./data/");
    parser.addOption(saveFramesOption);


    parser.process(QCoreApplication::arguments());

    RenderInfo info;

    info.size = tryParseSize(parser.value(resolutionOption));
    info.particleCount = tryConvertInt(parser.value(particleOption), "particle count");
    info.framesToRender = tryConvertInt(parser.value(framesOption), "frame count");
    info.seed = tryConvertInt(parser.value(seedOption), "seed");
    info.saveFrames = parser.isSet(saveFramesOption);

    qCInfo(lcRecorder).noquote() << info << "->" << parser.value(outputOption);

    m_output = new QFile(parser.value(outputOption), this);

    // parent = nullptr so I can move the renderer between threads
    m_renderer = new Renderer(nullptr, this, info);

    QThread *renderThread = new QThread(this);

    m_renderer->moveToThread(renderThread);
    m_output->open(QFile::WriteOnly);

    m_recorder->setVideoBitRate(25000_kbps);
    m_recorder->setAudioBitRate(25000_kbps);

    m_recorder->setVideoFrameRate(60);
    m_recorder->setOutputDevice(m_output);
    m_recorder->setQuality(QMediaRecorder::VeryHighQuality); // doesn't seem to have any effect
    // m_recorder->setEncodingMode(QMediaRecorder::TwoPassEncoding); // seems to not change anything either

    QMediaMetaData meta;
    meta.insert(QMediaMetaData::Author, "RandomlyCoded");
    meta.insert(QMediaMetaData::VideoBitRate, m_recorder->videoBitRate());
    m_recorder->addMetaData(meta);

    QMediaFormat format = m_recorder->mediaFormat();
    format.setVideoCodec(QMediaFormat::VideoCodec::Theora);
    m_recorder->setMediaFormat(format);

    m_session->setRecorder(m_recorder);
    m_session->setVideoFrameInput(m_input);

    connect(m_input, &QVideoFrameInput::readyToSendVideoFrame, this, [this] () {m_renderer->render(); }); // doesn't work with `m_renderer, &Renderer::render`; might be because m_renderer lives in a different thread?
    connect(m_renderer, &Renderer::frameRendered, m_input, &QVideoFrameInput::sendVideoFrame);
    connect(m_recorder, &QMediaRecorder::errorOccurred, [] (QMediaRecorder::Error error, const QString &errorString) { qCWarning(lcRecorder) << error << errorString; });
    connect(m_recorder, &QMediaRecorder::recorderStateChanged, this, &Recorder::onMediaRecorderStateChanged);

    m_recorder->record();

    qCInfo(lcRecorder) << "FPS:" << m_recorder->videoFrameRate() << "bps:" << m_recorder->videoBitRate();
    qCInfo(lcRecorder) << "saving to" << m_output->fileName() << "type" << m_recorder->mediaFormat().fileFormat() << "using codec" << m_recorder->mediaFormat().videoCodec();
}

void Recorder::setPreviewOutput(QVideoWidget *widget)
{
    qCInfo(lcRecorder) << "new preview:" << widget;

    m_preview = widget;
    m_session->setVideoOutput(widget);
}

void Recorder::stop()
{
    m_recorder->stop();
}

void Recorder::onMediaRecorderStateChanged(QMediaRecorder::RecorderState state)
{
    qCInfo(lcRecorder) << "Recorder state changed!" << state;

    if (state == QMediaRecorder::StoppedState) {
        m_output->close();
        QMessageBox done(m_preview);

        done.setText("rendering done");
        done.setStandardButtons(QMessageBox::Ok);

        done.exec();
        QGuiApplication::exit();
    }
}

} // namespace randomly
