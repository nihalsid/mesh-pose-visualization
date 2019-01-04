#include "pch.h"
// Include GLFW
#include <GLFW/glfw3.h>
#include "controls.hpp"

extern GLFWwindow* window; // The "extern" keyword here is to access the variable "window" declared in tutorialXXX.cpp. This is a hack to keep the tutorials simple. Please avoid this.

// Include GLM
using namespace glm;

glm::mat4 ViewMatrix;
glm::mat4 ProjectionMatrix;

glm::mat4 getViewMatrix() {
	return ViewMatrix;
}
glm::mat4 getProjectionMatrix() {
	return ProjectionMatrix;
}


// Initial position : on +Z
glm::vec3 position = glm::vec3(3, 3, 3);
// Initial horizontal angle : toward -Z
float horizontalAngle = 3.14f;
// Initial vertical angle : none
float verticalAngle = 0.0f;
// Initial Field of View
float initialFoV = 45.0f;

float speed = 3.0f; // 3 units / second
float mouseSpeed = 0.005f;


void setProjectionMatrix(float alpha, float beta, float cx, float cy, int width, int height) {
	float f = 100.0f;
	float n = 0.2f;
	ProjectionMatrix[0][0] = 2 * alpha / width;
	ProjectionMatrix[0][1] = 0;
	ProjectionMatrix[0][2] = 0;
	ProjectionMatrix[0][3] = 0;
	ProjectionMatrix[1][0] = 0;
	ProjectionMatrix[1][1] = - 2 * beta / height;
	ProjectionMatrix[1][2] = 0;
	ProjectionMatrix[1][3] = 0;
	ProjectionMatrix[2][0] = 2 * (cx / width) - 1;
	ProjectionMatrix[2][1] = -2 * (cy / height) + 1;
	ProjectionMatrix[2][2] = -(f + n) / (f - n);
	ProjectionMatrix[2][3] = -1;
	ProjectionMatrix[3][0] = 0;
	ProjectionMatrix[3][1] = 0;
	ProjectionMatrix[3][2] = -2 * f * n / (f - n);
	ProjectionMatrix[3][3] = 0;
}

void setViewMatrix(float matrixEntriesRowMajor[]) {
	glm::mat4x4 cam2WorldMatrix;
	cam2WorldMatrix[0][0] = matrixEntriesRowMajor[0];
	cam2WorldMatrix[1][0] = matrixEntriesRowMajor[1];
	cam2WorldMatrix[2][0] = matrixEntriesRowMajor[2];
	cam2WorldMatrix[3][0] = matrixEntriesRowMajor[3];
	cam2WorldMatrix[0][1] = matrixEntriesRowMajor[4];
	cam2WorldMatrix[1][1] = matrixEntriesRowMajor[5];
	cam2WorldMatrix[2][1] = matrixEntriesRowMajor[6];
	cam2WorldMatrix[3][1] = matrixEntriesRowMajor[7];
	cam2WorldMatrix[0][2] = matrixEntriesRowMajor[8];
	cam2WorldMatrix[1][2] = matrixEntriesRowMajor[9];
	cam2WorldMatrix[2][2] = matrixEntriesRowMajor[10];
	cam2WorldMatrix[3][2] = matrixEntriesRowMajor[11];
	cam2WorldMatrix[0][3] = matrixEntriesRowMajor[12];
	cam2WorldMatrix[1][3] = matrixEntriesRowMajor[13];
	cam2WorldMatrix[2][3] = matrixEntriesRowMajor[14];
	cam2WorldMatrix[3][3] = matrixEntriesRowMajor[15];
	glm::mat4x4 world2camMatrix = glm::inverse(cam2WorldMatrix);
	glm::mat4x4 cam(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1);
	ViewMatrix = cam * world2camMatrix;
	
}

void computeMatricesFromInputs() {

	// glfwGetTime is called only once, the first time this function is called
	static double lastTime = glfwGetTime();

	// Compute time difference between current and last frame
	double currentTime = glfwGetTime();
	float deltaTime = float(currentTime - lastTime);

	// Get mouse position
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	// Reset mouse position for next frame
	glfwSetCursorPos(window, 960 / 2, 540 / 2);

	// Compute new orientation
	horizontalAngle += mouseSpeed * float(960 / 2 - xpos);
	verticalAngle += mouseSpeed * float(540 / 2 - ypos);

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	glm::vec3 direction(
		cos(verticalAngle) * sin(horizontalAngle),
		sin(verticalAngle),
		cos(verticalAngle) * cos(horizontalAngle)
	);

	// Right vector
	glm::vec3 right = glm::vec3(
		sin(horizontalAngle - 3.14f / 2.0f),
		0,
		cos(horizontalAngle - 3.14f / 2.0f)
	);

	// Up vector
	glm::vec3 up = glm::cross(right, direction);

	// Move forward
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
		position += direction * deltaTime * speed;
	}
	// Move backward
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		position -= direction * deltaTime * speed;
	}
	// Strafe right
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		position += right * deltaTime * speed;
	}
	// Strafe left
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		position -= right * deltaTime * speed;
	}

	float FoV = initialFoV;// - 5 * glfwGetMouseWheel(); // Now GLFW 3 requires setting up a callback for this. It's a bit too complicated for this beginner's tutorial, so it's disabled instead.

	// Projection matrix : 45� Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	ProjectionMatrix = glm::perspective(glm::radians(FoV), 4.0f / 3.0f, 0.1f, 100.0f);
	// Camera matrix
	ViewMatrix = glm::lookAt(
		position,           // Camera is here
		position + direction, // and looks here : at the same position, plus "direction"
		up                  // Head is up (set to 0,-1,0 to look upside-down)
	);

	// For the next frame, the "last time" will be "now"
	lastTime = currentTime;
}