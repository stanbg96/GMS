#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>

static float bgR = 0.15f, bgG = 0.18f, bgB = 0.24f;
static float aspect = 1.0f;
static float camX = 0.0f, camY = 5.5f, camZ = 10.0f;

static int gameScore = 0;
static bool gameWon = false;
static float startX = 0.0f, startY = 0.6f, startZ = 0.0f;

enum Behavior { B_STATIC = 0, B_PLAYER = 1, B_SPIN = 2, B_PATROL_X = 3, B_PATROL_Z = 4 };
enum TouchRule { T_NONE = 0, T_COLLECT = 1, T_HAZARD = 2, T_WIN = 3 };

struct Entity {
    bool active;
    float x, y, z, sx, sy, sz, rotY;
    float r, g, b;
    int behavior, touchRule;
    float origin;
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
    -0.5f,-0.5f,-0.5f, 0,0,-1, -0.5f, 0.5f,-0.5f, 0,0,-1, 0.5f, 0.5f,-0.5f, 0,0,-1,
    -0.5f,-0.5f,-0.5f, 0,0,-1,  0.5f, 0.5f,-0.5f, 0,0,-1, 0.5f,-0.5f,-0.5f, 0,0,-1,
    -0.5f, 0.5f,-0.5f, 0,1,0, -0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f, 0.5f, 0,1,0,
    -0.5f, 0.5f,-0.5f, 0,1,0,  0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f,-0.5f, 0,1,0,
    -0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f, 0.5f, 0,-1,0,
    -0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f, 0.5f, 0,-1,0,-0.5f,-0.5f, 0.5f, 0,-1,0,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f, 0.5f,-0.5f, 1,0,0,  0.5f, 0.5f, 0.5f, 1,0,0,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f, 0.5f, 0.5f, 1,0,0,  0.5f,-0.5f, 0.5f, 1,0,0,
    -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f,-0.5f, 0.5f,-1,0,0, -0.5f, 0.5f, 0.5f,-1,0,0,
    -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f, 0.5f, 0.5f,-1,0,0, -0.5f, 0.5f,-0.5f,-1,0,0
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
    "    float d = max(dot(normalize(vN), normalize(vec3(0.4,0.9,0.5))), 0.0) * 0.55 + 0.45;\n"
    "    oC = vec4(uCol * d, 1.0);\n"
    "}\n";

static void mat4_identity(float* m) {
    for (int i=0; i<16; i++) m[i] = (i%5==0)?1.0f:0.0f;
}

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

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    static float timeTicks = 0.0f;
    timeTicks += 0.035f;

    int pIdx = -1;
    for(int i=0; i<MAX_ENTITIES; i++){
        if(!entities[i].active) continue;
        if(entities[i].behavior == B_PLAYER) pIdx = i;
        else if(entities[i].behavior == B_SPIN) entities[i].rotY += 3.5f;
        else if(entities[i].behavior == B_PATROL_X) entities[i].x = entities[i].origin + sinf(timeTicks)*3.0f;
        else if(entities[i].behavior == B_PATROL_Z) entities[i].z = entities[i].origin + sinf(timeTicks)*3.0f;
    }

    if(pIdx >= 0) {
        camX = entities[pIdx].x;
        camY = entities[pIdx].y + 4.8f;
        camZ = entities[pIdx].z + 8.5f;

        float px = entities[pIdx].x, py = entities[pIdx].y, pz = entities[pIdx].z;
        for(int i=0; i<MAX_ENTITIES; i++){
            if(!entities[i].active || i == pIdx) continue;
            float dx = px - entities[i].x, dy = py - entities[i].y, dz = pz - entities[i].z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if(dist < (entities[pIdx].sx + entities[i].sx)*0.45f){
                if(entities[i].touchRule == T_COLLECT) { entities[i].active = false; gameScore++; }
                else if(entities[i].touchRule == T_HAZARD) { entities[pIdx].x = startX; entities[pIdx].y = startY; entities[pIdx].z = startZ; }
                else if(entities[i].touchRule == T_WIN) { gameWon = true; }
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

    float gM[16], gMVP[16]; mat4_identity(gM); mat4_mul(gMVP, VP, gM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, gMVP);
    glUniform3f(colorLoc, 0.30f, 0.35f, 0.42f);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gridVerts);
    glDrawArrays(GL_LINES, 0, GRID_LINES * 2);

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
Java_com_aigame_engine_NativeEngine_movePlayer(JNIEnv*, jobject, jfloat f, jfloat s) {
    for(int i=0; i<MAX_ENTITIES; i++){
        if(entities[i].active && entities[i].behavior == B_PLAYER){
            entities[i].z -= f * 0.28f;
            entities[i].x += s * 0.28f;
            break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_spawnEntity(JNIEnv*, jobject,
        jfloat x, jfloat y, jfloat z, jfloat sx, jfloat sy, jfloat sz,
        jfloat r, jfloat g, jfloat b, jint beh, jint touch) {
    for(int i=0; i<MAX_ENTITIES; i++){
        if(!entities[i].active){
            entities[i] = { true, x, y, z, sx, sy, sz, 0.0f, r, g, b, beh, touch, (beh==B_PATROL_X)?x:z };
            if(beh == B_PLAYER){ startX = x; startY = y; startZ = z; }
            break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_clearWorld(JNIEnv*, jobject) {
    for(int i=0; i<MAX_ENTITIES; i++) entities[i].active = false;
    gameScore = 0; gameWon = false;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_aigame_engine_NativeEngine_getScore(JNIEnv*, jobject) { return gameScore; }

extern "C" JNIEXPORT jboolean JNICALL
Java_com_aigame_engine_NativeEngine_isWon(JNIEnv*, jobject) { return gameWon; }

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setSky(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    bgR = r; bgG = g; bgB = b;
}
