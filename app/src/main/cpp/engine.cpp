#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>

static float bgR = 0.18f, bgG = 0.20f, bgB = 0.24f; // Модерен Godot Dark Studio цвят
static float aspect = 1.0f;

// Камера (позиция и ъгъл)
static float camX = 0.0f, camY = 6.0f, camZ = 13.0f;
static float camPitch = -0.38f;
static float camYaw = 0.0f;

static int gameScore = 0;
static bool gameWon = false;
static float startX = 0.0f, startY = 0.6f, startZ = 0.0f;

enum Behavior { B_STATIC = 0, B_PLAYER = 1, B_SPIN = 2, B_PATROL_X = 3, B_PATROL_Z = 4 };
enum TouchRule { T_NONE = 0, T_COLLECT = 1, T_HAZARD = 2, T_WIN = 3 };

struct Entity {
    bool active;
    float x, y, z;
    float sx, sy, sz;
    float rotY;
    float r, g, b;
    int behavior;
    int touchRule;
    float origin;
};

#define MAX_ENTITIES 96
static Entity entities[MAX_ENTITIES];

static GLuint shaderProg = 0;
static GLint mvpLoc = -1, colorLoc = -1, useLightLoc = -1;

#define GRID_LINES 42
static float gridVerts[GRID_LINES * 2 * 3];

// 3D оси: Червена (X), Зелена (Y), Синя (Z) точно като в Godot
static const float AXIS_VERTS[] = {
    -12.0f, 0.01f,  0.0f,   12.0f, 0.01f,  0.0f, // X ос
      0.0f, 0.01f,-12.0f,    0.0f, 0.01f, 12.0f, // Z ос
      0.0f, 0.0f,   0.0f,    0.0f,  4.0f,  0.0f  // Y ос (нагоре)
};

static const float CUBE[] = {
    -0.5f,-0.5f, 0.5f, 0,0,1,  0.5f,-0.5f, 0.5f, 0,0,1,  0.5f, 0.5f, 0.5f, 0,0,1,
    -0.5f,-0.5f, 0.5f, 0,0,1,  0.5f, 0.5f, 0.5f, 0,0,1, -0.5f, 0.5f, 0.5f, 0,0,1,
    -0.5f,-0.5f,-0.5f, 0,0,-1, -0.5f, 0.5f,-0.5f, 0,0,-1, 0.5f, 0.5f,-0.5f, 0,0,-1,
    -0.5f,-0.5f,-0.5f, 0,0,-1,  0.5f, 0.5f,-0.5f, 0,0,-1, 0.5f,-0.5f,-0.5f, 0,0,-1,
    -0.5f, 0.5f,-0.5f, 0,1,0, -0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f, 0.5f, 0,1,0,
    -0.5f, 0.5f,-0.5f, 0,1,0,  0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f,-0.5f, 0,1,0,
    -0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f, 0.5f, 0,-1,0,
    -0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f, 0.5f, 0,-1,0,-0.5f,-0.5f, 0.5f, 0,-1,0,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f, 0.5f,-0.5f, 1,0,0,  0.5f, 0.5f, 0.5f, 1,0,0,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f, 0.5f, 0.5f, 1,0,0,  0.5f,-0.5f, 0.5f, 1,0,0,
    -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f,-0.5f, 0.5f,-1,0,0, -0.5f, 0.5f, 0.5f,-1,0,0,
    -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f, 0.5f, 0.5f,-1,0,0, -0.5f, 0.5f,-0.5f,-1,0,0
};

static const char* VS =
    "#version 300 es\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNorm;\n"
    "uniform mat4 uMVP;\n"
    "out vec3 vN;\n"
    "void main() {\n"
    "    vN = aNorm;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* FS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vN;\n"
    "uniform vec3 uCol;\n"
    "uniform int uUseLight;\n"
    "out vec4 oC;\n"
    "void main() {\n"
    "    if (uUseLight == 1) {\n"
    "        float d = max(dot(normalize(vN), normalize(vec3(0.4, 0.9, 0.5))), 0.0) * 0.60 + 0.40;\n"
    "        oC = vec4(uCol * d, 1.0);\n"
    "    } else {\n"
    "        oC = vec4(uCol, 1.0);\n" // За мрежата и осите – чист ясен цвят без бъгове
    "    }\n"
    "}\n";

static void mat4_identity(float* m) {
    for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

static void mat4_mul(float* out, const float* a, const float* b) {
    float t[16];
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            t[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] +
                           a[1 * 4 + r] * b[c * 4 + 1] +
                           a[2 * 4 + r] * b[c * 4 + 2] +
                           a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
    for (int i = 0; i < 16; i++) out[i] = t[i];
}

static void mat4_lookat(float* m, float ex, float ey, float ez, float tx, float ty, float tz, float ux, float uy, float uz) {
    float fx = tx - ex, fy = ty - ey, fz = tz - ez;
    float rlf = 1.0f / sqrtf(fx*fx + fy*fy + fz*fz);
    fx *= rlf; fy *= rlf; fz *= rlf;

    float rx = fy * uz - fz * uy;
    float ry = fz * ux - fx * uz;
    float rz = fx * uy - fy * ux;
    float rlr = 1.0f / sqrtf(rx*rx + ry*ry + rz*rz);
    rx *= rlr; ry *= rlr; rz *= rlr;

    float ux2 = ry * fz - rz * fy;
    float uy2 = rz * fx - rx * fz;
    float uz2 = rx * fy - ry * fx;

    m[0] = rx;  m[1] = ux2; m[2] = -fx; m[3] = 0.0f;
    m[4] = ry;  m[5] = uy2; m[6] = -fy; m[7] = 0.0f;
    m[8] = rz;  m[9] = uz2; m[10]= -fz; m[11]= 0.0f;
    m[12]= -(rx*ex + ry*ey + rz*ez);
    m[13]= -(ux2*ex + uy2*ey + uz2*ez);
    m[14]= (fx*ex + fy*ey + fz*ez);
    m[15]= 1.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceCreated(JNIEnv*, jobject) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &VS, nullptr); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &FS, nullptr); glCompileShader(fs);
    shaderProg = glCreateProgram();
    glAttachShader(shaderProg, vs); glAttachShader(shaderProg, fs);
    glLinkProgram(shaderProg);

    mvpLoc = glGetUniformLocation(shaderProg, "uMVP");
    colorLoc = glGetUniformLocation(shaderProg, "uCol");
    useLightLoc = glGetUniformLocation(shaderProg, "uUseLight");
    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for (int i = -10; i <= 10; i++) {
        gridVerts[idx++] = (float)i; gridVerts[idx++] = 0.005f; gridVerts[idx++] = -10.0f;
        gridVerts[idx++] = (float)i; gridVerts[idx++] = 0.005f; gridVerts[idx++] =  10.0f;
        gridVerts[idx++] = -10.0f;   gridVerts[idx++] = 0.005f; gridVerts[idx++] = (float)i;
        gridVerts[idx++] =  10.0f;   gridVerts[idx++] = 0.005f; gridVerts[idx++] = (float)i;
    }

    // Стартов играч в центъра, за да има видим 3D свят още при отваряне
    entities[0] = { true, 0.0f, 0.6f, 0.0f, 1.2f, 1.2f, 1.2f, 0.0f, 0.0f, 0.85f, 0.95f, B_PLAYER, T_NONE, 0.0f };
    for (int i = 1; i < MAX_ENTITIES; i++) entities[i].active = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0, 0, w, h);
    aspect = (float)w / (float)(h > 0 ? h : 1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    static float timeTicks = 0.0f;
    timeTicks += 0.035f;

    int pIdx = -1;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entities[i].active) continue;
        if (entities[i].behavior == B_PLAYER) pIdx = i;
        else if (entities[i].behavior == B_SPIN) entities[i].rotY += 3.5f;
        else if (entities[i].behavior == B_PATROL_X) entities[i].x = entities[i].origin + sinf(timeTicks) * 3.0f;
        else if (entities[i].behavior == B_PATROL_Z) entities[i].z = entities[i].origin + sinf(timeTicks) * 3.0f;
    }

    if (pIdx >= 0) {
        camX = entities[pIdx].x;
        camY = entities[pIdx].y + 5.5f;
        camZ = entities[pIdx].z + 10.5f;

        float px = entities[pIdx].x, py = entities[pIdx].y, pz = entities[pIdx].z;
        for (int i = 0; i < MAX_ENTITIES; i++) {
            if (!entities[i].active || i == pIdx) continue;
            float dx = px - entities[i].x, dy = py - entities[i].y, dz = pz - entities[i].z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist < (entities[pIdx].sx + entities[i].sx) * 0.48f) {
                if (entities[i].touchRule == T_COLLECT) { entities[i].active = false; gameScore++; }
                else if (entities[i].touchRule == T_HAZARD) { entities[pIdx].x = startX; entities[pIdx].y = startY; entities[pIdx].z = startZ; }
                else if (entities[i].touchRule == T_WIN) { gameWon = true; }
            }
        }
    }

    glClearColor(bgR, bgG, bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (shaderProg == 0) return;

    // Перспектива
    float P[16]; mat4_identity(P);
    float tanHalf = tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    P[0] = 1.0f / (aspect * tanHalf); P[5] = 1.0f / tanHalf;
    P[10] = -(100.0f + 0.1f) / (100.0f - 0.1f); P[11] = -1.0f;
    P[14] = -(2.0f * 100.0f * 0.1f) / (100.0f - 0.1f); P[15] = 0.0f;

    // Камера LookAt – винаги насочена към центъра на действието
    float V[16];
    float targetObjY = (pIdx >= 0) ? entities[pIdx].y : 0.8f;
    mat4_lookat(V, camX, camY, camZ, (pIdx >= 0) ? entities[pIdx].x : 0.0f, targetObjY, (pIdx >= 0) ? entities[pIdx].z : 0.0f, 0.0f, 1.0f, 0.0f);

    float VP[16]; mat4_mul(VP, P, V);
    glUseProgram(shaderProg);

    // 1. Изчертаване на 3D мрежата (Grid)
    glUniform1i(useLightLoc, 0); // Без светлосянка, чист цвят
    float gM[16], gMVP[16]; mat4_identity(gM); mat4_mul(gMVP, VP, gM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, gMVP);
    glUniform3f(colorLoc, 0.32f, 0.36f, 0.42f); // Godot Grid цвят

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gridVerts);
    glDrawArrays(GL_LINES, 0, GRID_LINES * 2);

    // 2. Оси (X - Червена, Z - Синя, Y - Зелена)
    glUniform3f(colorLoc, 0.95f, 0.25f, 0.25f); // X ос
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, &AXIS_VERTS[0]);
    glDrawArrays(GL_LINES, 0, 2);

    glUniform3f(colorLoc, 0.25f, 0.55f, 0.95f); // Z ос
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, &AXIS_VERTS[6]);
    glDrawArrays(GL_LINES, 0, 2);

    glUniform3f(colorLoc, 0.25f, 0.90f, 0.35f); // Y ос (височина)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, &AXIS_VERTS[12]);
    glDrawArrays(GL_LINES, 0, 2);

    // 3. Рендиране на 3D фигурите с осветление
    glUniform1i(useLightLoc, 1);
    glEnableVertexAttribArray(1);

    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entities[i].active) continue;

        float rad = entities[i].rotY * 0.017453f;
        float cr = cosf(rad), sr = sinf(rad);

        float M[16], MVP[16]; mat4_identity(M);
        M[0] = cr * entities[i].sx; M[2] = sr * entities[i].sx;
        M[5] = entities[i].sy;
        M[8] = -sr * entities[i].sz; M[10] = cr * entities[i].sz;
        M[12] = entities[i].x; M[13] = entities[i].y; M[14] = entities[i].z;

        mat4_mul(MVP, VP, M);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, MVP);
        glUniform3f(colorLoc, entities[i].r, entities[i].g, entities[i].b);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), CUBE);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), &CUBE[3]);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_movePlayer(JNIEnv*, jobject, jfloat f, jfloat s) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (entities[i].active && entities[i].behavior == B_PLAYER) {
            float speed = 0.28f;
            entities[i].z -= f * speed;
            entities[i].x += s * speed;
            break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_spawnEntity(JNIEnv*, jobject,
        jfloat x, jfloat y, jfloat z, jfloat sx, jfloat sy, jfloat sz,
        jfloat r, jfloat g, jfloat b, jint beh, jint touch) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entities[i].active) {
            entities[i] = { true, x, y, z, sx, sy, sz, 0.0f, r, g, b, beh, touch, (beh == B_PATROL_X) ? x : z };
            if (beh == B_PLAYER) { startX = x; startY = y; startZ = z; }
            break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_clearWorld(JNIEnv*, jobject) {
    for (int i = 0; i < MAX_ENTITIES; i++) entities[i].active = false;
    gameScore = 0; gameWon = false;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_aigame_engine_NativeEngine_getScore(JNIEnv*, jobject) { return gameScore; }

extern "C" JNIEXPORT jboolean JNICALL
Java_com_aigame_engine_NativeEngine_isWon(JNIEnv*, jobject) { return gameWon; }

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setSky(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    bgR = r; bgG = g; bgB = b;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_zoomCamera(JNIEnv*, jobject, jfloat delta) {
    camZ -= delta;
    if (camZ < 5.0f) camZ = 5.0f;
    if (camZ > 30.0f) camZ = 30.0f;
}
