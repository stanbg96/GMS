#include <jni.h>
#include <GLES3/gl3.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "GMS_ENGINE", __VA_ARGS__)

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceCreated(JNIEnv* env, jobject) {
    LOGI("OpenGL ES 3.0 Surface Created!");
    // Задаваме тъмносин/сив цвят за фон на 3D света
    glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv* env, jobject, jint width, jint height) {
    LOGI("Surface Changed: %d x %d", width, height);
    // Настройваме изгледа спрямо размера на екрана
    glViewport(0, 0, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv* env, jobject) {
    // Изчистваме екрана всеки кадър (подготовка за рисуване на 3D обекти)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
