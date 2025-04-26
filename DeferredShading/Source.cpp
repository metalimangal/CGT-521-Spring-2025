//When using this as a template, be sure to make these changes in the new project: 
//1. In Debugging properties set the Environment to PATH=%PATH%;$(SolutionDir)\lib;
//2. Change window_title below
//3. Copy assets (mesh and texture) to new project directory

/*

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
#include<map>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif


const int Scene::InitWindowWidth = 1024;
const int Scene::InitWindowHeight = 1024;

static const std::string vertex_shader("multi_vs.glsl");
static const std::string fragment_shader("multi_fs.glsl");


static const std::string vertex_shader_geometry("template_vs.glsl");
static const std::string fragment_shader_geometry("template_fs.glsl");

static const std::string vertex_shader_deferred("template_vs.glsl");
static const std::string fragment_shader_deferred("template_fs.glsl");

GLuint shader_program = -1;

static const std::string mesh_name = "Amago0.obj";
//static const std::string mesh_name = "cat.obj";
static const std::string texture_name = "AmagoT.bmp";
//static const std::string texture_name = "cat.jpg";

std::vector<std::string> modelFiles;
std::vector<std::string> textureFiles;
std::map<std::string, MeshData> loadedMeshes;
std::map<std::string, GLuint> loadedTextures;

GLuint texture_id = -1; //Texture map for mesh
MeshData mesh_data;
MeshData mesh_data2;

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

GLuint gpuTimerQuery = 0;
bool timerInitialized = false;
std::deque<float> frameTimes;
const int maxFrameSamples = 100;



unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;

int numLights = 100;

std::vector<std::string> GetFilesInDirectory(const std::string& path, const std::vector<std::string>& extensions) {
    std::vector<std::string> files;

#ifdef _WIN32
    std::string searchPath = path + "/*.*";
    WIN32_FIND_DATA fd;
    HANDLE hFind = ::FindFirstFile(searchPath.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string filename = fd.cFileName;
                for (const auto& ext : extensions) {
                    if (filename.size() >= ext.size() &&
                        filename.substr(filename.size() - ext.size()) == ext) {
                        files.push_back(filename);
                        break;
                    }
                }
            }
        } while (::FindNextFile(hFind, &fd));
        ::FindClose(hFind);
    }
#else
    DIR* dir;
    struct dirent* ent;
    if ((dir = opendir(path.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string filename = ent->d_name;
            for (const auto& ext : extensions) {
                if (filename.size() >= ext.size() &&
                    filename.substr(filename.size() - ext.size()) == ext) {
                    files.push_back(filename);
                    break;
                }
            }
        }
        closedir(dir);
    }
#endif

    std::sort(files.begin(), files.end());
    return files;
}




class SceneObject {
public:
    glm::vec3 position, rotation, scale;
    MeshData mesh;
    GLuint texture;
    std::string meshName;
    std::string textureName;

    SceneObject(const glm::vec3& pos = {}, const glm::vec3& rot = {}, const glm::vec3& scl = glm::vec3(1.0f),
        const std::string& meshFile = "", const std::string& texFile = "")
        : position(pos), rotation(rot), scale(scl), meshName(meshFile), textureName(texFile) {}

    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        return glm::scale(model, scale);
    }
};


std::vector<SceneObject> sceneObjects;


void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
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



// This function gets called every time the scene gets redisplayed
void Scene::Display(GLFWwindow* window)
{
    //Clear the screen to the color previously specified in the glClearColor(...) call.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBeginQuery(GL_TIME_ELAPSED, gpuTimerQuery);

    Camera::V = glm::lookAt(glm::vec3(Uniforms::SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    Uniforms::SceneData.PV = Camera::P * Camera::V;
    Uniforms::BufferSceneData();

    glUseProgram(shader_program);


    //Note that we don't need to set the value of a uniform here. The value is set with the "binding" in the layout qualifier
    glBindTextureUnit(0, texture_id);


    glm::mat4 model = glm::mat4(1.0f);

    model = glm::translate(model, glm::vec3(0.0, 7.0f, 0.0f));
    model = glm::scale(model, glm::vec3(7.5f, 7.5f, 7.5f));
    glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(model));

    glUniform1i(Uniforms::UniformLocs::invertedNormals, 1);
    renderCube();
    glUniform1i(Uniforms::UniformLocs::invertedNormals, 0);



    for (auto& obj : sceneObjects) {
        glm::mat4 model = obj.getModelMatrix();
        model = model * glm::scale(glm::mat4(1.0f), glm::vec3(obj.mesh.mScaleFactor));
        glUniformMatrix4fv(Uniforms::UniformLocs::M, 1, false, glm::value_ptr(model));

        glBindTextureUnit(0, obj.texture);
        obj.mesh.DrawMesh();  // Uses the mesh's own VAO and submesh info


    }


    //For meshes with multiple submeshes use mesh_data.DrawMesh(); 


    // Populate 100 lights (example)

    //for (int i = 0; i < Uniforms::LightData.numLights; ++i)
    //{
    //    float angle = 2.0f * glm::pi<float>() * (float)i / Uniforms::LightData.numLights;
    //    float x = 3.0f * cos(angle);
    //    float z = 3.0f * sin(angle);

    //    Uniforms::LightData.positions[i] = glm::vec4(x, 2.0f, z, 1.0f);
    //    Uniforms::LightData.diffuseColors[i] = glm::vec4(glm::vec3(0.5f + 0.5f * sin(i)), 1.0f);
    //    Uniforms::LightData.specularColors[i] = glm::vec4(1.0f);
    //    Uniforms::LightData.radii[i] = 5.0f;
    //

    //Uniforms::BufferLightData();

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

    // Swap front and back buffers 
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
    //ImGui::Text("Add New Object");
    //if (ImGui::Button("Add Object"))
    //{
    //    sceneObjects.push_back(SceneObject());
    //}


    ImGui::Separator();
    ImGui::Text("Add Object From File");

    static std::vector<std::string> modelFiles = GetFilesInDirectory("models/", { ".obj" });
    static std::vector<std::string> textureFiles = GetFilesInDirectory("textures/", {
        ".bmp", ".png", ".jpg", ".jpeg", ".tga"
        });


    static int selectedModel = 0;
    static int selectedTexture = 0;

    if (ImGui::BeginCombo("Model File", modelFiles[selectedModel].c_str())) {
        for (int i = 0; i < modelFiles.size(); i++) {
            bool is_selected = (selectedModel == i);
            if (ImGui::Selectable(modelFiles[i].c_str(), is_selected)) {
                selectedModel = i;
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::BeginCombo("Texture File", textureFiles[selectedTexture].c_str())) {
        for (int i = 0; i < textureFiles.size(); i++) {
            bool is_selected = (selectedTexture == i);
            if (ImGui::Selectable(textureFiles[i].c_str(), is_selected)) {
                selectedTexture = i;
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Add Selected Object")) {

        std::string model = modelFiles[selectedModel];
        std::string tex = textureFiles[selectedTexture];
        sceneObjects.emplace_back(glm::vec3(0), glm::vec3(0), glm::vec3(1), model, tex);
        sceneObjects.back().mesh = loadedMeshes[model];
        sceneObjects.back().texture = loadedTextures[tex];

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
    modelFiles = GetFilesInDirectory("models/", { ".obj" });
    textureFiles = GetFilesInDirectory("textures/", { ".bmp", ".png", ".jpg", ".jpeg", ".tga" });

    for (const auto& model : modelFiles) {
        loadedMeshes[model] = LoadMesh("models/" + model);
    }
    for (const auto& tex : textureFiles) {
        loadedTextures[tex] = LoadTexture("textures/" + tex);
    }


    Camera::UpdateP();
    Uniforms::Init();

    if (!timerInitialized) {
        glGenQueries(1, &gpuTimerQuery);
        timerInitialized = true;
    }
}

*/