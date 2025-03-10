#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Uniforms
{
   void Init();
   void BufferSceneData();

   //This structure mirrors the uniform block declared in the shader
   struct SceneUniforms
   {
      glm::mat4 P;
      glm::mat4 V;
      glm::mat4 PV;	//camera projection * view matrix
      glm::vec4 eye_w;	//world-space eye position
      glm::vec4 clear_color = (1.0f/255.0f)*glm::vec4(238.0f, 181.0f, 130.0f, 255.0f);
      glm::vec4 fog_color = (1.0f/255.0f)*glm::vec4(230.0f, 181.0f, 130.0f, 255.0f);
   };

   struct LightUniforms
   {
      glm::vec4 La = (1.0f/255.0f)*glm::vec4(148.0f, 105.0f, 89.0f, 1.0f);	//ambient light color
      glm::vec4 Ld = (1.0f/255.0f)*glm::vec4(133.0f, 184.0f, 190.0f, 1.0f);	//diffuse light color
      glm::vec4 Ls = (1.0f/255.0f)*glm::vec4(168.0f, 167.0f, 63.0f, 255.0f);	//specular light color
      glm::vec4 light_w = glm::vec4(0.0f, 2.2, 1.0f, 1.0f); //world-space light position
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
   extern const int NUM_MATERIALS;
   extern MaterialUniforms MaterialData[];

   //IDs for the buffer objects holding the uniform block data
   extern GLuint scene_ubo;
   extern GLuint light_ubo;
   extern GLuint material_ubo[];

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
      extern int pass;
   };
};
