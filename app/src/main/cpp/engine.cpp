#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <string>
#include <vector>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static float bgR = 0.15f, bgG = 0.18f, bgB = 0.24f;
static float aspect = 1.0f;
static float camX = 0.0f, camY = 6.0f, camZ = 12.0f;

static int gameScore = 0;
static bool gameWon = false;
static float pStartX = 0.0f, pStartY = 0.6f, pStartZ = 0.0f;

// 1. EnTT ECS Компоненти
struct TransformComponent {
    float x, y, z;
    float sx, sy, sz;
    float rotY;
};

struct RenderComponent {
    float r, g, b;
    int shapeType; // 0 = Box, 1 = Pyramid, 2 = Crystal
};

struct PhysicsComponent {
    float vx, vy, vz;
    int behavior;  // 0: Static, 1: Player, 2: Spin, 3: Patrol X, 4: Patrol Z
    int touchRule; // 0: None, 1: Collect, 2: Hazard, 3: Win
    float origin;
};

struct StatsComponent {
    int hp;
};

// Главен EnTT Регистър за всички обекти в сцената
static entt::registry gRegistry;

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
    "    float d = max(dot(normalize(vN), normalize(vec3(0.4,0.9,0.5))), 0.0) * 0.55 + 0.45;\n"
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

    gRegistry.clear();
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0,0,w,h);
    aspect = (float)w / (float)(h>0?h:1);
}

// 2. ECS СИСТЕМИ (EnTT Systems в Game Loop)
extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    static float timeTicks = 0.0f;
    timeTicks += 0.035f;

    float playerX = 0.0f, playerY = 0.6f, playerZ = 0.0f;
    bool hasPlayer = false;

    // Системи за физика и поведение в EnTT
    auto physView = gRegistry.view<TransformComponent, PhysicsComponent>();
    for(auto entity : physView) {
        auto& t = physView.get<TransformComponent>(entity);
        auto& p = physView.get<PhysicsComponent>(entity);

        if(p.behavior == 1) { // PLAYER
            playerX = t.x; playerY = t.y; playerZ = t.z;
            hasPlayer = true;
        } else if(p.behavior == 2) { // SPIN (Монети/Диаманти)
            t.rotY += 3.5f;
        } else if(p.behavior == 3) { // PATROL X
            t.x = p.origin + sinf(timeTicks) * 3.0f;
        } else if(p.behavior == 4) { // PATROL Z
            t.z = p.origin + sinf(timeTicks) * 3.0f;
        }
    }

    // Камера и колизии на играча
    if(hasPlayer) {
        camX = playerX;
        camY = playerY + 4.8f;
        camZ = playerZ + 8.5f;

        for(auto entity : physView) {
            auto& t = physView.get<TransformComponent>(entity);
            auto& p = physView.get<PhysicsComponent>(entity);
            if(p.behavior == 1) continue;

            float dx = playerX - t.x, dy = playerY - t.y, dz = playerZ - t.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if(dist < (1.2f + t.sx) * 0.45f) {
                if(p.touchRule == 1) { // COLLECT
                    gRegistry.destroy(entity);
                    gameScore++;
                } else if(p.touchRule == 2) { // HAZARD
                    auto pView = gRegistry.view<TransformComponent, PhysicsComponent>();
                    for(auto pe : pView) {
                        if(pView.get<PhysicsComponent>(pe).behavior == 1) {
                            pView.get<TransformComponent>(pe).x = pStartX;
                            pView.get<TransformComponent>(pe).y = pStartY;
                            pView.get<TransformComponent>(pe).z = pStartZ;
                        }
                    }
                } else if(p.touchRule == 3) { // WIN
                    gameWon = true;
                }
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

    // Под
    float gM[16], gMVP[16]; mat4_identity(gM); mat4_mul(gMVP, VP, gM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, gMVP);
    glUniform3f(colorLoc, 0.30f, 0.35f, 0.42f);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, gridVerts);
    glDrawArrays(GL_LINES, 0, GRID_LINES * 2);

    // EnTT Рендър система: чертае всички активни обекти
    glEnableVertexAttribArray(1);
    auto renderView = gRegistry.view<TransformComponent, RenderComponent>();
    for(auto entity : renderView) {
        auto& t = renderView.get<TransformComponent>(entity);
        auto& r = renderView.get<RenderComponent>(entity);

        float rad = t.rotY * 0.017453f;
        float cr = cosf(rad), sr = sinf(rad);
        float M[16], MVP[16]; mat4_identity(M);
        M[0] = cr * t.sx; M[2] = sr * t.sx;
        M[5] = t.sy;
        M[8] = -sr * t.sz; M[10] = cr * t.sz;
        M[12] = t.x; M[13] = t.y; M[14] = t.z;

        mat4_mul(MVP, VP, M);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, MVP);
        glUniform3f(colorLoc, r.r, r.g, r.b);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), CUBE);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), &CUBE[3]);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

// 3. nlohmann/json Световен парсър за генерирани от AI сцени
extern "C" JNIEXPORT jboolean JNICALL
Java_com_aigame_engine_NativeEngine_loadSceneJson(JNIEnv* env, jobject, jstring jsonStr) {
    const char* str = env->GetStringUTFChars(jsonStr, nullptr);
    std::string s(str);
    env->ReleaseStringUTFChars(jsonStr, str);

    try {
        auto j = json::parse(s);
        gRegistry.clear();
        gameScore = 0;
        gameWon = false;

        if (j.contains("sky") && j["sky"].is_array() && j["sky"].size() >= 3) {
            bgR = j["sky"][0]; bgG = j["sky"][1]; bgB = j["sky"][2];
        }

        if (j.contains("entities") && j["entities"].is_array()) {
            for (const auto& item : j["entities"]) {
                auto e = gRegistry.create();

                float x = item.value("pos", std::vector<float>{0,0,0})[0];
                float y = item.value("pos", std::vector<float>{0,0,0})[1];
                float z = item.value("pos", std::vector<float>{0,0,0})[2];

                float sx = item.value("scale", std::vector<float>{1,1,1})[0];
                float sy = item.value("scale", std::vector<float>{1,1,1})[1];
                float sz = item.value("scale", std::vector<float>{1,1,1})[2];

                float rot = item.value("rot", 0.0f);
                float cr = item.value("color", std::vector<float>{0.8f,0.8f,0.8f})[0];
                float cg = item.value("color", std::vector<float>{0.8f,0.8f,0.8f})[1];
                float cb = item.value("color", std::vector<float>{0.8f,0.8f,0.8f})[2];

                int shape = item.value("shape", 0);
                int behavior = item.value("behavior", 0);
                int touch = item.value("touch", 0);
                int hp = item.value("hp", 100);

                gRegistry.emplace<TransformComponent>(e, x, y, z, sx, sy, sz, rot);
                gRegistry.emplace<RenderComponent>(e, cr, cg, cb, shape);
                gRegistry.emplace<PhysicsComponent>(e, 0.0f, 0.0f, 0.0f, behavior, touch, (behavior == 3 ? x : z));
                gRegistry.emplace<StatsComponent>(e, hp);

                if (behavior == 1) { // PLAYER START
                    pStartX = x; pStartY = y; pStartZ = z;
                }
            }
        }
        return JNI_TRUE;
    } catch (...) {
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_movePlayer(JNIEnv*, jobject, jfloat f, jfloat s) {
    auto view = gRegistry.view<TransformComponent, PhysicsComponent>();
    for(auto entity : view) {
        if(view.get<PhysicsComponent>(entity).behavior == 1) {
            auto& t = view.get<TransformComponent>(entity);
            t.z -= f * 0.28f;
            t.x += s * 0.28f;
            break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_clearWorld(JNIEnv*, jobject) {
    gRegistry.clear();
    gameScore = 0; gameWon = false;
}

extern "C" JNIEXPORT jint JNICALL Java_com_aigame_engine_NativeEngine_getScore(JNIEnv*, jobject) { return gameScore; }
extern "C" JNIEXPORT jboolean JNICALL Java_com_aigame_engine_NativeEngine_isWon(JNIEnv*, jobject) { return gameWon; }
