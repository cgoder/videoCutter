
#include "native-lib.h"

long start_time=0;

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

int clip_video(double starttime, double endtime, const char *in_filename, const char *out_filename) {
    NATIVE_LOGI("开始执行裁剪");
    AVOutputFormat *outputFormat = NULL;
    AVFormatContext *inFormatContext = NULL, *outFormatContext = NULL;
    AVPacket packet;
    int ret;
    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;

    //1、注册所有组件
    NATIVE_LOGI("1、注册所有组件");
    av_register_all();

    //2、开始读取输入视频文件
    NATIVE_LOGI("2、开始读取输入视频文件");
    if ((ret = avformat_open_input(&inFormatContext, in_filename, 0, 0)) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open input file %s", in_filename);
        return ret;
    }

    //3、获取视频流媒体信息
    NATIVE_LOGI("3、获取视频流媒体信息");
    if ((ret = avformat_find_stream_info(inFormatContext, 0)) < 0) {
        NATIVE_LOGE("\tFailed to retrieve input stream information");
        return ret;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "bit_rate:%lld\n"
                                "duration:%lld\n"
                                "nb_streams:%d\n"
                                "fps_prob_size:%d\n"
                                "format_probesize:%d\n"
                                "max_picture_buffer:%d\n"
                                "max_chunk_size:%d\n"
                                "format_name:%s",
                        inFormatContext->bit_rate,
                        inFormatContext->duration,
                        inFormatContext->nb_streams,
                        inFormatContext->fps_probe_size,
                        inFormatContext->format_probesize,
                        inFormatContext->max_picture_buffer,
                        inFormatContext->max_chunk_size,
                        inFormatContext->iformat->name);

    //4、创建输出的AVFormatContext对象
    NATIVE_LOGI("4、创建输出的AVFormatContext对象");
    avformat_alloc_output_context2(&outFormatContext, NULL, NULL, out_filename);
    if (!outFormatContext) {
        NATIVE_LOGE("\tCould not create output context\n");
        ret = AVERROR_UNKNOWN;
        return ret;
    }

    //设置stream_mapping
    NATIVE_LOGI("5、设置stream_mapping");
    stream_mapping_size = inFormatContext->nb_streams;
    stream_mapping = (int*)av_mallocz_array((size_t)stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping){
        ret=AVERROR(ENOMEM);
        NATIVE_LOGE("Error while set stream_mapping");
        return ret;
    }

    outputFormat = outFormatContext->oformat;
    //6、根据输入流设置相应的输出流参数
    NATIVE_LOGI("6、根据输入流设置相应的输出流参数");
    for (int i = 0; i < inFormatContext->nb_streams; i++) {
        AVStream *in_stream = inFormatContext->streams[i];
        AVStream *out_stream = avformat_new_stream(outFormatContext, NULL);
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE){
            stream_mapping[i] = -1;
            continue;
        }
        stream_mapping[i] = stream_index++;

        if (!out_stream) {
            NATIVE_LOGE("\tFailed to create output stream");
            ret = AVERROR_UNKNOWN;
            return ret;
        }

        if ((ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar)) < 0) {
            NATIVE_LOGE("\tFailed to copy codec parameters");
            return ret;
        }

        out_stream->codecpar->codec_tag = 0;

    }


    //7、检查输出文件是否正确配置完成
    NATIVE_LOGI("7、检查输出文件是否正确配置完成");
    if (!(outputFormat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outFormatContext->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            NATIVE_LOGE("\tCould not open output file %s", out_filename);
            return ret;
        }
    }

    //8、写入Stream头部信息
    NATIVE_LOGI("8、写入Stream头部信息");
    ret = avformat_write_header(outFormatContext, NULL);
    if (ret < 0) {
        NATIVE_LOGE("\tError occurred while write header");
        return ret;
    }



    //9、设置dts和pts变量的内存
    NATIVE_LOGI("9、设置dts和pts变量的内存");
    int64_t *dts_start_from = (int64_t *) malloc(sizeof(int64_t) * inFormatContext->nb_streams);
    memset(dts_start_from, 0, sizeof(int64_t) * inFormatContext->nb_streams);
    int64_t *pts_start_from = (int64_t *) malloc(sizeof(int64_t) * inFormatContext->nb_streams);
    memset(pts_start_from, 0, sizeof(int64_t) * inFormatContext->nb_streams);

    //10、定位当前位置到裁剪的起始位置 from_seconds
    NATIVE_LOGI("10、定位当前位置到裁剪的起始位置:%lld, stream: %d, ", (int64_t) (starttime * AV_TIME_BASE), stream_index);

    ret = av_seek_frame(inFormatContext, -1, (int64_t) (starttime * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        NATIVE_LOGE("\tError seek to the start");
        return ret;
    }

    //11、开始写入视频信息
    NATIVE_LOGI("11、开始写入视频信息");
    int k = 0;
    while (1) {
        k++;
        NATIVE_LOGI("<1> -----------------------------< Loop：%d >-------------------------------", k);
        AVStream *in_stream, *out_stream;

        NATIVE_LOGI("<2> Read frame");
        ret = av_read_frame(inFormatContext, &packet);
        if (ret < 0) {
            break;
        }


        NATIVE_LOGI("\tPacket stream_index：%d", packet.stream_index);
        in_stream = inFormatContext->streams[packet.stream_index];
        if (packet.stream_index >= stream_mapping_size ||
            stream_mapping[packet.stream_index] < 0) {
            NATIVE_LOGE("reach end");
            av_packet_unref(&packet);
            continue;
        }

        //convert coding
//        avcodec_send_frame(in_stream->codec, frame);
//        AVCodecContext *outCodecContext = avcodec_alloc_context3(in_stream->codec->codec);
//        avcodec_parameters_to_context(outCodecContext, in_stream->codecpar);
//        outCodecContext->bit_rate = 40000;
//        avcodec_receive_packet(outCodecContext, &packet);

        packet.stream_index= stream_mapping[packet.stream_index];

        out_stream = outFormatContext->streams[packet.stream_index];

        av_dict_copy(&(out_stream->metadata), in_stream->metadata, AV_DICT_IGNORE_SUFFIX);

        NATIVE_LOGI("\tin_steam bit_rate:%lld", in_stream->codecpar->bit_rate);
        NATIVE_LOGI("\tin_steam bits_codec_sample:%d", in_stream->codecpar->bits_per_coded_sample);
        NATIVE_LOGI("\tin_steam bits_per_raw_sample:%d", in_stream->codecpar->bits_per_raw_sample);


//        log_packet(inFormatContext, &packet, "in");

        //av_q2d转换AVRational(包含分子分母的结构)类型为double,此过程有损
        NATIVE_LOGI("\tin_stream time_base: %d/%d", in_stream->time_base.num, in_stream->time_base.den);
        if (av_q2d(in_stream->time_base) * packet.pts > endtime) {
            NATIVE_LOGI("Reach End");
            av_packet_unref(&packet);
            break;
        }

        if (dts_start_from[packet.stream_index] == 0) {
            dts_start_from[packet.stream_index] = packet.dts;
            NATIVE_LOGI("\tdts_start_from: %lld", dts_start_from[packet.stream_index]);
        }

        if (pts_start_from[packet.stream_index] == 0) {
            pts_start_from[packet.stream_index] = packet.pts;
            NATIVE_LOGI("\tpts_start_from: %lld", pts_start_from[packet.stream_index]);
        }

        //判断dts和pts的关系，如果 dts < pts 那么当调用 av_write_frame() 时会导致 Invalid Argument
        if (dts_start_from[packet.stream_index] < pts_start_from[packet.stream_index]){
            NATIVE_LOGI("pts is smaller than dts, resting pts to equal dts");
            pts_start_from[packet.stream_index] = dts_start_from[packet.stream_index];
        }

        NATIVE_LOGI("<3> Packet timestamp");
        packet.pts = av_rescale_q_rnd(packet.pts - pts_start_from[packet.stream_index],
                                      in_stream->time_base, out_stream->time_base,
                                      AV_ROUND_INF );
        packet.dts = av_rescale_q_rnd(packet.dts - dts_start_from[packet.stream_index],
                                      in_stream->time_base, out_stream->time_base,
                                      AV_ROUND_ZERO);
        if (packet.pts < 0) {
            packet.pts = 0;
        }
        if (packet.dts < 0) {
            packet.dts = 0;
        }
        NATIVE_LOGI("PTS:%lld\tDTS:%lld", packet.dts, packet.dts);
        packet.duration = av_rescale_q((int64_t) packet.duration, in_stream->time_base,
                                       out_stream->time_base);
        packet.pos = -1;

        //将当前Packet写入输出文件
        NATIVE_LOGI("<4> Write Packet");
        NATIVE_LOGI("\tAVFormatContext State:%d", outFormatContext != NULL);
        NATIVE_LOGI("\tPacket State:%d", &packet != NULL);
        if ((ret = av_interleaved_write_frame(outFormatContext, &packet)) < 0) {
            NATIVE_LOGE("\tError write packet\n");
            return ret;
        }
        //重置Packet对象
        NATIVE_LOGI("<5> Unref Packet");
        av_packet_unref(&packet);
    }
    free(dts_start_from);
    free(pts_start_from);

    //12、写入stream尾部信息
    NATIVE_LOGI("12、写入stream尾部信息");
    av_write_trailer(outFormatContext);

    //13、收尾：回收内存，关闭流...
    NATIVE_LOGI("13、收尾：回收内存，关闭流...");
    avformat_close_input(&inFormatContext);

    if (outFormatContext && !(outputFormat->flags & AVFMT_NOFILE)) {
        avio_closep(&outFormatContext->pb);
    }
    avformat_free_context(outFormatContext);

    return 0;
}

int remux_video(const char *in_filename, const char *out_filename) {
    NATIVE_LOGI("1、开始无损提取");
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;
    NATIVE_LOGI("2、初始化所有组件");
    av_register_all();
    NATIVE_LOGI("3、读取输入文件");
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        NATIVE_LOGE("Could not open input file %s", in_filename);
        return ret;
    }
    NATIVE_LOGI("4、获取输入流信息");
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        NATIVE_LOGE("Failed to retrieve input stream information");
        return ret;
    }
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    NATIVE_LOGI("5、创建输出AVFormatContext");
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        NATIVE_LOGE("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        return ret;
    }
    NATIVE_LOGI("6、设置stream_mapping");
    stream_mapping_size = ifmt_ctx->nb_streams;
    stream_mapping = (int *) av_mallocz_array((size_t) stream_mapping_size,
                                              sizeof(*stream_mapping));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        NATIVE_LOGE("Error set stream_mapping");
        return ret;
    }
    NATIVE_LOGI("7、开始循环读取流");
    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        NATIVE_LOGI("(1)、-----------------<流编号：%d>-----------------", i);
        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }
        stream_mapping[i] = stream_index++;
        NATIVE_LOGI("(2)、创建新的输出流");
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            NATIVE_LOGE("Failed allocating output stream");
            ret = AVERROR_UNKNOWN;
            return ret;
        }
        if ((ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar)) < 0) {
            NATIVE_LOGE("Failed to copy codec parameters\n");
            return ret;
        }
        out_stream->codecpar->codec_tag = 0;
    }
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    NATIVE_LOGI("8、检测输出文件配置");
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            NATIVE_LOGE("Could not open output file %s", out_filename);
            return ret;
        }
    }
    NATIVE_LOGI("9、写入文件头信息");
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        NATIVE_LOGE("Error occurred when opening output file");
        return ret;
    }
    int k = 0;
    NATIVE_LOGI("10、开始写入到输出流");
    while (1) {
        k++;
        NATIVE_LOGE("(1) -----------------------<循环：%d>--------------------", k);
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;
        in_stream = ifmt_ctx->streams[pkt.stream_index];
        if (pkt.stream_index >= stream_mapping_size ||
            stream_mapping[pkt.stream_index] < 0) {
            NATIVE_LOGE("reach end");
            av_packet_unref(&pkt);
            continue;
        }
        pkt.stream_index = stream_mapping[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        log_packet(ifmt_ctx, &pkt, "in");
        /* copy packet */
        NATIVE_LOGE("(2) 设置timestamp");
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                                   AV_ROUND_PASS_MINMAX);//AV_ROUND_NEAR_INF|
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
                                   AV_ROUND_PASS_MINMAX);
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        log_packet(ofmt_ctx, &pkt, "out");
        NATIVE_LOGE("(3) 写入frame");
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            NATIVE_LOGE("Error muxing packet");
            break;
        }
        av_packet_unref(&pkt);
    }
    NATIVE_LOGE("11、写入文件尾部信息");
    av_write_trailer(ofmt_ctx);
    NATIVE_LOGE("12、收尾：回收内存，关闭流...");
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt != NULL && ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    av_freep(&stream_mapping);
    if (ret < 0 && ret != AVERROR_EOF) {
        NATIVE_FF_LOGE(ret);
        return 1;
    }
    return 0;
}

void toast(JNIEnv *env, jobject instance, const char *message) {
    NATIVE_LOGI("%s, total use time: %ld", message, clock() - start_time);
}

JNIEXPORT jint JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpeg_1clip_1do(JNIEnv *env, jobject instance,
                                                                 jdouble startTime, jdouble endTime,
                                                                 jstring inFile_,
                                                                 jstring outFile_) {
    jint ret = 0;

    const char *inFile = env->GetStringUTFChars(inFile_, 0);
    const char *outFile = env->GetStringUTFChars(outFile_, 0);

    start_time=clock();
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
Java_com_suning_sports_videocutter_MainActivity_ffmpeg_1remux_1do(JNIEnv *env, jobject instance,
                                                                  jstring inFile_,
                                                                  jstring outFile_) {
    jint ret=0;
    const char *inFile = env->GetStringUTFChars(inFile_, 0);
    const char *outFile = env->GetStringUTFChars(outFile_, 0);

    if(remux_video(inFile, outFile) == 0)
    {
        toast(env, instance, "Remux video finished");
        ret = -1;
    }
    else
    {
        toast(env, instance, "Remux video failed");
        ret = 0;
    }

    env->ReleaseStringUTFChars(inFile_, inFile);
    env->ReleaseStringUTFChars(outFile_, outFile);
    return ret;
}

