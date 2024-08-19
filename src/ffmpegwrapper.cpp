#include "ffmpegwrapper.h"
#include <QDebug>

FFmpegWrapper::FFmpegWrapper(QObject *parent) : QObject(parent),
    inputFormatContext(nullptr), outputFormatContext(nullptr),
    inputCodecContext(nullptr), outputCodecContext(nullptr),
    swsContext(nullptr), videoStreamIndex(-1),
    inputVideoStream(nullptr), outputVideoStream(nullptr),
    cropX(0), cropY(0), cropWidth(0), cropHeight(0),
    rescaleWidth(0), rescaleHeight(0),
    trimStart(0), trimEnd(0)
{
    avformat_network_init();
}

FFmpegWrapper::~FFmpegWrapper()
{
    cleanUp();
}

bool FFmpegWrapper::openInputFile(const QString& filename)
{
    if (avformat_open_input(&inputFormatContext, filename.toUtf8().constData(), nullptr, nullptr) < 0) {
        emit errorOccurred("Could not open input file");
        return false;
    }

    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
        emit errorOccurred("Could not find stream information");
        return false;
    }

    videoStreamIndex = av_find_best_stream(inputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        emit errorOccurred("Could not find video stream");
        return false;
    }

    inputVideoStream = inputFormatContext->streams[videoStreamIndex];
    const AVCodec *codec = avcodec_find_decoder(inputVideoStream->codecpar->codec_id);
    if (!codec) {
        emit errorOccurred("Could not find decoder");
        return false;
    }

    inputCodecContext = avcodec_alloc_context3(codec);
    if (!inputCodecContext) {
        emit errorOccurred("Could not allocate decoder context");
        return false;
    }

    if (avcodec_parameters_to_context(inputCodecContext, inputVideoStream->codecpar) < 0) {
        emit errorOccurred("Could not copy codec parameters");
        return false;
    }

    if (avcodec_open2(inputCodecContext, codec, nullptr) < 0) {
        emit errorOccurred("Could not open codec");
        return false;
    }

    return true;
}

bool FFmpegWrapper::setupOutputFile(const QString& filename)
{
    avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, filename.toUtf8().constData());
    if (!outputFormatContext) {
        emit errorOccurred("Could not create output context");
        return false;
    }

    outputVideoStream = avformat_new_stream(outputFormatContext, nullptr);
    if (!outputVideoStream) {
        emit errorOccurred("Could not create output video stream");
        return false;
    }

    const AVCodec *outputCodec = avcodec_find_encoder(outputFormatContext->oformat->video_codec);
    if (!outputCodec) {
        emit errorOccurred("Could not find encoder");
        return false;
    }

    outputCodecContext = avcodec_alloc_context3(outputCodec);
    if (!outputCodecContext) {
        emit errorOccurred("Could not allocate encoder context");
        return false;
    }

    // Set output codec parameters (todo: adjust)
    outputCodecContext->height = inputCodecContext->height;
    outputCodecContext->width = inputCodecContext->width;
    outputCodecContext->sample_aspect_ratio = inputCodecContext->sample_aspect_ratio;
    outputCodecContext->pix_fmt = outputCodec->pix_fmts ? outputCodec->pix_fmts[0] : AV_PIX_FMT_YUV420P;
    outputCodecContext->time_base = inputVideoStream->time_base;

    if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
        outputCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(outputCodecContext, outputCodec, nullptr) < 0) {
        emit errorOccurred("Could not open output codec");
        return false;
    }

    if (avcodec_parameters_from_context(outputVideoStream->codecpar, outputCodecContext) < 0) {
        emit errorOccurred("Could not copy encoder parameters to output stream");
        return false;
    }

    outputVideoStream->time_base = outputCodecContext->time_base;

    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outputFormatContext->pb, filename.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            emit errorOccurred("Could not open output file");
            return false;
        }
    }

    if (avformat_write_header(outputFormatContext, nullptr) < 0) {
        emit errorOccurred("Could not write output file header");
        return false;
    }

    return true;
}

bool FFmpegWrapper::cropVideo(int x, int y, int width, int height)
{
    cropX = x;
    cropY = y;
    cropWidth = width;
    cropHeight = height;
    return true;
}

bool FFmpegWrapper::rescaleVideo(int width, int height)
{
    rescaleWidth = width;
    rescaleHeight = height;
    return true;
}

bool FFmpegWrapper::trimVideo(double startTime, double endTime)
{
    trimStart = startTime;
    trimEnd = endTime;
    return true;
}

bool FFmpegWrapper::convertFormat(const QString& format)
{
    outputFormat = format;
    return true;
}

bool FFmpegWrapper::processVideo()
{
    if (!initializeTransformations()) {
        return false;
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *filteredFrame = av_frame_alloc();

    int64_t startPts = trimStart * inputVideoStream->time_base.den / inputVideoStream->time_base.num;
    int64_t endPts = trimEnd * inputVideoStream->time_base.den / inputVideoStream->time_base.num;

    while (av_read_frame(inputFormatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            if (packet->pts < startPts) {
                av_packet_unref(packet);
                continue;
            }
            if (endPts > 0 && packet->pts > endPts) {
                av_packet_unref(packet);
                break;
            }

            int ret = avcodec_send_packet(inputCodecContext, packet);
            if (ret < 0) {
                emit errorOccurred("Error sending packet to decoder");
                av_packet_unref(packet);
                return false;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(inputCodecContext, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    emit errorOccurred("Error receiving frame from decoder");
                    av_packet_unref(packet);
                    return false;
                }

                // Apply transformations (crop and rescale)
                sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height,
                          filteredFrame->data, filteredFrame->linesize);

                if (!writeFrame(packet, filteredFrame)) {
                    av_packet_unref(packet);
                    return false;
                }

                av_frame_unref(frame);
            }
        }
        av_packet_unref(packet);

        // Update progress (this is a simplified progress calculation)
        int progress = (int)(100.0 * packet->pts / inputVideoStream->duration);
        emit progressUpdated(progress);
    }

    // Flush encoder
    avcodec_send_frame(outputCodecContext, nullptr);
    while (true) {
        int ret = avcodec_receive_packet(outputCodecContext, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            emit errorOccurred("Error flushing encoder");
            return false;
        }
        av_interleaved_write_frame(outputFormatContext, packet);
        av_packet_unref(packet);
    }

    av_write_trailer(outputFormatContext);

    av_frame_free(&frame);
    av_frame_free(&filteredFrame);
    av_packet_free(&packet);

    return true;
}

bool FFmpegWrapper::initializeTransformations()
{
    int finalWidth = rescaleWidth > 0 ? rescaleWidth : (cropWidth > 0 ? cropWidth : inputCodecContext->width);
    int finalHeight = rescaleHeight > 0 ? rescaleHeight : (cropHeight > 0 ? cropHeight : inputCodecContext->height);

    swsContext = sws_getContext(
        inputCodecContext->width, inputCodecContext->height, inputCodecContext->pix_fmt,
        finalWidth, finalHeight, outputCodecContext->pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!swsContext) {
        emit errorOccurred("Could not initialize scaling context");
        return false;
    }

    return true;
}

bool FFmpegWrapper::writeFrame(AVPacket *packet, AVFrame *frame)
{
    int ret = avcodec_send_frame(outputCodecContext, frame);
    if (ret < 0) {
        emit errorOccurred("Error sending frame to encoder");
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(outputCodecContext, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            emit errorOccurred("Error receiving packet from encoder");
            return false;
        }

        packet->stream_index = videoStreamIndex;
        av_packet_rescale_ts(packet, inputVideoStream->time_base, outputVideoStream->time_base);

        ret = av_interleaved_write_frame(outputFormatContext, packet);
        if (ret < 0) {
            emit errorOccurred("Error writing frame");
            return false;
        }
    }

    return true;
}

void FFmpegWrapper::cleanUp()
{
    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    if (inputCodecContext) {
        avcodec_free_context(&inputCodecContext);
    }
    if (outputCodecContext) {
        avcodec_free_context(&outputCodecContext);
    }
    if (inputFormatContext) {
        avformat_close_input(&inputFormatContext);
    }
    if (outputFormatContext) {
        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&outputFormatContext->pb);
        }
        avformat_free_context(outputFormatContext);
    }
}