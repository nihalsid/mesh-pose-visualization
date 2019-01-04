#ifndef CONTROLS_HPP
#define CONTROLS_HPP
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void computeMatricesFromInputs();
glm::mat4 getViewMatrix();
glm::mat4 getProjectionMatrix();
void setProjectionMatrix(float alpha, float beta, float cx, float cy, int width, int height);
void setViewMatrix(float matrixEntriesRowMajor[]);
#endif