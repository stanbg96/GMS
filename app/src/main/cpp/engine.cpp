#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <android/log.h>

static float bgR = 0.08f, bgG = 0.09f, bgB = 0.12f;
static float cubeR = 0.1f, cubeG = 0.4f, cubeB = 0.9f; // Красиво синьо
static bool showCube = true;
static float rotSpeed = 0.2f; // По-плавно въртене по подразбиране
static float cubeScale = 0.8f;
static float currentAngle = 0.0f;

static GLuint shaderProgram = 0;
static GLint mvpLoc = -1, modelLoc = -1, colorLoc = -1;
static float aspect = 1.0f;

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
    "uniform mat4 uModel;\n"
    "out vec3 vWorldPos;\n"
    "void main() {\n"
    "    vWorldPos = vec3(uModel * vec4(aPos, 1.0));\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vWorldPos;\n"
    "uniform vec3 uColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec3 N = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));\n"
    "    vec3 L = normalize(vec3(0.5, 1.0, 0.7));\n"
    "    float diff = max(dot(N, L), 0.25);\n"
    "    FragColor = vec4(uColor * diff, 1.0);\n"
    "}\n";

static void mat4_identity(float* m) {
    for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

static void mat4_mul(float* out, const float* a, const float* b) {
    float temp[16];
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            temp[c * 4 + r] =
                a[0 * 4 + r] * b[c * 4 + 0] +
                a[1 * 4 + r] * b[c * 4 + 1] +
                a[2 * 4 + r] * b[c * 4 + 2] +
                a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
    for (int i = 0; i < 16; i++) out[i] = temp[i];
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceCreated(JNIEnv*, jobject) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &VERTEX_SHADER, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &FRAGMENT_SHADER, nullptr);
    glCompileShader(fs);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    modelLoc = glGetUniformLocation(shaderProgram, "uModel");
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

    // 1. Perspective Projection
    float P[16];
    mat4_identity(P);
    float tanHalf = tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    P[0] = 1.0f / (aspect * tanHalf);
    P[5] = 1.0f / tanHalf;
    P[10] = -(100.0f + 0.1f) / (100.0f - 0.1f);
    P[11] = -1.0f;
    P[14] = -(2.0f * 100.0f * 0.1f) / (100.0f - 0.1f);
    P[15] = 0.0f;

    // 2. Model Matrix (Мащаб, Ротация, Отдалечаване назад на Z = -3.2)
    float M[16], R[16], S[16];
    mat4_identity(M);
    M[14] = -3.2f; // Камерата е назад

    mat4_identity(S);
    S[0] = cubeScale; S[5] = cubeScale; S[10] = cubeScale;

    mat4_identity(R);
    float cY = cosf(currentAngle), sY = sinf(currentAngle);
    float cX = cosf(currentAngle * 0.7f), sX = sinf(currentAngle * 0.7f);
    R[0] = cY; R[2] = sY; R[5] = cX; R[6] = -sX; R[8] = -sY; R[9] = sX; R[10] = cY * cX;

    float model[16], mvp[16];
    mat4_mul(model, M, R);
    mat4_mul(model, model, S);
    mat4_mul(mvp, P, model);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
    glUniform3f(colorLoc, cubeR, cubeG, cubeB);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, CUBE_VERTICES);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDisableVertexAttribArray(0);
}

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

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setCubeScale(JNIEnv*, jobject, jfloat scale) {
    cubeScale = scale;
}
