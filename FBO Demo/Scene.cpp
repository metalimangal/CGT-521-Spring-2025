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
#include "AttriblessRendering.h"

static const std::string vertex_shader("fbo_demo_vs.glsl");
static const std::string fragment_shader("fbo_demo_fs.glsl");
GLuint shader_program = -1;

static const std::string mesh_name = "Amago0.obj";
static const std::string texture_name = "AmagoT.bmp";

GLuint texture_id = -1; //Texture map for mesh
MeshData mesh_data;

GLuint fbo = -1;
GLuint fbo_tex = -1;
int shader_mode = 0;

float angle = 0.0f;
float scale = 1.0f;
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

   const int InitWindowWidth = 1024;
   const int InitWindowHeight = 1024;

   int WindowWidth = InitWindowWidth;
   int WindowHeight = InitWindowHeight;
}


// This function gets called every time the scene gets redisplayed
void Scene::Display(GLFWwindow* window)
{
   Camera::V = glm::lookAt(glm::vec3(Uniforms::SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
   Uniforms::SceneData.PV = Camera::P * Camera::V;
   Uniforms::BufferSceneData();

   glUseProgram(shader_program);
   //Set uniforms
   glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(scale * mesh_data.mScaleFactor));
   glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M));

   ////////////////////////////////////////////////////////////////////////////
   //Render pass 0
   ////////////////////////////////////////////////////////////////////////////
 
   //Note that we don't need to set the value of a uniform here. The value is set with the "binding" in the layout qualifier
   glBindTextureUnit(0, texture_id);

   //Pass 0: scene will be rendered into fbo attachment
   glUniform1i(Uniforms::UniformLocs::pass, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo); // Render to FBO.
   glDrawBuffer(GL_COLOR_ATTACHMENT0); //Out variable in frag shader will be written to the texture attached to GL_COLOR_ATTACHMENT0.

   //Make the viewport match the FBO texture size.
   glViewport(0, 0, Scene::WindowWidth, Scene::WindowHeight);

   //Clear the FBO attached texture.
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   //Draw mesh
   glBindVertexArray(mesh_data.mVao);
   glDrawElements(GL_TRIANGLES, mesh_data.mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);


   ////////////////////////////////////////////////////////////////////////////
   //Render pass 1
   ////////////////////////////////////////////////////////////////////////////

   glUniform1i(Uniforms::UniformLocs::pass, 1);

   //Don't draw this pass into the FBO. Draw to the back buffer
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glDrawBuffer(GL_BACK);
   
   //Make the viewport match the FBO texture size.
   glViewport(0, 0, Scene::WindowWidth, Scene::WindowHeight);

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
   glBindTextureUnit(1, fbo_tex);

   bind_attribless_vao();
   draw_attribless_quad();

#pragma endregion pass 1

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

   //Show fbo_tex for debugging purposes. This is highly recommended for multipass rendering.
   ImGui::Image((ImTextureID)fbo_tex, ImVec2(128.0f, 128.0f), ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));

   ImGui::SliderFloat("View angle", &angle, -glm::pi<float>(), +glm::pi<float>());
   ImGui::SliderFloat("Scale", &scale, -10.0f, +10.0f);

   ImGui::Text("Mode ="); ImGui::SameLine();
   ImGui::RadioButton("No effect", &shader_mode, 0); ImGui::SameLine();
   ImGui::RadioButton("Blur", &shader_mode, 1); ImGui::SameLine();
   ImGui::RadioButton("Edge detect", &shader_mode, 2); ImGui::SameLine();
   ImGui::RadioButton("Vignette", &shader_mode, 3); ImGui::SameLine();
   ImGui::RadioButton("Glitch", &shader_mode, 4); ImGui::SameLine();
   ImGui::RadioButton("Gamma", &shader_mode, 5);
   glUniform1i(Uniforms::UniformLocs::mode, shader_mode);

   ImGui::SliderFloat("View angle", &angle, -glm::pi<float>(), +glm::pi<float>());
   ImGui::SliderFloat("Scale", &scale, -10.0f, +10.0f);
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

#pragma region FBO creation
   //Create a texture object and set initial wrapping and filtering state
   glGenTextures(1, &fbo_tex);
   glBindTexture(GL_TEXTURE_2D, fbo_tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, Scene::InitWindowWidth, Scene::InitWindowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glBindTexture(GL_TEXTURE_2D, 0);

   //Create renderbuffer for depth.
   GLuint rbo = 0;
   glGenRenderbuffers(1, &rbo);
   glBindRenderbuffer(GL_RENDERBUFFER, rbo);
   glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Scene::InitWindowWidth, Scene::InitWindowHeight);

   //Create the framebuffer object
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   //attach the texture we just created to color attachment 1
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_tex, 0);
   //attach depth renderbuffer to depth attachment
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo); 
   //unbind the fbo
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

#pragma endregion
   Camera::UpdateP();
   Uniforms::Init();
}