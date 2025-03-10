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

#include "Cube.h"

const int Scene::InitWindowWidth = 1024;
const int Scene::InitWindowHeight = 1024;

const std::string vertex_shader("conditional_vs.glsl");
const std::string fragment_shader("conditional_fs.glsl");
GLuint shader_program = -1;

static const std::string mesh_name = "Spaceship.obj";
//By xeni @ cg trader
//https://www.cgtrader.com/free-3d-models/space/spaceship/war-spaceship
MeshData mesh_data;

static const std::string texture_name = "AmagoT.bmp";
GLuint texture_id = -1; //Texture map for mesh

static const std::string occluder_name = "skyscraper.obj";
//By zullr32 @ cgtrader
//https://www.cgtrader.com/free-3d-print-models/miniatures/architecture/empire-trust-building
MeshData occluder_data;
CubeData cube_data; //for bounding box

//A total of row*row occluders and meshes will be drawn. row*row occlusion queries will also be created
const int rows = 10;

GLuint OcclusionQueries[rows*rows];
GLuint TimerQuery;

glm::mat4 M_box;
glm::mat4 M_mesh;

void draw_occluders();
void draw_bounding_volumes();
void conditional_draw_meshes();
void draw_meshes();

enum DRAW_MODE
{
   DRAW_SCENE, 
   DRAW_DEBUG, 
   CONDITIONAL_RENDERING
};

int DrawMode = DRAW_SCENE;
int ConditionalRenderMode = GL_QUERY_WAIT;

//GUI variables
float angle = 0.0f;
bool recording = false;
float time_sec = 0.0f;
bool pause = false;

namespace Scene
{
   const int msaa_samples = 4;
   const bool enable_msaa = true;

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
         Uniforms::SceneData.P = P;
      }
      void UpdateV()
      {
         Uniforms::SceneData.V = Camera::V;
         Uniforms::SceneData.PV = Camera::P * Camera::V;
         Uniforms::BufferSceneData();
      }
   }
}


glm::vec3 occluder_pos(int i, int j)
{
   glm::vec3 p;
   p.x = 2.0f*(i-rows/2) + 1.0f;
   p.y = 0.0f;
   p.z = glm::mod(2.0f*(j-rows/2) - time_sec, float(2.0*rows));
   return p;
}

glm::vec3 mesh_pos(int i, int j)
{
   float dir = 2.0*float(j%2)-1.0f;
   glm::vec3 p;
   p.x = glm::mod(2.0f*(i + rows/2) + dir*time_sec, float(2.0f*rows)) - float(rows) + 0.5f*abs(sin(float(i)+float(j)));
   p.y = +1.2f+1.0f*sin(float(i)+float(j));
   p.z = glm::mod(2.0f*(j-rows/2) + 1.0f - time_sec, float(2.0f*rows));
   return p;
}

//draw a grid of occluding objects in pass 0
void draw_occluders()
{
   glBindBufferBase(GL_UNIFORM_BUFFER, Uniforms::UboBinding::material, Uniforms::material_ubo[0]);
   glUniform1i(Uniforms::UniformLocs::pass, 0);
   glBindVertexArray(occluder_data.mVao);
   glm::mat4 S_occluder = glm::scale(3.0f*glm::vec3(occluder_data.mScaleFactor));

   const glm::vec3 axis = glm::vec3(0.0f, 1.0f, 0.0f);
   glm::mat4 R[4] = {glm::mat4(1.0f), glm::rotate(0.5f*glm::pi<float>(), axis),
                     glm::rotate(glm::pi<float>(), axis), glm::rotate(1.5f*glm::pi<float>(), axis)};
   for(int i=0; i<rows; i++)
   {
      for(int j=0; j<rows; j++)
      {
         glm::mat4 M = glm::translate(occluder_pos(i,j))*S_occluder*R[(i+j)%4];
         glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M));
         occluder_data.DrawMesh();
      }
   }
}

//draw grid of mesh bounding volumes in pass 1
void draw_bounding_volumes()
{
   glUniform1i(Uniforms::UniformLocs::pass, 1);
   glBindVertexArray(cube_data.mVao);
   glBindTexture(GL_TEXTURE_2D, 0);

   const glm::mat4 R[2] = {glm::mat4(1.0f), glm::rotate(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f))};

   for(int i=0; i<rows; i++)
   {
      for(int j=0; j<rows; j++)
      {
         glm::mat4 M = glm::translate(mesh_pos(i,j))*R[j%2]*M_box;
         glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M));
         glBeginQuery(GL_ANY_SAMPLES_PASSED, OcclusionQueries[i+rows*j]);
         cube_data.DrawMesh();
         glEndQuery(GL_ANY_SAMPLES_PASSED);
      }
   }
}

//draw grid of meshes in pass 2 WITHOUT conditional rendering
void draw_meshes()
{
   glBindBufferBase(GL_UNIFORM_BUFFER, Uniforms::UboBinding::material, Uniforms::material_ubo[1]);
   glUniform1i(Uniforms::UniformLocs::pass, 2);
   glBindVertexArray(mesh_data.mVao);
   glBindTexture(GL_TEXTURE_2D, texture_id);

   const glm::mat4 R[2] = {glm::mat4(1.0f), glm::rotate(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f))};

   for(int i=0; i<rows; i++)
   {
      for(int j=0; j<rows; j++)
      {
         glm::mat4 M = glm::translate(mesh_pos(i,j))*R[j%2]*M_mesh;
         glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M));

         mesh_data.DrawMesh();
      }
   }
}

//draw grid of meshes in pass 2 WITH conditional rendering
//This is the same function as draw_meshes, but with glBeginConditionalRender/glEndConditionalRender
void conditional_draw_meshes()
{
   glBindBufferBase(GL_UNIFORM_BUFFER, Uniforms::UboBinding::material, Uniforms::material_ubo[1]);
   glUniform1i(Uniforms::UniformLocs::pass, 2);
   glBindVertexArray(mesh_data.mVao);
   glBindTexture(GL_TEXTURE_2D, texture_id);

   glm::mat4 R[2] = {glm::mat4(1.0f), glm::rotate(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f))};

   for(int i=0; i<rows; i++)
   {
      for(int j=0; j<rows; j++)
      {
         glm::mat4 M = glm::translate(mesh_pos(i,j))*R[j%2]*M_mesh;
         glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(M));

         glBeginConditionalRender(OcclusionQueries[i+rows*j], ConditionalRenderMode);
	         mesh_data.DrawMesh();
         glEndConditionalRender();
      }
   }
}

void draw_debug_scene()
{
  // draw_occluders();

   //draw bounding volumes wireframe
   glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
   draw_bounding_volumes();

   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   draw_meshes();
}

void draw_scene()
{
   draw_occluders();
   draw_meshes();
}

void draw_conditional_scene()
{
   draw_occluders();

   //draw bounding volumes, but don't update framebuffer
   glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
   glDepthMask(GL_FALSE);
   draw_bounding_volumes();

   //conditionally render meshes to framebuffer.
   //Rendering will only actually happen if the bounding box generated fragments
   glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
   glDepthMask(GL_TRUE);
   conditional_draw_meshes();
}


// This function gets called every time the scene gets redisplayed
void Scene::Display(GLFWwindow* window)
{
   //Clear the screen to the color previously specified in the glClearColor(...) call.
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   Uniforms::SceneData.eye_w = glm::vec4(0.0f, 2.0f, 0.5f, 1.0f);
   Camera::V = glm::lookAt(glm::vec3(Uniforms::SceneData.eye_w), glm::vec3(0.0f, 0.0f, 12.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f));
   Camera::UpdateV();

   glUseProgram(shader_program);
   //Note that we don't need to set the value of a uniform here. The value is set with the "binding" in the layout qualifier
   glBindTextureUnit(0, texture_id);

   if(DrawMode == DRAW_SCENE)
   {
      draw_scene();
   }
   else if(DrawMode == DRAW_DEBUG)
   {   
      draw_debug_scene();
   }
   else if(DrawMode == CONDITIONAL_RENDERING)
   {
      draw_conditional_scene();
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

   ImGui::Checkbox("Pause", &pause);
   ImGui::SliderFloat("View angle", &angle, -glm::pi<float>(), +glm::pi<float>());

   ImGui::RadioButton("Draw Scene", &DrawMode, DRAW_SCENE); ImGui::SameLine();
   ImGui::RadioButton("Draw Debug", &DrawMode, DRAW_DEBUG); ImGui::SameLine();
   ImGui::RadioButton("Conditional Render", &DrawMode, CONDITIONAL_RENDERING); 

   if(DrawMode==CONDITIONAL_RENDERING)
   {
      ImGui::RadioButton("GL_QUERY_WAIT", &ConditionalRenderMode, GL_QUERY_WAIT); ImGui::SameLine();
      ImGui::RadioButton("GL_QUERY_NO_WAIT", &ConditionalRenderMode, GL_QUERY_NO_WAIT); 
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
   time_sec = static_cast<float>(glfwGetTime());

   //Pass time_sec value to the shaders
   glUniform1f(Uniforms::UniformLocs::time, time_sec);

   glEndQuery(GL_TIME_ELAPSED); 

   GLint available=0;
   while(!available)
   {
		glGetQueryObjectiv(TimerQuery, GL_QUERY_RESULT_AVAILABLE, &available);
   }

   GLuint64 elapsed;
   glGetQueryObjectui64v(TimerQuery, GL_QUERY_RESULT, &elapsed);
   float delta_seconds = 1e-9f*elapsed;
   glBeginQuery(GL_TIME_ELAPSED, TimerQuery);

   printf("delta_seconds = %f\n", delta_seconds);  
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
      glClearColor(Uniforms::SceneData.clear_color.r, Uniforms::SceneData.clear_color.g, Uniforms::SceneData.clear_color.b, Uniforms::SceneData.clear_color.a);

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

   if(enable_msaa)
   {
      glEnable(GL_MULTISAMPLE);
   }
   else
   {
      glDisable(GL_MULTISAMPLE);
   }

   mesh_data = LoadMesh(mesh_name);
   texture_id = LoadTexture(texture_name);
   occluder_data = LoadMesh(occluder_name);
   cube_data.CreateCube();

   aiVector3D box_width = mesh_data.mBbMax-mesh_data.mBbMin;
   aiVector3D box_center = 0.5f*(mesh_data.mBbMax+mesh_data.mBbMin);

   glm::vec3 cen(box_center.x, box_center.y, box_center.z);
   glm::vec3 w(box_width.x, box_width.y, box_width.z);
   glm::mat4 R0 = glm::rotate(glm::pi<float>()/2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
   
   M_box = R0*glm::scale(cube_data.mScaleFactor*mesh_data.mScaleFactor*glm::vec3(box_width.x, box_width.y, box_width.z));
   M_mesh = R0*glm::scale(glm::vec3(mesh_data.mScaleFactor))*glm::translate(-cen);

   glGenQueries(1, &TimerQuery);
   glBeginQuery(GL_TIME_ELAPSED, TimerQuery);

   glGenQueries(rows*rows, OcclusionQueries);

   Camera::UpdateP();
   Uniforms::Init();
}