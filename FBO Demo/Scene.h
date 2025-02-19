#pragma once

#include <glm/glm.hpp>

namespace Scene
{
   void DrawGui(GLFWwindow* window);
   void Display(GLFWwindow* window);
   void Idle();
   void Init();
   void ReloadShader();
   void framebuffer_size_callback(GLFWwindow* window, int width, int height);
   void mouse_callback(GLFWwindow* window, int button, int action, int mods);

   extern const int InitWindowWidth;
   extern const int InitWindowHeight;
   extern int WindowWidth;
   extern int WindowHeight;

   namespace Camera
   {
      extern glm::mat4 P;
      extern glm::mat4 V;

      extern float Aspect;
      extern float NearZ;
      extern float FarZ;
      extern float Fov;

      void UpdateP();
   }
};
