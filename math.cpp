#include "math.h"
Mat4 Multiply(const Mat4 a, const Mat4 b) {
    Mat4 result{};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result[i][j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    return result;
}

static Mat4 CreateIdentityMatrix() {
    Mat4 mat{};
    for (int i = 0; i < 4; i++) {
        mat[i][i] = 1.0f;
    }
    return mat;
}

static Mat4 CreateTranslationMatrix(const float position[3]) {
    Mat4 mat = CreateIdentityMatrix();
    mat[3][0] = position[0];
    mat[3][1] = position[1];
    mat[3][2] = position[2];
    return mat;
}

static Mat4 CreateScaleMatrix(const float scale[3]) {
    Mat4 mat = CreateIdentityMatrix();
    mat[0][0] = scale[0];
    mat[1][1] = scale[1];
    mat[2][2] = scale[2];
    return mat;
}

static Mat4 CreateRotationMatrix(const float rotation[3]) {
    float rx = rotation[0];
    float ry = rotation[1];
    float rz = rotation[2];

    Mat4 rotX = CreateIdentityMatrix();
    rotX[1][1] = cosf(rx);
    rotX[1][2] = -sinf(rx);
    rotX[2][1] = sinf(rx);
    rotX[2][2] = cosf(rx);

    Mat4 rotY = CreateIdentityMatrix();
    rotY[0][0] = cosf(ry);
    rotY[0][2] = sinf(ry);
    rotY[2][0] = -sinf(ry);
    rotY[2][2] = cosf(ry);

    Mat4 rotZ = CreateIdentityMatrix();
    rotZ[0][0] = cosf(rz);
    rotZ[0][1] = -sinf(rz);
    rotZ[1][0] = sinf(rz);
    rotZ[1][1] = cosf(rz);

    return Multiply(rotX, Multiply(rotY, rotZ));
}

Mat4 CalculateModelMatrix(const float position[3], const float rotation[3], const float scale[3]) {
    Mat4 S = CreateScaleMatrix(scale);
    Mat4 R = CreateRotationMatrix(rotation);
    Mat4 T = CreateTranslationMatrix(position);

    // Row-major: Model = S * R * T
    return Multiply(S, Multiply(R, T));
}

static Mat4 Transpose(const Mat4& m) {
    Mat4 t{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            t[i][j] = m[j][i];
        }
    }
    return t;
}

Mat4 CalculateInverseModelMatrix(const float position[3], const float rotation[3], const float scale[3]) {
    float iS[3] = { 1.0f / scale[0], 1.0f / scale[1], 1.0f / scale[2] };
    float iP[3] = { -position[0], -position[1], -position[2] };

    Mat4 invScale = CreateScaleMatrix(iS);
    Mat4 invRot = Transpose(CreateRotationMatrix(rotation));
    Mat4 invTrans = CreateTranslationMatrix(iP);

    return Multiply(invTrans, Multiply(invRot, invScale));
}
