#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "GMS_3D", __VA_ARGS__)

static float bgR = 0.1f, bgG = 0.12f, bgB = 0.15f;
static float cubeR = 0.2f, cubeG = 0.8f, cubeB = 0.3f;
static bool showCube = true;
static float rotSpeed = 1.0f;
static float currentAngle = 0.0f;

static GLuint shaderProgram = 0;
static GLint mvpLoc = -1;
static GLint colorLoc = -1;
static float aspect = 1.0f;

// 36 върха за 3D куб (12 триъгълника)
static const float CUBE_VERTICES[] = {
    -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,
     0.5f, -0.5f, -0.5f,   0.5f,  0.5f,  0.5f,   0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f
};

static const char* VERTEX_SHADER =
    "#version 300 es\n"
    "layout(location = 0) in vec3 aPos;\n"
    "uniform mat4 uMVP;\n"
    "out vec3 vPos;\n"
    "void main() {\n"
    "    vPos = aPos;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vPos;\n"
    "uniform vec3 uColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec3 shade = uColor * (0.6 + 0.4 * abs(vPos));\n"
    "    FragColor = vec4(shade, 1.0);\n"
    "}\n";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceCreated(JNIEnv*, jobject) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    colorLoc = glGetUniformLocation(shaderProgram, "uColor");

    glEnable(GL_DEPTH_TEST);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0, 0, w, h);
    aspect = (float)w / (float)(h > 0 ? h : 1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    glClearColor(bgR, bgG, bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!showCube || shaderProgram == 0) return;

    currentAngle += 0.02f * rotSpeed;

    float radY = currentAngle;
    float radX = currentAngle * 0.6f;
    float cosY = cosf(radY), sinY = sinf(radY);
    float cosX = cosf(radX), sinX = sinf(radX);

    // Перспективна матрица + Ротация и отдалечаване Z = -2.5
    float fov = 1.0f / tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    float zDist = 2.5f;

    float mvp[16] = {
        (fov / aspect) * cosY,  (fov / aspect) * sinY * sinX,  -cosY * sinX / zDist,  0.0f,
        0.0f,                   fov * cosX,                     -sinX / zDist,         0.0f,
        (fov / aspect) * -sinY, (fov / aspect) * cosY * sinX,  -cosY * cosX / zDist,  -1.0f / zDist,
        0.0f,                   0.0f,                           1.0f,                  1.0f
    };

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp);
    glUniform3f(colorLoc, cubeR, cubeG, cubeB);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, CUBE_VERTICES);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDisableVertexAttribArray(0);
}

// JNI функции за контрол от AI
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setBackgroundColor(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    bgR = r; bgG = g; bgB = b;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setCubeColor(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    cubeR = r; cubeG = g; cubeB = b;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setCubeVisible(JNIEnv*, jobject, jboolean visible) {
    showCube = visible;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setRotationSpeed(JNIEnv*, jobject, jfloat speed) {
    rotSpeed = speed;
}
