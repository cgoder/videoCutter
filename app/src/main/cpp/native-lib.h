//
// Created by tienc on 2017/5/10.
//

#ifndef VIDEOCUTTER_NATIVE_LIB_H
#define VIDEOCUTTER_NATIVE_LIB_H


#include <jni.h>
#include <Android/log.h>

#define TAG "native_lib"
#define NATIVE_LOGI(format, ...) __android_log_print(ANDROID_LOG_INFO, TAG, format, ## __VA_ARGS__)
#define NATIVE_LOGE(format, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, format, ## __VA_ARGS__)
#define NATIVE_FF_LOGE(error_code) __android_log_print(ANDROID_LOG_ERROR, TAG, "Error detail: %s", av_err2str(error_code));


extern "C" {


#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

int clip_video(double starttime, double endtime, const char* in_filename, const char* out_filename);

void toast(JNIEnv* env, jobject instance, const char* message);



JNIEXPORT jint JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpeg_1clip_1do(JNIEnv *env, jobject instance,
                                                                 jdouble startTime, jdouble endTime,
                                                                 jstring inFile_,
                                                                 jstring outFile_);

JNIEXPORT jstring JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpegInit(JNIEnv *env, jobject instance);


JNIEXPORT jint JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpeg_1remux_1do(JNIEnv *env, jobject instance,
                                                                  jstring inFile_,
                                                                  jstring outFile_);

}

#endif //VIDEOCUTTER_NATIVE_LIB_H
