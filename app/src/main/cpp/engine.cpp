#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>

static float bgR = 0.45f, bgG = 0.7f, bgB = 0.95f; // Красиво небе по подразбиране
static float aspect = 1.0f;

static float camYaw = 0.7f;
static float camPitch = 0.45f;
static float camDist = 13.0f; // Отдалечена камера, за да се виждат цели сгради

struct Entity {
    bool active;
    float x, y, z;
    float scale;
    float r, g, b;
};

#define MAX_ENTITIES 128
static Entity entityPool[MAX_ENTITIES];

static GLuint shaderProgram = 0;
static GLint mvpLoc = -1, colorLoc = -1;

#define GRID_LINES 34
static float gridVertices[GRID_LINES * 2 * 3];

// 36 върха: Позиция (X,Y,Z) + Нормали (NX,NY,NZ)
static const float CUBE_DATA[] = {
    // Front (+Z)
    -0.5f,-0.5f, 0.5f,  0,0,1,   0.5f,-0.5f, 0.5f,  0,0,1,   0.5f, 0.5f, 0.5f,  0,0,1,
    -0.5f,-0.5f, 0.5f,  0,0,1,   0.5f, 0.5f, 0.5f,  0,0,1,  -0.5f, 0.5f, 0.5f,  0,0,1,
    // Back (-Z)
    -0.5f,-0.5f,-0.5f,  0,0,-1, -0.5f, 0.5f,-0.5f,  0,0,-1,  0.5f, 0.5f,-0.5f,  0,0,-1,
    -0.5f,-0.5f,-0.5f,  0,0,-1,  0.5f, 0.5f,-0.5f,  0,0,-1,  0.5f,-0.5f,-0.5f,  0,0,-1,
    // Top (+Y)
    -0.5f, 0.5f,-0.5f,  0,1,0,  -0.5f, 0.5f, 0.5f,  0,1,0,   0.5f, 0.5f, 0.5f,  0,1,0,
    -0.5f, 0.5f,-0.5f,  0,1,0,   0.5f, 0.5f, 0.5f,  0,1,0,   0.5f, 0.5f,-0.5f,  0,1,0,
    // Bottom (-Y)
    -0.5f,-0.5f,-0.5f,  0,-1,0,  0.5f,-0.5f,-0.5f,  0,-1,0,  0.5f,-0.5f, 0.5f,  0,-1,0,
    -0.5f,-0.5f,-0.5f,  0,-1,0,  0.5f,-0.5f, 0.5f,  0,-1,0, -0.5f,-0.5f, 0.5f,  0,-1,0,
    // Right (+X)
     0.5f,-0.5f,-0.5f,  1,0,0,   0.5f, 0.5f,-0.5f,  1,0,0,   0.5f, 0.5f, 0.5f,  1,0,0,
     0.5f,-0.5f,-0.5f,  1,0,0,   0.5f, 0.5f, 0.5f,  1,0,0,   0.5f,-0.5f, 0.5f,  1,0,0,
    // Left (-X)
    -0.5f,-0.5f,-0.5f, -1,0,0,  -0.5f,-0.5f, 0.5f, -1,0,0,  -0.5f, 0.5f, 0.5f, -1,0,0,
    -0.5f,-0.5f,-0.5f, -1,0,0,  -0.5f, 0.5f, 0.5f, -1,0,0,  -0.5f, 0.5f,-0.5f, -1,0,0
};

static const char* VERTEX_SHADER =
    "#version 300 es\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "uniform mat4 uMVP;\n"
    "out vec3 vLocalPos;\n"
    "out vec3 vNormal;\n"
    "void main() {\n"
    "    vLocalPos = aPos;\n"
    "    vNormal = aNormal;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vLocalPos;\n"
    "in vec3 vNormal;\n"
    "uniform vec3 uColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    // 3D Осветление според посоката на стената\n"
    "    vec3 L = normalize(vec3(0.4, 0.9, 0.5));\n"
    "    float diff = max(dot(vNormal, L), 0.0) * 0.55 + 0.45;\n"
    "    // Тъмни контури (Outlines) по ръбовете на всеки блок\n"
    "    vec3 d = abs(vLocalPos);\n"
    "    int edges = 0;\n"
    "    if (d.x > 0.44) edges++;\n"
    "    if (d.y > 0.44) edges++;\n"
    "    if (d.z > 0.44) edges++;\n"
    "    float edgeFactor = (edges >= 2) ? 0.35 : 1.0;\n"
    "    FragColor = vec4(uColor * diff * edgeFactor, 1.0);\n"
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
    colorLoc = glGetUniformLocation(shaderProgram, "uColor");

    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for (int i = -8; i <= 8; i++) {
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.0f; gridVertices[idx++] = -8.0f;
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.0f; gridVertices[idx++] =  8.0f;
        gridVertices[idx++] = -8.0f;    gridVertices[idx++] = 0.0f; gridVertices[idx++] = (float)i;
        gridVertices[idx++] =  8.0f;    gridVertices[idx++] = 0.0f; gridVertices[idx++] = (float)i;
    }

    for (int i = 0; i < MAX_ENTITIES; i++) entityPool[i].active = false;
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

    if (shaderProgram == 0) return;

    float P[16];
    mat4_identity(P);
    float tanHalf = tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    P[0] = 1.0f / (aspect * tanHalf);
    P[5] = 1.0f / tanHalf;
    P[10] = -(100.0f + 0.1f) / (100.0f - 0.1f);
    P[11] = -1.0f;
    P[14] = -(2.0f * 100.0f * 0.1f) / (100.0f - 0.1f);
    P[15] = 0.0f;

    float V[16], RotX[16], RotY[16], Trans[16];
    mat4_identity(Trans); Trans[14] = -camDist;

    mat4_identity(RotX);
    float cX = cosf(camPitch), sX = sinf(camPitch);
    RotX[5] = cX; RotX[6] = -sX; RotX[9] = sX; RotX[10] = cX;

    mat4_identity(RotY);
    float cY = cosf(camYaw), sY = sinf(camYaw);
    RotY[0] = cY; RotY[2] = sY; RotY[8] = -sY; RotY[10] = cY;

    mat4_mul(V, Trans, RotX);
    mat4_mul(V, V, RotY);

    float VP[16];
    mat4_mul(VP, P, V);

    glUseProgram(shaderProgram);

    // 1. Grid
    float gridM[16], gridMVP[16];
    mat4_identity(gridM);
    mat4_mul(gridMVP, VP, gridM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, gridMVP);
    glUniform3f(colorLoc, 0.35f, 0.45f, 0.55f);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gridVertices);
    glDrawArrays(GL_LINES, 0, GRID_LINES * 2);

    // 2. Всички кубове с нормали и контури
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entityPool[i].active) continue;

        float M[16], S[16], T[16], MVP[16];
        mat4_identity(T);
        T[12] = entityPool[i].x;
        T[13] = entityPool[i].y;
        T[14] = entityPool[i].z;

        mat4_identity(S);
        float sc = entityPool[i].scale;
        S[0] = sc; S[5] = sc; S[10] = sc;

        mat4_mul(M, T, S);
        mat4_mul(MVP, VP, M);

        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, MVP);
        glUniform3f(colorLoc, entityPool[i].r, entityPool[i].g, entityPool[i].b);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), CUBE_DATA);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), &CUBE_DATA[3]);

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_rotateCamera(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    camYaw += dx;
    camPitch += dy;
    if (camPitch > 1.5f) camPitch = 1.5f;
    if (camPitch < 0.05f) camPitch = 0.05f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_zoomCamera(JNIEnv*, jobject, jfloat zoom) {
    camDist += zoom;
    if (camDist < 3.0f) camDist = 3.0f;
    if (camDist > 40.0f) camDist = 40.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_clearWorld(JNIEnv*, jobject) {
    for (int i = 0; i < MAX_ENTITIES; i++) entityPool[i].active = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_spawnCube(JNIEnv*, jobject, jfloat x, jfloat y, jfloat z, jfloat scale, jfloat r, jfloat g, jfloat b) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entityPool[i].active) {
            entityPool[i] = { true, x, y, z, scale, r, g, b };
            break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setBackgroundColor(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    bgR = r; bgG = g; bgB = b;
}
