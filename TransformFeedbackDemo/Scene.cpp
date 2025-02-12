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
#include "UniformGui.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

const int Scene::InitWindowWidth = 1024;
const int Scene::InitWindowHeight = 1024;

static const std::string vertex_shader("xform_feedback_vs.glsl");
static const std::string fragment_shader("xform_feedback_fs.glsl");
GLuint shader_program = -1;

#pragma region transform feedback object and buffer declarations
   //Ping-pong pairs of objects and buffers since we don't have simultaneous read/write access to VBOs.
   GLuint vao[2] = {-1, -1};
   GLuint vbo[2] = {-1, -1};
   const int num_particles = 100000;
   GLuint tfo[2] = { -1, -1 }; //transform feedback objects
   //Names of the VS out variables that should be captured by transform feedback
   const char *xform_feedback_varyings[] = { "pos_out", "vel_out", "age_out" };

   namespace AttribLocs
   {
      int pos = 0;
      int vel = 1;
      int age = 2;
   }

   //These indices get swapped every frame to perform the ping-ponging
   int read_index = 0;  //initially read from vbo[0]
   int write_index = 1; //initially write to vbo[1]

   void transform_feedback_relink(GLuint shader_program);
#pragma endregion transform feedback object and buffer declarations

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


   //Set uniforms
   glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(scale));
   glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M));

   glDepthMask(GL_FALSE); //Disable depth write for particles

#pragma region transform feedback rendering

   const bool TFO_SUPPORTED = true;

   if(TFO_SUPPORTED == true)
   {
      //Bind the current write transform feedback object.
      glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfo[write_index]);
   }
   else
   {
      //Binding the transform feedback object recalls the buffer range state shown below. If
      //your system does not support transform feedback objects you can uncomment the following lines.
      const GLint pos_varying = 0;
      const GLint vel_varying = 1;
      const GLint age_varying = 2;

      glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, pos_varying, vbo[write_index]);
      glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, vel_varying, vbo[write_index]);
      glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, age_varying, vbo[write_index]);
   }

   //Prepare the pipeline for transform feedback
   glBeginTransformFeedback(GL_POINTS);
   glBindVertexArray(vao[read_index]);
   glDrawArrays(GL_POINTS, 0, num_particles);
   glEndTransformFeedback();

   if(TFO_SUPPORTED == true)
   {
      //Unbind the current write transform feedback object.
      glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
   }

   //Ping-pong the buffers.
   read_index = 1 - read_index;
   write_index = 1 - write_index;

#pragma endregion transform feedback rendering

   glDepthMask(GL_TRUE);


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

   UniformGui(shader_program);

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

      transform_feedback_relink(shader_program);
   }
}

void transform_feedback_relink(GLuint shader_program)
{
   //You need to specify which varying variables will capture transform feedback values.
   glTransformFeedbackVaryings(shader_program, 3, xform_feedback_varyings, GL_INTERLEAVED_ATTRIBS);

   //Must relink the program after specifying transform feedback varyings.
   glLinkProgram(shader_program);
   int status;
   glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
   if (status == GL_FALSE)
   {
      DebugBreak();
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

 #pragma region render state for particles

   //Enable alpha blending
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE); //additive alpha blending
   //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //semitransparent alpha blending

   //Note: The next two states are important for particle systems
   //Enable gl_PointCoord in shader
   glEnable(GL_POINT_SPRITE);
   //Allow setting point size in fragment shader
   glEnable(GL_PROGRAM_POINT_SIZE);

#pragma endregion render state for particles

   //create TFOs
   glGenTransformFeedbacks(2, tfo);
  
   //These are the indices in the array passed to glTransformFeedbackVaryings (const char *vars[] = { "pos_out", "vel_out", "age_out" };)
   const GLint pos_varying = 0;
   const GLint vel_varying = 1;
   const GLint age_varying = 2;

   //create VAOs and VBOs
   glGenVertexArrays(2, vao);
   glGenBuffers(2, vbo);   

   const int stride = 7*sizeof(float);
   const int pos_offset = 0;
   const int vel_offset = sizeof(glm::vec3);
   const int age_offset = 2*sizeof(glm::vec3);
   const int pos_size = num_particles * sizeof(glm::vec3);
   const int vel_size = num_particles * sizeof(glm::vec3);
   const int age_size = num_particles * sizeof(float);

   for(int i=0; i<2; i++)
   {
      //Create VAO and VBO with interleaved attributes
      glBindVertexArray(vao[i]); 
      glBindBuffer(GL_ARRAY_BUFFER, vbo[i]); 
      
      //All attributes are initially 0.0
      glBufferData(GL_ARRAY_BUFFER, sizeof(float)*7*num_particles, nullptr, GL_DYNAMIC_COPY);
      float zero = 0.0f;
      glClearNamedBufferData(vbo[i], GL_R32F, GL_RED, GL_FLOAT, (void*)&zero);

      glEnableVertexAttribArray(AttribLocs::pos); 
      glVertexAttribPointer(AttribLocs::pos, 3, GL_FLOAT, false, stride, BUFFER_OFFSET(pos_offset)); 

      glEnableVertexAttribArray(AttribLocs::vel); 
      glVertexAttribPointer(AttribLocs::vel, 3, GL_FLOAT, false, stride, BUFFER_OFFSET(vel_offset));            
                                                                                    
      glEnableVertexAttribArray(AttribLocs::age); 
      glVertexAttribPointer(AttribLocs::age, 1, GL_FLOAT, false, stride, BUFFER_OFFSET(age_offset));


      //Tell the TFO where each varying variable should be written in the VBO.
      glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfo[i]);

      //Specify VBO to write into
      glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, pos_varying, vbo[i]);
      glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, vel_varying, vbo[i]);
      glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, age_varying, vbo[i]);

      glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
      glBindVertexArray(0); //unbind vao
      glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind vbo
   }

   ReloadShader();
   Camera::UpdateP();
   Uniforms::Init();
}