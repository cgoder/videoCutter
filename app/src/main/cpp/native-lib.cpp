#include <jni.h>
#include <string>


extern "C" {

JNIEXPORT jstring JNICALL
Java_com_suning_sports_videocutter_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpegInit(JNIEnv *env, jobject instance) {

    std::string hello = "Hello from ffmpeg";
    return env->NewStringUTF(hello.c_str());
}

JNIEXPORT jint JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpeg_1clip_1setPostion(JNIEnv *env,
                                                                         jobject instance) {

    // TODO
    std::string hello = "Now in ffmpeg_clip_setPostion()!";
    return 1;
}

JNIEXPORT jint JNICALL
Java_com_suning_sports_videocutter_MainActivity_ffmpeg_1clip_1do(JNIEnv *env, jobject instance) {

    // TODO
    std::string hello = "Now in ffmpeg_clip_do()!";
    return 1;
}

}

