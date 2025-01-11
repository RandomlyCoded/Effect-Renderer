#ifndef RECORDER_H
#define RECORDER_H

#include <QObject>
#include <QVideoFrame>
#include <QMediaRecorder>

class QFile;
class QMediaCaptureSession;
class QVideoFrameInput;
class QVideoWidget;

namespace randomly {

class Renderer;

class Recorder : public QObject
{
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);

    void setPreviewOutput(QVideoWidget *widget);

    void stop();

    void onMediaRecorderStateChanged(QMediaRecorder::RecorderState state);

private:
    QVideoWidget *m_preview = nullptr;

    QVideoFrameInput *m_input;
    QVideoFrame m_frame;
    QMediaCaptureSession *m_session;
    QMediaRecorder *m_recorder;
    QFile *m_output;

    Renderer *m_renderer;
};

} // namespace randomly

#endif // RECORDER_H
