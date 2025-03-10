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

static const std::string vertex_shader("instancing_vs.glsl");
static const std::string fragment_shader("instancing_fs.glsl");
GLuint shader_program = -1;

static const std::vector<std::string> mesh_name{ "sphere.obj", "rock1.obj", "rock2.obj" };
static const std::vector<std::string> texture_name{ "2k_saturn.jpg", "rock1.jpg", "rock2.jpg" };

GLuint texture_id[3] = { -1, -1, -1 }; //Texture maps for meshes
MeshData mesh_data[3];

glm::vec3 camera_pos(3.5f, 2.9f, 1.2f);
int n_instances = 1000;
bool useInstancing = true;

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

   Camera::V = glm::lookAt(glm::vec3(Uniforms::SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.2f, 1.0f));
   Uniforms::SceneData.PV = Camera::P * Camera::V;
   Uniforms::BufferSceneData();

   glUseProgram(shader_program);
 

   glm::mat4 M[3];
   M[0] = glm::rotate(glm::pi<float>() / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(1.0f));
   M[1] = glm::translate(glm::vec3(2.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(0.00003f));
   M[2] = glm::translate(glm::vec3(2.2f, 0.0f, 0.0f)) * glm::scale(glm::vec3(0.00005f));


   enum MESH { PLANET, ASTEROID1, ASTEROID2 };

   //Draw planet
   int i = PLANET;
   glUniform1i(Uniforms::UniformLocs::isAsteroid, 0);
   glBindTextureUnit(0, texture_id[i]);
   glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M[i]));
   glBindVertexArray(mesh_data[i].mVao);
   glDrawElements(GL_TRIANGLES, mesh_data[i].mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);

   //Draw asteroids
   glUniform1i(Uniforms::UniformLocs::isAsteroid, 1);

   if (useInstancing == true)
   {
      for (i = ASTEROID1; i <= ASTEROID2; i++)
      {
         glBindTextureUnit(0, texture_id[i]);
         glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M[i]));
         glBindVertexArray(mesh_data[i].mVao);
         glDrawElementsInstanced(GL_TRIANGLES, mesh_data[i].mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0, n_instances);
      }
   }
   else
   {
      for (i = ASTEROID1; i <= ASTEROID2; i++)
      {
         glBindTextureUnit(0, texture_id[i]);

         glBindVertexArray(mesh_data[i].mVao);
         for (int inst = 0; inst < n_instances; inst++)
         {
            glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M[i]));
            glDrawElements(GL_TRIANGLES, mesh_data[i].mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);
         }
      }
   }




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

   ImGui::SliderFloat3("Camera pos", &Uniforms::SceneData.eye_w.x, -10.0f, +10.0f);
   ImGui::Checkbox("Use instancing", &useInstancing);
   ImGui::SliderInt("Num Instances", &n_instances, 1, 10000);


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
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

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

   for (int i = 0; i < 3; i++)
   {
      mesh_data[i] = LoadMesh(mesh_name[i]);
      texture_id[i] = LoadTexture(texture_name[i]);
   }

   Uniforms::SceneData.eye_w = glm::vec4(camera_pos, 1.0f);

   Camera::UpdateP();
   Uniforms::Init();
}