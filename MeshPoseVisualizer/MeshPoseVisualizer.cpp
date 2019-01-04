// MeshPoseVisualizer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <algorithm> 

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "shader.hpp"
#include "controls.hpp"

GLFWwindow* window = nullptr;

#include <algorithm> 
#include <functional> 
#include <filesystem>
#include <math.h>  

void __inline swap(unsigned char& x, unsigned char& y) {
	unsigned char temp = x;
	x = y;
	y = temp;
}

// trim from start
static inline std::string &ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
	return ltrim(rtrim(s));
}

static std::string GetBaseDir(const std::string& filepath) {
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

static bool FileExists(const std::string& abs_filename) {
	bool ret;
	FILE* fp = fopen(abs_filename.c_str(), "rb");
	if (fp) {
		ret = true;
		fclose(fp);
	}
	else {
		ret = false;
	}

	return ret;
}

static void CheckErrors(std::string desc) {
	GLenum e = glGetError();
	if (e != GL_NO_ERROR) {
		fprintf(stderr, "OpenGL error in \"%s\": %d (%d)\n", desc.c_str(), e, e);
		exit(20);
	}
}

typedef struct {
	std::vector<float> vertices;
	std::vector<float> uvs;
	std::vector<float> normals;
	std::vector<float> colors;
	std::vector<float> faces;
	std::vector<float> faceAreas;
	int numTriangles;
	size_t material_id;
} DrawObject;

static void CalcNormal(float N[3], float v0[3], float v1[3], float v2[3]) {
	float v10[3];
	v10[0] = v1[0] - v0[0];
	v10[1] = v1[1] - v0[1];
	v10[2] = v1[2] - v0[2];

	float v20[3];
	v20[0] = v2[0] - v0[0];
	v20[1] = v2[1] - v0[1];
	v20[2] = v2[2] - v0[2];

	N[0] = v20[1] * v10[2] - v20[2] * v10[1];
	N[1] = v20[2] * v10[0] - v20[0] * v10[2];
	N[2] = v20[0] * v10[1] - v20[1] * v10[0];

	float len2 = N[0] * N[0] + N[1] * N[1] + N[2] * N[2];
	if (len2 > 0.0f) {
		float len = sqrtf(len2);

		N[0] /= len;
		N[1] /= len;
		N[2] /= len;
	}
}


static bool LoadObjAndConvert(float bmin[3], float bmax[3], std::vector<DrawObject>* drawObjects, std::vector<tinyobj::material_t>& materials, std::map<std::string, GLuint>& textures, const char* filename) {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;

	std::string base_dir = GetBaseDir(filename);
	if (base_dir.empty()) {
		base_dir = ".";
	}
#ifdef _WIN32
	base_dir += "\\";
#else
	base_dir += "/";
#endif

	std::string warn;
	std::string err;
	
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, base_dir.c_str());
	if (!warn.empty()) {
		std::cout << "WARN: " << warn << std::endl;
	}
	if (!err.empty()) {
		std::cerr << err << std::endl;
	}

	if (!ret) {
		std::cerr << "Failed to load " << filename << std::endl;
		return false;
	}

	printf("# of vertices  = %d\n", (int)(attrib.vertices.size()) / 3);
	printf("# of normals   = %d\n", (int)(attrib.normals.size()) / 3);
	printf("# of texcoords = %d\n", (int)(attrib.texcoords.size()) / 2);
	printf("# of materials = %d\n", (int)materials.size());
	printf("# of shapes    = %d\n", (int)shapes.size());

	// Append `default` material
	materials.push_back(tinyobj::material_t());

	for (size_t i = 0; i < materials.size(); i++) {
		printf("material[%d].diffuse_texname = %s\n", int(i),
			materials[i].diffuse_texname.c_str());
	}

	// Load diffuse textures
	{
		for (size_t m = 0; m < materials.size(); m++) {
			tinyobj::material_t* mp = &materials[m];

			if (mp->diffuse_texname.length() > 0) {
				// Only load the texture if it is not already loaded
				if (textures.find(mp->diffuse_texname) == textures.end()) {
					GLuint texture_id;
					int w, h;
					int comp;

					std::string texture_filename = mp->diffuse_texname;
					if (!FileExists(texture_filename)) {
						// Append base dir.
						texture_filename = base_dir + mp->diffuse_texname;
						if (!FileExists(texture_filename)) {
							std::cerr << "Unable to find file: " << mp->diffuse_texname << std::endl;
							exit(1);
						}
					}

					unsigned char* image = stbi_load(texture_filename.c_str(), &w, &h, &comp, STBI_default);
					if (!image) {
						std::cerr << "Unable to load texture: " << texture_filename << std::endl;
						exit(1);
					}
					std::cout << "Loaded texture: " << texture_filename << ", w = " << w << ", h = " << h << ", comp = " << comp << std::endl;

					glGenTextures(1, &texture_id);
					glBindTexture(GL_TEXTURE_2D, texture_id);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					if (comp == 3) {
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
					}
					else if (comp == 4) {
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
					}
					else {
						assert(0);  // TODO
					}
					glBindTexture(GL_TEXTURE_2D, 0);
					stbi_image_free(image);
					textures.insert(std::make_pair(mp->diffuse_texname, texture_id));
				}
			}
		}
	}

	bmin[0] = bmin[1] = bmin[2] = std::numeric_limits<float>::max();
	bmax[0] = bmax[1] = bmax[2] = -std::numeric_limits<float>::max();

	{
		for (size_t s = 0; s < shapes.size(); s++) {
			DrawObject o;

			for (size_t f = 0; f < shapes[s].mesh.indices.size() / 3; f++) {
				tinyobj::index_t idx0 = shapes[s].mesh.indices[3 * f + 0];
				tinyobj::index_t idx1 = shapes[s].mesh.indices[3 * f + 1];
				tinyobj::index_t idx2 = shapes[s].mesh.indices[3 * f + 2];

				int current_material_id = shapes[s].mesh.material_ids[f];

				if ((current_material_id < 0) || (current_material_id >= static_cast<int>(materials.size()))) {
					// Invaid material ID. Use default material.
					current_material_id = materials.size() - 1;  // Default material is added to the last item in `materials`.
				}

				float diffuse[3];
				for (size_t i = 0; i < 3; i++) {
					diffuse[i] = materials[current_material_id].diffuse[i];
				}

				float tc[3][2];
				
				if (attrib.texcoords.size() > 0) {
					if ((idx0.texcoord_index < 0) || (idx1.texcoord_index < 0) || (idx2.texcoord_index < 0)) {
						// face does not contain valid uv index.
						tc[0][0] = 0.0f;
						tc[0][1] = 0.0f;
						tc[1][0] = 0.0f;
						tc[1][1] = 0.0f;
						tc[2][0] = 0.0f;
						tc[2][1] = 0.0f;
					}
					else {
						assert(attrib.texcoords.size() >
							size_t(2 * idx0.texcoord_index + 1));
						assert(attrib.texcoords.size() >
							size_t(2 * idx1.texcoord_index + 1));
						assert(attrib.texcoords.size() >
							size_t(2 * idx2.texcoord_index + 1));

						// Flip Y coord.
						tc[0][0] = attrib.texcoords[2 * idx0.texcoord_index];
						tc[0][1] = 1.0f - attrib.texcoords[2 * idx0.texcoord_index + 1];
						tc[1][0] = attrib.texcoords[2 * idx1.texcoord_index];
						tc[1][1] = 1.0f - attrib.texcoords[2 * idx1.texcoord_index + 1];
						tc[2][0] = attrib.texcoords[2 * idx2.texcoord_index];
						tc[2][1] = 1.0f - attrib.texcoords[2 * idx2.texcoord_index + 1];
					}
				}
				else {
					tc[0][0] = 0.0f;
					tc[0][1] = 0.0f;
					tc[1][0] = 0.0f;
					tc[1][1] = 0.0f;
					tc[2][0] = 0.0f;
					tc[2][1] = 0.0f;
				}

				float v[3][3];
				for (int k = 0; k < 3; k++) {
					int f0 = idx0.vertex_index;
					int f1 = idx1.vertex_index;
					int f2 = idx2.vertex_index;
					assert(f0 >= 0);
					assert(f1 >= 0);
					assert(f2 >= 0);

					v[0][k] = attrib.vertices[3 * f0 + k];
					v[1][k] = attrib.vertices[3 * f1 + k];
					v[2][k] = attrib.vertices[3 * f2 + k];
					bmin[k] = std::min(v[0][k], bmin[k]);
					bmin[k] = std::min(v[1][k], bmin[k]);
					bmin[k] = std::min(v[2][k], bmin[k]);
					bmax[k] = std::max(v[0][k], bmax[k]);
					bmax[k] = std::max(v[1][k], bmax[k]);
					bmax[k] = std::max(v[2][k], bmax[k]);
				}

				float n[3][3];
				{
					bool invalid_normal_index = false;
					if (attrib.normals.size() > 0) {
						int nf0 = idx0.normal_index;
						int nf1 = idx1.normal_index;
						int nf2 = idx2.normal_index;

						if ((nf0 < 0) || (nf1 < 0) || (nf2 < 0)) {
							// normal index is missing from this face.
							invalid_normal_index = true;
						}
						else {
							for (int k = 0; k < 3; k++) {
								assert(size_t(3 * nf0 + k) < attrib.normals.size());
								assert(size_t(3 * nf1 + k) < attrib.normals.size());
								assert(size_t(3 * nf2 + k) < attrib.normals.size());
								n[0][k] = attrib.normals[3 * nf0 + k];
								n[1][k] = attrib.normals[3 * nf1 + k];
								n[2][k] = attrib.normals[3 * nf2 + k];
							}
						}
					}
					else {
						invalid_normal_index = true;
					}

					if (invalid_normal_index) {
						// compute geometric normal
						CalcNormal(n[0], v[0], v[1], v[2]);
						n[1][0] = n[0][0];
						n[1][1] = n[0][1];
						n[1][2] = n[0][2];
						n[2][0] = n[0][0];
						n[2][1] = n[0][1];
						n[2][2] = n[0][2];
					}
				}

				int fr = (1 + f) % 256;
				int fg = ((1 + f) / 256) % 256;
				int fb = ((1 + f) / 256 / 256) % 256;

				for (int k = 0; k < 3; k++) {
					o.vertices.push_back(v[k][0]);
					o.vertices.push_back(v[k][1]);
					o.vertices.push_back(v[k][2]);
					o.normals.push_back(n[k][0]);
					o.normals.push_back(n[k][1]);
					o.normals.push_back(n[k][2]);
					// Combine normal and diffuse to get color.
					float normal_factor = 0.2;
					float diffuse_factor = 1 - normal_factor;
					float c[3] = { n[k][0] * normal_factor + diffuse[0] * diffuse_factor,
								  n[k][1] * normal_factor + diffuse[1] * diffuse_factor,
								  n[k][2] * normal_factor + diffuse[2] * diffuse_factor };
					float len2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
					if (len2 > 0.0f) {
						float len = sqrtf(len2);

						c[0] /= len;
						c[1] /= len;
						c[2] /= len;
					}
					o.colors.push_back(c[0] * 0.5 + 0.5);
					o.colors.push_back(c[1] * 0.5 + 0.5);
					o.colors.push_back(c[2] * 0.5 + 0.5);

					o.faces.push_back(fr / 256.0);
					o.faces.push_back(fg / 256.0);
					o.faces.push_back(fb / 256.0);

					o.uvs.push_back(tc[k][0]);
					o.uvs.push_back(tc[k][1]);
				}

				// area of triangle 
				// s = 0.5 * sqrt ( (x2 * y3 - x3 * y2) ^ 2  + (x3 * y1 - x1 * y3) ^ 2 + (x1 * y2 - x2 * y1)  )
				// A = v[0], B = v[1], C = v[2]
				// v10 = v[1] - v[0]
				// v20 = v[2] - v[0]
				// s = 0.5 * sqrt ( (v10[1] * v20[2] - v10[2] * v20[1]) ^ 2  + (v10[2] * v20[0] - v10[0] * v20[2]) ^ 2 + (v10[0] * v20[1] - v10[1] * v20[0]) )
				float v10[3], v20[3];
				v10[0] = v[1][0] - v[0][0];
				v10[1] = v[1][1] - v[0][1];
				v10[2] = v[1][2] - v[0][2];
				v20[0] = v[2][0] - v[0][0];
				v20[1] = v[2][1] - v[0][1];
				v20[2] = v[2][2] - v[0][2];
				;
				float area = 0.5 * sqrt(pow((v10[1] * v20[2] - v10[2] * v20[1]), 2.f) + pow(v10[2] * v20[0] - v10[0] * v20[2], 2.f) + pow(v10[0] * v20[1] - v10[1] * v20[0], 2.f));
				o.faceAreas.push_back(area);
			}

			o.numTriangles = 0;

			// OpenGL viewer does not support texturing with per-face material.
			if (shapes[s].mesh.material_ids.size() > 0 && shapes[s].mesh.material_ids.size() > s) {
				o.material_id = shapes[s].mesh.material_ids[0];  // use the material ID
																 // of the first face.
			}
			else {
				o.material_id = materials.size() - 1;  // = ID for default material.
			}
			printf("shape[%d] material_id %d\n", int(s), int(o.material_id));

			if (o.vertices.size() > 0) {
				o.numTriangles = o.vertices.size() / 3 ;  // 3:vtx, 3:normal, 3:col, 2:texcoord
				printf("shape[%d] # of triangles = %d\n", static_cast<int>(s), o.numTriangles);
			}

			drawObjects->push_back(o);
		}
	}

	printf("bmin = %f, %f, %f\n", bmin[0], bmin[1], bmin[2]);
	printf("bmax = %f, %f, %f\n", bmax[0], bmax[1], bmax[2]);

	return true;
}

void readMatrixFile(const std::string& filePath, float* arrayRef) {
	std::ifstream infile(filePath);
	std::string line, strtoken;
	int arrInd = 0;
	while (std::getline(infile, line)) {
		if (line.length() > 0) {
			std::stringstream linestream(line);
			for (int i = 0; i < 4; i++) {
				std::getline(linestream, strtoken, ' ');
				arrayRef[arrInd++] = std::stof(ltrim(rtrim(strtoken)));
			}
		}
	}
	infile.close();
}
std::string getBasename(std::string filename) {
	const size_t last_slash_idx = filename.find_last_of("\\/");
	if (std::string::npos != last_slash_idx)
	{
		filename.erase(0, last_slash_idx + 1);
	}
	const size_t period_idx = filename.find('.');
	if (std::string::npos != period_idx)
	{
		filename.erase(period_idx);
	}
	return filename;
}

int main(int argc, char** argv) {
	//std::string objFile = "D:\\nihalsid\\Label23D\\server\\static\\test\\cube.obj";
	std::string rootDir = argv[1];
	std::string objFile = rootDir + "\\mesh\\mesh.refined.obj";
	std::vector<std::string> cam2WorldMatrixFiles; 
	std::vector<std::string> faceMapFiles;
	std::string faceAreasFile = rootDir + "\\face_maps\\areas.txt";
	for (const auto & entry : std::experimental::filesystem::directory_iterator(rootDir + "\\color\\")) {
		std::string basename = getBasename(entry.path().string());
		cam2WorldMatrixFiles.push_back(rootDir + "\\pose\\" + basename + ".pose.txt");
		faceMapFiles.push_back(rootDir + "\\face_maps\\" + basename + ".facemap.png");
	}
	std::string camIntrinsicsFile = rootDir + "\\camera\\intrinsic_color.txt";


	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(960, 540, "MeshPoseVisualization", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	// Hide the mouse and enable unlimited mouvement
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// Set the mouse at the center of the screen
	glfwPollEvents();
	glfwSetCursorPos(window, 960 / 2, 540 / 2);

	// null background
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);

	// Shade model flat for no interpolation
	glShadeModel(GL_FLAT);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders("TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader");

	// Get a handle for our "MVP" uniform
	GLuint MatrixID = glGetUniformLocation(programID, "MVP");

	// Get a handle for our "myTextureSampler" uniform
	GLuint TextureID = glGetUniformLocation(programID, "myTextureSampler");

	std::vector<DrawObject> drawObjects;
	std::map<std::string, GLuint> textures;
	std::vector<tinyobj::material_t> materials;
	float bmin[3], bmax[3];
	LoadObjAndConvert(bmin, bmax, &drawObjects, materials, textures, objFile.c_str());

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, drawObjects[0].vertices.size() * sizeof(float), &drawObjects[0].vertices[0], GL_STATIC_DRAW);

	/*
	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, drawObjects[0].uvs.size() * sizeof(float), &drawObjects[0].uvs[0], GL_STATIC_DRAW);
	*/

	GLuint colorbuffer;
	glGenBuffers(1, &colorbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
	glBufferData(GL_ARRAY_BUFFER, drawObjects[0].faces.size() * sizeof(float), &drawObjects[0].faces[0], GL_STATIC_DRAW);

	std::ofstream areasStream(faceAreasFile);
	for (int i = 0; i < drawObjects[0].faceAreas.size(); i++) {
		areasStream << drawObjects[0].faceAreas[i] << "\n";
	}
	areasStream.close();

	float cam2WorldRowMajor[16], camIntrinsicRowMajor[16];
	readMatrixFile(camIntrinsicsFile, camIntrinsicRowMajor);
	setProjectionMatrix(camIntrinsicRowMajor[0], camIntrinsicRowMajor[5], camIntrinsicRowMajor[2], camIntrinsicRowMajor[6], 960, 540);

	for (int i = 0; i < cam2WorldMatrixFiles.size(); i++) {

		readMatrixFile(cam2WorldMatrixFiles[i], cam2WorldRowMajor);
		setViewMatrix(cam2WorldRowMajor);

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use our shader
		glUseProgram(programID);

		// Compute the MVP matrix from keyboard and mouse input
		//computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

		// Send our transformation to the currently bound shader, 
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

		// Bind our texture in Texture Unit 0
		// glActiveTexture(GL_TEXTURE0);
		// glBindTexture(GL_TEXTURE_2D, textures[materials[drawObjects[0].material_id].diffuse_texname]);
		// Set our "myTextureSampler" sampler to use Texture Unit 0
		// glUniform1i(TextureID, 0);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : colors
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
		glVertexAttribPointer(
			1,                                // attribute
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// Draw the triangle !
		glDrawArrays(GL_TRIANGLES, 0, drawObjects[0].numTriangles);
		unsigned char* image = (unsigned char*)malloc(sizeof(unsigned char) * 960 * 540 * 3);
		glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, image);
		for (int r_idx = 0; r_idx < 540 / 2; r_idx++) {
			for (int c_idx = 0; c_idx < 960; c_idx++) {
				swap(image[(r_idx * 960 + c_idx) * 3 + 0], image[((540 - r_idx - 1) * 960 + c_idx) * 3 + 0]);
				swap(image[(r_idx * 960 + c_idx) * 3 + 1], image[((540 - r_idx - 1) * 960 + c_idx) * 3 + 1]);
				swap(image[(r_idx * 960 + c_idx) * 3 + 2], image[((540 - r_idx - 1) * 960 + c_idx) * 3 + 2]);
			}
		}
		stbi_write_png(faceMapFiles[i].c_str(), 960, 540, 3, image, 960 * 3);
		free(image);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();
	} 
	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &colorbuffer);
	glDeleteProgram(programID);
	glDeleteTextures(1, &textures[materials[drawObjects[0].material_id].name]);
	glDeleteVertexArrays(1, &VertexArrayID);

}

