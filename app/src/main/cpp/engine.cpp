#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>

static float bgR = 0.12f, bgG = 0.10f, bgB = 0.16f;
static float aspect = 1.0f;
static float camX = 0.0f, camY = 5.5f, camZ = 10.0f;

static int playerHp = 100, playerMaxHp = 100;
static int gameScore = 0;
static bool gameWon = false;

// Физика на играча (Скок и Гравитация)
static float playerX = 0.0f, playerY = 0.0f, playerZ = 2.0f;
static float playerVY = 0.0f;
static bool isGrounded = true;

// Анимационни таймери
static float punchTimer = 0.0f;
static float kickTimer = 0.0f;
static float enemyHitTimer = 0.0f;

// Враг
static float enemyX = 0.0f, enemyY = 0.0f, enemyZ = -3.5f;
static int enemyHp = 80;
static bool enemyActive = true;

static GLuint shaderProg = 0;
static GLint mvpLoc = -1, colorLoc = -1, texTypeLoc = -1;

#define GRID_LINES 42
static float gridVerts[GRID_LINES * 2 * 3];

static const float ARENA_FLOOR[] = {
    -25.0f, 0.0f, -25.0f,  0,1,0,   25.0f, 0.0f, -25.0f,  0,1,0,   25.0f, 0.0f,  25.0f,  0,1,0,
    -25.0f, 0.0f, -25.0f,  0,1,0,   25.0f, 0.0f,  25.0f,  0,1,0,  -25.0f, 0.0f,  25.0f,  0,1,0
};

static const float CUBE[] = {
    -0.5f,-0.5f, 0.5f, 0,0,1,  0.5f,-0.5f, 0.5f, 0,0,1,  0.5f, 0.5f, 0.5f, 0,0,1,
    -0.5f,-0.5f, 0.5f, 0,0,1,  0.5f, 0.5f, 0.5f, 0,0,1, -0.5f, 0.5f, 0.5f, 0,0,1,
    -0.5f,-0.5f,-0.5f, 0,0,-1, -0.5f, 0.5f,-0.5f, 0,0,-1,  0.5f, 0.5f,-0.5f, 0,0,-1,
    -0.5f,-0.5f,-0.5f, 0,0,-1,  0.5f, 0.5f,-0.5f, 0,0,-1,  0.5f,-0.5f,-0.5f, 0,0,-1,
    -0.5f, 0.5f,-0.5f, 0,1,0, -0.5f, 0.5f, 0.5f, 0,1,0,   0.5f, 0.5f, 0.5f, 0,1,0,
    -0.5f, 0.5f,-0.5f, 0,1,0,  0.5f, 0.5f, 0.5f, 0,1,0,   0.5f, 0.5f,-0.5f, 0,1,0,
    -0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f, 0.5f, 0,-1,0,
    -0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f, 0.5f, 0,-1,0, -0.5f,-0.5f, 0.5f, 0,-1,0,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f, 0.5f,-0.5f, 1,0,0,   0.5f, 0.5f, 0.5f, 1,0,0,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f, 0.5f, 0.5f, 1,0,0,   0.5f,-0.5f, 0.5f, 1,0,0,
    -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f,-0.5f, 0.5f,-1,0,0,  -0.5f, 0.5f, 0.5f,-1,0,0,
    -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f, 0.5f, 0.5f,-1,0,0,  -0.5f, 0.5f,-0.5f,-1,0,0
};

static const char* VS =
    "#version 300 es\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNorm;\n"
    "uniform mat4 uMVP;\n"
    "out vec3 vPos;\n"
    "out vec3 vN;\n"
    "void main(){ vPos=aPos; vN=aNorm; gl_Position=uMVP*vec4(aPos,1.0); }\n";

static const char* FS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vPos;\n"
    "in vec3 vN;\n"
    "uniform vec3 uCol;\n"
    "uniform int uTexType;\n"
    "out vec4 oC;\n"
    "void main(){\n"
    "    float d = max(dot(normalize(vN), normalize(vec3(0.4,0.9,0.5))), 0.0) * 0.65 + 0.35;\n"
    "    vec3 col = uCol;\n"
    "    if(uTexType == 1) {\n" // Плочи на пода
    "        vec2 grid = abs(fract(vPos.xz * 0.5) - 0.5);\n"
    "        float lines = smoothstep(0.45, 0.49, max(grid.x, grid.y));\n"
    "        col = mix(col, vec3(0.1, 0.1, 0.12), lines);\n"
    "    } else if(uTexType == 2) {\n" // Черен боен колан
    "        if(abs(vPos.y) < 0.12) col = vec3(0.1, 0.1, 0.1);\n"
    "    }\n"
    "    oC = vec4(col * d, 1.0);\n"
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

static void drawPartRot(float px, float py, float pz, float sx, float sy, float sz, float rotX, float rotY, float r, float g, float b, int texType, const float* VP) {
    float radY = rotY * 0.017453f, radX = rotX * 0.017453f;
    float cy = cosf(radY), sy_val = sinf(radY);
    float cx = cosf(radX), sx_val = sinf(radX);

    float M[16], MVP[16]; mat4_identity(M);
    M[0] = cy * sx;
    M[1] = sx_val * sy_val * sx;
    M[2] = cx * sy_val * sx;
    M[5] = cx * sy;
    M[6] = -sx_val * sy;
    M[8] = -sy_val * sz;
    M[9] = sx_val * cy * sz;
    M[10]= cx * cy * sz;
    M[12]= px; M[13]= py; M[14]= pz;

    mat4_mul(MVP, VP, M);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, MVP);
    glUniform3f(colorLoc, r, g, b);
    glUniform1i(texTypeLoc, texType);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), CUBE);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), &CUBE[3]);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

// 3D АНИМИРАН БОЕЦ (С истински ритник и юмрук)
static void drawAnimatedFighter(float bx, float by, float bz, float rotY, float r, float g, float b, bool isPunching, bool isKicking, bool isHurt, const float* VP) {
    float hurtTilt = isHurt ? -35.0f : 0.0f; // Врагът се накланя назад при удар

    // 1. Торс с кимоно и колан
    drawPartRot(bx, by + 1.1f, bz, 0.75f, 0.85f, 0.45f, hurtTilt, rotY, r, g, b, 2, VP);

    // 2. Глава с бойна лента
    drawPartRot(bx, by + 1.8f, bz, 0.45f, 0.45f, 0.45f, hurtTilt, rotY, 0.95f, 0.80f, 0.65f, 0, VP);
    drawPartRot(bx, by + 1.88f, bz, 0.48f, 0.12f, 0.48f, hurtTilt, rotY, r * 0.8f, g * 0.8f, b * 0.8f, 0, VP);

    // 3. КРАКА: При РИТНИК десният крак се вдига хоризонтално на 85 градуса напред!
    float rightLegRotX = isKicking ? 85.0f : 0.0f;
    float rightLegY = isKicking ? (by + 0.95f) : (by + 0.45f);
    float rightLegZ = isKicking ? (bz - 0.55f) : bz;

    drawPartRot(bx - 0.22f, by + 0.45f, bz, 0.28f, 0.85f, 0.32f, 0.0f, rotY, 0.18f, 0.18f, 0.20f, 0, VP); // Ляв опорен крак
    drawPartRot(bx + 0.22f, rightLegY, rightLegZ, 0.28f, 0.85f, 0.32f, rightLegRotX, rotY, 0.18f, 0.18f, 0.20f, 0, VP); // ДЕСЕН РИТАЩ КРАК!

    // 4. РЪЦЕ: При ЮМРУК дясната ръка замахва напред
    float punchForward = isPunching ? -0.85f : 0.0f;
    drawPartRot(bx - 0.52f, by + 1.15f, bz + 0.15f, 0.22f, 0.75f, 0.24f, 0.0f, rotY, r, g, b, 0, VP); // Лява ръка
    drawPartRot(bx + 0.52f, by + 1.15f, bz + punchForward, 0.24f, 0.24f, 0.85f, 0.0f, rotY, r, g, b, 0, VP); // Дясна удряща!
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceCreated(JNIEnv*, jobject) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,nullptr); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,nullptr); glCompileShader(fs);
    shaderProg = glCreateProgram();
    glAttachShader(shaderProg, vs); glAttachShader(shaderProg, fs);
    glLinkProgram(shaderProg);
    mvpLoc = glGetUniformLocation(shaderProg, "uMVP");
    colorLoc = glGetUniformLocation(shaderProg, "uCol");
    texTypeLoc = glGetUniformLocation(shaderProg, "uTexType");
    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for(int i=-10; i<=10; i++){
        gridVerts[idx++]=(float)i; gridVerts[idx++]=0.01f; gridVerts[idx++]=-10.0f;
        gridVerts[idx++]=(float)i; gridVerts[idx++]=0.01f; gridVerts[idx++]= 10.0f;
        gridVerts[idx++]=-10.0f;   gridVerts[idx++]=0.01f; gridVerts[idx++]=(float)i;
        gridVerts[idx++]= 10.0f;   gridVerts[idx++]=0.01f; gridVerts[idx++]=(float)i;
    }

    playerX = 0.0f; playerY = 0.0f; playerZ = 2.0f;
    playerVY = 0.0f; isGrounded = true;
    playerHp = 100;
    enemyX = 0.0f; enemyY = 0.0f; enemyZ = -3.5f;
    enemyHp = 80; enemyActive = true;
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

    // 1. ФИЗИКА НА СКОКА И ГРАВИТАЦИЯ
    if (!isGrounded) {
        playerY += playerVY;
        playerVY -= 0.022f; // Земно притегляне
        if (playerY <= 0.0f) {
            playerY = 0.0f;
            playerVY = 0.0f;
            isGrounded = true; // Приземяване
        }
    }

    // 2. ВРАГЪТ ПРЕСЛЕДВА И АТАКУВА
    if (enemyActive) {
        float dx = playerX - enemyX;
        float dz = playerZ - enemyZ;
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

    // 3. КАМЕРА
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
    glEnable(GL_DEPTH_TEST);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Каменна арена под
    float floorM[16], floorMVP[16];
    mat4_identity(floorM); mat4_mul(floorMVP, VP, floorM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, floorMVP);
    glUniform3f(colorLoc, 0.28f, 0.26f, 0.32f);
    glUniform1i(texTypeLoc, 1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), ARENA_FLOOR);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), &ARENA_FLOOR[3]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Рендиране на Играча (Шаолин)
    drawAnimatedFighter(playerX, playerY, playerZ, 0.0f, 0.95f, 0.75f, 0.1f, punchTimer > 0.0f, kickTimer > 0.0f, false, VP);

    // Рендиране на Врага (Нинджа)
    if (enemyActive) {
        drawAnimatedFighter(enemyX, enemyY, enemyZ, 180.0f, 0.55f, 0.15f, 0.75f, false, false, enemyHitTimer > 0.0f, VP);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

// 1 = УДАР (Punch), 2 = РИТНИК (Kick), 3 = СКОК (Jump)
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_triggerAction(JNIEnv*, jobject, jint actionType) {
    if (actionType == 1) { // ЮМРУК
        punchTimer = 0.25f;
        float dx = enemyX - playerX, dz = enemyZ - playerZ;
        float dist = sqrtf(dx*dx + dz*dz);
        if (enemyActive && dist < 2.5f) {
            enemyHp -= 25;
            enemyHitTimer = 0.3f;
            enemyZ -= 1.2f; // Откат назад
            if (enemyHp <= 0) { enemyActive = false; gameScore += 100; gameWon = true; }
        }
    } else if (actionType == 2) { // РИТНИК
        kickTimer = 0.32f; // Вдига крака на 85 градуса!
        float dx = enemyX - playerX, dz = enemyZ - playerZ;
        float dist = sqrtf(dx*dx + dz*dz);
        if (enemyActive && dist < 2.8f) {
            enemyHp -= 40;
            enemyHitTimer = 0.45f;
            enemyZ -= 2.2f; // Мощен ритник – врагът отхвърча далеч назад!
            if (enemyHp <= 0) { enemyActive = false; gameScore += 100; gameWon = true; }
        }
    } else if (actionType == 3) { // СКОК
        if (isGrounded) {
            playerVY = 0.32f; // Излита нагоре в 3D пространството
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
    playerX = 0.0f; playerY = 0.0f; playerZ = 2.0f;
    playerVY = 0.0f; isGrounded = true;
    playerHp = 100;
    enemyX = 0.0f; enemyY = 0.0f; enemyZ = -3.5f;
    enemyHp = 80; enemyActive = true;
    gameScore = 0; gameWon = false;
}

extern "C" JNIEXPORT jint JNICALL Java_com_aigame_engine_NativeEngine_getPlayerHp(JNIEnv*, jobject) { return playerHp; }
extern "C" JNIEXPORT jint JNICALL Java_com_aigame_engine_NativeEngine_getScore(JNIEnv*, jobject) { return gameScore; }
extern "C" JNIEXPORT jboolean JNICALL Java_com_aigame_engine_NativeEngine_isWon(JNIEnv*, jobject) { return gameWon; }
