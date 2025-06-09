#include "recorder.h"

#include "renderer.h"

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

inline int operator""_kbps(quint64 v)
{
    return v * 1000;
}

} // namespace

Q_LOGGING_CATEGORY(lcRecorder, "randomly.Recorder");

Recorder::Recorder(QObject *parent)
    : QObject{parent}
    , m_input(new QVideoFrameInput(this))
    , m_session(new QMediaCaptureSession(this))
    , m_recorder(new QMediaRecorder(this))
    , m_output(new QFile("output.mp4", this))
    , m_renderer(new Renderer(nullptr, this, {1920, 1080}, 1200, true, 0, 1000))
{
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
