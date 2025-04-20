#include <windows.h>

//When using this as a template, be sure to make these changes in the new project: 
//1. In Debugging properties set the Environment to PATH=%PATH%;$(SolutionDir)\lib;
//2. Change window_title below
//3. Copy assets (mesh and texture) to new project directory

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "InitShader.h"    //Functions for loading shaders from text files
#include "LoadMesh.h"      //Functions for creating OpenGL buffers from mesh files
#include "LoadTexture.h"   //Functions for creating OpenGL textures from image files
#include "VideoRecorder.h"      //Functions for saving videos
#include "DebugCallback.h"

const int init_window_width = 1024;
const int init_window_height = 1024;
const char* const window_title = "Shadow Lab";

static const std::string vertex_shader("shadowmap_vs.glsl");
static const std::string fragment_shader("shadowmap_fs.glsl");
GLuint shader_program = -1;

static const std::string mesh_name = "dragon.obj";

MeshData mesh_data;
GLuint ground_vao;

GLuint fbo_id = -1;       // framebuffer object,
GLuint shadow_map_texture_id = -1;
int shadow_map_size = 2048; //Lab: this is the size of the shadow map. Try making it bigger.

float light_fov = glm::pi<float>() / 2.0f;

float angle = 0.0f;
float scale = 1.0f;
float aspect = 1.0f;
bool recording = false;

static float polygonOffsetFactor = 1.0f;
static float polygonOffsetUnits = 0.5f;
int pcf_mode = 0; // 0 = 2x2, 1 = 4x4

float w_numerator = 1.5f;
int w_numerator_index = 2;
float w_numerator_options[] = { 0.5f, 1.0f, 1.5f, 2.0f };
const int num_options = sizeof(w_numerator_options) / sizeof(float);

bool debug_lit4 = false;



//This structure mirrors the uniform block declared in the shader
struct SceneUniforms
{
	glm::mat4 PV;	//camera projection * view matrix
	glm::mat4 V;
	glm::mat4 Shadow; //shadow matrix
	glm::vec4 eye_w;	//world-space eye position
} SceneData;

struct LightUniforms
{
	glm::vec4 La = glm::vec4(0.2f, 0.2f, 0.30f, 1.0f);	//ambient light color
	glm::vec4 Ld = glm::vec4(0.7f, 0.7f, 0.4f, 1.0f);	//diffuse light color
	glm::vec4 Ls = glm::vec4(0.4f, 0.4f, 0.2f, 1.0f);	//specular light color
	glm::vec4 light_w = glm::vec4(0.0f, 1.2, 1.0f, 1.0f); //world-space light position

} LightData;

struct MaterialUniforms
{
	glm::vec4 ka = glm::vec4(1.0f);	//ambient material color
	glm::vec4 kd = glm::vec4(1.0f);	//diffuse material color
	glm::vec4 ks = glm::vec4(1.0f);	//specular material color
	float shininess = 20.0f;         //specular exponent
} MaterialData[2];

//IDs for the buffer objects holding the uniform block data
GLuint scene_ubo = -1;
GLuint light_ubo = -1;
GLuint material_ubo[2] = { -1, -1 };

namespace UboBinding
{
	//These values come from the binding value specified in the shader block layout
	int scene = 0;
	int light = 1;
	int material = 2;
}

//Locations for the uniforms which are not in uniform blocks
namespace UniformLocs
{
	int M = 0; //model matrix
	int time = 1;
	int pass = 2;
	int u_pcf_mode = 3;
	int w_numerator = 4;
	int debug_lit4 = 5;
}


//For an explanation of this program's structure see https://www.glfw.org/docs/3.3/quick.html 

void draw_gui(GLFWwindow* window)
{
	//Begin ImGui Frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	//Draw Gui
	ImGui::Begin("Debug window");
	if (ImGui::Button("Quit"))
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	const int filename_len = 256;
	static char video_filename[filename_len] = "capture.mp4";

	ImGui::InputText("Video filename", video_filename, filename_len);
	ImGui::SameLine();
	if (recording == false)
	{
		if (ImGui::Button("Start Recording"))
		{
			int w, h;
			glfwGetFramebufferSize(window, &w, &h);
			recording = true;
			const int fps = 60;
			const int bitrate = 4000000;
			VideoRecorder::Start(video_filename, w, h, fps, bitrate); //Uses ffmpeg
		}
	}
	else
	{
		if (ImGui::Button("Stop Recording"))
		{
			recording = false;
			VideoRecorder::Stop(); //Uses ffmpeg
		}
	}

	ImGui::SliderFloat("View angle", &angle, -glm::pi<float>(), +glm::pi<float>());
	ImGui::SliderFloat("Scale", &scale, -10.0f, +10.0f);

	ImGui::SliderFloat("Light FOV", &light_fov, 0.0f, glm::pi<float>());

	ImGui::SliderFloat("Polygon Offset Factor", &polygonOffsetFactor, -5.0f, 5.0f);
	ImGui::SliderFloat("Polygon Offset Units", &polygonOffsetUnits, -5.0f, 5.0f);


	// Display selected value
	ImGui::Text("Current Shadow Map Size: %d", shadow_map_size);


	ImGui::Image((ImTextureID)shadow_map_texture_id, ImVec2(128.0f, 128.0f), ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));


	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

	//static bool show_test = false;
	//ImGui::ShowDemoWindow(&show_test);

	//End ImGui Frame
	//Set texture compare mode for this draw call, then restore it later
	int mode = -1;
	glGetTextureParameteriv(shadow_map_texture_id, GL_TEXTURE_COMPARE_MODE, &mode);
	glTextureParameteri(shadow_map_texture_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glTextureParameteri(shadow_map_texture_id, GL_TEXTURE_COMPARE_MODE, mode);
}

void draw_scene(int pass);

// This function gets called every time the scene gets redisplayed
void display(GLFWwindow* window)
{
	//Clear the screen to the color previously specified in the glClearColor(...) call.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	SceneData.eye_w = glm::vec4(0.0f, 1.0f, 3.0f, 1.0f);
	glm::mat4 P_camera = glm::perspective(glm::pi<float>() / 4.0f, aspect, 1.0f, 400.0f);
	glm::mat4 V_camera = glm::lookAt(glm::vec3(SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	//P_light: field-of-view and near/far clip plane of light
	glm::mat4 P_light = glm::perspective(light_fov, 1.0f, 0.1f, 10.0f);
	glm::mat4 V_light = glm::lookAt(glm::vec3(LightData.light_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	const glm::mat4 S = glm::translate(glm::vec3(0.5f)) * glm::scale(glm::vec3(0.5f)); //scale and bias matrix
	SceneData.Shadow = S * P_light * V_light * glm::inverse(V_camera); //this matrix transforms camera-space coordinates to shadow map texture coordinates

	glUniform1i(UniformLocs::u_pcf_mode, pcf_mode);
	glUniform1f(UniformLocs::w_numerator, w_numerator);
	glUniform1i(UniformLocs::debug_lit4, debug_lit4 ? 1 : 0);

	glUseProgram(shader_program);

	//pass 0: render to shadow map
	glUniform1i(UniformLocs::pass, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo_id); // render depth to shadowmap
	glDrawBuffer(GL_NONE);
	//glViewport(0, 0, shadow_map_size, shadow_map_size);
	glViewport(0, 0, shadow_map_size-8, shadow_map_size-8);  //*hack* (Why does this work?)
	glClear(GL_DEPTH_BUFFER_BIT);

	SceneData.PV = P_light * V_light;
	SceneData.V = V_light;
	glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo); //Bind the OpenGL UBO before we update the data.
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &SceneData); //Upload the new uniform values.

	glEnable(GL_POLYGON_OFFSET_FILL);
	//glPolygonOffset(1.0f, 1.0f); //No offset is being applied when params are 0.0, 0.0. Try changing these numbers to fix the shadow map "acne" problem
	//glPolygonOffset(1.0f, 0.0f); //No offset is being applied when params are 0.0, 0.0. Try changing these numbers to fix the shadow map "acne" problem
	glPolygonOffset(polygonOffsetFactor, polygonOffsetUnits);
	draw_scene(0);
	glDisable(GL_POLYGON_OFFSET_FILL);

	//pass 1: render scene using shadow map
	glUniform1i(UniformLocs::pass, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // do not render to texture
	glDrawBuffer(GL_BACK);
	glViewport(0, 0, init_window_width, init_window_height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	SceneData.PV = P_camera * V_camera;
	SceneData.V = V_camera;
	glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &SceneData); //Upload the new uniform values.
	glBindBuffer(GL_UNIFORM_BUFFER, 0); //unbind the ubo
	glBindTextureUnit(0, shadow_map_texture_id);

	draw_scene(1);

	draw_gui(window);

	if (recording == true)
	{
		glFinish();
		glReadBuffer(GL_BACK);
		int w, h;
		glfwGetFramebufferSize(window, &w, &h);
		VideoRecorder::EncodeBuffer(GL_BACK);
	}

	/* Swap front and back buffers */
	glfwSwapBuffers(window);
}

void draw_scene(int pass)
{
	glm::mat4 M_mesh = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(scale * mesh_data.mScaleFactor));
	glm::mat4 M_ground = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::translate(glm::vec3(0.0f, -0.5f, 0.0f)) * glm::rotate(-glm::pi<float>() / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(100.0f));

	//draw mesh
	glBindBuffer(GL_UNIFORM_BUFFER, material_ubo[0]);
	glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::material, material_ubo[0]);
	glUniformMatrix4fv(UniformLocs::M, 1, false, glm::value_ptr(M_mesh));
	glBindVertexArray(mesh_data.mVao);
	glDrawElements(GL_TRIANGLES, mesh_data.mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);

	if (pass == 1)
	{
		//draw ground plane
		glBindBuffer(GL_UNIFORM_BUFFER, material_ubo[0]);
		glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::material, material_ubo[1]);
		glUniformMatrix4fv(UniformLocs::M, 1, false, glm::value_ptr(M_ground));
		glBindVertexArray(ground_vao);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
}

void idle()
{
	float time_sec = static_cast<float>(glfwGetTime());

	//Pass time_sec value to the shaders
	glUniform1f(UniformLocs::time, time_sec);
}

void reload_shader()
{
	GLuint new_shader = InitShader(vertex_shader.c_str(), fragment_shader.c_str());

	if (new_shader == -1) // loading failed
	{
		glClearColor(1.0f, 0.0f, 1.0f, 0.0f); //change clear color if shader can't be compiled
	}
	else
	{
		glClearColor(0.35f, 0.35f, 0.35f, 0.0f);

		if (shader_program != -1)
		{
			glDeleteProgram(shader_program);
		}
		shader_program = new_shader;
	}
}

//This function gets called when a key is pressed
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	//std::cout << "key : " << key << ", " << char(key) << ", scancode: " << scancode << ", action: " << action << ", mods: " << mods << std::endl;

	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case 'r':
		case 'R':
			reload_shader();
			break;

		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			break;

		case GLFW_KEY_P: 
			pcf_mode = 1 - pcf_mode;
			std::cout << "Toggled PCF mode: " << (pcf_mode == 0 ? "2x2" : "4x4") << std::endl;
			break;

		case GLFW_KEY_W:
			w_numerator_index = (w_numerator_index + 1) % num_options;
			w_numerator = w_numerator_options[w_numerator_index];
			std::cout << "Softness numerator set to: " << w_numerator << std::endl;
			break;

		case GLFW_KEY_L: 
			debug_lit4 = !debug_lit4;
			std::cout << "Debug lit4 mode: " << (debug_lit4 ? "ON" : "OFF") << std::endl;
			break;
		}
	}
}

//This function gets called when the mouse moves over the window.
void mouse_cursor(GLFWwindow* window, double x, double y)
{
	//std::cout << "cursor pos: " << x << ", " << y << std::endl;
}

//This function gets called when a mouse button is pressed.
void mouse_button(GLFWwindow* window, int button, int action, int mods)
{
	//std::cout << "button : "<< button << ", action: " << action << ", mods: " << mods << std::endl;
}

void resize(GLFWwindow* window, int width, int height)
{
	//Set viewport to cover entire framebuffer
	glViewport(0, 0, width, height);
	//Set aspect ratio used in view matrix calculation
	aspect = float(width) / float(glm::max(height, 1));
}

//Initialize OpenGL state. This function only gets called once.
void initOpenGL()
{
	glewInit();

#ifdef _DEBUG
	RegisterDebugCallback();
#endif

	//Print out information about the OpenGL version supported by the graphics driver.	
	std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
	std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
	std::cout << "Version: " << glGetString(GL_VERSION) << std::endl;
	std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
	glEnable(GL_DEPTH_TEST);

	reload_shader();
	mesh_data = LoadMesh(mesh_name);

	//Create and initialize uniform buffers
	glGenBuffers(1, &scene_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneUniforms), nullptr, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
	glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::scene, scene_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

	glGenBuffers(1, &light_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, light_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(LightUniforms), &LightData, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
	glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::light, light_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.


	MaterialData[0].ka = glm::vec4(1.0f, 0.9f, 0.8f, 1.0f);
	MaterialData[0].kd = MaterialData[0].ka;
	MaterialData[1].shininess = 1.0f;
	glGenBuffers(2, material_ubo);
	for (int i = 0; i < 2; i++)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, material_ubo[i]);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(MaterialUniforms), &MaterialData[i], GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
		glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::material, material_ubo[i]); //Associate this uniform buffer with the uniform block in the shader that has the same binding.
	}
	glBindBuffer(GL_UNIFORM_BUFFER, 0);


	//vertices for ground plane
	glGenVertexArrays(1, &ground_vao);
	glBindVertexArray(ground_vao);

	float vertices[] = { -1.0f, -1.0f, 0.0f,
						-1.0f, +1.0f, 0.0f,
						+1.0f, -1.0f, 0.0f,
						+1.0f, +1.0f, 0.0f };

	//create vertex buffer for vertex coords
	GLuint ground_vbo = -1;
	glGenBuffers(1, &ground_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, ground_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	int pos_loc = glGetAttribLocation(shader_program, "pos_attrib");
	glEnableVertexAttribArray(pos_loc);
	glVertexAttribPointer(pos_loc, 3, GL_FLOAT, false, 0, 0);
	int normal_loc = glGetAttribLocation(shader_program, "normal_attrib");
	glVertexAttrib3f(normal_loc, 0.0f, 0.0f, +1.0f);
	glBindVertexArray(0);

	//create shadow map texture to hold depth values (note: not a renderbuffer)
	glBindTexture(GL_TEXTURE_2D, shadow_map_texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, shadow_map_size, shadow_map_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	glBindTexture(GL_TEXTURE_2D, 0);

	//create the framebuffer object: only has a depth attachment
	glGenFramebuffers(1, &fbo_id);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_map_texture_id, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



//C++ programs start executing in the main() function.
int main(int argc, char** argv)
{
	GLFWwindow* window;

	/* Initialize the library */
	if (!glfwInit())
	{
		return -1;
	}

#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(init_window_width, init_window_height, window_title, NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	//Register callback functions with glfw. 
	glfwSetKeyCallback(window, keyboard);
	glfwSetCursorPosCallback(window, mouse_cursor);
	glfwSetMouseButtonCallback(window, mouse_button);
	glfwSetFramebufferSizeCallback(window, resize);

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	initOpenGL();

	//Init ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 150");

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		idle();
		display(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	// Cleanup ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}