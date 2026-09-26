#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>

static float bgR = 0.20f, bgG = 0.22f, bgB = 0.26f; // Тъмно модерен студиен цвят (Godot style)
static float aspect = 1.0f;
static int screenW = 1080, screenH = 1920;

// Свободна FPS / Editor Камера
static float camX = 0.0f, camY = 2.5f, camZ = 8.0f;
static float camYaw = 0.0f;
static float camPitch = -0.25f;

// Обекти в сцената
struct EditorObject {
    bool active;
    int type; // 0: Къща/Блок, 1: Кола/Болид, 2: Кула/Колона
    float x, y, z;
    float scale;
    float rotY; // Завъртане в градуси
    float r, g, b;
};

#define MAX_OBJECTS 64
static EditorObject sceneObjects[MAX_OBJECTS];
static int selectedObjectIndex = -1; // -1 = нищо не е избрано

static GLuint shaderProgram = 0;
static GLint mvpLoc = -1, colorLoc = -1, isSelectedLoc = -1;

#define GRID_LINES 42
static float gridVertices[GRID_LINES * 2 * 3];

static const float CUBE_DATA[] = {
    -0.5f,-0.5f, 0.5f,  0,0,1,   0.5f,-0.5f, 0.5f,  0,0,1,   0.5f, 0.5f, 0.5f,  0,0,1,
    -0.5f,-0.5f, 0.5f,  0,0,1,   0.5f, 0.5f, 0.5f,  0,0,1,  -0.5f, 0.5f, 0.5f,  0,0,1,
    -0.5f,-0.5f,-0.5f,  0,0,-1, -0.5f, 0.5f,-0.5f,  0,0,-1,  0.5f, 0.5f,-0.5f,  0,0,-1,
    -0.5f,-0.5f,-0.5f,  0,0,-1,  0.5f, 0.5f,-0.5f,  0,0,-1,  0.5f,-0.5f,-0.5f,  0,0,-1,
    -0.5f, 0.5f,-0.5f,  0,1,0,  -0.5f, 0.5f, 0.5f,  0,1,0,   0.5f, 0.5f, 0.5f,  0,1,0,
    -0.5f, 0.5f,-0.5f,  0,1,0,   0.5f, 0.5f, 0.5f,  0,1,0,   0.5f, 0.5f,-0.5f,  0,1,0,
    -0.5f,-0.5f,-0.5f,  0,-1,0,  0.5f,-0.5f,-0.5f,  0,-1,0,  0.5f,-0.5f, 0.5f,  0,-1,0,
    -0.5f,-0.5f,-0.5f,  0,-1,0,  0.5f,-0.5f, 0.5f,  0,-1,0, -0.5f,-0.5f, 0.5f,  0,-1,0,
     0.5f,-0.5f,-0.5f,  1,0,0,   0.5f, 0.5f,-0.5f,  1,0,0,   0.5f, 0.5f, 0.5f,  1,0,0,
     0.5f,-0.5f,-0.5f,  1,0,0,   0.5f, 0.5f, 0.5f,  1,0,0,   0.5f,-0.5f, 0.5f,  1,0,0,
    -0.5f,-0.5f,-0.5f, -1,0,0,  -0.5f,-0.5f, 0.5f, -1,0,0,  -0.5f, 0.5f, 0.5f, -1,0,0,
    -0.5f,-0.5f,-0.5f, -1,0,0,  -0.5f, 0.5f, 0.5f, -1,0,0,  -0.5f, 0.5f,-0.5f, -1,0,0
};

static const char* VERTEX_SHADER =
    "#version 300 es\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "uniform mat4 uMVP;\n"
    "out vec3 vNormal;\n"
    "void main() {\n"
    "    vNormal = aNormal;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vNormal;\n"
    "uniform vec3 uColor;\n"
    "uniform int uIsSelected;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec3 N = normalize(vNormal);\n"
    "    vec3 L = normalize(vec3(0.4, 0.9, 0.5));\n"
    "    float diff = max(dot(N, L), 0.0) * 0.55 + 0.45;\n"
    "    vec3 col = uColor * diff;\n"
    "    // Маркиране на избрания обект в златист контур/цвят\n"
    "    if (uIsSelected == 1) {\n"
    "        col = mix(col, vec3(1.0, 0.85, 0.2), 0.55);\n"
    "    }\n"
    "    FragColor = vec4(col, 1.0);\n"
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
    isSelectedLoc = glGetUniformLocation(shaderProgram, "uIsSelected");

    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for (int i = -10; i <= 10; i++) {
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.0f; gridVertices[idx++] = -10.0f;
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.0f; gridVertices[idx++] =  10.0f;
        gridVertices[idx++] = -10.0f;   gridVertices[idx++] = 0.0f; gridVertices[idx++] = (float)i;
        gridVertices[idx++] =  10.0f;   gridVertices[idx++] = 0.0f; gridVertices[idx++] = (float)i;
    }

    // Стартови 3D обекти за редактора
    for (int i = 0; i < MAX_OBJECTS; i++) sceneObjects[i].active = false;

    // Обект 1: Червена спортна кола
    sceneObjects[0] = { true, 1, 0.0f, 0.5f, 0.0f, 1.2f, 0.0f, 0.9f, 0.2f, 0.2f };
    // Обект 2: Синя сграда
    sceneObjects[1] = { true, 0, -3.5f, 1.0f, -2.0f, 1.8f, 0.0f, 0.2f, 0.5f, 0.9f };
    // Обект 3: Зелена кула
    sceneObjects[2] = { true, 2,  3.5f, 1.5f, -1.0f, 1.0f, 0.0f, 0.2f, 0.8f, 0.3f };
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0, 0, w, h);
    screenW = w;
    screenH = h;
    aspect = (float)w / (float)(h > 0 ? h : 1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    glClearColor(bgR, bgG, bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (shaderProgram == 0) return;

    // Перспективна матрица
    float P[16];
    mat4_identity(P);
    float tanHalf = tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    P[0] = 1.0f / (aspect * tanHalf);
    P[5] = 1.0f / tanHalf;
    P[10] = -(100.0f + 0.1f) / (100.0f - 0.1f);
    P[11] = -1.0f;
    P[14] = -(2.0f * 100.0f * 0.1f) / (100.0f - 0.1f);
    P[15] = 0.0f;

    // View матрица за свободна FPS камера
    float cP = cosf(camPitch), sP = sinf(camPitch);
    float cY = cosf(camYaw),   sY = sinf(camYaw);

    float forwardX = cP * sY, forwardY = sP, forwardZ = -cP * cY;
    float rightX = cY, rightY = 0.0f, rightZ = sY;
    float upX = -sP * sY, upY = cP, upZ = sP * cY;

    float V[16];
    mat4_identity(V);
    V[0] = rightX;   V[1] = upX;   V[2] = -forwardX;  V[3] = 0.0f;
    V[4] = rightY;   V[5] = upY;   V[6] = -forwardY;  V[7] = 0.0f;
    V[8] = rightZ;   V[9] = upZ;   V[10]= -forwardZ;  V[11]= 0.0f;
    V[12]= -(rightX*camX + rightY*camY + rightZ*camZ);
    V[13]= -(upX*camX + upY*camY + upZ*camZ);
    V[14]=  (forwardX*camX + forwardY*camY + forwardZ*camZ);
    V[15]= 1.0f;

    float VP[16];
    mat4_mul(VP, P, V);

    glUseProgram(shaderProgram);

    // 1. Grid (Координатна мрежа на редактора)
    float gridM[16], gridMVP[16];
    mat4_identity(gridM);
    mat4_mul(gridMVP, VP, gridM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, gridMVP);
    glUniform3f(colorLoc, 0.38f, 0.42f, 0.48f);
    glUniform1i(isSelectedLoc, 0);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gridVertices);
    glDrawArrays(GL_LINES, 0, GRID_LINES * 2);

    // 2. Рендиране на 3D обектите в редактора
    glEnableVertexAttribArray(1);
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!sceneObjects[i].active) continue;

        float rad = sceneObjects[i].rotY * 0.0174532925f;
        float cR = cosf(rad), sR = sinf(rad);

        float M[16], MVP[16];
        mat4_identity(M);
        M[0] = cR * sceneObjects[i].scale;
        M[2] = sR * sceneObjects[i].scale;
        M[5] = sceneObjects[i].scale;
        M[8] = -sR * sceneObjects[i].scale;
        M[10]= cR * sceneObjects[i].scale;
        M[12]= sceneObjects[i].x;
        M[13]= sceneObjects[i].y;
        M[14]= sceneObjects[i].z;

        mat4_mul(MVP, VP, M);

        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, MVP);
        glUniform3f(colorLoc, sceneObjects[i].r, sceneObjects[i].g, sceneObjects[i].b);
        glUniform1i(isSelectedLoc, (i == selectedObjectIndex) ? 1 : 0);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), CUBE_DATA);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), &CUBE_DATA[3]);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

// ДВИЖЕНИЕ НА КАМЕРАТА (чрез Джойстик)
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_moveCamera(JNIEnv*, jobject, jfloat forwardInput, jfloat strafeInput) {
    float cY = cosf(camYaw), sY = sinf(camYaw);
    float speed = 0.18f;

    // Движение спрямо посоката на погледа
    camX += (sY * forwardInput + cY * strafeInput) * speed;
    camZ += (-cY * forwardInput + sY * strafeInput) * speed;
}

// ЗАВЪРТАНЕ НА ПОГЛЕДА (чрез Touch Drag)
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_rotateLook(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    camYaw += dx;
    camPitch += dy;
    if (camPitch > 1.3f) camPitch = 1.3f;
    if (camPitch < -1.3f) camPitch = -1.3f;
}

// RAYCASTING: Клик върху обект за селекция
extern "C" JNIEXPORT jint JNICALL
Java_com_aigame_engine_NativeEngine_pickObject(JNIEnv*, jobject, jfloat tapX, jfloat tapY) {
    if (screenW <= 0 || screenH <= 0) return -1;

    float ndcX = (2.0f * tapX) / (float)screenW - 1.0f;
    float ndcY = 1.0f - (2.0f * tapY) / (float)screenH;

    float tanHalf = tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    float cP = cosf(camPitch), sP = sinf(camPitch);
    float cY = cosf(camYaw),   sY = sinf(camYaw);

    float fX = cP * sY, fY = sP, fZ = -cP * cY;
    float rX = cY, rY = 0.0f, rZ = sY;
    float uX = -sP * sY, uY = cP, uZ = sP * cY;

    // Вектор на лъча от камерата в пространството
    float rayX = fX + rX * (ndcX * tanHalf * aspect) + uX * (ndcY * tanHalf);
    float rayY = fY + rY * (ndcX * tanHalf * aspect) + uY * (ndcY * tanHalf);
    float rayZ = fZ + rZ * (ndcX * tanHalf * aspect) + uZ * (ndcY * tanHalf);
    float len = sqrtf(rayX*rayX + rayY*rayY + rayZ*rayZ);
    rayX /= len; rayY /= len; rayZ /= len;

    // Търсене на най-близкия обект, пресечен от лъча
    int closestIdx = -1;
    float minT = 1e9f;

    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!sceneObjects[i].active) continue;

        float ox = sceneObjects[i].x - camX;
        float oy = sceneObjects[i].y - camY;
        float oz = sceneObjects[i].z - camZ;

        float dot = ox * rayX + oy * rayY + oz * rayZ;
        if (dot < 0.0f) continue; // Зад камерата

        float perpDistSq = (ox*ox + oy*oy + oz*oz) - (dot * dot);
        float radius = sceneObjects[i].scale * 0.75f;

        if (perpDistSq <= (radius * radius)) {
            if (dot < minT) {
                minT = dot;
                closestIdx = i;
            }
        }
    }

    selectedObjectIndex = closestIdx;
    return selectedObjectIndex;
}

// ОПЕРАЦИИ ВЪРХУ ИЗБРАНИЯ ОБЕКТ
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_deleteSelected(JNIEnv*, jobject) {
    if (selectedObjectIndex >= 0 && selectedObjectIndex < MAX_OBJECTS) {
        sceneObjects[selectedObjectIndex].active = false;
        selectedObjectIndex = -1;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_duplicateSelected(JNIEnv*, jobject) {
    if (selectedObjectIndex < 0 || selectedObjectIndex >= MAX_OBJECTS) return;
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!sceneObjects[i].active) {
            sceneObjects[i] = sceneObjects[selectedObjectIndex];
            sceneObjects[i].x += 1.2f;
            sceneObjects[i].z += 1.2f;
            selectedObjectIndex = i; // Маркира новото копие
            break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_scaleSelected(JNIEnv*, jobject, jfloat factor) {
    if (selectedObjectIndex >= 0 && selectedObjectIndex < MAX_OBJECTS) {
        sceneObjects[selectedObjectIndex].scale *= factor;
        if (sceneObjects[selectedObjectIndex].scale < 0.2f) sceneObjects[selectedObjectIndex].scale = 0.2f;
        if (sceneObjects[selectedObjectIndex].scale > 8.0f) sceneObjects[selectedObjectIndex].scale = 8.0f;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_rotateSelected(JNIEnv*, jobject, jfloat deg) {
    if (selectedObjectIndex >= 0 && selectedObjectIndex < MAX_OBJECTS) {
        sceneObjects[selectedObjectIndex].rotY += deg;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_moveSelectedY(JNIEnv*, jobject, jfloat deltaY) {
    if (selectedObjectIndex >= 0 && selectedObjectIndex < MAX_OBJECTS) {
        sceneObjects[selectedObjectIndex].y += deltaY;
        if (sceneObjects[selectedObjectIndex].y < 0.2f) sceneObjects[selectedObjectIndex].y = 0.2f;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_spawnNewObject(JNIEnv*, jobject) {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!sceneObjects[i].active) {
            // Поставя нов обект точно пред камерата на земята
            float fX = sinf(camYaw), fZ = -cosf(camYaw);
            sceneObjects[i] = { true, 0, camX + fX * 4.0f, 0.7f, camZ + fZ * 4.0f, 1.2f, 0.0f, 0.9f, 0.5f, 0.1f };
            selectedObjectIndex = i;
            break;
        }
    }
}
