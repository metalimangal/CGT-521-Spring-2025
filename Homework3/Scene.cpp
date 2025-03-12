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

static const std::string vertex_shader("template_vs.glsl");
static const std::string fragment_shader("template_fs.glsl");
GLuint shader_program = -1;

static const std::string mesh_name = "teapot.obj";
static const std::string texture_name = "AmagoT.bmp";

static bool enableF = true;
static bool enableD = true;
static bool enableG = true;

GLuint texture_id = -1; //Texture map for mesh
MeshData mesh_data;

float angle = 0.0f;
float scale = 1.0f;
glm::vec3 rotationEuler = glm::vec3(0.0f, 0.0f, 0.0f);
bool recording = false;
int lightingMode = 0; // 0: None, 1: Fresnel, 2: Distribution, 3: Geometry, 4: Full Lighting


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

   Camera::V = glm::lookAt(glm::vec3(Uniforms::SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
   Uniforms::SceneData.PV = Camera::P * Camera::V;
   Uniforms::BufferSceneData();

   glUseProgram(shader_program);
   //Note that we don't need to set the value of a uniform here. The value is set with the "binding" in the layout qualifier
   glBindTextureUnit(0, texture_id);

   //Set uniforms
   //glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(scale * mesh_data.mScaleFactor));
   glm::mat4 M =
       glm::rotate(rotationEuler.x, glm::vec3(1.0f, 0.0f, 0.0f)) *
       glm::rotate(rotationEuler.y, glm::vec3(0.0f, 1.0f, 0.0f)) *
       glm::rotate(rotationEuler.z, glm::vec3(0.0f, 0.0f, 1.0f)) *
       glm::scale(glm::vec3(scale * mesh_data.mScaleFactor));

   glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M));

   glUniform1i(Uniforms::UniformLocs::enableF, enableF);
   glUniform1i(Uniforms::UniformLocs::enableD, enableD);
   glUniform1i(Uniforms::UniformLocs::enableG, enableG);

   glBindBuffer(GL_UNIFORM_BUFFER, Uniforms::material_ubo);
   glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Uniforms::MaterialData), &Uniforms::MaterialData);
   glBindBuffer(GL_UNIFORM_BUFFER, 0);
 
   glBindVertexArray(mesh_data.mVao);
   //glDrawElements(GL_TRIANGLES, mesh_data.mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);
   mesh_data.DrawMesh();
   //For meshes with multiple submeshes use mesh_data.DrawMesh(); 

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
   ImGui::SliderFloat3("Rotation (XYZ)", glm::value_ptr(rotationEuler), -glm::pi<float>(), glm::pi<float>());


   ImGui::SliderFloat("Scale", &scale, -10.0f, +10.0f);

   ImGui::Text("Select Lighting Mode:");
   if (ImGui::RadioButton("None", lightingMode == 0)) lightingMode = 0;
   if (ImGui::RadioButton("Fresnel (F)", lightingMode == 1)) lightingMode = 1;
   if (ImGui::RadioButton("Distribution (D)", lightingMode == 2)) lightingMode = 2;
   if (ImGui::RadioButton("Geometry (G)", lightingMode == 3)) lightingMode = 3;
   if (ImGui::RadioButton("Full Lighting", lightingMode == 4)) lightingMode = 4;

   // Update boolean flags based on the selected mode
   enableF = (lightingMode == 1 || lightingMode == 4);
   enableD = (lightingMode == 2 || lightingMode == 4);
   enableG = (lightingMode == 3 || lightingMode == 4);


   ImGui::Begin("Material Settings");

   // Ambient color (ka)
   ImGui::ColorEdit4("Ambient (ka)", glm::value_ptr(Uniforms::MaterialData.ka));

   // Diffuse color (kd)
   ImGui::ColorEdit4("Diffuse (kd)", glm::value_ptr(Uniforms::MaterialData.kd));

   // Specular color (ks)
   ImGui::ColorEdit4("Specular (ks)", glm::value_ptr(Uniforms::MaterialData.ks));

   // Shininess slider
   ImGui::SliderFloat("Shininess", &Uniforms::MaterialData.shininess, 1.0f, 128.0f);

   // Eta (refractive index)
   ImGui::SliderFloat("Eta (Refractive Index)", &Uniforms::MaterialData.eta, 1.0f, 100.0f);

   // Roughness (m in Cook-Torrance)
   ImGui::SliderFloat("Roughness (m)", &Uniforms::MaterialData.roughness, 0.01f, 1.0f);

   ImGui::End();



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
   mesh_data = LoadMesh(mesh_name);
   texture_id = LoadTexture(texture_name);

   Camera::UpdateP();
   Uniforms::Init();
}