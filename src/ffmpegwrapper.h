#ifndef FFMPEGWRAPPER_H
#define FFMPEGWRAPPER_H

#include <QString>
#include <QObject>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class FFmpegWrapper : public QObject
{
    Q_OBJECT

public:
    explicit FFmpegWrapper(QObject *parent = nullptr);
    ~FFmpegWrapper();

    bool openInputFile(const QString& filename);
    bool setupOutputFile(const QString& filename);
    bool cropVideo(int x, int y, int width, int height);
    bool rescaleVideo(int width, int height);
    bool trimVideo(double startTime, double endTime);
    bool convertFormat(const QString& format);
    bool processVideo();

signals:
    void progressUpdated(int progress);
    void errorOccurred(const QString& error);

private:
    AVFormatContext *inputFormatContext;
    AVFormatContext *outputFormatContext;
    AVCodecContext *inputCodecContext;
    AVCodecContext *outputCodecContext;
    SwsContext *swsContext;

    int videoStreamIndex;
    AVStream *inputVideoStream;
    AVStream *outputVideoStream;

    // Transformation parameters
    int cropX, cropY, cropWidth, cropHeight;
    int rescaleWidth, rescaleHeight;
    double trimStart, trimEnd;
    QString outputFormat;

    bool initializeTransformations();
    bool writeFrame(AVPacket *packet, AVFrame *frame);
    void cleanUp();
};

#endif // FFMPEGWRAPPER_H