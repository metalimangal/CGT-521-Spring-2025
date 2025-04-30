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

#include <deque>


const int Scene::InitWindowWidth = 1024;
const int Scene::InitWindowHeight = 1024;
int gBufferWidth = Scene::InitWindowWidth;
int gBufferHeight = Scene::InitWindowHeight;


static const std::string vertex_shader("multi_vs.glsl");
static const std::string fragment_shader("multi_fs.glsl");


static const std::string vertex_shader_geometry("geometry_vs.glsl");
static const std::string fragment_shader_geometry("geometry_fs.glsl");

static const std::string vertex_shader_deferred("deferred_vs.glsl");
static const std::string fragment_shader_deferred("deferred_fs.glsl");

GLuint shader_program = -1;
GLuint shader_program_geometry = -1;
GLuint shader_program_deferred = -1;

static const std::string mesh_names[] = { "models/Amago0.obj", "models/cat.obj", "models/sm_gamecontroller.fbx", "models/sm_gamecontroller.fbx" };
static const std::string texture_names[] = { "textures/AmagoT.bmp", "textures/cat.jpeg", "textures/T_GameController_Black_BaseColor.png", "textures/T_GameController_Gold_BaseColor.png"};


GLuint texture_ids[std::size(texture_names)] = {-1}; // Initialize with default GLuint values (0)
MeshData mesh_data_array[std::size(mesh_names)];  // Array for mesh data

GLuint gBuffer;
GLuint gPosition, gNormal, gAlbedoSpec;
GLuint rboDepth;


static const std::string cubeTextureName = "textures/"; //I'll try adding the cube texture here for the rendering cube

GLuint cubeTextureID = -1;
glm::vec3 cubeColor = glm::vec3(0.3f, 0.7f, 1.0f); // To change the render cube color
glm::vec3 cubePosition = glm::vec3(0.936f, 5.596f, 1.512f);
glm::vec3 cubeScale = glm::vec3(7.5f);


float angle = 0.0f;
float scale = 1.0f;
bool recording = false;


enum class RenderMode {
    Forward,
    Deferred
};

RenderMode currentRenderMode = RenderMode::Forward;



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

GLuint gpuTimerQuery = 0;
bool timerInitialized = false;
std::deque<float> frameTimes;
const int maxFrameSamples = 100;



unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;

int numLights = 100;


class SceneObject {
public:
    glm::vec3 position;
    glm::vec3 rotation; // Rotation in degrees for x, y, z axes
    glm::vec3 scale;
    int meshIndex;

    SceneObject(const glm::vec3& pos = glm::vec3(0.0f),
        const glm::vec3& rot = glm::vec3(0.0f),
        const glm::vec3& scl = glm::vec3(1.0f), const int index = 0)
        : position(pos), rotation(rot), scale(scl), meshIndex(index){}

    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, scale);
        return model;
    }
};

std::vector<SceneObject> sceneObjects;

std::vector<SceneObject> initialObjects = {
    SceneObject(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f), 2),
    SceneObject(glm::vec3(1.5f, 0.0f, -2.0f), glm::vec3(0.0f, 45.0f, 0.0f), glm::vec3(0.5f), 1),
    SceneObject(glm::vec3(-0.5f, 0.0f, 1.5f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.75f), 0)
};

glm::vec3 newObjectPosition = glm::vec3(0.0f, 0.0f, 0.0f);

void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // positions          // normals           // texcoords
            // Back face
            -1.0f, -1.0f, -1.0f,   0.0f,  0.0f, -1.0f,   0.0f, 0.0f,
             1.0f,  1.0f, -1.0f,   0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
             1.0f, -1.0f, -1.0f,   0.0f,  0.0f, -1.0f,   1.0f, 0.0f,

             1.0f,  1.0f, -1.0f,   0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f,   0.0f,  0.0f, -1.0f,   0.0f, 0.0f,
            -1.0f,  1.0f, -1.0f,   0.0f,  0.0f, -1.0f,   0.0f, 1.0f,

            // Front face
            -1.0f, -1.0f,  1.0f,   0.0f,  0.0f, 1.0f,    0.0f, 0.0f,
             1.0f, -1.0f,  1.0f,   0.0f,  0.0f, 1.0f,    1.0f, 0.0f,
             1.0f,  1.0f,  1.0f,   0.0f,  0.0f, 1.0f,    1.0f, 1.0f,

             1.0f,  1.0f,  1.0f,   0.0f,  0.0f, 1.0f,    1.0f, 1.0f,
            -1.0f,  1.0f,  1.0f,   0.0f,  0.0f, 1.0f,    0.0f, 1.0f,
            -1.0f, -1.0f,  1.0f,   0.0f,  0.0f, 1.0f,    0.0f, 0.0f,

            // Left face
            -1.0f,  1.0f,  1.0f,  -1.0f, 0.0f, 0.0f,     1.0f, 0.0f,
            -1.0f,  1.0f, -1.0f,  -1.0f, 0.0f, 0.0f,     1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f,  -1.0f, 0.0f, 0.0f,     0.0f, 1.0f,

            -1.0f, -1.0f, -1.0f,  -1.0f, 0.0f, 0.0f,     0.0f, 1.0f,
            -1.0f, -1.0f,  1.0f,  -1.0f, 0.0f, 0.0f,     0.0f, 0.0f,
            -1.0f,  1.0f,  1.0f,  -1.0f, 0.0f, 0.0f,     1.0f, 0.0f,

            // Right face
             1.0f,  1.0f,  1.0f,   1.0f, 0.0f, 0.0f,     1.0f, 0.0f,
             1.0f, -1.0f, -1.0f,   1.0f, 0.0f, 0.0f,     0.0f, 1.0f,
             1.0f,  1.0f, -1.0f,   1.0f, 0.0f, 0.0f,     1.0f, 1.0f,

             1.0f, -1.0f, -1.0f,   1.0f, 0.0f, 0.0f,     0.0f, 1.0f,
             1.0f,  1.0f,  1.0f,   1.0f, 0.0f, 0.0f,     1.0f, 0.0f,
             1.0f, -1.0f,  1.0f,   1.0f, 0.0f, 0.0f,     0.0f, 0.0f,

             // Bottom face
             -1.0f, -1.0f, -1.0f,   0.0f, -1.0f, 0.0f,    0.0f, 1.0f,
              1.0f, -1.0f, -1.0f,   0.0f, -1.0f, 0.0f,    1.0f, 1.0f,
              1.0f, -1.0f,  1.0f,   0.0f, -1.0f, 0.0f,    1.0f, 0.0f,

              1.0f, -1.0f,  1.0f,   0.0f, -1.0f, 0.0f,    1.0f, 0.0f,
             -1.0f, -1.0f,  1.0f,   0.0f, -1.0f, 0.0f,    0.0f, 0.0f,
             -1.0f, -1.0f, -1.0f,   0.0f, -1.0f, 0.0f,    0.0f, 1.0f,

             // Top face
             -1.0f,  1.0f, -1.0f,   0.0f, 1.0f, 0.0f,     0.0f, 1.0f,
              1.0f,  1.0f,  1.0f,   0.0f, 1.0f, 0.0f,     1.0f, 0.0f,
              1.0f,  1.0f, -1.0f,   0.0f, 1.0f, 0.0f,     1.0f, 1.0f,

              1.0f,  1.0f,  1.0f,   0.0f, 1.0f, 0.0f,     1.0f, 0.0f,
             -1.0f,  1.0f, -1.0f,   0.0f, 1.0f, 0.0f,     0.0f, 1.0f,
             -1.0f,  1.0f,  1.0f,   0.0f, 1.0f, 0.0f,     0.0f, 0.0f
        };

        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}



void renderSceneGeometry()
{
    glm::mat4 model = glm::mat4(1.0f);

    // Render Cube
    model = glm::translate(model, cubePosition);
    model = glm::scale(model, cubeScale);
    glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(model));
    glUniform1i(Uniforms::UniformLocs::invertedNormals, 1);
    glBindTextureUnit(0, cubeTextureID);
    renderCube();
    glUniform1i(Uniforms::UniformLocs::invertedNormals, 0);

    // Render Scene Objects
    for (const auto& obj : sceneObjects)
    {
        glm::mat4 model = obj.getModelMatrix();
        int meshIndex = obj.meshIndex;

        if (meshIndex >= 0 && meshIndex < std::size(mesh_names)) // Ensure valid index
        {
            glBindTextureUnit(0, texture_ids[meshIndex]);
            model = model * glm::scale(glm::mat4(1.0f), glm::vec3(mesh_data_array[meshIndex].mScaleFactor));

            glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(model));
            glBindVertexArray(mesh_data_array[meshIndex].mVao);
            mesh_data_array[meshIndex].DrawMesh();
        }
    }
}


unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

        // position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        // texcoord attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void ResizeGBuffer(int width, int height) {
    gBufferWidth = width;
    gBufferHeight = height;

    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}



// This function gets called every time the scene gets redisplayed
void Scene::Display(GLFWwindow* window)
{
   //Clear the screen to the color previously specified in the glClearColor(...) call.
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glBeginQuery(GL_TIME_ELAPSED, gpuTimerQuery);


   int currentWidth, currentHeight;
   glfwGetFramebufferSize(window, &currentWidth, &currentHeight);

   if (currentWidth != gBufferWidth || currentHeight != gBufferHeight)
   {
       ResizeGBuffer(currentWidth, currentHeight);
       Scene::Camera::Aspect = static_cast<float>(currentWidth) / static_cast<float>(currentHeight);
       Scene::Camera::UpdateP();
   }


   Camera::V = glm::lookAt(glm::vec3(Uniforms::SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
   Uniforms::SceneData.PV = Camera::P * Camera::V;
   Uniforms::BufferSceneData();

   if (currentRenderMode == RenderMode::Deferred)
   {
       // ---- GEOMETRY PASS ----
       glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
       glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

       glUseProgram(shader_program_geometry);

       renderSceneGeometry(); // ⬅️ New function

       glBindFramebuffer(GL_FRAMEBUFFER, 0);

       // ---- LIGHTING PASS ----
       glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
       glUseProgram(shader_program_deferred);

       glActiveTexture(GL_TEXTURE0);
       glBindTexture(GL_TEXTURE_2D, gPosition);
       glActiveTexture(GL_TEXTURE1);
       glBindTexture(GL_TEXTURE_2D, gNormal);
       glActiveTexture(GL_TEXTURE2);
       glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

       renderQuad(); // ⬅️ Fullscreen pass
   }
   else
   {
       // ---- FORWARD RENDERING ----
       glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

       glUseProgram(shader_program);

       renderSceneGeometry(); // ⬅️ Reuse the same geometry drawing function
   }


   glEndQuery(GL_TIME_ELAPSED);

   GLuint64 elapsedTime = 0;
   glGetQueryObjectui64v(gpuTimerQuery, GL_QUERY_RESULT, &elapsedTime);
   float ms = static_cast<float>(elapsedTime) / 1e6f; // Convert to ms

   frameTimes.push_back(ms);
   if (frameTimes.size() > maxFrameSamples)
       frameTimes.pop_front();


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
   if (ImGui::Button("Show ImGui Demo Window"))
   {
      show_imgui_demo = true;
   }

   ImGui::Separator();
   ImGui::Text("Change Render Cube Properties");

   ImGui::SliderFloat3("Cube Position", glm::value_ptr(cubePosition), -10.0, 10.0);
   ImGui::SliderFloat3("Cube Scale", glm::value_ptr(cubeScale), -10.0, 10.0);


   ImGui::Text("Rendering Mode");
   int mode = static_cast<int>(currentRenderMode);
   ImGui::RadioButton("Forward", &mode, 0);
   ImGui::RadioButton("Deferred", &mode, 1);
   currentRenderMode = static_cast<RenderMode>(mode);


   ImGui::Separator();

   ImGui::Separator();
   ImGui::Text("Add New Object");
   static int selectedMeshIndex = 0; // 0: mesh_data, 1: mesh_data2
   // Dynamically generate the combo box items based on the size of the mesh_names array
   std::string comboItems;
   for (size_t i = 0; i < std::size(mesh_names); ++i) {
      comboItems += "Mesh " + std::to_string(i) + '\0';
   }
   ImGui::Combo("Mesh", &selectedMeshIndex, comboItems.c_str());

   if (ImGui::Button("Add Object"))
   {
       sceneObjects.push_back(SceneObject(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), selectedMeshIndex));
   }


   // Display and edit existing object positions
   ImGui::Separator();
   ImGui::Text("Edit Object Transformations");

   for (size_t i = 0; i < sceneObjects.size(); ++i)
   {
       ImGui::PushID(static_cast<int>(i)); // Ensure unique ID for each slider group
       ImGui::SliderFloat3(("Position " + std::to_string(i)).c_str(), glm::value_ptr(sceneObjects[i].position), -10.0f, 10.0f);
       ImGui::SliderFloat3(("Rotation " + std::to_string(i)).c_str(), glm::value_ptr(sceneObjects[i].rotation), -180.0f, 180.0f);
       ImGui::SliderFloat3(("Scale " + std::to_string(i)).c_str(), glm::value_ptr(sceneObjects[i].scale), 0.1f, 10.0f);

       if (ImGui::Button("Remove"))
       {
           sceneObjects.erase(sceneObjects.begin() + i);
       }
       ImGui::PopID();
   }

   ImGui::Separator();

   ImGui::Image((ImTextureID)gPosition, ImVec2(256, 256));
   ImGui::Image((ImTextureID)gNormal, ImVec2(256, 256));
   ImGui::Image((ImTextureID)gAlbedoSpec, ImVec2(256, 256));


   ImGui::Separator();



   ImGui::Begin("Material Settings");

   // Ambient color (ka)
   ImGui::ColorEdit4("Ambient (ka)", glm::value_ptr(Uniforms::MaterialData.ka));

   // Diffuse color (kd)
   ImGui::ColorEdit4("Diffuse (kd)", glm::value_ptr(Uniforms::MaterialData.kd));

   // Specular color (ks)
   ImGui::ColorEdit4("Specular (ks)", glm::value_ptr(Uniforms::MaterialData.ks));

   // Shininess slider
   ImGui::SliderFloat("Shininess", &Uniforms::MaterialData.shininess, 1.0f, 128.0f);


   ImGui::End();

   ImGui::Begin("Lights");
      ImGui::SliderInt("Num Lights", &numLights, 1, Uniforms::MAX_LIGHTS);
       glUniform1i(Uniforms::UniformLocs::numLights, numLights);


       ImGui::ColorEdit4("Ambient Light Color", glm::value_ptr(Uniforms::LightData.ambientColor));

           static glm::vec3 basePosition = glm::vec3(0.0f, 2.0f, 0.0f);
           static float spacing = 1.5f;
           static float globalRadius = 5.0f;
           static glm::vec4 globalDiffuse = glm::vec4(1.0f, 0.6f, 0.3f, 1.0f);
           static glm::vec4 globalSpecular = glm::vec4(1.0f);
           static glm::vec4 globalColor = glm::vec4(1.0f);




           bool updateAll = false;

           updateAll |= ImGui::DragFloat3("Base Position", &basePosition.x, 0.1f);
           updateAll |= ImGui::DragFloat("Spacing", &spacing, 0.1f, 0.1f, 10.0f);
           updateAll |= ImGui::DragFloat("Radius", &globalRadius, 0.1f, 0.1f, 100.0f);
           updateAll |= ImGui::ColorEdit4("Diffuse Color", &globalDiffuse.x);
           updateAll |= ImGui::ColorEdit4("Specular Color", &globalSpecular.x);

           if (updateAll) {
               for (int i = 0; i < numLights; ++i) {
                   float angle = 2.0f * glm::pi<float>() * i / numLights;
                   float x = basePosition.x + cos(angle) * spacing * i * 0.1f;
                   float z = basePosition.z + sin(angle) * spacing * i * 0.1f;
                   float y = basePosition.y;

                   Uniforms::LightData.positions[i] = glm::vec4(x, y, z, 1.0f);
                   Uniforms::LightData.radii[i] = globalRadius;
                   Uniforms::LightData.diffuseColors[i] = globalDiffuse;
                   Uniforms::LightData.specularColors[i] = globalSpecular;
               }
           }
      

       ImGui::End();



       //ImGui::Begin("Lights");

       //static int mode = 0; // 0: Circle, 1: Grid
       //ImGui::RadioButton("Circle Mode", &mode, 0); ImGui::SameLine();
       //ImGui::RadioButton("Grid Mode", &mode, 1);

       //// Global UI
       //ImGui::SliderInt("Num Lights", &numLights, 1, Uniforms::MAX_LIGHTS);
       //glUniform1i(Uniforms::UniformLocs::numLights, numLights);
       //ImGui::ColorEdit4("Ambient Light", glm::value_ptr(Uniforms::LightData.ambientColor));
       //static float radius = 5.0f;
       //ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.0f);

       //// Base Position and Spacing
       //static glm::vec3 basePosition = glm::vec3(0.0f, 2.0f, 0.0f);
       //static float spacing = 1.5f;
       //ImGui::DragFloat3("Base Position", &basePosition.x, 0.1f);
       //ImGui::DragFloat("Spacing", &spacing, 0.1f, 0.1f, 10.0f);

       //// --- MODE 0: CIRCLE ---
       //static glm::vec4 globalDiffuse = glm::vec4(1.0f, 0.6f, 0.3f, 1.0f);
       //static glm::vec4 globalSpecular = glm::vec4(1.0f);

       //// --- MODE 1: GRID ---
       //const int colorSetCount = 3;
       //const int lightsPerGroup = 10;

       //static glm::vec4 groupDiffuseColors[colorSetCount] = {
       //    glm::vec4(1.0f, 0.3f, 0.3f, 1.0f),
       //    glm::vec4(0.3f, 1.0f, 0.3f, 1.0f),
       //    glm::vec4(0.3f, 0.3f, 1.0f, 1.0f)
       //};

       //static glm::vec4 groupSpecularColors[colorSetCount] = {
       //    glm::vec4(1.0f),
       //    glm::vec4(0.8f, 0.8f, 0.2f, 1.0f),
       //    glm::vec4(0.5f, 1.0f, 1.0f, 1.0f)
       //};

       //bool updateLights = false;

       //if (mode == 0) {
       //    ImGui::ColorEdit4("Global Diffuse", &globalDiffuse.x);
       //    ImGui::ColorEdit4("Global Specular", &globalSpecular.x);
       //}
       //else {
       //    for (int i = 0; i < colorSetCount; ++i) {
       //        ImGui::ColorEdit4(("Diffuse Group " + std::to_string(i)).c_str(), &groupDiffuseColors[i].x);
       //        ImGui::ColorEdit4(("Specular Group " + std::to_string(i)).c_str(), &groupSpecularColors[i].x);
       //    }
       //}

       //updateLights = ImGui::Button("Update Lights");

       //if (updateLights) {


       //    for (int i = 0; i < numLights; ++i) {
       //        float y = basePosition.y;
       //        float x, z;

       //        if (mode == 0) {
       //            // Circle layout
       //            float angle = 2.0f * glm::pi<float>() * i / numLights;
       //            x = basePosition.x + cos(angle) * spacing * i * 0.1f;
       //            z = basePosition.z + sin(angle) * spacing * i * 0.1f;

       //            Uniforms::LightData.diffuseColors[i] = globalDiffuse;
       //            Uniforms::LightData.specularColors[i] = globalSpecular;
       //        }
       //        else {
       //            // Grid layout
       //            int row = i / 10;
       //            int col = i % 10;
       //            x = basePosition.x + col * spacing;
       //            z = basePosition.z + row * spacing;

       //            int groupIndex = (i / lightsPerGroup) % colorSetCount;
       //            Uniforms::LightData.diffuseColors[i] = groupDiffuseColors[groupIndex];
       //            Uniforms::LightData.specularColors[i] = groupSpecularColors[groupIndex];
       //        }

       //        Uniforms::LightData.positions[i] = glm::vec4(x, y, z, 1.0f);
       //        Uniforms::LightData.radii[i] = radius;
       //    }

       //}

       //ImGui::End();

   
   Uniforms::BufferLightData();


   glBindBuffer(GL_UNIFORM_BUFFER, Uniforms::material_ubo);
   glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Uniforms::MaterialData), &Uniforms::MaterialData);
   glBindBuffer(GL_UNIFORM_BUFFER, 0);

   ImGui::Separator();
   ImGui::Text("GPU Frame Time (ms)");

   static float frameTimeBuffer[100];
   std::copy(frameTimes.begin(), frameTimes.end(), frameTimeBuffer);
   int sampleCount = static_cast<int>(frameTimes.size());

   ImGui::PlotLines("GPU Time", frameTimeBuffer, sampleCount, 0, nullptr, 0.0f, 50.0f, ImVec2(0, 150));




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
     GLuint new_geometry_shader = InitShader(vertex_shader_geometry.c_str(), fragment_shader_geometry.c_str());  
     GLuint new_deferred_shader = InitShader(vertex_shader_deferred.c_str(), fragment_shader_deferred.c_str());  

     if (new_shader == -1 || new_geometry_shader == -1 || new_deferred_shader == -1) // loading failed  
     {  
        DebugBreak(); // alert user by breaking and showing debugger  
        glClearColor(1.0f, 0.0f, 1.0f, 0.0f); // change clear color if shader can't be compiled  
     }  
     else  
     {  
        glClearColor(0.35f, 0.35f, 0.35f, 0.0f);  

        if (shader_program != -1)  
        {  
           glDeleteProgram(shader_program);  
        }  
        shader_program = new_shader;  

        if (shader_program_geometry != -1)  
        {  
           glDeleteProgram(shader_program_geometry);  
        }  
        shader_program_geometry = new_geometry_shader;  

        if (shader_program_deferred != -1)  
        {  
           glDeleteProgram(shader_program_deferred);  
        }  
        shader_program_deferred = new_deferred_shader;  
     }  
}

void createCubeTexture() {
    // Create a simple 1x1 texture for cube
    glGenTextures(1, &cubeTextureID);
    glBindTexture(GL_TEXTURE_2D, cubeTextureID);

    // Create a single pixel color (e.g., blue)
    unsigned char pixelColor[3] = { 76, 179, 255 }; // RGB (0.3, 0.7, 1.0) scaled to 0-255

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixelColor);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);


}

void AddDeferredShadingParams() {
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    // - Position color buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Scene::InitWindowWidth, Scene::InitWindowHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    // - Normal color buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Scene::InitWindowWidth, Scene::InitWindowHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    // - Albedo + Specular color buffer
    glGenTextures(1, &gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Scene::InitWindowWidth, Scene::InitWindowHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

    // - Depth buffer
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Scene::InitWindowWidth, Scene::InitWindowHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    // - Tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    // - Finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
   // Replace the following lines:  
   // mesh_data = LoadMesh(mesh_name);  
   // mesh_data2 = LoadMesh(mesh_name2);  
   // texture_id = LoadTexture(texture_name);  
   // texture_id2 = LoadTexture(texture_name2);  

   // With the following implementation using arrays:  
   for (int i = 0; i < std::size(mesh_names); ++i) {  
      mesh_data_array[i] = LoadMesh(mesh_names[i]);  
      texture_ids[i] = LoadTexture(texture_names[i]);  
   }


   createCubeTexture();
   AddDeferredShadingParams();

   Camera::UpdateP();
   Uniforms::Init();

   if (!timerInitialized) {
       glGenQueries(1, &gpuTimerQuery);
       timerInitialized = true;
   }


   sceneObjects.insert(sceneObjects.end(), initialObjects.begin(), initialObjects.end());
}