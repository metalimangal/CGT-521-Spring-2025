#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Uniforms
{
   void Init();
   void BufferSceneData();
   void BufferLightData();

   //This structure mirrors the uniform block declared in the shader
   struct SceneUniforms
   {
      glm::mat4 PV;	//camera projection * view matrix
      glm::vec4 eye_w = glm::vec4(0.0f, 0.0f, 3.0f, 1.0f);	//world-space eye position
   };

   const int MAX_LIGHTS = 1000;

   struct LightUniforms
   {

       glm::vec4 ambientColor = glm::vec4(1.0f);
       glm::vec4 diffuseColors[MAX_LIGHTS];
       glm::vec4 positions[MAX_LIGHTS];
       
       glm::vec4 specularColors[MAX_LIGHTS];
       float radii[MAX_LIGHTS];

   };


   struct MaterialUniforms
   {
      glm::vec4 ka = glm::vec4(1.0f);	//ambient material color
      glm::vec4 kd = glm::vec4(1.0f);	//diffuse material color
      glm::vec4 ks = glm::vec4(1.0f);	//specular material color
      float shininess = 20.0f;         //specular exponent
   };

   extern SceneUniforms SceneData;
   extern LightUniforms LightData;
   extern MaterialUniforms MaterialData;

   //IDs for the buffer objects holding the uniform block data
   extern GLuint scene_ubo;
   extern GLuint light_ubo;
   extern GLuint material_ubo;

   namespace UboBinding
   {
      //These values come from the binding value specified in the shader block layout
      extern int scene;
      extern int light;
      extern int material;
   };

   //Locations for the uniforms which are not in uniform blocks
   namespace UniformLocs
   {
      extern int M; //model matrix
      extern int time;
      extern int invertedNormals;
      extern int numLights;
   };
};
