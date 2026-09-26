#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>

static float bgR = 0.55f, bgG = 0.68f, bgB = 0.82f;
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

// Буфер за до 12,000 триъгълни върха за фини 3D детайли
#define MAX_VERTICES 12000
static Vertex meshBuffer[MAX_VERTICES];
static int vertexCount = 0;

static GLuint shaderProgram = 0;
static GLint mvpLoc = -1, colorLoc = -1, eyePosLoc = -1;

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

// Шейдър с чисто, гладко осветление без ръбове от кубчета
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
    "    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.45;\n"
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

static void addVertex(float x, float y, float z, float nx, float ny, float nz, float r, float g, float b) {
    if (vertexCount < MAX_VERTICES) {
        meshBuffer[vertexCount++] = { x, y, z, nx, ny, nz, r, g, b };
    }
}

// 1. Гладко заоблен цилиндър (за истински кръгли гуми и колони)
static void addCylinderX(float cx, float cy, float cz, float radius, float width, float r, float g, float b) {
    const int segs = 16;
    float hw = width * 0.5f;

    for (int i = 0; i < segs; i++) {
        float a0 = (float)i * 6.2831853f / (float)segs;
        float a1 = (float)(i + 1) * 6.2831853f / (float)segs;
        float y0 = cosf(a0) * radius, z0 = sinf(a0) * radius;
        float y1 = cosf(a1) * radius, z1 = sinf(a1) * radius;

        // Външна заоблена повърхност (гума с гладки нормали)
        addVertex(cx - hw, cy + y0, cz + z0,  0.0f, cosf(a0), sinf(a0),  r, g, b);
        addVertex(cx + hw, cy + y0, cz + z0,  0.0f, cosf(a0), sinf(a0),  r, g, b);
        addVertex(cx + hw, cy + y1, cz + z1,  0.0f, cosf(a1), sinf(a1),  r, g, b);

        addVertex(cx - hw, cy + y0, cz + z0,  0.0f, cosf(a0), sinf(a0),  r, g, b);
        addVertex(cx + hw, cy + y1, cz + z1,  0.0f, cosf(a1), sinf(a1),  r, g, b);
        addVertex(cx - hw, cy + y1, cz + z1,  0.0f, cosf(a1), sinf(a1),  r, g, b);

        // Джанти (капаци от двете страни)
        addVertex(cx + hw, cy, cz,           1.0f, 0.0f, 0.0f,  r*0.8f, g*0.8f, b*0.8f);
        addVertex(cx + hw, cy + y0, cz + z0, 1.0f, 0.0f, 0.0f,  r*0.8f, g*0.8f, b*0.8f);
        addVertex(cx + hw, cy + y1, cz + z1, 1.0f, 0.0f, 0.0f,  r*0.8f, g*0.8f, b*0.8f);

        addVertex(cx - hw, cy, cz,          -1.0f, 0.0f, 0.0f,  r*0.8f, g*0.8f, b*0.8f);
        addVertex(cx - hw, cy + y1, cz + z1,-1.0f, 0.0f, 0.0f,  r*0.8f, g*0.8f, b*0.8f);
        addVertex(cx - hw, cy + y0, cz + z0,-1.0f, 0.0f, 0.0f,  r*0.8f, g*0.8f, b*0.8f);
    }
}

// 2. Скосена триъгълна призма (за наклонени покриви и аеродинамични предни стъкла)
static void addWedge(float cx, float cy, float cz, float sx, float sy, float sz, float r, float g, float b) {
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;

    // Наклонен скат (покрив / предно стъкло)
    float nx = 0.0f, ny = hz, nz = hy;
    float len = sqrtf(ny * ny + nz * nz);
    ny /= len; nz /= len;

    addVertex(cx - hx, cy - hy, cz + hz,  nx, ny, nz,  r, g, b);
    addVertex(cx + hx, cy - hy, cz + hz,  nx, ny, nz,  r, g, b);
    addVertex(cx + hx, cy + hy, cz - hz,  nx, ny, nz,  r, g, b);

    addVertex(cx - hx, cy - hy, cz + hz,  nx, ny, nz,  r, g, b);
    addVertex(cx + hx, cy + hy, cz - hz,  nx, ny, nz,  r, g, b);
    addVertex(cx - hx, cy + hy, cz - hz,  nx, ny, nz,  r, g, b);

    // Вертикална задна стена
    addVertex(cx - hx, cy - hy, cz - hz,  0, 0, -1,  r*0.9f, g*0.9f, b*0.9f);
    addVertex(cx - hx, cy + hy, cz - hz,  0, 0, -1,  r*0.9f, g*0.9f, b*0.9f);
    addVertex(cx + hx, cy + hy, cz - hz,  0, 0, -1,  r*0.9f, g*0.9f, b*0.9f);

    addVertex(cx - hx, cy - hy, cz - hz,  0, 0, -1,  r*0.9f, g*0.9f, b*0.9f);
    addVertex(cx + hx, cy + hy, cz - hz,  0, 0, -1,  r*0.9f, g*0.9f, b*0.9f);
    addVertex(cx + hx, cy - hy, cz - hz,  0, 0, -1,  r*0.9f, g*0.9f, b*0.9f);
}

// 3. Гладък 3D правоъгълен панел (за купе, стени, греди)
static void addBox(float cx, float cy, float cz, float sx, float sy, float sz, float r, float g, float b) {
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;

    // 6 стени с чисти полигонални нормали
    // Front (+Z)
    addVertex(cx-hx, cy-hy, cz+hz, 0,0,1, r,g,b); addVertex(cx+hx, cy-hy, cz+hz, 0,0,1, r,g,b); addVertex(cx+hx, cy+hy, cz+hz, 0,0,1, r,g,b);
    addVertex(cx-hx, cy-hy, cz+hz, 0,0,1, r,g,b); addVertex(cx+hx, cy+hy, cz+hz, 0,0,1, r,g,b); addVertex(cx-hx, cy+hy, cz+hz, 0,0,1, r,g,b);
    // Back (-Z)
    addVertex(cx-hx, cy-hy, cz-hz, 0,0,-1, r*0.8f,g*0.8f,b*0.8f); addVertex(cx-hx, cy+hy, cz-hz, 0,0,-1, r*0.8f,g*0.8f,b*0.8f); addVertex(cx+hx, cy+hy, cz-hz, 0,0,-1, r*0.8f,g*0.8f,b*0.8f);
    addVertex(cx-hx, cy-hy, cz-hz, 0,0,-1, r*0.8f,g*0.8f,b*0.8f); addVertex(cx+hx, cy+hy, cz-hz, 0,0,-1, r*0.8f,g*0.8f,b*0.8f); addVertex(cx+hx, cy-hy, cz-hz, 0,0,-1, r*0.8f,g*0.8f,b*0.8f);
    // Top (+Y)
    addVertex(cx-hx, cy+hy, cz-hz, 0,1,0, r*1.1f,g*1.1f,b*1.1f); addVertex(cx-hx, cy+hy, cz+hz, 0,1,0, r*1.1f,g*1.1f,b*1.1f); addVertex(cx+hx, cy+hy, cz+hz, 0,1,0, r*1.1f,g*1.1f,b*1.1f);
    addVertex(cx-hx, cy+hy, cz-hz, 0,1,0, r*1.1f,g*1.1f,b*1.1f); addVertex(cx+hx, cy+hy, cz+hz, 0,1,0, r*1.1f,g*1.1f,b*1.1f); addVertex(cx+hx, cy+hy, cz-hz, 0,1,0, r*1.1f,g*1.1f,b*1.1f);
    // Bottom (-Y)
    addVertex(cx-hx, cy-hy, cz-hz, 0,-1,0, r*0.5f,g*0.5f,b*0.5f); addVertex(cx+hx, cy-hy, cz-hz, 0,-1,0, r*0.5f,g*0.5f,b*0.5f); addVertex(cx+hx, cy-hy, cz+hz, 0,-1,0, r*0.5f,g*0.5f,b*0.5f);
    addVertex(cx-hx, cy-hy, cz-hz, 0,-1,0, r*0.5f,g*0.5f,b*0.5f); addVertex(cx+hx, cy-hy, cz+hz, 0,-1,0, r*0.5f,g*0.5f,b*0.5f); addVertex(cx-hx, cy-hy, cz+hz, 0,-1,0, r*0.5f,g*0.5f,b*0.5f);
    // Right (+X)
    addVertex(cx+hx, cy-hy, cz-hz, 1,0,0, r*0.9f,g*0.9f,b*0.9f); addVertex(cx+hx, cy+hy, cz-hz, 1,0,0, r*0.9f,g*0.9f,b*0.9f); addVertex(cx+hx, cy+hy, cz+hz, 1,0,0, r*0.9f,g*0.9f,b*0.9f);
    addVertex(cx+hx, cy-hy, cz-hz, 1,0,0, r*0.9f,g*0.9f,b*0.9f); addVertex(cx+hx, cy+hy, cz+hz, 1,0,0, r*0.9f,g*0.9f,b*0.9f); addVertex(cx+hx, cy-hy, cz+hz, 1,0,0, r*0.9f,g*0.9f,b*0.9f);
    // Left (-X)
    addVertex(cx-hx, cy-hy, cz-hz, -1,0,0, r*0.7f,g*0.7f,b*0.7f); addVertex(cx-hx, cy-hy, cz+hz, -1,0,0, r*0.7f,g*0.7f,b*0.7f); addVertex(cx-hx, cy+hy, cz+hz, -1,0,0, r*0.7f,g*0.7f,b*0.7f);
    addVertex(cx-hx, cy-hy, cz-hz, -1,0,0, r*0.7f,g*0.7f,b*0.7f); addVertex(cx-hx, cy+hy, cz+hz, -1,0,0, r*0.7f,g*0.7f,b*0.7f); addVertex(cx-hx, cy+hy, cz-hz, -1,0,0, r*0.7f,g*0.7f,b*0.7f);
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

    vertexCount = 0;
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

    // 1. Изчертаване на целия сглобен 3D модел в ЕДИН draw call (Хардуерно оптимизирано)
    if (vertexCount > 0) {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshBuffer[0].x);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshBuffer[0].nx);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshBuffer[0].r);

        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
}

// JNI команди за истински 3D примитиви
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_clearMesh(JNIEnv*, jobject) {
    vertexCount = 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_addBox(JNIEnv*, jobject, jfloat x, jfloat y, jfloat z, jfloat sx, jfloat sy, jfloat sz, jfloat r, jfloat g, jfloat b) {
    addBox(x, y, z, sx, sy, sz, r, g, b);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_addCylinder(JNIEnv*, jobject, jfloat x, jfloat y, jfloat z, jfloat radius, jfloat width, jfloat r, jfloat g, jfloat b) {
    addCylinderX(x, y, z, radius, width, r, g, b);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_addWedge(JNIEnv*, jobject, jfloat x, jfloat y, jfloat z, jfloat sx, jfloat sy, jfloat sz, jfloat r, jfloat g, jfloat b) {
    addWedge(x, y, z, sx, sy, sz, r, g, b);
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
    if (camDist < 3.0f) camDist = 3.0f;
    if (camDist > 40.0f) camDist = 40.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setBackgroundColor(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    bgR = r; bgG = g; bgB = b;
}
