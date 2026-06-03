#ifndef MATH_H
#define MATH_H

#include <cmath>
#include <array>


using Mat4 = std::array<std::array<float, 4>, 4>;
Mat4 Multiply(Mat4 a,Mat4 b);
Mat4 CalculateModelMatrix(const float position[3], const float rotationDeg[3], const float scale[3]);
Mat4 CalculateInverseModelMatrix(const float position[3], const float rotationDeg[3], const float scale[3]);
#endif