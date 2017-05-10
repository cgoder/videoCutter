
#include "native-lib.h"


static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag) {
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    NATIVE_LOGI(
            "%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
            tag,
            av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
            av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
            av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
            pkt->stream_index);
}

int
clip_video(double starttime, double endtime, const char *in_filename, const char *out_filename) {
    NATIVE_LOGI("%s", "Start cut video");
    AVOutputFormat *outputFormat = NULL;
    AVFormatContext *inFormatContext = NULL, *outFormatContext = NULL;
    AVPacket packet;
    int return_code, i;

    //1、注册所有组件
    NATIVE_LOGI("%s", "注册所有组件");
    av_register_all();

    //2、开始读取输入视频文件
    if ((return_code = avformat_open_input(&inFormatContext, in_filename, 0, 0)) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open input file %s", in_filename);
        NATIVE_FF_LOGE(return_code);
        return -1;
    }

    //3、获取视频流媒体信息
    NATIVE_LOGI("%s", "获取视频流媒体信息");
    if ((return_code = avformat_find_stream_info(inFormatContext, 0)) < 0) {
        NATIVE_LOGE("%s", "Failed to retrieve input stream information");
        NATIVE_FF_LOGE(return_code);
        return -1;
    }

    //4、创建输出的AVFormatContext对象
    NATIVE_LOGI("%s", "创建输出的AVFormatContext对象");
    avformat_alloc_output_context2(&outFormatContext, NULL, NULL, out_filename);
    if (!outFormatContext) {
        NATIVE_LOGE("%s", "Could not create output context\n");
        return_code = AVERROR_UNKNOWN;
        NATIVE_FF_LOGE(return_code);
        return -1;
    }
    outputFormat = outFormatContext->oformat;
    //5、根据输入流设置相应的输出流参数（不发生转码）
    NATIVE_LOGI("%s", "根据输入流设置相应的输出流参数（不发生转码）");
    for (i = 0; i < inFormatContext->nb_streams; i++) {
        AVStream *in_stream = inFormatContext->streams[i];
//        AVCodecParameters *inCodecParameters = in_stream->codecpar;
        AVStream *out_stream = avformat_new_stream(outFormatContext, NULL);

        if (!out_stream) {
            NATIVE_LOGE("%s", "Failed to create output stream\n");
            return_code = AVERROR_UNKNOWN;
            NATIVE_FF_LOGE(return_code);
            return -1;
        }

        if ((return_code = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar)) <
            0) {
            NATIVE_LOGE("%s", "Failed to codecpar from input to output stream codecpar\n");
            NATIVE_FF_LOGE(return_code);
            return -1;
        }

        out_stream->codecpar->codec_tag = 0;

    }

    //6、检查输出文件是否正确配置完成
    NATIVE_LOGI("%s", "检查输出文件是否正确配置完成");
    if (!(outputFormat->flags & AVFMT_NOFILE)) {
        return_code = avio_open(&outFormatContext->pb, out_filename, AVIO_FLAG_WRITE);
        if (return_code < 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open output file %s",
                                out_filename);
            NATIVE_FF_LOGE(return_code);
            return -1;
        }
    }

    //7、写入Stream头部信息
    NATIVE_LOGI("%s", "写入Stream头部信息");
    return_code = avformat_write_header(outFormatContext, NULL);
    if (return_code < 0) {
        NATIVE_LOGE("%s", "Error occurred when opening output file\n");
        NATIVE_FF_LOGE(return_code);
        return -1;
    }

    //8、定位当前位置到裁剪的起始位置 from_seconds
    NATIVE_LOGI("%s", "定位当前位置到裁剪的起始位置 from_seconds");
    return_code = av_seek_frame(inFormatContext, -1, (int64_t) (starttime * AV_TIME_BASE),
                                AVSEEK_FLAG_ANY);
    if (return_code < 0) {
        NATIVE_LOGE("%s", "Error seek to the start\n");
        NATIVE_FF_LOGE(return_code);
        return -1;
    }

    //9、计算起始时间戳
    NATIVE_LOGI("%s", "计算起始时间戳");
    int64_t *dts_start_from = (int64_t *) malloc(sizeof(int64_t) * inFormatContext->nb_streams);
    memset(dts_start_from, 0, sizeof(int64_t) * inFormatContext->nb_streams);
    int64_t *pts_start_from = (int64_t *) malloc(sizeof(int64_t) * inFormatContext->nb_streams);
    memset(pts_start_from, 0, sizeof(int64_t) * inFormatContext->nb_streams);

    //10、开始写入视频信息
    NATIVE_LOGI("%s", "开始写入视频信息");
    while (1) {
        AVStream *in_stream, *out_stream;

        return_code = av_read_frame(inFormatContext, &packet);
        if (return_code < 0) {
            break;
        }

        in_stream = inFormatContext->streams[packet.stream_index];
        out_stream = outFormatContext->streams[packet.stream_index];
        NATIVE_LOGI("当前Packet索引：%d", packet.stream_index);

        log_packet(inFormatContext, &packet, "in");

        //av_q2d转换AVRational(包含分子分母的结构)类型为double,此过程有损
        if (av_q2d(in_stream->time_base) * packet.pts > endtime) {
            //当前的时间大于转换时间，则转换完成
            NATIVE_LOGI("%s", "到达截止时间点");
            av_packet_unref(&packet);
            break;
        }

        if (dts_start_from[packet.stream_index] == 0) {
            dts_start_from[packet.stream_index] = packet.dts;
            __android_log_print(ANDROID_LOG_INFO, TAG, "dts_start_from: %d",
                                av_ts2str(dts_start_from[packet.stream_index]));
        }
        if (pts_start_from[packet.stream_index] == 0) {
            pts_start_from[packet.stream_index] = packet.pts;
            __android_log_print(ANDROID_LOG_INFO, TAG, "pts_start_from: %d",
                                av_ts2str(pts_start_from[packet.stream_index]));
        }

        //拷贝数据包Packet对象(视频存储的单元)
        NATIVE_LOGI("%s", "拷贝数据包Packet对象(视频存储的单元)");
        packet.pts = av_rescale_q_rnd(packet.pts - pts_start_from[packet.stream_index],
                                      in_stream->time_base, out_stream->time_base,
                                      AV_ROUND_NEAR_INF); //AV_ROUND_PASS_MINMAX
        packet.dts = av_rescale_q_rnd(packet.dts - dts_start_from[packet.stream_index],
                                      in_stream->time_base, out_stream->time_base,
                                      AV_ROUND_NEAR_INF);
        if (packet.pts < 0) {
            packet.pts = 0;
        }
        if (packet.dts < 0) {
            packet.dts = 0;
        }
        packet.duration = (int) av_rescale_q((int64_t) packet.duration, in_stream->time_base,
                                             out_stream->time_base);
        packet.pos = -1;
        log_packet(outFormatContext, &packet, "out");

        //将当前Packet写入输出文件
        NATIVE_LOGI("%s", "将当前Packet写入输出文件");

        if ((return_code = av_write_frame(outFormatContext, &packet)) < 0) {
            NATIVE_LOGE("%s", "Error muxing packet\n");
            NATIVE_FF_LOGE(return_code);
            break;
        }
        //重置Packet对象
        av_packet_unref(&packet);
    }
    free(dts_start_from);
    free(pts_start_from);

    //11、写入stream尾部信息
    av_write_trailer(outFormatContext);

    //12、收尾：回收内存，关闭流...
    avformat_close_input(&inFormatContext);

    if (outFormatContext && !(outputFormat->flags & AVFMT_NOFILE)) {
        avio_closep(&outFormatContext->pb);
    }
    avformat_free_context(outFormatContext);

    return 0;
}


void toast(JNIEnv *env, jobject instance, const char *message) {
    jclass jc = env->GetObjectClass(instance);
    jmethodID jmID = env->GetMethodID(jc, "toast", "(Ljava/lang/String;)V");
    jstring _message = env->NewStringUTF(message);
    env->CallVoidMethod(jc, jmID, _message);
}

JNIEXPORT jint JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpeg_1clip_1do(JNIEnv *env, jobject instance,
                                                                 jdouble startTime, jdouble endTime,
                                                                 jstring inFile_,
                                                                 jstring outFile_) {
    jint ret = 0;

    const char *inFile = env->GetStringUTFChars(inFile_, 0);
    const char *outFile = env->GetStringUTFChars(outFile_, 0);


    if(clip_video(startTime, endTime, inFile, outFile) == 0)
    {
        toast(env, instance, "Clip video finished");
        ret = -1;
    }
    else
    {
        toast(env, instance, "Clip video failed");
        ret = 0;
    }

    env->ReleaseStringUTFChars(inFile_, inFile);
    env->ReleaseStringUTFChars(outFile_, outFile);

    return ret;
}


JNIEXPORT jstring JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpegInit(JNIEnv *env, jobject instance) {

    return env->NewStringUTF("Hello from ffmpeg");
}

JNIEXPORT jint JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpeg_1clip_1setPostion(JNIEnv *env,
                                                                         jobject instance) {

    return 1;
}

