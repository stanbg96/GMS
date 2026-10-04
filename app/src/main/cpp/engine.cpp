#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>

static float bgR = 0.12f, bgG = 0.10f, bgB = 0.16f;
static float aspect = 1.0f;
static float camX = 0.0f, camY = 5.5f, camZ = 10.0f;

static int playerHp = 100, playerMaxHp = 100;
static int gameScore = 0;
static bool gameWon = false;

// Позиция и скок на играча
static float playerX = 0.0f, playerY = 0.0f, playerZ = 2.0f;
static float playerVY = 0.0f;
static bool isGrounded = true;

// Анимации
static float punchTimer = 0.0f;
static float kickTimer = 0.0f;
static float enemyHitTimer = 0.0f;

// Враг
static float enemyX = 0.0f, enemyY = 0.0f, enemyZ = -3.5f;
static int enemyHp = 80;
static bool enemyActive = true;

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b;
};

static std::vector<Vertex> meshBuffer;

static GLuint shaderProg = 0;
static GLint mvpLoc = -1, eyePosLoc = -1;

#define GRID_LINES 42
static float gridVerts[GRID_LINES * 2 * 3];

static const float ARENA_FLOOR[] = {
    -25.0f, 0.0f, -25.0f,  0,1,0,   25.0f, 0.0f, -25.0f,  0,1,0,   25.0f, 0.0f,  25.0f,  0,1,0,
    -25.0f, 0.0f, -25.0f,  0,1,0,   25.0f, 0.0f,  25.0f,  0,1,0,  -25.0f, 0.0f,  25.0f,  0,1,0
};

static const char* VS =
    "#version 300 es\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNorm;\n"
    "layout(location=2) in vec3 aColor;\n"
    "uniform mat4 uMVP;\n"
    "out vec3 vWorldPos;\n"
    "out vec3 vNormal;\n"
    "out vec3 vColor;\n"
    "void main(){\n"
    "    vWorldPos = aPos;\n"
    "    vNormal = aNorm;\n"
    "    vColor = aColor;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* FS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vWorldPos;\n"
    "in vec3 vNormal;\n"
    "in vec3 vColor;\n"
    "uniform vec3 uEyePos;\n"
    "out vec4 FragColor;\n"
    "void main(){\n"
    "    vec3 N = normalize(vNormal);\n"
    "    vec3 L = normalize(vec3(0.4, 0.9, 0.5));\n"
    "    vec3 V = normalize(uEyePos - vWorldPos);\n"
    "    vec3 H = normalize(L + V);\n"
    "    float diff = max(dot(N, L), 0.0) * 0.60 + 0.40;\n"
    "    float spec = pow(max(dot(N, H), 0.0), 24.0) * 0.35;\n"
    "    FragColor = vec4(vColor * diff + vec3(spec), 1.0);\n"
    "}\n";

static void mat4_identity(float* m) { for(int i=0; i<16; i++) m[i]=(i%5==0)?1.0f:0.0f; }

static void mat4_mul(float* out, const float* a, const float* b) {
    float t[16];
    for(int c=0; c<4; c++){
        for(int r=0; r<4; r++){
            t[c*4+r] = a[0*4+r]*b[c*4+0] + a[1*4+r]*b[c*4+1] + a[2*4+r]*b[c*4+2] + a[3*4+r]*b[c*4+3];
        }
    }
    for(int i=0; i<16; i++) out[i] = t[i];
}

static void mat4_lookat(float* m, float ex, float ey, float ez, float tx, float ty, float tz, float ux, float uy, float uz) {
    float fx = tx - ex, fy = ty - ey, fz = tz - ez;
    float rlf = 1.0f / sqrtf(fx*fx + fy*fy + fz*fz);
    fx *= rlf; fy *= rlf; fz *= rlf;

    float rx = fy * uz - fz * uy, ry = fz * ux - fx * uz, rz = fx * uy - fy * ux;
    float rlr = 1.0f / sqrtf(rx*rx + ry*ry + rz*rz);
    rx *= rlr; ry *= rlr; rz *= rlr;

    float ux2 = ry * fz - rz * fy, uy2 = rz * fx - rx * fz, uz2 = rx * fy - ry * fx;

    m[0] = rx;  m[1] = ux2; m[2] = -fx; m[3] = 0.0f;
    m[4] = ry;  m[5] = uy2; m[6] = -fy; m[7] = 0.0f;
    m[8] = rz;  m[9] = uz2; m[10]= -fz; m[11]= 0.0f;
    m[12]= -(rx*ex + ry*ey + rz*ez);
    m[13]= -(ux2*ex + uy2*ey + uz2*ez);
    m[14]= (fx*ex + fy*ey + fz*ez);
    m[15]= 1.0f;
}

// 1. ГЛАДКО ЗAOБЛЕН КРАЙНИК (ОБЪЛ ЦИЛИНДЪР СЪС СГЪВКА)
static void addSmoothLimb(float x1, float y1, float z1, float x2, float y2, float z2, float radius, float r, float g, float b) {
    float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
    float len = sqrtf(dx*dx + dy*dy + dz*dz);
    if (len < 0.001f) return;

    float wx = dx / len, wy = dy / len, wz = dz / len;
    float ux = (fabsf(wy) < 0.9f) ? -wz : 0.0f;
    float uy = 0.0f;
    float uz = (fabsf(wy) < 0.9f) ? wx : 1.0f;
    float ulen = sqrtf(ux*ux + uz*uz);
    ux /= ulen; uz /= ulen;

    float vx = wy * uz - wz * uy;
    float vy = wz * ux - wx * uz;
    float vz = wx * uy - wy * ux;

    const int segs = 8;
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i * 6.28318f / segs;
        float a1 = (float)(i + 1) * 6.28318f / segs;

        float r0x = ux * cosf(a0) + vx * sinf(a0);
        float r0y = uy * cosf(a0) + vy * sinf(a0);
        float r0z = uz * cosf(a0) + vz * sinf(a0);

        float r1x = ux * cosf(a1) + vx * sinf(a1);
        float r1y = uy * cosf(a1) + vy * sinf(a1);
        float r1z = uz * cosf(a1) + vz * sinf(a1);

        Vertex v0 = { x1 + r0x * radius, y1 + r0y * radius, z1 + r0z * radius, r0x, r0y, r0z, r, g, b };
        Vertex v1 = { x2 + r0x * radius, y2 + r0y * radius, z2 + r0z * radius, r0x, r0y, r0z, r, g, b };
        Vertex v2 = { x2 + r1x * radius, y2 + r1y * radius, z2 + r1z * radius, r1x, r1y, r1z, r, g, b };
        Vertex v3 = { x1 + r1x * radius, y1 + r1y * radius, z1 + r1z * radius, r1x, r1y, r1z, r, g, b };

        meshBuffer.push_back(v0); meshBuffer.push_back(v1); meshBuffer.push_back(v2);
        meshBuffer.push_back(v0); meshBuffer.push_back(v2); meshBuffer.push_back(v3);
    }
}

// 2. ИСТИНСКА КРЪГЛА 3D ГЛАВА (ГЛАДКА СФЕРА БЕЗ КУТИИ)
static void addSmoothSphere(float cx, float cy, float cz, float rad, float r, float g, float b) {
    const int rings = 6, sectors = 8;
    for (int i = 0; i < rings; i++) {
        float lat0 = -1.57079f + (float)i * 3.14159f / rings;
        float lat1 = -1.57079f + (float)(i + 1) * 3.14159f / rings;
        float y0 = sinf(lat0) * rad, r0 = cosf(lat0) * rad;
        float y1 = sinf(lat1) * rad, r1 = cosf(lat1) * rad;

        for (int j = 0; j < sectors; j++) {
            float lon0 = (float)j * 6.28318f / sectors;
            float lon1 = (float)(j + 1) * 6.28318f / sectors;
            float x00 = cosf(lon0) * r0, z00 = sinf(lon0) * r0;
            float x01 = cosf(lon1) * r0, z01 = sinf(lon1) * r0;
            float x10 = cosf(lon0) * r1, z10 = sinf(lon0) * r1;
            float x11 = cosf(lon1) * r1, z11 = sinf(lon1) * r1;

            Vertex v0 = { cx + x00, cy + y0, cz + z00, x00/rad, y0/rad, z00/rad, r, g, b };
            Vertex v1 = { cx + x10, cy + y1, cz + z10, x10/rad, y1/rad, z10/rad, r, g, b };
            Vertex v2 = { cx + x11, cy + y1, cz + z11, x11/rad, y1/rad, z11/rad, r, g, b };
            Vertex v3 = { cx + x01, cy + y0, cz + z01, x01/rad, y0/rad, z01/rad, r, g, b };

            meshBuffer.push_back(v0); meshBuffer.push_back(v1); meshBuffer.push_back(v2);
            meshBuffer.push_back(v0); meshBuffer.push_back(v2); meshBuffer.push_back(v3);
        }
    }
}

// 3. СГЛОБЯВАНЕ НА ОРГАНИЧЕН 3D БОЕЦ (ГЛАВА, МУСКУЛЕСТ ТОРС, ОБЛИ КРАКА И РЪЦЕ)
static void drawOrganicFighter(float bx, float by, float bz, float r, float g, float b, bool isPunching, bool isKicking, bool isHurt) {
    float tiltZ = isHurt ? 0.45f : 0.0f; // При удар се накланя назад

    // Кръгла глава
    addSmoothSphere(bx, by + 1.85f, bz + tiltZ, 0.28f, 0.92f, 0.78f, 0.65f); // Лице
    addSmoothLimb(bx - 0.26f, by + 1.95f, bz + tiltZ, bx + 0.26f, by + 1.95f, bz + tiltZ, 0.08f, r * 0.8f, g * 0.8f, b * 0.8f); // Бойна лента

    // Мускулест заоблен торс (от раменете до кръста)
    addSmoothLimb(bx, by + 0.95f, bz + tiltZ, bx, by + 1.55f, bz + tiltZ, 0.36f, r, g, b); // Кимоно
    addSmoothLimb(bx - 0.32f, by + 0.95f, bz + tiltZ, bx + 0.32f, by + 0.95f, bz + tiltZ, 0.12f, 0.1f, 0.1f, 0.1f); // Черен колан

    // Ляв опорен крак
    addSmoothLimb(bx - 0.20f, by + 0.90f, bz, bx - 0.20f, by + 0.08f, bz, 0.14f, 0.20f, 0.20f, 0.22f);

    // ДЕСЕН КРАК: При РИТНИК се вдига на 85 градуса напред!
    if (isKicking) {
        // ИСТИНСКИ БОЕН РИТНИК (Тазобедрена става -> Коляно -> Изпънат крак напред)
        addSmoothLimb(bx + 0.20f, by + 0.90f, bz, bx + 0.20f, by + 0.95f, bz - 0.70f, 0.14f, 0.20f, 0.20f, 0.22f);
        addSmoothLimb(bx + 0.20f, by + 0.95f, bz - 0.70f, bx + 0.20f, by + 1.05f, bz - 1.45f, 0.13f, 0.92f, 0.78f, 0.65f); // Изпънато стъпало
    } else {
        addSmoothLimb(bx + 0.20f, by + 0.90f, bz, bx + 0.20f, by + 0.08f, bz, 0.14f, 0.20f, 0.20f, 0.22f);
    }

    // Лява ръка в бойна стойка
    addSmoothLimb(bx - 0.38f, by + 1.48f, bz, bx - 0.38f, by + 1.15f, bz - 0.35f, 0.11f, r, g, b);

    // Дясна ръка: При ЮМРУК замахва директно напред!
    if (isPunching) {
        addSmoothLimb(bx + 0.38f, by + 1.48f, bz, bx + 0.38f, by + 1.42f, bz - 1.15f, 0.12f, r, g, b);
        addSmoothSphere(bx + 0.38f, by + 1.42f, bz - 1.25f, 0.12f, 0.92f, 0.78f, 0.65f); // Юмрук
    } else {
        addSmoothLimb(bx + 0.38f, by + 1.48f, bz, bx + 0.38f, by + 1.15f, bz - 0.35f, 0.11f, r, g, b);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceCreated(JNIEnv*, jobject) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,nullptr); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,nullptr); glCompileShader(fs);
    shaderProg = glCreateProgram();
    glAttachShader(shaderProg, vs); glAttachShader(shaderProg, fs);
    glLinkProgram(shaderProg);
    mvpLoc = glGetUniformLocation(shaderProg, "uMVP");
    eyePosLoc = glGetUniformLocation(shaderProg, "uEyePos");
    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for(int i=-10; i<=10; i++){
        gridVerts[idx++]=(float)i; gridVerts[idx++]=0.01f; gridVerts[idx++]=-10.0f;
        gridVerts[idx++]=(float)i; gridVerts[idx++]=0.01f; gridVerts[idx++]= 10.0f;
        gridVerts[idx++]=-10.0f;   gridVerts[idx++]=0.01f; gridVerts[idx++]=(float)i;
        gridVerts[idx++]= 10.0f;   gridVerts[idx++]=0.01f; gridVerts[idx++]=(float)i;
    }

    playerX = 0.0f; playerY = 0.0f; playerZ = 2.0f; playerVY = 0.0f; isGrounded = true; playerHp = 100;
    enemyX = 0.0f; enemyY = 0.0f; enemyZ = -3.5f; enemyHp = 80; enemyActive = true;
    gameScore = 0; gameWon = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0,0,w,h);
    aspect = (float)w / (float)(h>0?h:1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    if (punchTimer > 0.0f) punchTimer -= 0.05f;
    if (kickTimer > 0.0f) kickTimer -= 0.05f;
    if (enemyHitTimer > 0.0f) enemyHitTimer -= 0.05f;

    // Гравитация и скок
    if (!isGrounded) {
        playerY += playerVY;
        playerVY -= 0.022f;
        if (playerY <= 0.0f) { playerY = 0.0f; playerVY = 0.0f; isGrounded = true; }
    }

    // Врагът преследва
    if (enemyActive) {
        float dx = playerX - enemyX, dz = playerZ - enemyZ;
        float dist = sqrtf(dx*dx + dz*dz);
        if (dist > 1.6f && enemyHitTimer <= 0.0f) {
            enemyX += (dx / dist) * 0.045f;
            enemyZ += (dz / dist) * 0.045f;
        } else if (dist <= 1.6f && enemyHitTimer <= 0.0f) {
            static int atkCd = 0;
            if (++atkCd > 35) {
                atkCd = 0;
                playerHp -= 10;
                if (playerHp <= 0) { playerHp = playerMaxHp; playerX = 0; playerZ = 2.0f; }
            }
        }
    }

    camX = playerX;
    camY = playerY + 5.5f;
    camZ = playerZ + 9.5f;

    glClearColor(bgR, bgG, bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(shaderProg == 0) return;

    float P[16]; mat4_identity(P);
    float tanHalf = tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    P[0] = 1.0f / (aspect * tanHalf); P[5] = 1.0f / tanHalf;
    P[10] = -100.1f / 99.9f; P[11] = -1.0f; P[14] = -20.0f / 99.9f; P[15] = 0.0f;

    float V[16];
    mat4_lookat(V, camX, camY, camZ, playerX, playerY + 1.0f, playerZ, 0.0f, 1.0f, 0.0f);

    float VP[16]; mat4_mul(VP, P, V);
    glUseProgram(shaderProg);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, VP);
    glUniform3f(eyePosLoc, camX, camY, camZ);

    // ГЕНЕРИРАНЕ НА ГЛАДКИЯ СВЯТ (БЕЗ КУБОВЕ)
    meshBuffer.clear();

    // 1. Четири каменни колони на арената
    addSmoothLimb(-8.0f, 0.0f, -8.0f, -8.0f, 4.0f, -8.0f, 0.45f, 0.45f, 0.42f, 0.48f);
    addSmoothLimb( 8.0f, 0.0f, -8.0f,  8.0f, 4.0f, -8.0f, 0.45f, 0.45f, 0.42f, 0.48f);
    addSmoothLimb(-8.0f, 0.0f,  8.0f, -8.0f, 4.0f,  8.0f, 0.45f, 0.45f, 0.42f, 0.48f);
    addSmoothLimb( 8.0f, 0.0f,  8.0f,  8.0f, 4.0f,  8.0f, 0.45f, 0.45f, 0.42f, 0.48f);

    // 2. Шаолин боец (Играч - Жълто кимоно)
    drawOrganicFighter(playerX, playerY, playerZ, 0.95f, 0.78f, 0.12f, punchTimer > 0.0f, kickTimer > 0.0f, false);

    // 3. Враг Нинджа (Тъмно лилаво)
    if (enemyActive) {
        drawOrganicFighter(enemyX, enemyY, enemyZ, 0.55f, 0.15f, 0.75f, false, false, enemyHitTimer > 0.0f);
    }

    // Рендиране на всички полигони наведнъж
    if (!meshBuffer.empty()) {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshBuffer[0].x);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshBuffer[0].nx);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshBuffer[0].r);

        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)meshBuffer.size());

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_triggerAction(JNIEnv*, jobject, jint actionType) {
    if (actionType == 1) { // ЮМРУК
        punchTimer = 0.25f;
        float dx = enemyX - playerX, dz = enemyZ - playerZ;
        float dist = sqrtf(dx*dx + dz*dz);
        if (enemyActive && dist < 2.5f) {
            enemyHp -= 25;
            enemyHitTimer = 0.30f;
            enemyZ -= 1.2f;
            if (enemyHp <= 0) { enemyActive = false; gameScore += 100; gameWon = true; }
        }
    } else if (actionType == 2) { // РИТНИК С КРАК
        kickTimer = 0.35f;
        float dx = enemyX - playerX, dz = enemyZ - playerZ;
        float dist = sqrtf(dx*dx + dz*dz);
        if (enemyActive && dist < 2.8f) {
            enemyHp -= 45;
            enemyHitTimer = 0.45f;
            enemyZ -= 2.2f; // Мощен ритник назад!
            if (enemyHp <= 0) { enemyActive = false; gameScore += 100; gameWon = true; }
        }
    } else if (actionType == 3) { // СКОК
        if (isGrounded) {
            playerVY = 0.32f;
            isGrounded = false;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_movePlayer(JNIEnv*, jobject, jfloat f, jfloat s) {
    playerZ -= f * 0.28f;
    playerX += s * 0.28f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_restartGame(JNIEnv*, jobject) {
    playerX = 0.0f; playerY = 0.0f; playerZ = 2.0f; playerVY = 0.0f; isGrounded = true; playerHp = 100;
    enemyX = 0.0f; enemyY = 0.0f; enemyZ = -3.5f; enemyHp = 80; enemyActive = true;
    gameScore = 0; gameWon = false;
}

extern "C" JNIEXPORT jint JNICALL Java_com_aigame_engine_NativeEngine_getPlayerHp(JNIEnv*, jobject) { return playerHp; }
extern "C" JNIEXPORT jint JNICALL Java_com_aigame_engine_NativeEngine_getScore(JNIEnv*, jobject) { return gameScore; }
extern "C" JNIEXPORT jboolean JNICALL Java_com_aigame_engine_NativeEngine_isWon(JNIEnv*, jobject) { return gameWon; }
