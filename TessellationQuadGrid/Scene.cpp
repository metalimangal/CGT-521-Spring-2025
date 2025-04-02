//When using this as a template, be sure to make these changes in the new project: 
//1. In Debugging properties set the Environment to PATH=%PATH%;$(SolutionDir)\lib;
//2. Copy assets (mesh and texture) to new project directory

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
#include <fstream>
#include <sstream>
#include <string>

#include "Scene.h"
#include "Uniforms.h"
#include "InitShader.h"    //Functions for loading shaders from text files
#include "LoadMesh.h"      //Functions for creating OpenGL buffers from mesh files
#include "LoadTexture.h"   //Functions for creating OpenGL textures from image files
#include "VideoRecorder.h"      //Functions for saving videos
#include "DebugCallback.h"

const int Scene::InitWindowWidth = 1024;
const int Scene::InitWindowHeight = 1024;

static const std::string vertex_shader("tessellation_vs.glsl");
static const std::string tess_ctrl_shader("tessellation_tc.glsl");
static const std::string tess_eval_shader("tessellation_te.glsl");
static const std::string fragment_shader("tessellation_fs.glsl");

int ngrid = 10;
GLuint patch_vao = -1;
GLuint prim_query = -1;
unsigned int primitives_generated = -1;

bool recording = false;

namespace Scene
{
   namespace Camera
   {
      glm::mat4 P;
      glm::mat4 V;

      float Aspect = 1.0f;
      float NearZ = 0.1f;
      float FarZ = 100.0f;
      float Fov = glm::pi<float>() / 4.0f;

      void UpdateP()
      {
         P = glm::perspective(Fov, Aspect, NearZ, FarZ);
      }
   }

   const int msaa_samples = 4;
   GLuint shader_program = -1;
}


// This function gets called every time the scene gets redisplayed
void Scene::Display(GLFWwindow* window)
{
   //Clear the screen to the color previously specified in the glClearColor(...) call.
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   //Camera::V = glm::lookAt(glm::vec3(Uniforms::SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
   //Uniforms::SceneData.PV = Camera::P * Camera::V;
   Uniforms::BufferSceneData();

   glUseProgram(shader_program);

   glBindVertexArray(patch_vao);
   glPatchParameteri(GL_PATCH_VERTICES, 4); //number of input verts to the tess. control shader per patch.

   glBeginQuery(GL_PRIMITIVES_GENERATED, prim_query);

      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); //Draw wireframe so we can see the edges of generated triangles
	   glDrawArraysInstanced(GL_PATCHES, 0, 4, ngrid*ngrid); //Draw patches since we are using a tessellation shader.
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   glEndQuery(GL_PRIMITIVES_GENERATED);

   DrawGui(window);

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

   int available = 0;
   do
   {
      //wait until query is complete
      glGetQueryObjectiv(prim_query, GL_QUERY_RESULT_AVAILABLE, &available);
   }
   while(!available);

   glGetQueryObjectuiv(prim_query, GL_QUERY_RESULT, &primitives_generated);
}

void Scene::DrawGui(GLFWwindow* window)
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
   static bool show_imgui_demo = false;

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
   ImGui::SameLine();
   ImGui::InputText("Video filename", video_filename, filename_len);

   int mode=-1;
   glGetProgramiv(shader_program, GL_TESS_GEN_MODE, &mode);
   switch(mode)
   {
      case GL_QUADS:
         ImGui::Text("Tess Gen Mode = Quads");
      break;
      case GL_TRIANGLES:
         ImGui::Text("Tess Gen Mode = Triangles");
      break;
      case GL_ISOLINES:
         ImGui::Text("Tess Gen Mode = Isolines");
      break;
   }

   int spacing = -1;
   glGetProgramiv(shader_program, GL_TESS_GEN_SPACING, &spacing);
   switch(spacing)
   {
      case GL_EQUAL:
         ImGui::Text("Tess Gen Spacing = Equal");
      break;
      case GL_FRACTIONAL_EVEN:
         ImGui::Text("Tess Gen Spacing = Fractional Even");
      break;
      case GL_FRACTIONAL_ODD:
         ImGui::Text("Tess Gen Spacing = Fractional Odd");
      break;
   }

   ImGui::Text("Primitives generated = %d", primitives_generated);

   ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

   ImGui::End();

   if (show_imgui_demo == true)
   {
      ImGui::ShowDemoWindow(&show_imgui_demo);
   }

   //End ImGui Frame
   ImGui::Render();
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Scene::Idle()
{
   float time_sec = static_cast<float>(glfwGetTime());

   //Pass time_sec value to the shaders
   //glUniform1f(Uniforms::UniformLocs::time, time_sec);
}

void Scene::ReloadShader()
{
   GLuint new_shader = InitShader(vertex_shader.c_str(), tess_ctrl_shader.c_str(), tess_eval_shader.c_str(), fragment_shader.c_str());

   if (new_shader == -1) // loading failed
   {
      DebugBreak(); //alert user by breaking and showing debugger
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

//Initialize OpenGL state. This function only gets called once.
void Scene::Init()
{
   glewInit();
   RegisterDebugCallback();

   //Print out information about the OpenGL version supported by the graphics driver.	
   std::ostringstream oss;
   oss << "GL_VENDOR: " << glGetString(GL_VENDOR) << std::endl;
   oss << "GL_RENDERER: " << glGetString(GL_RENDERER) << std::endl;
   oss << "GL_VERSION: " << glGetString(GL_VERSION) << std::endl;
   oss << "GL_SHADING_LANGUAGE_VERSION: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

   int max_uniform_block_size = 0;
   glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_uniform_block_size);
   oss << "GL_MAX_UNIFORM_BLOCK_SIZE: " << max_uniform_block_size << std::endl;

   int max_storage_block_size = 0;
   glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_storage_block_size);
   oss << "GL_MAX_SHADER_STORAGE_BLOCK_SIZE: " << max_storage_block_size << std::endl;

   int max_texture_size = 0;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
   oss << "GL_MAX_TEXTURE_SIZE: " << max_texture_size << std::endl;

   int max_3d_texture_size = 0;
   glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max_3d_texture_size);
   oss << "GL_MAX_3D_TEXTURE_SIZE: " << max_3d_texture_size << std::endl;

   //Output to console and file
   std::cout << oss.str();

   std::fstream info_file("info.txt", std::ios::out | std::ios::trunc);
   info_file << oss.str();
   info_file.close();
   glEnable(GL_DEPTH_TEST);

   if(Scene::msaa_samples > 1)
   {
      glEnable(GL_MULTISAMPLE);
   }

   ReloadShader();

   //vertices for patch
   glGenVertexArrays(1, &patch_vao);
   glBindVertexArray(patch_vao);

   float vertices[] = {-1.0f, -1.0f, 0.0f, 
                        1.0f, -1.0f, 0.0f, 
                        1.0f, +1.0f, 0.0f,
                       -1.0f, +1.0f, 0.0f};
   
   //create vertex buffer for vertex coords
   GLuint patch_vbo = -1;
   glGenBuffers(1, &patch_vbo);
   glBindBuffer(GL_ARRAY_BUFFER, patch_vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
   int pos_loc = glGetAttribLocation(shader_program, "pos_attrib");
   glEnableVertexAttribArray(pos_loc);
	glVertexAttribPointer(pos_loc, 3, GL_FLOAT, false, 0, 0);
  
   glGenQueries(1, &prim_query);


   Camera::UpdateP();
   Uniforms::Init();
}