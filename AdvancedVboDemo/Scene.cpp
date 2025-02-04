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
#include "Surf.h"

const int Scene::InitWindowWidth = 1024;
const int Scene::InitWindowHeight = 1024;

static const std::string vertex_shader("vbo_vs.glsl");
static const std::string fragment_shader("vbo_fs.glsl");
GLuint shader_program = -1;

surf_vao surf[7];
int draw_surf = 0; // Which of the previous VAOs to draw

float angle = 0.0f;
float scale = 0.1f;
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
}


// This function gets called every time the scene gets redisplayed
void Scene::Display(GLFWwindow* window)
{
   //Clear the screen to the color previously specified in the glClearColor(...) call.
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   Camera::V = glm::lookAt(glm::vec3(Uniforms::SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
   Uniforms::SceneData.PV = Camera::P * Camera::V;
   Uniforms::BufferSceneData();

   glUseProgram(shader_program);
   
   //Set uniforms
   glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::scale(glm::vec3(scale));
   glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M));

   //Draw a surface from currently selected VAO
   glBindVertexArray(surf[draw_surf].vao);
   surf[draw_surf].Draw();

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


   ImGui::SliderFloat("View angle", &angle, -glm::pi<float>(), +glm::pi<float>());
   ImGui::SliderFloat("Scale", &scale, -10.0f, +10.0f);

   static bool enable_culling = false;
   if (ImGui::Checkbox("Backface culling", &enable_culling))
   {
      if (enable_culling) glEnable(GL_CULL_FACE);
      else glDisable(GL_CULL_FACE);
   }

   static int polygon_mode = GL_FILL;
   ImGui::Text("PolygonMode ="); ImGui::SameLine();
   ImGui::RadioButton("GL_FILL", &polygon_mode, GL_FILL); ImGui::SameLine();
   ImGui::RadioButton("GL_LINE", &polygon_mode, GL_LINE); ImGui::SameLine();
   ImGui::RadioButton("GL_POINT", &polygon_mode, GL_POINT);
   glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);

   ImGui::Separator();
   ImGui::Text("Which VBO to draw "); ImGui::SameLine();
   ImGui::RadioButton("GL_POINTS (pos only)", &draw_surf, 0); ImGui::SameLine();
   ImGui::RadioButton("GL_TRIANGLES (pos only)", &draw_surf, 1); ImGui::SameLine();
   ImGui::RadioButton("GL_POINTS separate", &draw_surf, 2);
   ImGui::RadioButton("GL_TRIANGLES separate", &draw_surf, 3); ImGui::SameLine();
   ImGui::RadioButton("GL_TRIANGLES interleaved", &draw_surf, 4); ImGui::SameLine();
   ImGui::RadioButton("indexed GL_TRIANGLES interleaved", &draw_surf, 5); ImGui::SameLine();
   ImGui::RadioButton("indexed GL_TRIANGLE_STRIP interleaved", &draw_surf, 6);
   ImGui::Separator();



   if (ImGui::Button("Show ImGui Demo Window"))
   {
      show_imgui_demo = true;
   }
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
   glUniform1f(Uniforms::UniformLocs::time, time_sec);
}

void Scene::ReloadShader()
{
   GLuint new_shader = InitShader(vertex_shader.c_str(), fragment_shader.c_str());

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

   ReloadShader();

 #pragma region VBO and VBO creation
   const int n = 50;
   //for drawing GL_POINTS with positions only
   surf[0] = create_surf_points_vao(n);

   //for drawing GL_TRIANGLES with positions only
   surf[1] = create_surf_triangles_vao(n);

   //for drawing GL_POINTS with positions, tex_coords and normals separate in the vbo
   surf[2] = create_surf_separate_points_vao(n);

   //for drawing GL_TRIANGLES with positions, tex_coords and normals separate in the vbo
   surf[3] = create_surf_separate_vao(n);

   //for drawing GL_TRIANGLES with positions, tex_coords and normals interleaved in the vbo
   surf[4] = create_surf_interleaved_vao(n);

   //for drawing indexed GL_TRIANGLES with positions, tex_coords and normals interleaved in the vbo
   surf[5] = create_indexed_surf_interleaved_vao(n);

   surf[6] = create_indexed_strip_surf_interleaved_vao(n);
#pragma endregion

   glPointSize(5.0f);
   Uniforms::SceneData.eye_w = glm::vec4(0.0f, 3.0f, 1.0f, 1.0f);

   Camera::UpdateP();
   Uniforms::Init();
}