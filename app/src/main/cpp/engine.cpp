#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>

static float bgR = 0.12f, bgG = 0.10f, bgB = 0.15f;
static float aspect = 1.0f;

static float camX = 0.0f, camY = 5.5f, camZ = 10.0f;
static int playerHp = 100, playerMaxHp = 100;
static int gameScore = 0;
static bool gameWon = false;
static float pStartX = 0.0f, pStartY = 0.6f, pStartZ = 2.0f;

static float attackAnimTimer = 0.0f; // Анимация на удара

enum Tag { TAG_SOLID = 0, TAG_PLAYER = 1, TAG_ENEMY = 2, TAG_ITEM = 3, TAG_BULLET = 4, TAG_GOAL = 5 };

struct Entity {
    bool active;
    int tag;
    float x, y, z;
    float vx, vy, vz;
    float sx, sy, sz;
    float rotY;
    float r, g, b;
    int hp;
    int aiType;
};

#define MAX_ENTITIES 96
static Entity entities[MAX_ENTITIES];

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

// 8-стенен кристал / диамант (Octahedron)
static const float GEM[] = {
    0.0f, 0.6f, 0.0f, 0,1,1,   -0.4f, 0.0f,  0.4f, 0,1,1,   0.4f, 0.0f,  0.4f, 0,1,1,
    0.0f, 0.6f, 0.0f, 1,1,0,    0.4f, 0.0f,  0.4f, 1,1,0,   0.4f, 0.0f, -0.4f, 1,1,0,
    0.0f, 0.6f, 0.0f, 0,1,-1,   0.4f, 0.0f, -0.4f, 0,1,-1, -0.4f, 0.0f, -0.4f, 0,1,-1,
    0.0f, 0.6f, 0.0f, -1,1,0,  -0.4f, 0.0f, -0.4f, -1,1,0, -0.4f, 0.0f,  0.4f, -1,1,0,
    0.0f,-0.6f, 0.0f, 0,-1,1,   0.4f, 0.0f,  0.4f, 0,-1,1, -0.4f, 0.0f,  0.4f, 0,-1,1,
    0.0f,-0.6f, 0.0f, 1,-1,0,   0.4f, 0.0f, -0.4f, 1,-1,0,  0.4f, 0.0f,  0.4f, 1,-1,0,
    0.0f,-0.6f, 0.0f, 0,-1,-1, -0.4f, 0.0f, -0.4f, 0,-1,-1,  0.4f, 0.0f, -0.4f, 0,-1,-1,
    0.0f,-0.6f, 0.0f, -1,-1,0, -0.4f, 0.0f,  0.4f, -1,-1,0, -0.4f, 0.0f, -0.4f, -1,-1,0
};

// Шейдър с Процедурни Текстури (Плочи за под, колан за кимоно и блясък)
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
    "uniform int uTexType; // 0=чист, 1=каменни плочи, 2=боец/кимоно\n"
    "out vec4 oC;\n"
    "void main(){\n"
    "    float d = max(dot(normalize(vN), normalize(vec3(0.4,0.9,0.5))), 0.0) * 0.65 + 0.35;\n"
    "    vec3 col = uCol;\n"
    "    // 1. Текстура на каменни плочи за пода с фуги\n"
    "    if(uTexType == 1) {\n"
    "        vec2 grid = abs(fract(vPos.xz * 0.5) - 0.5);\n"
    "        float lines = smoothstep(0.45, 0.49, max(grid.x, grid.y));\n"
    "        col = mix(col, vec3(0.1, 0.1, 0.12), lines);\n"
    "    }\n"
    "    // 2. Текстура за кимоно на боец (черен колан през средата)\n"
    "    else if(uTexType == 2) {\n"
    "        if(abs(vPos.y) < 0.12) col = vec3(0.1, 0.1, 0.1); // Черен колан\n"
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

    for(int i=0; i<MAX_ENTITIES; i++) entities[i].active = false;

    // Шаолин боец (Жълто кимоно) срещу Нинджа (Тъмно лилаво)
    entities[0] = { true, TAG_PLAYER, 0.0f, 0.0f, 2.0f, 0,0,0, 1.0f, 1.0f, 1.0f, 0.0f, 0.95f, 0.75f, 0.1f, 100, 0 };
    entities[1] = { true, TAG_ENEMY,  0.0f, 0.0f,-3.0f, 0,0,0, 1.0f, 1.0f, 1.0f, 0.0f, 0.55f, 0.15f, 0.75f, 80, 1 };
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0,0,w,h);
    aspect = (float)w / (float)(h>0?h:1);
}

// Помощна функция за рисуване на част от тяло
static void drawPart(float px, float py, float pz, float sx, float sy, float sz, float rotY, float r, float g, float b, int texType, const float* VP) {
    float rad = rotY * 0.017453f;
    float cr = cosf(rad), sr = sinf(rad);
    float M[16], MVP[16]; mat4_identity(M);
    M[0] = cr * sx; M[2] = sr * sx;
    M[5] = sy;
    M[8] = -sr * sz; M[10] = cr * sz;
    M[12] = px; M[13] = py; M[14] = pz;
    mat4_mul(MVP, VP, M);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, MVP);
    glUniform3f(colorLoc, r, g, b);
    glUniform1i(texTypeLoc, texType);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), CUBE);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), &CUBE[3]);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

// РЕНДИРАНЕ НА АНАТОМИЧЕН 3D БОЕЦ (Глава, Торс с колан, Крака и Ръце за бой)
static void drawHumanoidFighter(const Entity& e, bool isAttacking, const float* VP) {
    float bx = e.x, by = e.y, bz = e.z, rot = e.rotY;

    // 1. Торс с кимоно и колан (текстура тип 2)
    drawPart(bx, by + 1.1f, bz, 0.75f, 0.85f, 0.45f, rot, e.r, e.g, e.b, 2, VP);

    // 2. Глава с бойна маска/лента
    drawPart(bx, by + 1.8f, bz, 0.45f, 0.45f, 0.45f, rot, 0.95f, 0.80f, 0.65f, 0, VP);
    drawPart(bx, by + 1.88f, bz, 0.48f, 0.12f, 0.48f, rot, e.r * 0.8f, e.g * 0.8f, e.b * 0.8f, 0, VP); // Лента

    // 3. Крака в бойна стойка
    drawPart(bx - 0.22f, by + 0.45f, bz, 0.28f, 0.85f, 0.32f, rot, 0.18f, 0.18f, 0.20f, 0, VP);
    drawPart(bx + 0.22f, by + 0.45f, bz, 0.28f, 0.85f, 0.32f, rot, 0.18f, 0.18f, 0.20f, 0, VP);

    // 4. Ръце (при атака дясната ръка прави истински замах напред!)
    float punchForward = isAttacking ? -0.85f : 0.0f;
    drawPart(bx - 0.52f, by + 1.15f, bz + 0.15f, 0.22f, 0.75f, 0.24f, rot, e.r, e.g, e.b, 0, VP); // Лява
    drawPart(bx + 0.52f, by + 1.15f, bz + punchForward, 0.24f, 0.24f, 0.85f, rot, e.r, e.g, e.b, 0, VP); // Дясна удряща!
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    if (attackAnimTimer > 0.0f) attackAnimTimer -= 0.05f;

    int pIdx = -1;
    for(int i=0; i<MAX_ENTITIES; i++){
        if(entities[i].active && entities[i].tag == TAG_PLAYER) { pIdx = i; break; }
    }

    // Врагът преследва играча
    for(int i=0; i<MAX_ENTITIES; i++){
        if(!entities[i].active) continue;
        if(entities[i].aiType == 1 && pIdx >= 0) {
            float dx = entities[pIdx].x - entities[i].x;
            float dz = entities[pIdx].z - entities[i].z;
            float dist = sqrtf(dx*dx + dz*dz);
            if(dist > 1.6f) {
                entities[i].x += (dx / dist) * 0.045f;
                entities[i].z += (dz / dist) * 0.045f;
            } else {
                static int atkCd = 0;
                if(++atkCd > 30) {
                    atkCd = 0;
                    playerHp -= 10;
                    if(playerHp <= 0) { playerHp = playerMaxHp; entities[pIdx].x = pStartX; entities[pIdx].z = pStartZ; }
                }
            }
        }
    }

    float targetX = 0.0f, targetY = 1.2f, targetZ = 0.0f;
    if(pIdx >= 0) {
        targetX = entities[pIdx].x;
        targetY = entities[pIdx].y + 1.0f;
        targetZ = entities[pIdx].z;
        camX = targetX;
        camY = targetY + 5.5f;
        camZ = targetZ + 9.5f;
    }

    glClearColor(bgR, bgG, bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(shaderProg == 0) return;

    float P[16]; mat4_identity(P);
    float tanHalf = tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    P[0] = 1.0f / (aspect * tanHalf); P[5] = 1.0f / tanHalf;
    P[10] = -100.1f / 99.9f; P[11] = -1.0f; P[14] = -20.0f / 99.9f; P[15] = 0.0f;

    float V[16];
    mat4_lookat(V, camX, camY, camZ, targetX, targetY, targetZ, 0.0f, 1.0f, 0.0f);

    float VP[16]; mat4_mul(VP, P, V);
    glUseProgram(shaderProg);
    glEnable(GL_DEPTH_TEST);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // 1. Каменен под с плочи (текстура тип 1)
    float floorM[16], floorMVP[16];
    mat4_identity(floorM); mat4_mul(floorMVP, VP, floorM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, floorMVP);
    glUniform3f(colorLoc, 0.28f, 0.26f, 0.32f);
    glUniform1i(texTypeLoc, 1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), ARENA_FLOOR);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), &ARENA_FLOOR[3]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 2. Рендиране на бойци (хуманоиди) и бонуси (диаманти)
    for(int i=0; i<MAX_ENTITIES; i++){
        if(!entities[i].active) continue;

        // Ако е Играч или Враг -> Рендираме истински 3D боец!
        if(entities[i].tag == TAG_PLAYER || entities[i].tag == TAG_ENEMY) {
            bool isAtk = (entities[i].tag == TAG_PLAYER && attackAnimTimer > 0.0f);
            drawHumanoidFighter(entities[i], isAtk, VP);
        }
        // Ако е Диамант/Монета -> Рендираме истински кристален октаедър!
        else if(entities[i].tag == TAG_ITEM) {
            entities[i].rotY += 4.0f;
            float rad = entities[i].rotY * 0.017453f;
            float cr = cosf(rad), sr = sinf(rad);
            float M[16], MVP[16]; mat4_identity(M);
            M[0] = cr * entities[i].sx; M[2] = sr * entities[i].sx;
            M[5] = entities[i].sy;
            M[8] = -sr * entities[i].sz; M[10] = cr * entities[i].sz;
            M[12] = entities[i].x; M[13] = entities[i].y + 0.8f; M[14] = entities[i].z;
            mat4_mul(MVP, VP, M);
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, MVP);
            glUniform3f(colorLoc, entities[i].r, entities[i].g, entities[i].b);
            glUniform1i(texTypeLoc, 0);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), GEM);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), &GEM[3]);
            glDrawArrays(GL_TRIANGLES, 0, 24);
        }
        // Всичко останало (стени/платформи)
        else {
            drawPart(entities[i].x, entities[i].y + entities[i].sy*0.5f, entities[i].z,
                     entities[i].sx, entities[i].sy, entities[i].sz, entities[i].rotY,
                     entities[i].r, entities[i].g, entities[i].b, 0, VP);
        }
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_triggerAction(JNIEnv*, jobject, jint actionType) {
    int pIdx = -1;
    for(int i=0; i<MAX_ENTITIES; i++){
        if(entities[i].active && entities[i].tag == TAG_PLAYER){ pIdx = i; break; }
    }
    if(pIdx < 0) return;

    attackAnimTimer = 0.25f; // Пуска анимацията на замаха с юмрук

    float px = entities[pIdx].x, pz = entities[pIdx].z;

    for(int i=0; i<MAX_ENTITIES; i++){
        if(!entities[i].active || entities[i].tag != TAG_ENEMY) continue;
        float dx = entities[i].x - px, dz = entities[i].z - pz;
        float dist = sqrtf(dx*dx + dz*dz);
        if(dist < 2.8f){
            int dmg = (actionType == 1) ? 30 : 50;
            entities[i].hp -= dmg;
            if(dist > 0.05f) {
                entities[i].x += (dx / dist) * 1.8f; // Откат назад
                entities[i].z += (dz / dist) * 1.8f;
            }
            if(entities[i].hp <= 0){
                entities[i].active = false;
                gameScore += 100;
                gameWon = true; // ПОБЕДА!
            }
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_movePlayer(JNIEnv*, jobject, jfloat f, jfloat s) {
    for(int i=0; i<MAX_ENTITIES; i++){
        if(entities[i].active && entities[i].tag == TAG_PLAYER){
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
        jfloat r, jfloat g, jfloat b, jint tag, jint hp, jint aiType) {
    for(int i=0; i<MAX_ENTITIES; i++){
        if(!entities[i].active){
            entities[i] = { true, tag, x, y, z, 0.0f, 0.0f, 0.0f, sx, sy, sz, 0.0f, r, g, b, hp, aiType };
            if(tag == TAG_PLAYER){ pStartX = x; pStartY = y; pStartZ = z; playerHp = hp; playerMaxHp = hp; }
            break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_clearWorld(JNIEnv*, jobject) {
    for(int i=0; i<MAX_ENTITIES; i++) entities[i].active = false;
    playerHp = 100; gameScore = 0; gameWon = false;
}

extern "C" JNIEXPORT jint JNICALL Java_com_aigame_engine_NativeEngine_getPlayerHp(JNIEnv*, jobject) { return playerHp; }
extern "C" JNIEXPORT jint JNICALL Java_com_aigame_engine_NativeEngine_getScore(JNIEnv*, jobject) { return gameScore; }
extern "C" JNIEXPORT jboolean JNICALL Java_com_aigame_engine_NativeEngine_isWon(JNIEnv*, jobject) { return gameWon; }

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setSky(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    bgR = r; bgG = g; bgB = b;
}
