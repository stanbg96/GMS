#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <string>
#include <vector>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static float bgR = 0.12f, bgG = 0.10f, bgB = 0.15f; // Тъмно Mortal Kombat небе
static float aspect = 1.0f;
static float camX = 0.0f, camY = 5.5f, camZ = 10.0f;

static int playerHp = 100, playerMaxHp = 100;
static int gameScore = 0;
static bool gameWon = false;
static float pStartX = 0.0f, pStartY = 0.0f, pStartZ = 2.0f;
static float attackAnimTimer = 0.0f;

struct TransformComponent {
    float x, y, z;
    float sx, sy, sz;
    float rotY;
};

struct RenderComponent {
    float r, g, b;
    int shapeType; // 0 = Box, 1 = Humanoid Fighter, 2 = Gem
    int texType;   // 0 = Smooth, 1 = Stone Tiles, 2 = Belt
};

struct PhysicsComponent {
    float vx, vy, vz;
    int behavior;  // 0: Static, 1: Player, 2: Spin, 3: Patrol X, 4: Enemy Chase
    int touchRule; // 0: None, 1: Collect, 2: Hazard/Enemy, 3: Win
    float origin;
};

struct StatsComponent {
    int hp;
};

static entt::registry gRegistry;

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
    "    if(uTexType == 1) {\n"
    "        vec2 grid = abs(fract(vPos.xz * 0.5) - 0.5);\n"
    "        float lines = smoothstep(0.45, 0.49, max(grid.x, grid.y));\n"
    "        col = mix(col, vec3(0.1, 0.1, 0.12), lines);\n"
    "    } else if(uTexType == 2) {\n"
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

static void drawHumanoidFighter(float bx, float by, float bz, float rot, float r, float g, float b, bool isAttacking, const float* VP) {
    drawPart(bx, by + 1.1f, bz, 0.75f, 0.85f, 0.45f, rot, r, g, b, 2, VP);
    drawPart(bx, by + 1.8f, bz, 0.45f, 0.45f, 0.45f, rot, 0.95f, 0.80f, 0.65f, 0, VP);
    drawPart(bx, by + 1.88f, bz, 0.48f, 0.12f, 0.48f, rot, r * 0.8f, g * 0.8f, b * 0.8f, 0, VP);
    drawPart(bx - 0.22f, by + 0.45f, bz, 0.28f, 0.85f, 0.32f, rot, 0.18f, 0.18f, 0.20f, 0, VP);
    drawPart(bx + 0.22f, by + 0.45f, bz, 0.28f, 0.85f, 0.32f, rot, 0.18f, 0.18f, 0.20f, 0, VP);
    float punchForward = isAttacking ? -0.85f : 0.0f;
    drawPart(bx - 0.52f, by + 1.15f, bz + 0.15f, 0.22f, 0.75f, 0.24f, rot, r, g, b, 0, VP);
    drawPart(bx + 0.52f, by + 1.15f, bz + punchForward, 0.24f, 0.24f, 0.85f, rot, r, g, b, 0, VP);
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

    gRegistry.clear();
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0,0,w,h);
    aspect = (float)w / (float)(h>0?h:1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_onDrawFrame(JNIEnv*, jobject) {
    if (attackAnimTimer > 0.0f) attackAnimTimer -= 0.05f;

    float playerX = 0.0f, playerY = 0.0f, playerZ = 0.0f;
    bool hasPlayer = false;

    // Движение на враговете в EnTT
    auto view = gRegistry.view<TransformComponent, PhysicsComponent>();
    for(auto entity : view) {
        auto& t = view.get<TransformComponent>(entity);
        auto& p = view.get<PhysicsComponent>(entity);

        if(p.behavior == 1) { // PLAYER
            playerX = t.x; playerY = t.y; playerZ = t.z;
            hasPlayer = true;
        }
    }

    if(hasPlayer) {
        for(auto entity : view) {
            auto& t = view.get<TransformComponent>(entity);
            auto& p = view.get<PhysicsComponent>(entity);

            if(p.behavior == 4) { // ВРАГ преследва играча
                float dx = playerX - t.x, dz = playerZ - t.z;
                float dist = sqrtf(dx*dx + dz*dz);
                if(dist > 1.6f) {
                    t.x += (dx / dist) * 0.045f;
                    t.z += (dz / dist) * 0.045f;
                } else {
                    static int atkCd = 0;
                    if(++atkCd > 30) {
                        atkCd = 0;
                        playerHp -= 10;
                        if(playerHp <= 0) { playerHp = playerMaxHp; }
                    }
                }
            }
        }
    }

    float targetX = playerX, targetY = playerY + 1.0f, targetZ = playerZ;
    camX = targetX;
    camY = targetY + 5.5f;
    camZ = targetZ + 9.5f;

    // Защита: никога чисто бял екран!
    if (bgR > 0.85f && bgG > 0.85f && bgB > 0.85f) {
        bgR = 0.12f; bgG = 0.10f; bgB = 0.15f;
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

    // Каменна арена под
    float floorM[16], floorMVP[16];
    mat4_identity(floorM); mat4_mul(floorMVP, VP, floorM);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, floorMVP);
    glUniform3f(colorLoc, 0.28f, 0.26f, 0.32f);
    glUniform1i(texTypeLoc, 1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), ARENA_FLOOR);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), &ARENA_FLOOR[3]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Рендиране през EnTT
    auto renderView = gRegistry.view<TransformComponent, RenderComponent>();
    for(auto entity : renderView) {
        auto& t = renderView.get<TransformComponent>(entity);
        auto& r = renderView.get<RenderComponent>(entity);

        if(r.shapeType == 1) { // 3D Хуманоиден боец (Shaolin & Ninja)
            bool isAtk = (t.x == playerX && attackAnimTimer > 0.0f);
            drawHumanoidFighter(t.x, t.y, t.z, t.rotY, r.r, r.g, r.b, isAtk, VP);
        } else {
            drawPart(t.x, t.y + t.sy*0.5f, t.z, t.sx, t.sy, t.sz, t.rotY, r.r, r.g, r.b, 0, VP);
        }
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

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
            if (bgR > 1.0f) bgR /= 255.0f;
            if (bgG > 1.0f) bgG /= 255.0f;
            if (bgB > 1.0f) bgB /= 255.0f;
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
                if (cr > 1.0f) cr /= 255.0f;
                if (cg > 1.0f) cg /= 255.0f;
                if (cb > 1.0f) cb /= 255.0f;

                int shape = item.value("shape", 0);
                int behavior = item.value("behavior", 0);
                int touch = item.value("touch", 0);
                int hp = item.value("hp", 100);

                // Автоматично прави играчите и враговете на хуманоидни бойци
                if (behavior == 1 || behavior == 4) shape = 1;

                gRegistry.emplace<TransformComponent>(e, x, y, z, sx, sy, sz, rot);
                gRegistry.emplace<RenderComponent>(e, cr, cg, cb, shape, 0);
                gRegistry.emplace<PhysicsComponent>(e, 0.0f, 0.0f, 0.0f, behavior, touch, x);
                gRegistry.emplace<StatsComponent>(e, hp);

                if (behavior == 1) {
                    pStartX = x; pStartY = y; pStartZ = z;
                    playerHp = hp; playerMaxHp = hp;
                }
            }
        }
        return JNI_TRUE;
    } catch (...) {
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_triggerAction(JNIEnv*, jobject, jint actionType) {
    attackAnimTimer = 0.25f;

    auto view = gRegistry.view<TransformComponent, PhysicsComponent, StatsComponent>();
    float px = 0, pz = 0;
    for(auto e : view) {
        if(view.get<PhysicsComponent>(e).behavior == 1) {
            px = view.get<TransformComponent>(e).x;
            pz = view.get<TransformComponent>(e).z;
            break;
        }
    }

    for(auto e : view) {
        auto& p = view.get<PhysicsComponent>(e);
        if(p.behavior == 4) { // ВРАГ
            auto& t = view.get<TransformComponent>(e);
            auto& s = view.get<StatsComponent>(e);
            float dx = t.x - px, dz = t.z - pz;
            float dist = sqrtf(dx*dx + dz*dz);
            if(dist < 2.8f) {
                s.hp -= 40;
                if(dist > 0.05f) {
                    t.x += (dx / dist) * 1.8f;
                    t.z += (dz / dist) * 1.8f;
                }
                if(s.hp <= 0) {
                    gRegistry.destroy(e);
                    gameScore += 100;
                    gameWon = true;
                }
            }
        }
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
    playerHp = 100; gameScore = 0; gameWon = false;
}

extern "C" JNIEXPORT jint JNICALL Java_com_aigame_engine_NativeEngine_getPlayerHp(JNIEnv*, jobject) { return playerHp; }
extern "C" JNIEXPORT jint JNICALL Java_com_aigame_engine_NativeEngine_getScore(JNIEnv*, jobject) { return gameScore; }
extern "C" JNIEXPORT jboolean JNICALL Java_com_aigame_engine_NativeEngine_isWon(JNIEnv*, jobject) { return gameWon; }
