#include <jni.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>

static float bgR = 0.55f, bgG = 0.68f, bgB = 0.82f;
static float aspect = 1.0f;

static float camYaw = 0.85f;
static float camPitch = 0.45f;
static float camDist = 12.0f;
static float targetY = 1.2f;

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b;
};

static std::vector<Vertex> meshVertices;

static GLuint shaderProgram = 0;
static GLint mvpLoc = -1, eyePosLoc = -1;

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

void parseObjString(const std::string& objData, float r, float g, float b) {
    std::vector<float> tempPos;
    std::vector<float> tempNorm;
    meshVertices.clear();

    std::stringstream ss(objData);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.size() < 2) continue;
        if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if (sscanf(line.c_str() + 2, "%f %f %f", &x, &y, &z) == 3) {
                tempPos.push_back(x); tempPos.push_back(y); tempPos.push_back(z);
            }
        } else if (line[0] == 'v' && line[1] == 'n' && line[2] == ' ') {
            float nx, ny, nz;
            if (sscanf(line.c_str() + 3, "%f %f %f", &nx, &ny, &nz) == 3) {
                tempNorm.push_back(nx); tempNorm.push_back(ny); tempNorm.push_back(nz);
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            std::stringstream lineStream(line.substr(2));
            std::string faceToken;
            std::vector<int> faceV;
            std::vector<int> faceN;

            while (lineStream >> faceToken) {
                int vi = 0, ti = 0, ni = 0;
                if (sscanf(faceToken.c_str(), "%d/%d/%d", &vi, &ti, &ni) == 3) {
                    faceV.push_back(vi); faceN.push_back(ni);
                } else if (sscanf(faceToken.c_str(), "%d//%d", &vi, &ni) == 2) {
                    faceV.push_back(vi); faceN.push_back(ni);
                } else if (sscanf(faceToken.c_str(), "%d/%d", &vi, &ti) == 2) {
                    faceV.push_back(vi); faceN.push_back(0);
                } else if (sscanf(faceToken.c_str(), "%d", &vi) == 1) {
                    faceV.push_back(vi); faceN.push_back(0);
                }
            }

            if (faceV.size() >= 3) {
                for (size_t i = 1; i + 1 < faceV.size(); i++) {
                    int idx[3] = { 0, (int)i, (int)i + 1 };
                    for (int k = 0; k < 3; k++) {
                        int vIdx = (faceV[idx[k]] - 1) * 3;
                        if (vIdx >= 0 && vIdx + 2 < (int)tempPos.size()) {
                            float nx = 0.0f, ny = 1.0f, nz = 0.0f;
                            int nIdx = (faceN[idx[k]] - 1) * 3;
                            if (faceN[idx[k]] > 0 && nIdx >= 0 && nIdx + 2 < (int)tempNorm.size()) {
                                nx = tempNorm[nIdx]; ny = tempNorm[nIdx+1]; nz = tempNorm[nIdx+2];
                            }
                            meshVertices.push_back({ tempPos[vIdx], tempPos[vIdx+1], tempPos[vIdx+2], nx, ny, nz, r, g, b });
                        }
                    }
                }
            }
        }
    }

    // Автоматично центриране и поставяне точно на пода
    if (!meshVertices.empty()) {
        float minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9, minZ = 1e9, maxZ = -1e9;
        for (const auto& v : meshVertices) {
            if (v.x < minX) minX = v.x; if (v.x > maxX) maxX = v.x;
            if (v.y < minY) minY = v.y; if (v.y > maxY) maxY = v.y;
            if (v.z < minZ) minZ = v.z; if (v.z > maxZ) maxZ = v.z;
        }

        float cx = (minX + maxX) * 0.5f;
        float cy = minY; // Основата на гумите стъпва на пода
        float cz = (minZ + maxZ) * 0.5f;
        float maxDim = fmaxf(fmaxf(maxX - minX, maxY - minY), maxZ - minZ);
        float scale = (maxDim > 0.001f) ? (5.5f / maxDim) : 1.0f;

        for (auto& v : meshVertices) {
            v.x = (v.x - cx) * scale;
            v.y = (v.y - cy) * scale;
            v.z = (v.z - cz) * scale;
        }

        // Автоматично изчисляване на гладки нормали при липса
        for (size_t i = 0; i + 2 < meshVertices.size(); i += 3) {
            if (meshVertices[i].nx == 0 && meshVertices[i].ny == 1 && meshVertices[i].nz == 0) {
                float u1 = meshVertices[i+1].x - meshVertices[i].x;
                float u2 = meshVertices[i+1].y - meshVertices[i].y;
                float u3 = meshVertices[i+1].z - meshVertices[i].z;
                float v1 = meshVertices[i+2].x - meshVertices[i].x;
                float v2 = meshVertices[i+2].y - meshVertices[i].y;
                float v3 = meshVertices[i+2].z - meshVertices[i].z;
                float fnx = u2*v3 - u3*v2;
                float fny = u3*v1 - u1*v3;
                float fnz = u1*v2 - u2*v1;
                float l = sqrtf(fnx*fnx + fny*fny + fnz*fnz);
                if (l > 0.0001f) { fnx /= l; fny /= l; fnz /= l; }
                meshVertices[i].nx = meshVertices[i+1].nx = meshVertices[i+2].nx = fnx;
                meshVertices[i].ny = meshVertices[i+1].ny = meshVertices[i+2].ny = fny;
                meshVertices[i].nz = meshVertices[i+1].nz = meshVertices[i+2].nz = fnz;
            }
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
    eyePosLoc = glGetUniformLocation(shaderProgram, "uEyePos");

    glEnable(GL_DEPTH_TEST);

    int idx = 0;
    for (int i = -8; i <= 8; i++) {
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.005f; gridVertices[idx++] = -8.0f;
        gridVertices[idx++] = (float)i; gridVertices[idx++] = 0.005f; gridVertices[idx++] =  8.0f;
        gridVertices[idx++] = -8.0f;    gridVertices[idx++] = 0.005f; gridVertices[idx++] = (float)i;
        gridVertices[idx++] =  8.0f;    gridVertices[idx++] = 0.005f; gridVertices[idx++] = (float)i;
    }
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

    if (!meshVertices.empty()) {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshVertices[0].x);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshVertices[0].nx);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &meshVertices[0].r);

        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)meshVertices.size());

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_loadObjString(JNIEnv* env, jobject, jstring objStr, jfloat r, jfloat g, jfloat b) {
    const char* str = env->GetStringUTFChars(objStr, nullptr);
    parseObjString(std::string(str), r, g, b);
    env->ReleaseStringUTFChars(objStr, str);
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_clearMesh(JNIEnv*, jobject) {
    meshVertices.clear();
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
    if (camDist < 2.0f) camDist = 2.0f;
    if (camDist > 40.0f) camDist = 40.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aigame_engine_NativeEngine_setBackgroundColor(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    bgR = r; bgG = g; bgB = b;
}
