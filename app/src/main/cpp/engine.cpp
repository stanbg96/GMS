#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

static float bgR = 0.52f, bgG = 0.65f, bgB = 0.80f;
static float aspect = 1.0f;

static float camYaw = 0.8f;
static float camPitch = 0.55f;
static float camDist = 14.0f;
static float targetY = 1.0f;

static bool physicsEnabled = false;
static float gravityY = -9.81f;

struct Entity {
    bool active;
    float x, y, z;
    float vx, vy, vz;
    float scale;
    float r, g, b;
};

#define MAX_ENTITIES 128
static Entity entityPool[MAX_ENTITIES];

static GLuint shaderProgram = 0;
static GLint mvpLoc = -1, colorLoc = -1;

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
    "    vec3 L = normalize(vec3(0.4, 0.9, 0.5));\n"
    "    float diff = max(dot(vNormal, L), 0.0) * 0.5 + 0.5;\n"
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

static void spawnCubeInternal(float x, float y, float z, float scale, float r, float g, float b) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entityPool[i].active) {
            entityPool[i] = { true, x, y, z, 0.0f, 0.0f, 0.0f, scale, r, g, b };
            break;
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
    colorLoc = glGetUniformLocation(shaderProgram, "uColor");

    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for (int i = -8; i <= 8; i++) {
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.005f; gridVertices[idx++] = -8.0f;
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.005f; gridVertices[idx++] =  8.0f;
        gridVertices[idx++] = -8.0f;    gridVertices[idx++] = 0.005f; gridVertices[idx++] = (float)i;
        gridVertices[idx++] =  8.0f;    gridVertices[idx++] = 0.005f; gridVertices[idx++] = (float)i;
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
    if (physicsEnabled) {
        const float dt = 0.016f;
        for (int i = 0; i < MAX_ENTITIES; i++) {
            if (!entityPool[i].active) continue;
            entityPool[i].vy += gravityY * dt;
            entityPool[i].x += entityPool[i].vx * dt;
            entityPool[i].y += entityPool[i].vy * dt;
            entityPool[i].z += entityPool[i].vz * dt;

            float floorY = entityPool[i].scale * 0.5f;
            if (entityPool[i].y < floorY) {
                entityPool[i].y = floorY;
                entityPool[i].vy = -entityPool[i].vy * 0.3f;
                if (fabsf(entityPool[i].vy) < 0.2f) entityPool[i].vy = 0.0f;
                entityPool[i].vx *= 0.85f;
                entityPool[i].vz *= 0.85f;
            }
        }
    }

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

    // 1. Studio Floor
    float floorM[16], floorMVP[16];
    mat4_identity(floorM);
    mat4_mul(floorMVP, VP, floorM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, floorMVP);
    glUniform3f(colorLoc, 0.22f, 0.25f, 0.30f);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), FLOOR_VERTICES);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), &FLOOR_VERTICES[3]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 2. Grid Lines
    glUniform3f(colorLoc, 0.35f, 0.40f, 0.48f);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gridVertices);
    glDisableVertexAttribArray(1);
    glDrawArrays(GL_LINES, 0, GRID_LINES * 2);

    // 3. Axes
    glUniform3f(colorLoc, 0.9f, 0.25f, 0.25f);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, &AXIS_VERTICES[0]);
    glDrawArrays(GL_LINES, 0, 2);

    glUniform3f(colorLoc, 0.25f, 0.5f, 0.95f);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, &AXIS_VERTICES[6]);
    glDrawArrays(GL_LINES, 0, 2);

    // 4. Cubes
    glEnableVertexAttribArray(1);
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

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), CUBE_DATA);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), &CUBE_DATA[3]);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

// ПРОЦЕДУРНО СТРОЕНЕ НА ОБЕКТИ (Procedural Builders)
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_buildShape(JNIEnv* env, jobject, jstring type, jfloat r, jfloat g, jfloat b) {
    const char* str = env->GetStringUTFChars(type, nullptr);

    if (strcmp(str, "PYRAMID") == 0) {
        // Истинска 3D пирамида (3 нива: 3x3, 2x2, 1x1 = 14 куба)
        for (int x = -1; x <= 1; x++)
            for (int z = -1; z <= 1; z++)
                spawnCubeInternal(x * 1.0f, 0.5f, z * 1.0f, 0.98f, r, g, b);
        for (int x = 0; x <= 1; x++)
            for (int z = 0; z <= 1; z++)
                spawnCubeInternal(x * 1.0f - 0.5f, 1.5f, z * 1.0f - 0.5f, 0.98f, r, g, b);
        spawnCubeInternal(0.0f, 2.5f, 0.0f, 0.98f, r * 1.1f, g * 1.1f, b * 1.1f);
    }
    else if (strcmp(str, "CAR") == 0) {
        // Истинска 3D кола: 4 черни колела + шаси + стъклен покрив
        spawnCubeInternal(-0.9f, 0.35f, -0.8f, 0.65f, 0.15f, 0.15f, 0.15f);
        spawnCubeInternal( 0.9f, 0.35f, -0.8f, 0.65f, 0.15f, 0.15f, 0.15f);
        spawnCubeInternal(-0.9f, 0.35f,  0.8f, 0.65f, 0.15f, 0.15f, 0.15f);
        spawnCubeInternal( 0.9f, 0.35f,  0.8f, 0.65f, 0.15f, 0.15f, 0.15f);
        // Купе
        for (int x = -1; x <= 1; x++)
            for (int z = -1; z <= 1; z++)
                spawnCubeInternal(x * 0.8f, 0.9f, z * 0.8f, 0.85f, r, g, b);
        // Кабина със стъкла
        spawnCubeInternal(0.0f, 1.7f, 0.0f, 1.1f, 0.3f, 0.7f, 0.95f);
    }
    else if (strcmp(str, "HOUSE") == 0) {
        // Къща със стени и червен покрив
        for (int x = -1; x <= 0; x++) {
            for (int z = -1; z <= 0; z++) {
                spawnCubeInternal(x * 1.0f + 0.5f, 0.5f, z * 1.0f + 0.5f, 0.98f, r, g, b);
                spawnCubeInternal(x * 1.0f + 0.5f, 1.5f, z * 1.0f + 0.5f, 0.98f, r, g, b);
            }
        }
        spawnCubeInternal(0.0f, 2.5f, 0.0f, 1.4f, 0.85f, 0.2f, 0.2f);
    }
    else if (strcmp(str, "TOWER") == 0) {
        // Кула от 5 етажа
        for (int y = 0; y < 5; y++)
            spawnCubeInternal(0.0f, y * 1.0f + 0.5f, 0.0f, 0.98f, r, g, b);
        spawnCubeInternal(-0.4f, 5.3f, 0.0f, 0.45f, r * 1.2f, g * 1.2f, b * 1.2f);
        spawnCubeInternal( 0.4f, 5.3f, 0.0f, 0.45f, r * 1.2f, g * 1.2f, b * 1.2f);
    }

    env->ReleaseStringUTFChars(type, str);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setPhysicsEnabled(JNIEnv*, jobject, jboolean enable) {
    physicsEnabled = enable;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setGravity(JNIEnv*, jobject, jfloat g) {
    gravityY = g;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_applyExplosion(JNIEnv*, jobject, jfloat force) {
    physicsEnabled = true;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entityPool[i].active) continue;
        float dirX = entityPool[i].x + ((float)(rand() % 100) / 100.0f - 0.5f);
        float dirZ = entityPool[i].z + ((float)(rand() % 100) / 100.0f - 0.5f);
        float len = sqrtf(dirX * dirX + dirZ * dirZ) + 0.1f;
        entityPool[i].vx = (dirX / len) * force;
        entityPool[i].vy = force * 0.8f + ((float)(rand() % 100) / 100.0f) * force * 0.4f;
        entityPool[i].vz = (dirZ / len) * force;
    }
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
Java_com_aigame_engine_NativeEngine_clearWorld(JNIEnv*, jobject) {
    for (int i = 0; i < MAX_ENTITIES; i++) entityPool[i].active = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_spawnCube(JNIEnv*, jobject, jfloat x, jfloat y, jfloat z, jfloat scale, jfloat r, jfloat g, jfloat b) {
    spawnCubeInternal(x, y, z, scale, r, g, b);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setBackgroundColor(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    bgR = r; bgG = g; bgB = b;
}
