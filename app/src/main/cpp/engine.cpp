#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_com_aigame_engine_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "C++ Engine Initialized Successfully!";
    return env->NewStringUTF(hello.c_str());
}
