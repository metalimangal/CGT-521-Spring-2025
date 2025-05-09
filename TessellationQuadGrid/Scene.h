#pragma once

#include <glm/glm.hpp>

namespace Scene
{
   void DrawGui(GLFWwindow* window);
   void Display(GLFWwindow* window);
   void Idle();
   void Init();
   void ReloadShader();

   extern const int InitWindowWidth;
   extern const int InitWindowHeight;
   extern const int msaa_samples;
   extern GLuint shader_program;

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
