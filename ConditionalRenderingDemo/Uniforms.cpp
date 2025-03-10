#include "Uniforms.h"
#include <GL/glew.h>

namespace Uniforms
{
   SceneUniforms SceneData;
   LightUniforms LightData;
   const int NUM_MATERIALS = 2;
   MaterialUniforms MaterialData[NUM_MATERIALS];

   //IDs for the buffer objects holding the uniform block data
   GLuint scene_ubo = -1;
   GLuint light_ubo = -1;
   GLuint material_ubo[NUM_MATERIALS] = {-1};

   namespace UboBinding
   {
      //These values come from the binding value specified in the shader block layout
      int scene = 0;
      int light = 1;
      int material = 2;
   };

   //Locations for the uniforms which are not in uniform blocks
   namespace UniformLocs
   {
      int M = 0; //model matrix
      int time = 1;
      int pass = 2;
   };

   void Init()
   {
      //Create and initialize uniform buffers
      glGenBuffers(1, &Uniforms::scene_ubo);
      glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneUniforms), nullptr, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
      glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::scene, scene_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

      LightData.La = (1.0f/255.0f)*glm::vec4(245.0f, 178.0f, 154.0f, 255.0f);
      LightData.Ld = (1.0f/255.0f)*glm::vec4(216.0f, 140.0f, 201.0f, 255.0f);
      LightData.Ls = (1.0f/255.0f)*glm::vec4(77.0f, 77.0f, 77.0f, 255.0f);
      LightData.light_w = glm::vec4(4.5f, 72.0f, -23.0f, 255.0f);

      glGenBuffers(1, &light_ubo);
      glBindBuffer(GL_UNIFORM_BUFFER, light_ubo);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(LightUniforms), &LightData, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
      glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::light, light_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

      MaterialData[0].ka = 1.5f*(1.0f/255.0f)*glm::vec4(103.0f, 199.0f, 209.0f, 255.0f);
      MaterialData[0].kd = 1.5f*(1.0f/255.0f)*glm::vec4(104.0f, 125.0f, 164.0f, 255.0f);
      MaterialData[0].ks = (1.0f/255.0f)*glm::vec4(160.0f);
      MaterialData[0].shininess = 1.0f;

      MaterialData[1].ka = (1.0f/255.0f)*glm::vec4(163.0f, 192.0f, 247.0f, 255.0f);
      MaterialData[1].kd = (1.0f/255.0f)*glm::vec4(250.0f, 250.0f, 250.0f, 255.0f);
      MaterialData[1].ks = (1.0f/255.0f)*glm::vec4(255.0f);
      MaterialData[1].shininess = 91.0f;

      glGenBuffers(NUM_MATERIALS, material_ubo);
      for(int i=0; i<NUM_MATERIALS; i++)
      {
         glBindBuffer(GL_UNIFORM_BUFFER, material_ubo[i]);
         glBufferData(GL_UNIFORM_BUFFER, sizeof(MaterialUniforms), &MaterialData[i], GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
         glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::material, material_ubo[i]); //Associate this uniform buffer with the uniform block in the shader that has the same binding.
      }

      glBindBuffer(GL_UNIFORM_BUFFER, 0);
   }

   void BufferSceneData()
   {
      glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo); //Bind the OpenGL UBO before we update the data.
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &SceneData); //Upload the new uniform values.
      glBindBuffer(GL_UNIFORM_BUFFER, 0); //unbind the ubo
   }
};
