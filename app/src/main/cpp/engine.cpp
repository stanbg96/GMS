#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>

static float bgR = 0.10f, bgG = 0.08f, bgB = 0.12f; // Mortal Kombat нощно небе
static float aspect = 1.0f;

static float camX = 0.0f, camY = 5.5f, camZ = 10.0f;

static int playerHp = 100, playerMaxHp = 100;
static int gameScore = 0;
static bool gameWon = false;
static float pStartX = 0.0f, pStartY = 0.7f, pStartZ = 2.0f;

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
static GLint mvpLoc = -1, colorLoc = -1, useLightLoc = -1;

#define GRID_LINES 42
static float gridVerts[GRID_LINES * 2 * 3];

// Каменна бойна арена (Solid Ground Floor)
static const float ARENA_FLOOR[] = {
    -20.0f, 0.0f, -20.0f,  0,1,0,   20.0f, 0.0f, -20.0f,  0,1,0,   20.0f, 0.0f,  20.0f,  0,1,0,
    -20.0f, 0.0f, -20.0f,  0,1,0,   20.0f, 0.0f,  20.0f,  0,1,0,  -20.0f, 0.0f,  20.0f,  0,1,0
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
    "out vec3 vN;\n"
    "void main(){ vN=aNorm; gl_Position=uMVP*vec4(aPos,1.0); }\n";

static const char* FS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vN;\n"
    "uniform vec3 uCol;\n"
    "uniform int uUseLight;\n"
    "out vec4 oC;\n"
    "void main(){\n"
    "    if (uUseLight == 1) {\n"
    "        float d = max(dot(normalize(vN), normalize(vec3(0.4,0.9,0.5))), 0.0) * 0.60 + 0.40;\n"
    "        oC = vec4(uCol * d, 1.0);\n"
    "    } else {\n"
    "        oC = vec4(uCol, 1.0);\n"
    "    }\n"
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

// Перфектна камера, насочена винаги към играча и враговете
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
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,nullptr); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,nullptr); glCompileShader(fs);
    shaderProg = glCreateProgram();
    glAttachShader(shaderProg, vs); glAttachShader(shaderProg, fs);
    glLinkProgram(shaderProg);
    mvpLoc = glGetUniformLocation(shaderProg, "uMVP");
    colorLoc = glGetUniformLocation(shaderProg, "uCol");
    useLightLoc = glGetUniformLocation(shaderProg, "uUseLight");
    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for(int i=-10; i<=10; i++){
        gridVerts[idx++]=(float)i; gridVerts[idx++]=0.01f; gridVerts[idx++]=-10.0f;
        gridVerts[idx++]=(float)i; gridVerts[idx++]=0.01f; gridVerts[idx++]=10.0f;
        gridVerts[idx++]=-10.0f;   gridVerts[idx++]=0.01f; gridVerts[idx++]=(float)i;
        gridVerts[idx++]=10.0f;    gridVerts[idx++]=0.01f; gridVerts[idx++]=(float)i;
    }

    // Стартов боец и нинджа арена
    for(int i=0; i<MAX_ENTITIES; i++) entities[i].active = false;
    entities[0] = { true, TAG_PLAYER, 0.0f, 0.9f, 2.0f, 0,0,0, 1.0f, 1.8f, 0.8f, 0.0f, 0.95f, 0.75f, 0.1f, 100, 0 };
    entities[1] = { true, TAG_ENEMY,  0.0f, 0.9f,-3.0f, 0,0,0, 1.0f, 1.8f, 0.8f, 0.0f, 0.75f, 0.15f, 0.85f, 80, 1 };
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0,0,w,h);
    aspect = (float)w / (float)(h>0?h:1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    int pIdx = -1;
    for(int i=0; i<MAX_ENTITIES; i++){
        if(entities[i].active && entities[i].tag == TAG_PLAYER) { pIdx = i; break; }
    }

    // 1. Изкуствен интелект на враговете (Нинджите преследват играча)
    for(int i=0; i<MAX_ENTITIES; i++){
        if(!entities[i].active) continue;

        if(entities[i].aiType == 3) {
            // Куршум/снаряд
            entities[i].x += entities[i].vx;
            entities[i].z += entities[i].vz;
            for(int j=0; j<MAX_ENTITIES; j++){
                if(entities[j].active && entities[j].tag == TAG_ENEMY){
                    float dx = entities[i].x - entities[j].x, dz = entities[i].z - entities[j].z;
                    if(sqrtf(dx*dx + dz*dz) < 1.4f){
                        entities[j].hp -= 35;
                        entities[i].active = false;
                        if(entities[j].hp <= 0){ entities[j].active = false; gameScore += 100; }
                        break;
                    }
                }
            }
        }
        else if(entities[i].aiType == 1 && pIdx >= 0) {
            float dx = entities[pIdx].x - entities[i].x;
            float dz = entities[pIdx].z - entities[i].z;
            float dist = sqrtf(dx*dx + dz*dz);
            if(dist > 1.5f) {
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

    // 2. Камерата винаги следва играча отвисоко и под ъгъл
    float targetX = 0.0f, targetY = 0.9f, targetZ = 0.0f;
    if(pIdx >= 0) {
        targetX = entities[pIdx].x;
        targetY = entities[pIdx].y;
        targetZ = entities[pIdx].z;

        camX = targetX;
        camY = targetY + 6.0f;
        camZ = targetZ + 10.0f;
    }

    glClearColor(bgR, bgG, bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(shaderProg == 0) return;

    // Перспектива
    float P[16]; mat4_identity(P);
    float tanHalf = tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    P[0] = 1.0f / (aspect * tanHalf); P[5] = 1.0f / tanHalf;
    P[10] = -100.1f / 99.9f; P[11] = -1.0f; P[14] = -20.0f / 99.9f; P[15] = 0.0f;

    // LookAt матрица – фокусира се върху играча
    float V[16];
    mat4_lookat(V, camX, camY, camZ, targetX, targetY, targetZ, 0.0f, 1.0f, 0.0f);

    float VP[16]; mat4_mul(VP, P, V);
    glUseProgram(shaderProg);

    glEnable(GL_DEPTH_TEST);

    // 1. Каменен под на арената
    glUniform1i(useLightLoc, 1);
    float floorM[16], floorMVP[16];
    mat4_identity(floorM);
    mat4_mul(floorMVP, VP, floorM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, floorMVP);
    glUniform3f(colorLoc, 0.22f, 0.20f, 0.25f);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), ARENA_FLOOR);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), &ARENA_FLOOR[3]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 2. Линии на мрежата
    glUniform1i(useLightLoc, 0);
    glUniform3f(colorLoc, 0.35f, 0.32f, 0.40f);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gridVerts);
    glDisableVertexAttribArray(1);
    glDrawArrays(GL_LINES, 0, GRID_LINES * 2);

    // 3. Бойци и обекти
    glUniform1i(useLightLoc, 1);
    glEnableVertexAttribArray(1);
    for(int i=0; i<MAX_ENTITIES; i++){
        if(!entities[i].active) continue;
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
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), CUBE);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), &CUBE[3]);
        glDrawArrays(GL_TRIANGLES, 0, 36);
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

    float px = entities[pIdx].x, pz = entities[pIdx].z;

    // Удар / НУНЧАКУ срещу врага
    if(actionType == 1 || actionType == 2) {
        for(int i=0; i<MAX_ENTITIES; i++){
            if(entities[i].active && entities[i].tag == TAG_ENEMY){
                float dx = entities[i].x - px, dz = entities[i].z - pz;
                float dist = sqrtf(dx*dx + dz*dz);
                if(dist < 2.8f){
                    int dmg = (actionType == 1) ? 25 : 45;
                    entities[i].hp -= dmg;
                    // Отхвърляне на врага назад (Knockback)
                    if(dist > 0.05f) {
                        entities[i].x += (dx / dist) * 1.6f;
                        entities[i].z += (dz / dist) * 1.6f;
                    }
                    if(entities[i].hp <= 0){
                        entities[i].active = false;
                        gameScore += 100;
                        gameWon = true; // ПОБЕДА!
                    }
                }
            }
        }
    }
    else if(actionType == 3) {
        entities[pIdx].y += 2.0f; // Скок
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_movePlayer(JNIEnv*, jobject, jfloat f, jfloat s) {
    for(int i=0; i<MAX_ENTITIES; i++){
        if(entities[i].active && entities[i].tag == TAG_PLAYER){
            float speed = 0.30f;
            entities[i].z -= f * speed;
            entities[i].x += s * speed;
            if(entities[i].y > pStartY) entities[i].y -= 0.09f;
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
