#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>
#include <cstring>

static float bgR = 0.52f, bgG = 0.65f, bgB = 0.80f;
static float aspect = 1.0f;

static float camYaw = 0.85f;
static float camPitch = 0.45f;
static float camDist = 14.0f;
static float targetY = 0.8f;

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b;
};

static std::vector<Vertex> meshVertices;

static GLuint shaderProgram = 0;
static GLint mvpLoc = -1, eyePosLoc = -1;

#define GRID_LINES 34
static float gridVertices[GRID_LINES * 2 * 3];

static const float FLOOR_VERTICES[] = {
    -20.0f, 0.0f, -20.0f,  0,1,0,   20.0f, 0.0f, -20.0f,  0,1,0,   20.0f, 0.0f,  20.0f,  0,1,0,
    -20.0f, 0.0f, -20.0f,  0,1,0,   20.0f, 0.0f,  20.0f,  0,1,0,  -20.0f, 0.0f,  20.0f,  0,1,0
};

static const float AXIS_VERTICES[] = {
    -15.0f, 0.01f, 0.0f,   15.0f, 0.01f, 0.0f,
     0.0f,  0.01f,-15.0f,   0.0f,  0.01f, 15.0f
};

static const char* VERTEX_SHADER =
    "#version 300 es\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "layout(location = 2) in vec3 aColor;\n"
    "uniform mat4 uMVP;\n"
    "out vec3 vWorldPos;\n"
    "out vec3 vNormal;\n"
    "out vec3 vColor;\n"
    "void main() {\n"
    "    vWorldPos = aPos;\n"
    "    vNormal = aNormal;\n"
    "    vColor = aColor;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vWorldPos;\n"
    "in vec3 vNormal;\n"
    "in vec3 vColor;\n"
    "uniform vec3 uEyePos;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec3 N = normalize(vNormal);\n"
    "    vec3 L = normalize(vec3(0.4, 0.9, 0.5));\n"
    "    vec3 V = normalize(uEyePos - vWorldPos);\n"
    "    vec3 H = normalize(L + V);\n"
    "    float diff = max(dot(N, L), 0.0) * 0.65 + 0.35;\n"
    "    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.50;\n"
    "    vec3 finalCol = vColor * diff + vec3(spec);\n"
    "    FragColor = vec4(finalCol, 1.0);\n"
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

static void addTri(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    meshVertices.push_back(v1);
    meshVertices.push_back(v2);
    meshVertices.push_back(v3);
}

// Изчисляване на нормала за плавен триъгълник
static void addQuad(float x1, float y1, float z1,
                    float x2, float y2, float z2,
                    float x3, float y3, float z3,
                    float x4, float y4, float z4,
                    float r, float g, float b) {
    float ux = x2 - x1, uy = y2 - y1, uz = z2 - z1;
    float vx = x3 - x1, vy = y3 - y1, vz = z3 - z1;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float len = sqrtf(nx*nx + ny*ny + nz*nz);
    if (len > 0.0001f) { nx /= len; ny /= len; nz /= len; }

    Vertex v1 = { x1, y1, z1, nx, ny, nz, r, g, b };
    Vertex v2 = { x2, y2, z2, nx, ny, nz, r, g, b };
    Vertex v3 = { x3, y3, z3, nx, ny, nz, r, g, b };
    Vertex v4 = { x4, y4, z4, nx, ny, nz, r, g, b };

    addTri(v1, v2, v3);
    addTri(v1, v3, v4);
}

// 1. Истинско 3D колело с джанта и спици
static void addWheel(float cx, float cy, float cz, float radius, float width) {
    const int segs = 16;
    float hw = width * 0.5f;

    for (int i = 0; i < segs; i++) {
        float a0 = (float)i * 6.2831853f / (float)segs;
        float a1 = (float)(i + 1) * 6.2831853f / (float)segs;
        float y0 = cosf(a0) * radius, z0 = sinf(a0) * radius;
        float y1 = cosf(a1) * radius, z1 = sinf(a1) * radius;

        // Черна гума (Tire tread)
        addQuad(cx - hw, cy + y0, cz + z0,
                cx + hw, cy + y0, cz + z0,
                cx + hw, cy + y1, cz + z1,
                cx - hw, cy + y1, cz + z1,
                0.12f, 0.12f, 0.12f);

        // Сребриста алуминиева джанта
        float rimR = radius * 0.65f;
        float ry0 = cosf(a0) * rimR, rz0 = sinf(a0) * rimR;
        float ry1 = cosf(a1) * rimR, rz1 = sinf(a1) * rimR;

        addQuad(cx + hw * 1.01f, cy + ry0, cz + rz0,
                cx + hw * 1.01f, cy + y0, cz + z0,
                cx + hw * 1.01f, cy + y1, cz + z1,
                cx + hw * 1.01f, cy + ry1, cz + rz1,
                0.80f, 0.82f, 0.85f);
    }
}

// 2. ХАКЕРСКИ ПАРАМЕТРИЧЕН ЛОФТИНГ НА СПОРТЕН АВТОМОБИЛ (Гладка аеродинамична форма)
static void generateProceduralCar(float r, float g, float b) {
    meshVertices.clear();

    // 4 истински кръгли колела с джанти
    addWheel(-1.15f, 0.42f,  1.35f, 0.45f, 0.30f);
    addWheel( 1.15f, 0.42f,  1.35f, 0.45f, 0.30f);
    addWheel(-1.15f, 0.42f, -1.35f, 0.48f, 0.34f);
    addWheel( 1.15f, 0.42f, -1.35f, 0.48f, 0.34f);

    // Секции по дължината на колата (Z): от предна броня (+2.2) до спойлер (-2.2)
    struct CarStation {
        float z;
        float yBottom, yTop;
        float halfWidth;
    };

    CarStation stations[] = {
        {  2.3f, 0.25f, 0.55f, 0.95f }, // 0: Предна броня
        {  1.6f, 0.28f, 0.82f, 1.05f }, // 1: Преден капак (по-широк над гумите)
        {  0.7f, 0.30f, 1.00f, 1.02f }, // 2: Основа на предното стъкло
        { -0.1f, 0.30f, 1.45f, 0.88f }, // 3: Връх на предно стъкло / таван
        { -0.9f, 0.30f, 1.40f, 0.88f }, // 4: Задно стъкло
        { -1.7f, 0.30f, 0.95f, 1.10f }, // 5: Заден капак (мускулест калник)
        { -2.3f, 0.35f, 0.85f, 1.00f }  // 6: Задна броня
    };

    int numStations = 7;

    for (int i = 0; i < numStations - 1; i++) {
        CarStation s0 = stations[i];
        CarStation s1 = stations[i + 1];

        float colR = r, colG = g, colB = b;
        // Кабината (между станция 2 и 4) е с тъмни стъкла
        if (i == 2 || i == 3) {
            colR = 0.25f; colG = 0.55f; colB = 0.75f;
        }

        // Горна аеродинамична повърхност (капак, таван, багажник)
        addQuad(-s0.halfWidth * 0.7f, s0.yTop, s0.z,
                 s0.halfWidth * 0.7f, s0.yTop, s0.z,
                 s1.halfWidth * 0.7f, s1.yTop, s1.z,
                -s1.halfWidth * 0.7f, s1.yTop, s1.z,
                colR, colG, colB);

        // Странични заоблени панели (калници и врати)
        // Лява страна:
        addQuad(-s0.halfWidth, s0.yBottom, s0.z,
                -s0.halfWidth * 0.7f, s0.yTop, s0.z,
                -s1.halfWidth * 0.7f, s1.yTop, s1.z,
                -s1.halfWidth, s1.yBottom, s1.z,
                r * 0.9f, g * 0.9f, b * 0.9f);

        // Дясна страна:
        addQuad( s0.halfWidth * 0.7f, s0.yTop, s0.z,
                 s0.halfWidth, s0.yBottom, s0.z,
                 s1.halfWidth, s1.yBottom, s1.z,
                 s1.halfWidth * 0.7f, s1.yTop, s1.z,
                r * 0.9f, g * 0.9f, b * 0.9f);
    }

    // Предни ксенонови фарове
    addQuad(-0.85f, 0.55f, 2.25f, -0.45f, 0.55f, 2.28f, -0.45f, 0.70f, 2.15f, -0.85f, 0.70f, 2.12f, 1.0f, 1.0f, 0.8f);
    addQuad( 0.45f, 0.55f, 2.28f,  0.85f, 0.55f, 2.25f,  0.85f, 0.70f, 2.12f,  0.45f, 0.70f, 2.15f, 1.0f, 1.0f, 0.8f);

    // Задни LED стопове
    addQuad(-0.90f, 0.70f, -2.31f, 0.90f, 0.70f, -2.31f, 0.90f, 0.80f, -2.28f, -0.90f, 0.80f, -2.28f, 0.95f, 0.1f, 0.1f);

    // Спортен заден карбонов спойлер
    addQuad(-1.0f, 1.15f, -2.15f, 1.0f, 1.15f, -2.15f, 1.0f, 1.18f, -2.45f, -1.0f, 1.18f, -2.45f, 0.15f, 0.15f, 0.15f);
}

// 3. БЪЛГАРСКА ВЪЗРОЖДЕНСКА КЪЩА (Каменен зид, бял еркер, чардак и керемиден 4-скатен покрив)
static void generateProceduralHouse(float r, float g, float b) {
    meshVertices.clear();

    // Каменен приземен етаж (сива каменна зидария)
    addQuad(-2.2f, 0.0f,  2.0f,  2.2f, 0.0f,  2.0f,  2.2f, 1.6f,  2.0f, -2.2f, 1.6f,  2.0f, 0.42f, 0.42f, 0.44f);
    addQuad( 2.2f, 0.0f,  2.0f,  2.2f, 0.0f, -2.0f,  2.2f, 1.6f, -2.0f,  2.2f, 1.6f,  2.0f, 0.38f, 0.38f, 0.40f);
    addQuad( 2.2f, 0.0f, -2.0f, -2.2f, 0.0f, -2.0f, -2.2f, 1.6f, -2.0f,  2.2f, 1.6f, -2.0f, 0.42f, 0.42f, 0.44f);
    addQuad(-2.2f, 0.0f, -2.0f, -2.2f, 0.0f,  2.0f, -2.2f, 1.6f,  2.0f, -2.2f, 1.6f, -2.0f, 0.38f, 0.38f, 0.40f);

    // Изнесен втори етаж (бял еркер с дървени греди, стърчащ навън)
    addQuad(-2.6f, 1.6f,  2.4f,  2.6f, 1.6f,  2.4f,  2.6f, 3.2f,  2.4f, -2.6f, 3.2f,  2.4f, 0.96f, 0.95f, 0.92f);
    addQuad( 2.6f, 1.6f,  2.4f,  2.6f, 1.6f, -2.4f,  2.6f, 3.2f, -2.4f,  2.6f, 3.2f,  2.4f, 0.90f, 0.89f, 0.86f);
    addQuad( 2.6f, 1.6f, -2.4f, -2.6f, 1.6f, -2.4f, -2.6f, 3.2f, -2.4f,  2.6f, 3.2f, -2.4f, 0.96f, 0.95f, 0.92f);
    addQuad(-2.6f, 1.6f, -2.4f, -2.6f, 1.6f,  2.4f, -2.6f, 3.2f,  2.4f, -2.6f, 3.2f, -2.4f, 0.90f, 0.89f, 0.86f);

    // Тъмни дървени носещи греди под еркера
    addQuad(-2.6f, 1.6f, 2.4f, -2.4f, 1.6f, 2.4f, -2.2f, 1.0f, 2.0f, -2.4f, 1.0f, 2.0f, 0.30f, 0.18f, 0.10f);
    addQuad( 2.4f, 1.6f, 2.4f,  2.6f, 1.6f, 2.4f,  2.4f, 1.0f, 2.0f,  2.2f, 1.0f, 2.0f, 0.30f, 0.18f, 0.10f);

    // Автентичен 4-скатен надвесен керемиден покрив (стърчащ 60 см над стените)
    float roofBaseY = 3.2f;
    float roofPeakY = 4.4f;

    // Преден скат:
    addQuad(-3.1f, roofBaseY,  2.8f,  3.1f, roofBaseY,  2.8f,  1.8f, roofPeakY,  0.5f, -1.8f, roofPeakY,  0.5f, 0.78f, 0.32f, 0.18f);
    // Заден скат:
    addQuad( 3.1f, roofBaseY, -2.8f, -3.1f, roofBaseY, -2.8f, -1.8f, roofPeakY, -0.5f,  1.8f, roofPeakY, -0.5f, 0.70f, 0.28f, 0.15f);
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
    eyePosLoc = glGetUniformLocation(shaderProgram, "uEyePos");

    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for (int i = -8; i <= 8; i++) {
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.005f; gridVertices[idx++] = -8.0f;
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.005f; gridVertices[idx++] =  8.0f;
        gridVertices[idx++] = -8.0f;    gridVertices[idx++] = 0.005f; gridVertices[idx++] = (float)i;
        gridVertices[idx++] =  8.0f;    gridVertices[idx++] = 0.005f; gridVertices[idx++] = (float)i;
    }
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

    float eyeX = camDist * cosf(camPitch) * sinf(camYaw);
    float eyeY = camDist * sinf(camPitch) + targetY;
    float eyeZ = camDist * cosf(camPitch) * cosf(camYaw);

    float V[16], VP[16];
    mat4_lookat(V, eyeX, eyeY, eyeZ, 0.0f, targetY, 0.0f, 0.0f, 1.0f, 0.0f);
    mat4_mul(VP, P, V);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, VP);
    glUniform3f(eyePosLoc, eyeX, eyeY, eyeZ);

    if (!meshVertices.empty()) {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshVertices[0].x);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshVertices[0].nx);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshVertices[0].r);

        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)meshVertices.size());

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
}

// JNI функция за процедурно генериране
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_generateModel(JNIEnv* env, jobject, jstring typeStr, jfloat r, jfloat g, jfloat b) {
    const char* str = env->GetStringUTFChars(typeStr, nullptr);

    if (strcmp(str, "CAR") == 0) {
        generateProceduralCar(r, g, b);
    } else if (strcmp(str, "HOUSE") == 0) {
        generateProceduralHouse(r, g, b);
    }

    env->ReleaseStringUTFChars(typeStr, str);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_clearMesh(JNIEnv*, jobject) {
    meshVertices.clear();
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_rotateCamera(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    camYaw += dx;
    camPitch += dy;
    if (camPitch > 1.45f) camPitch = 1.45f;
    if (camPitch < 0.08f) camPitch = 0.08f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_zoomCamera(JNIEnv*, jobject, jfloat zoom) {
    camDist += zoom;
    if (camDist < 2.0f) camDist = 2.0f;
    if (camDist > 40.0f) camDist = 40.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setBackgroundColor(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    bgR = r; bgG = g; bgB = b;
}
