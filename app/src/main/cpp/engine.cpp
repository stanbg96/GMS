#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>

static float bgR = 0.55f, bgG = 0.68f, bgB = 0.82f;
static float aspect = 1.0f;

static float camYaw = 0.85f;
static float camPitch = 0.40f;
static float camDist = 13.0f;
static float targetY = 0.7f;

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
    "    float diff = max(dot(N, L), 0.0) * 0.60 + 0.40;\n"
    "    float spec = pow(max(dot(N, H), 0.0), 36.0) * 0.55;\n"
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

// Добавяне на плавен четириъгълник от 2 триъгълника
static void addSmoothQuad(const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex& v4) {
    meshVertices.push_back(v1);
    meshVertices.push_back(v2);
    meshVertices.push_back(v3);

    meshVertices.push_back(v1);
    meshVertices.push_back(v3);
    meshVertices.push_back(v4);
}

// 1. Истинско 3D колело с джанта и гума
static void addWheel(float cx, float cy, float cz, float radius, float width) {
    const int segs = 16;
    float hw = width * 0.5f;

    for (int i = 0; i < segs; i++) {
        float a0 = (float)i * 6.2831853f / (float)segs;
        float a1 = (float)(i + 1) * 6.2831853f / (float)segs;
        float y0 = cosf(a0) * radius, z0 = sinf(a0) * radius;
        float y1 = cosf(a1) * radius, z1 = sinf(a1) * radius;

        Vertex v1 = { cx - hw, cy + y0, cz + z0, 0.0f, cosf(a0), sinf(a0), 0.12f, 0.12f, 0.12f };
        Vertex v2 = { cx + hw, cy + y0, cz + z0, 0.0f, cosf(a0), sinf(a0), 0.12f, 0.12f, 0.12f };
        Vertex v3 = { cx + hw, cy + y1, cz + z1, 0.0f, cosf(a1), sinf(a1), 0.12f, 0.12f, 0.12f };
        Vertex v4 = { cx - hw, cy + y1, cz + z1, 0.0f, cosf(a1), sinf(a1), 0.12f, 0.12f, 0.12f };
        addSmoothQuad(v1, v2, v3, v4);

        // Джанта
        float rimR = radius * 0.65f;
        Vertex r1 = { cx + hw * 1.01f, cy, cz, 1.0f, 0.0f, 0.0f, 0.85f, 0.88f, 0.90f };
        Vertex r2 = { cx + hw * 1.01f, cy + cosf(a0)*rimR, cz + sinf(a0)*rimR, 1.0f, 0.0f, 0.0f, 0.85f, 0.88f, 0.90f };
        Vertex r3 = { cx + hw * 1.01f, cy + cosf(a1)*rimR, cz + sinf(a1)*rimR, 1.0f, 0.0f, 0.0f, 0.85f, 0.88f, 0.90f };
        meshVertices.push_back(r1); meshVertices.push_back(r2); meshVertices.push_back(r3);
    }
}

// 2. ПАРАМЕТРИЧЕН ЛОФТИНГ НА ИСТИНСКИ СПОРТЕН СУПЕРКАР (Spline Lofting)
static void generateProceduralSupercar(float r, float g, float b) {
    meshVertices.clear();

    // 4 кръгли колела с джанти
    addWheel(-1.18f, 0.42f,  1.40f, 0.44f, 0.32f);
    addWheel( 1.18f, 0.42f,  1.40f, 0.44f, 0.32f);
    addWheel(-1.22f, 0.45f, -1.35f, 0.47f, 0.36f);
    addWheel( 1.22f, 0.45f, -1.35f, 0.47f, 0.36f);

    const int stepsZ = 20; // 20 секции по дължината
    const int stepsS = 12; // 12 точки по всяка извивка

    auto getProfile = [](float t, float& height, float& width) {
        // Силует по дължината (Z): нос -> капак -> предно стъкло -> покрив -> заден спойлер
        if (t < 0.20f) {
            float k = t / 0.20f;
            height = 0.30f + 0.38f * k * k; // Нисък остър нос
            width = 0.85f + 0.30f * sinf(k * 3.14159f); // Предни калници
        } else if (t < 0.42f) {
            float k = (t - 0.20f) / 0.22f;
            height = 0.68f + 0.68f * sinf(k * 1.57f); // Скосено аеродинамично стъкло
            width = 1.05f - 0.10f * k; // Стесняване около кабината
        } else if (t < 0.68f) {
            float k = (t - 0.42f) / 0.26f;
            height = 1.36f + 0.04f * sinf(k * 3.14159f); // Нисък спортен покрив
            width = 0.95f + 0.30f * k; // Разширяване към задните калници
        } else if (t < 0.88f) {
            float k = (t - 0.68f) / 0.20f;
            height = 1.36f - 0.55f * k; // Плавно спускане на задното стъкло (Fastback)
            width = 1.25f; // Широка мускулеста задница
        } else {
            float k = (t - 0.88f) / 0.12f;
            height = 0.81f + 0.22f * sinf(k * 1.57f); // Заден антикрил / спойлер
            width = 1.15f;
        }
    };

    float lengthZ = 4.8f;
    float halfL = lengthZ * 0.5f;

    for (int iz = 0; iz < stepsZ; iz++) {
        float t0 = (float)iz / (float)stepsZ;
        float t1 = (float)(iz + 1) / (float)stepsZ;

        float z0 = halfL - t0 * lengthZ;
        float z1 = halfL - t1 * lengthZ;

        float h0, w0, h1, w1;
        getProfile(t0, h0, w0);
        getProfile(t1, h1, w1);

        bool isGlass0 = (t0 >= 0.35f && t0 <= 0.72f);
        bool isGlass1 = (t1 >= 0.35f && t1 <= 0.72f);

        for (int is = 0; is < stepsS; is++) {
            float s0 = -1.57079f + (float)is * 3.14159f / (float)stepsS;
            float s1 = -1.57079f + (float)(is + 1) * 3.14159f / (float)stepsS;

            float x00 = w0 * sinf(s0), y00 = 0.22f + (h0 - 0.22f) * cosf(s0);
            float x01 = w0 * sinf(s1), y01 = 0.22f + (h0 - 0.22f) * cosf(s1);
            float x10 = w1 * sinf(s0), y10 = 0.22f + (h1 - 0.22f) * cosf(s0);
            float x11 = w1 * sinf(s1), y11 = 0.22f + (h1 - 0.22f) * cosf(s1);

            float colR = r, colG = g, colB = b;
            if ((isGlass0 || isGlass1) && (fabsf(s0) < 1.0f || fabsf(s1) < 1.0f)) {
                colR = 0.22f; colG = 0.45f; colB = 0.65f; // Тонирано стъкло
            }

            // Изчисляване на гладки нормали за всяка точка
            float nx00 = sinf(s0), ny00 = cosf(s0), nz00 = 0.1f;
            float nx01 = sinf(s1), ny01 = cosf(s1), nz01 = 0.1f;
            float nx10 = sinf(s0), ny10 = cosf(s0), nz10 = -0.1f;
            float nx11 = sinf(s1), ny11 = cosf(s1), nz11 = -0.1f;

            Vertex v1 = { x00, y00, z0, nx00, ny00, nz00, colR, colG, colB };
            Vertex v2 = { x01, y01, z0, nx01, ny01, nz01, colR, colG, colB };
            Vertex v3 = { x11, y11, z1, nx11, ny11, nz11, colR, colG, colB };
            Vertex v4 = { x10, y10, z1, nx10, ny10, nz10, colR, colG, colB };

            addSmoothQuad(v1, v2, v3, v4);
        }
    }
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

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_generateSupercar(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    generateProceduralSupercar(r, g, b);
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
