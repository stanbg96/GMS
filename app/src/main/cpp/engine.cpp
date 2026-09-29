#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>

static float bgR = 0.14f, bgG = 0.16f, bgB = 0.22f;
static float aspect = 1.0f;
static float camX = 0.0f, camY = 5.5f, camZ = 10.0f;

static int playerHp = 100, playerMaxHp = 100;
static int gameScore = 0;
static bool gameWon = false;
static float pStartX = 0.0f, pStartY = 0.6f, pStartZ = 0.0f;

// Универсални тагове за всички игри
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
    int aiType; // 0: стои, 1: преследва играча, 2: патрулира, 3: лети напред (куршум)
};

#define MAX_ENTITIES 96
static Entity entities[MAX_ENTITIES];

static GLuint shaderProg = 0;
static GLint mvpLoc = -1, colorLoc = -1;

#define GRID_LINES 42
static float gridVerts[GRID_LINES * 2 * 3];

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
    "out vec4 oC;\n"
    "void main(){\n"
    "    float d = max(dot(normalize(vN), normalize(vec3(0.4,0.9,0.5))), 0.0) * 0.60 + 0.40;\n"
    "    oC = vec4(uCol * d, 1.0);\n"
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

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceCreated(JNIEnv*, jobject) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,nullptr); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,nullptr); glCompileShader(fs);
    shaderProg = glCreateProgram();
    glAttachShader(shaderProg, vs); glAttachShader(shaderProg, fs);
    glLinkProgram(shaderProg);
    mvpLoc = glGetUniformLocation(shaderProg, "uMVP");
    colorLoc = glGetUniformLocation(shaderProg, "uCol");
    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for(int i=-10; i<=10; i++){
        gridVerts[idx++]=(float)i; gridVerts[idx++]=0.0f; gridVerts[idx++]=-10.0f;
        gridVerts[idx++]=(float)i; gridVerts[idx++]=0.0f; gridVerts[idx++]=10.0f;
        gridVerts[idx++]=-10.0f;   gridVerts[idx++]=0.0f; gridVerts[idx++]=(float)i;
        gridVerts[idx++]=10.0f;    gridVerts[idx++]=0.0f; gridVerts[idx++]=(float)i;
    }
    for(int i=0; i<MAX_ENTITIES; i++) entities[i].active = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0,0,w,h);
    aspect = (float)w / (float)(h>0?h:1);
}

// 60 FPS УНИВЕРСАЛЕН ГЕЙМПЛЕЙ ЦИКЪЛ
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    int pIdx = -1;
    for(int i=0; i<MAX_ENTITIES; i++){
        if(entities[i].active && entities[i].tag == TAG_PLAYER) { pIdx = i; break; }
    }

    // 1. Изкуствен интелект и физика на снарядите
    for(int i=0; i<MAX_ENTITIES; i++){
        if(!entities[i].active) continue;

        // Физика на куршуми / снаряди
        if(entities[i].aiType == 3) {
            entities[i].x += entities[i].vx;
            entities[i].z += entities[i].vz;
            // Проверка за попадение във враг
            for(int j=0; j<MAX_ENTITIES; j++){
                if(entities[j].active && entities[j].tag == TAG_ENEMY){
                    float dx = entities[i].x - entities[j].x;
                    float dz = entities[i].z - entities[j].z;
                    if(sqrtf(dx*dx + dz*dz) < 1.4f){
                        entities[j].hp -= 35;
                        entities[i].active = false; // Куршумът изчезва
                        if(entities[j].hp <= 0){ entities[j].active = false; gameScore += 100; }
                        break;
                    }
                }
            }
        }
        // AI на враг: преследва играча
        else if(entities[i].aiType == 1 && pIdx >= 0) {
            float dx = entities[pIdx].x - entities[i].x;
            float dz = entities[pIdx].z - entities[i].z;
            float dist = sqrtf(dx*dx + dz*dz);
            if(dist > 1.4f) {
                entities[i].x += (dx / dist) * 0.045f;
                entities[i].z += (dz / dist) * 0.045f;
            } else {
                // Атака отблизо
                static int atkCooldown = 0;
                if(++atkCooldown > 40) {
                    atkCooldown = 0;
                    playerHp -= 10;
                    if(playerHp <= 0) { playerHp = playerMaxHp; entities[pIdx].x = pStartX; entities[pIdx].z = pStartZ; }
                }
            }
        }
        // Въртящ се предмет (монета/звезда)
        else if(entities[i].tag == TAG_ITEM) {
            entities[i].rotY += 3.5f;
        }
    }

    // 2. Колизии на Играча с предмети и финал
    if(pIdx >= 0) {
        float px = entities[pIdx].x, py = entities[pIdx].y, pz = entities[pIdx].z;

        // Камерата следва плавно играча
        camX = px * 0.5f;
        camY = py + 4.8f;
        camZ = pz + 8.5f;

        for(int i=0; i<MAX_ENTITIES; i++){
            if(!entities[i].active || i == pIdx) continue;
            float dx = px - entities[i].x, dz = pz - entities[i].z;
            float dist = sqrtf(dx*dx + dz*dz);

            if(dist < 1.3f) {
                if(entities[i].tag == TAG_ITEM) { entities[i].active = false; gameScore += 10; }
                else if(entities[i].tag == TAG_GOAL) { gameWon = true; }
            }
        }
    }

    glClearColor(bgR, bgG, bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(shaderProg == 0) return;

    float P[16]; mat4_identity(P);
    float tanHalf = tanf(45.0f * 0.5f * 3.14159f / 180.0f);
    P[0] = 1.0f / (aspect * tanHalf); P[5] = 1.0f / tanHalf;
    P[10] = -100.1f / 99.9f; P[11] = -1.0f; P[14] = -20.0f / 99.9f; P[15] = 0.0f;

    float V[16]; mat4_identity(V);
    float pitch = -0.32f;
    float cp = cosf(pitch), sp = sinf(pitch);
    V[0]=1.0f; V[5]=cp; V[6]=sp; V[9]=-sp; V[10]=cp;
    V[12]=-camX; V[13]=-(cp*camY - sp*camZ); V[14]=-(sp*camY + cp*camZ);

    float VP[16]; mat4_mul(VP, P, V);
    glUseProgram(shaderProg);

    glEnable(GL_DEPTH_TEST);
    glEnableVertexAttribArray(0);
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

// УНИВЕРСАЛНИ ДЕЙСТВИЯ НА БУТОНИТЕ (Trigger Actions)
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_triggerAction(JNIEnv*, jobject, jint actionType) {
    int pIdx = -1;
    for(int i=0; i<MAX_ENTITIES; i++){
        if(entities[i].active && entities[i].tag == TAG_PLAYER){ pIdx = i; break; }
    }
    if(pIdx < 0) return;

    // 1: Ръкопашен удар (Melee / Punch / Kick)
    if(actionType == 1) {
        float px = entities[pIdx].x, pz = entities[pIdx].z;
        for(int i=0; i<MAX_ENTITIES; i++){
            if(entities[i].active && entities[i].tag == TAG_ENEMY){
                float dx = entities[i].x - px, dz = entities[i].z - pz;
                float dist = sqrtf(dx*dx + dz*dz);
                if(dist < 2.5f){
                    entities[i].hp -= 40;
                    entities[i].x += (dx / dist) * 1.5f; // Откат назад
                    entities[i].z += (dz / dist) * 1.5f;
                    if(entities[i].hp <= 0){ entities[i].active = false; gameScore += 100; }
                }
            }
        }
    }
    // 2: Стрелба със снаряд (Shoot Bullet / Fireball)
    else if(actionType == 2) {
        for(int i=0; i<MAX_ENTITIES; i++){
            if(!entities[i].active){
                entities[i] = { true, TAG_BULLET, entities[pIdx].x, entities[pIdx].y, entities[pIdx].z - 1.0f,
                                0.0f, 0.0f, -0.45f, 0.35f, 0.35f, 0.35f, 0.0f, 1.0f, 0.85f, 0.1f, 1, 3 };
                break;
            }
        }
    }
    // 3: Скок / Ускорение (Jump / Dash)
    else if(actionType == 3) {
        entities[pIdx].y += 1.8f;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_movePlayer(JNIEnv*, jobject, jfloat f, jfloat s) {
    for(int i=0; i<MAX_ENTITIES; i++){
        if(entities[i].active && entities[i].tag == TAG_PLAYER){
            float speed = 0.30f;
            entities[i].z -= f * speed;
            entities[i].x += s * speed;
            if(entities[i].y > pStartY) entities[i].y -= 0.08f; // Гравитация след скок
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
