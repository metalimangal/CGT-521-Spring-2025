#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp> //for pi
#include <glm/gtx/matrix_transform_2d.hpp>
#include "Surf.h"

//Buffer offset casts an integer to a pointer so that it can be passed as 
// the last arg to glVertexAttribPointer

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

namespace AttribLocs
{
   int pos = 0; 
   int tex_coord = 1;
   int normal = 2; 
}


const unsigned int PRIMITIVE_RESTART_INDEX = 0xFFFFFFFF; // Max unsigned int

//The surface to draw.
float sinc2D(const glm::vec2& p)
{
   const float eps = 1e-12f;
   float r = glm::length(p);
   float z = 1.0f;

   if (r < eps)
   {
      return 1.0f;  
   }

   return z = sin(r) / r;
}

glm::vec3 surf(glm::mat3& M, int i, int j)
{
   glm::vec3 p = M*glm::vec3(float(i), float(j), 1.0f);
   return glm::vec3(p.x, p.y, 10.0f*sinc2D(p));
}

//Compute normal using finite differences
glm::vec3 normal(glm::mat3& M, int i, int j)
{
   glm::vec3 px0 = surf(M, i-1, j);
   glm::vec3 px1 = surf(M, i+1, j);

   glm::vec3 py0 = surf(M, i, j-1);
   glm::vec3 py1 = surf(M, i, j+1);

   glm::vec3 dFdx = 0.5f*(px1-px0);
   glm::vec3 dFdy = 0.5f*(py1-py0);

   return glm::normalize(glm::cross(dFdx, dFdy));
}

#pragma region Create surface: primitive = GL_POINTS, only 1 attribute in VBO
////////////////////////////////////////////////////////////////////////////////
/// Create surf points:
/// Places only positions into the VBO
////////////////////////////////////////////////////////////////////////////////

GLuint create_surf_points_vbo(int n_grid)
{
   //Declare a vector to hold GL_POINTS vertices
   const int num_vertices = n_grid*n_grid;
   std::vector<glm::vec3> surf_verts;
   surf_verts.reserve(num_vertices);

   //Create matrix to transform indices to points
   glm::mat3 T = glm::translate(glm::mat3(1.0f), glm::vec2(-float(n_grid)/2.0, -float(n_grid)/2.0));
   glm::mat3 S = glm::scale(glm::mat3(1.0f), glm::vec2(20.0f/n_grid));
   glm::mat3 M = S*T;

   for(int i=0; i<n_grid; i++)
   {
      for (int j=0; j<n_grid; j++)
      {
         surf_verts.push_back(surf(M,i,j));
      }
   }

   GLuint vbo;
   glGenBuffers(1, &vbo); //Generate vbo to hold vertex attributes for triangle.

   glBindBuffer(GL_ARRAY_BUFFER, vbo); //Specify the buffer where vertex attribute data is stored.
   
   //Upload from main memory to gpu memory.
   glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*surf_verts.size(), surf_verts.data(), GL_STATIC_DRAW);

   return vbo;
}

surf_vao create_surf_points_vao(int n)
{
   surf_vao surf;

   //Generate vao id to hold the mapping from attrib variables in shader to memory locations in vbo
   glGenVertexArrays(1, &surf.vao);

   //Binding vao means that bindbuffer, enablevertexattribarray and vertexattribpointer state will be remembered by vao
   glBindVertexArray(surf.vao);

   GLuint vbo = create_surf_points_vbo(n);
   surf.mode = GL_POINTS;
   surf.num_vertices = n*n;

   //Only vertex positions in vbo
   glEnableVertexAttribArray(AttribLocs::pos); //Enable the position attribute.

   //Tell opengl how to get the attribute values out of the vbo (stride and offset).
   glVertexAttribPointer(AttribLocs::pos, 3, GL_FLOAT, false, 3*sizeof(float), 0);

   return surf;
}
#pragma endregion

#pragma region Create surface: primitive = GL_TRIANGLES, only 1 attribute in VBO
////////////////////////////////////////////////////////////////////////////////
/// Create surf triangles:
/// Places only positions into the VBO in GL_TRIANGLES order
////////////////////////////////////////////////////////////////////////////////

GLuint create_surf_triangles_vbo(int n_grid)
{
   //Declare a vector to hold GL_TRIANGLES vertices
   const int num_vertices = (n_grid-1)*(n_grid-1)*2*3;
   std::vector<glm::vec3> surf_verts;
   surf_verts.reserve(num_vertices);

   //Create matrix to transform indices to points
   glm::mat3 T = glm::translate(glm::mat3(1.0f), glm::vec2(-float(n_grid)/2.0, -float(n_grid)/2.0));
   glm::mat3 S = glm::scale(glm::mat3(1.0f), glm::vec2(20.0f/n_grid));
   glm::mat3 M = S*T;

   for(int i=0; i<n_grid-1; i++)
   {
      for (int j=0; j<n_grid-1; j++)
      {
         //Insert triangle 1
         surf_verts.push_back(surf(M,i,j));
         surf_verts.push_back(surf(M,i+1,j));
         surf_verts.push_back(surf(M,i+1,j+1));

         //Insert triangle 2
         surf_verts.push_back(surf(M,i,j));
         surf_verts.push_back(surf(M,i+1,j+1));
         surf_verts.push_back(surf(M,i,j+1));
      }
   }

   GLuint vbo;
   glGenBuffers(1, &vbo); //Generate vbo to hold vertex attributes for triangle.

   glBindBuffer(GL_ARRAY_BUFFER, vbo); //Specify the buffer where vertex attribute data is stored.
   
   //Upload from main memory to gpu memory.
   glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*surf_verts.size(), surf_verts.data(), GL_STATIC_DRAW);

   return vbo;
}

surf_vao create_surf_triangles_vao(int n)
{
   surf_vao surf;

   //Generate vao id to hold the mapping from attrib variables in shader to memory locations in vbo
   glGenVertexArrays(1, &surf.vao);

   //Binding vao means that bindbuffer, enablevertexattribarray and vertexattribpointer state will be remembered by vao
   glBindVertexArray(surf.vao);

   GLuint vbo = create_surf_triangles_vbo(n);
   surf.mode = GL_TRIANGLES;
   const int num_triangles = (n - 1)*(n - 1) * 2;
   surf.num_vertices = num_triangles * 3;

   
   //Only vertex positions in vbo
   glEnableVertexAttribArray(AttribLocs::pos); //Enable the position attribute.

   //Tell opengl how to get the attribute values out of the vbo (stride and offset).
   glVertexAttribPointer(AttribLocs::pos, 3, GL_FLOAT, false, 3*sizeof(float), BUFFER_OFFSET(0));

   glBindVertexArray(0); //unbind the vao

   return surf;
}
#pragma endregion

#pragma region Create surface: primitive = GL_POINTS, 3 attributes stored separately in 1 VBO

////////////////////////////////////////////////////////////////////////////////
/// Create surf separate points:
/// Places positions, texture coordinates and normals into the VBO
/// in GL_POINTS order.
////////////////////////////////////////////////////////////////////////////////

GLuint create_surf_separate_points_vbo(int n_grid)
{
   //Declare a vector to hold GL_POINTS vertices, texture coordinates, and normals 
   const int num_vertices = n_grid*n_grid;
   std::vector<float> surf_verts;
   surf_verts.reserve(3*num_vertices + 2*num_vertices + 3*num_vertices);

   //Create matrix to transform indices to points
   glm::mat3 T = glm::translate(glm::mat3(1.0f), glm::vec2(-float(n_grid)/2.0, -float(n_grid)/2.0));
   glm::mat3 S = glm::scale(glm::mat3(1.0f), glm::vec2(20.0f/n_grid));
   glm::mat3 M = S*T;

   //Insert positions
   for(int i=0; i<n_grid; i++)
   {
      for (int j=0; j<n_grid; j++)
      {
         glm::vec3 pos = surf(M,i,j);
         for (int k = 0; k < 3; k++) surf_verts.push_back(pos[k]);
      }
   }

   //Insert texture coordinates
   for(int i=0; i<n_grid; i++)
   {
      for (int j=0; j<n_grid; j++)
      {
         glm::vec2 tex_coord = glm::vec2(float(i), float(j))/float(n_grid-1);
         surf_verts.push_back(tex_coord.x);
         surf_verts.push_back(tex_coord.y);
      }
   }

   //Insert normals
   for(int i=0; i<n_grid; i++)
   {
      for (int j=0; j<n_grid; j++)
      {
         glm::vec3 n = normal(M,i,j);
         for(int k=0; k<3; k++) surf_verts.push_back(n[k]);
      }
   }

   GLuint vbo;
   glGenBuffers(1, &vbo); //Generate vbo to hold vertex attributes for triangle.

   glBindBuffer(GL_ARRAY_BUFFER, vbo); //Specify the buffer where vertex attribute data is stored.
   
   //Upload from main memory to gpu memory.
   glBufferData(GL_ARRAY_BUFFER, sizeof(float)*surf_verts.size(), surf_verts.data(), GL_STATIC_DRAW);

   return vbo;
}


surf_vao create_surf_separate_points_vao(int n)
{
   surf_vao surf;

   //Generate vao id to hold the mapping from attrib variables in shader to memory locations in vbo
   glGenVertexArrays(1, &surf.vao);

   //Binding vao means that bindbuffer, enablevertexattribarray and vertexattribpointer state will be remembered by vao
   glBindVertexArray(surf.vao);

   GLuint vbo = create_surf_separate_points_vbo(n);
   surf.mode = GL_POINTS;
   surf.num_vertices = n*n;

   //Separate pos, tex_coord and normal in vbo
   glEnableVertexAttribArray(AttribLocs::pos); 
   glEnableVertexAttribArray(AttribLocs::tex_coord);
   glEnableVertexAttribArray(AttribLocs::normal); 

   //Tell opengl how to get the attribute values out of the vbo (stride and offset).
   glVertexAttribPointer(AttribLocs::pos, 3, GL_FLOAT, false, 3*sizeof(float), BUFFER_OFFSET(0));
   glVertexAttribPointer(AttribLocs::tex_coord, 2, GL_FLOAT, false, 2*sizeof(float), BUFFER_OFFSET(3*surf.num_vertices*sizeof(float)));
   glVertexAttribPointer(AttribLocs::normal, 3, GL_FLOAT, false, 3*sizeof(float), BUFFER_OFFSET((3+2)*surf.num_vertices*sizeof(float)));

   glBindVertexArray(0); //unbind the vao

   return surf;
}
#pragma endregion

#pragma region Create surface: primitive = GL_TRIANGLES, 3 attributes stored separately in 1 VBO

////////////////////////////////////////////////////////////////////////////////
/// Create surf separate
/// Places positions, texture coordinates and normals into the VBO
/// in GL_TRIANGLES order.
////////////////////////////////////////////////////////////////////////////////

GLuint create_surf_separate_vbo(int n_grid)
{
   //Declare a vector to hold GL_POINTS vertices, texture coordinates, and normals 
   const int num_vertices = (n_grid-1)*(n_grid-1)*2*3;

   //Note that we use a std::vector of floats here. This vector needs to hold vec3s and vec2s,
   //so we push each component of the vec3s and vec2s into this container.
   std::vector<float> surf_verts;
   surf_verts.reserve(3*num_vertices + 2*num_vertices + 3*num_vertices);

   //Create matrix to transform indices to points
   glm::mat3 T = glm::translate(glm::mat3(1.0f), glm::vec2(-float(n_grid)/2.0, -float(n_grid)/2.0));
   glm::mat3 S = glm::scale(glm::mat3(1.0f), glm::vec2(20.0f/n_grid));
   glm::mat3 M = S*T;

   //Insert positions
   for(int i=0; i<n_grid-1; i++)
   {
      for (int j=0; j<n_grid-1; j++)
      {
         glm::vec3 p0 = surf(M,i,j);
         glm::vec3 p1 = surf(M,i+1,j);
         glm::vec3 p2 = surf(M,i+1,j+1);
         glm::vec3 p3 = surf(M,i,j+1);

         //Insert triangle 1
         for (int k = 0; k < 3; k++) surf_verts.push_back(p0[k]); //vertex 0
         for (int k = 0; k < 3; k++) surf_verts.push_back(p1[k]); //vertex 1
         for (int k = 0; k < 3; k++) surf_verts.push_back(p2[k]); //vertex 2

         //Insert triangle 2
         for (int k = 0; k < 3; k++) surf_verts.push_back(p0[k]); //vertex 0
         for (int k = 0; k < 3; k++) surf_verts.push_back(p2[k]); //vertex 2
         for (int k = 0; k < 3; k++) surf_verts.push_back(p3[k]); //vertex 3
      }
   }


   //Insert texture coordinates
   for(int i=0; i<n_grid-1; i++)
   {
      for (int j=0; j<n_grid-1; j++)
      {
         glm::vec2 t0 = glm::vec2(float(i), float(j))/float(n_grid-1);
         glm::vec2 t1 = glm::vec2(float(i+1), float(j))/float(n_grid-1);
         glm::vec2 t2 = glm::vec2(float(i+1), float(j+1))/float(n_grid-1);
         glm::vec2 t3 = glm::vec2(float(i), float(j+1))/float(n_grid-1);

         //Insert triangle 1
         surf_verts.push_back(t0.x);   surf_verts.push_back(t0.y); 
         surf_verts.push_back(t1.x);   surf_verts.push_back(t1.y); 
         surf_verts.push_back(t2.x);   surf_verts.push_back(t2.y); 

         //Insert triangle 2
         surf_verts.push_back(t0.x);   surf_verts.push_back(t0.y); 
         surf_verts.push_back(t2.x);   surf_verts.push_back(t2.y); 
         surf_verts.push_back(t3.x);   surf_verts.push_back(t3.y); 
      }
   }

   //Insert normals
   for(int i=0; i<n_grid-1; i++)
   {
      for (int j=0; j<n_grid-1; j++)
      {
         glm::vec3 n0 = normal(M,i,j);
         glm::vec3 n1 = normal(M,i+1,j);
         glm::vec3 n2 = normal(M,i+1,j+1);
         glm::vec3 n3 = normal(M,i,j+1);

         //Insert triangle 1
         for (int k = 0; k < 3; k++) surf_verts.push_back(n0[k]); //vertex 0
         for (int k = 0; k < 3; k++) surf_verts.push_back(n1[k]); //vertex 1
         for (int k = 0; k < 3; k++) surf_verts.push_back(n2[k]); //vertex 2

         //Insert triangle 2
         for (int k = 0; k < 3; k++) surf_verts.push_back(n0[k]); //vertex 0
         for (int k = 0; k < 3; k++) surf_verts.push_back(n2[k]); //vertex 2
         for (int k = 0; k < 3; k++) surf_verts.push_back(n3[k]); //vertex 3
      }
   }

   GLuint vbo;
   glGenBuffers(1, &vbo); //Generate vbo to hold vertex attributes for triangle.
   glBindBuffer(GL_ARRAY_BUFFER, vbo); //Specify the buffer where vertex attribute data is stored.
   
   //Upload from main memory to gpu memory.
   glBufferData(GL_ARRAY_BUFFER, sizeof(float)*surf_verts.size(), surf_verts.data(), GL_STATIC_DRAW);

   return vbo;
}


surf_vao create_surf_separate_vao(int n)
{
   surf_vao surf;

   //Generate vao id to hold the mapping from attrib variables in shader to memory locations in vbo
   glGenVertexArrays(1, &surf.vao);

   //Binding vao means that bindbuffer, enablevertexattribarray and vertexattribpointer state will be remembered by vao
   glBindVertexArray(surf.vao);

   GLuint vbo = create_surf_separate_vbo(n);
   surf.mode = GL_TRIANGLES;
   const int num_triangles = (n - 1)*(n - 1) * 2;
   surf.num_vertices = num_triangles * 3;

   //Separate pos, tex_coord and normal in vbo
   glEnableVertexAttribArray(AttribLocs::pos); 
   glEnableVertexAttribArray(AttribLocs::tex_coord);
   glEnableVertexAttribArray(AttribLocs::normal); 

   //Tell opengl how to get the attribute values out of the vbo (stride and offset).
   glVertexAttribPointer(AttribLocs::pos, 3, GL_FLOAT, false, 3*sizeof(float), BUFFER_OFFSET(0));
   glVertexAttribPointer(AttribLocs::tex_coord, 2, GL_FLOAT, false, 2*sizeof(float), BUFFER_OFFSET(3*surf.num_vertices*sizeof(float)));
   glVertexAttribPointer(AttribLocs::normal, 3, GL_FLOAT, false, 3*sizeof(float), BUFFER_OFFSET((3+2)*surf.num_vertices*sizeof(float)));

   glBindVertexArray(0); //unbind the vao

   return surf;
}

#pragma endregion

#pragma region Create surface: primitive = GL_TRIANGLES, 3 attributes interleaved in 1 VBO
////////////////////////////////////////////////////////////////////////////////
/// Create surf interleaved
/// Places interleaved positions, texture coordinates and normals into the VBO
/// in TRIANGLES order.
////////////////////////////////////////////////////////////////////////////////

GLuint create_surf_interleaved_vbo(int n_grid)
{
   //Declare a vector to hold GL_POINTS vertices, texture coordinates, and normals 
   const int num_vertices = (n_grid-1)*(n_grid-1)*2*3;

   //Note that we use a std::vector of floats here. This vector needs to hold vec3s and vec2s,
   //so we push each component of the vec3s and vec2s into this container.
   std::vector<float> surf_verts;
   surf_verts.reserve(3*num_vertices + 2*num_vertices + 3*num_vertices);

   //Create matrix to transform indices to points
   glm::mat3 T = glm::translate(glm::mat3(1.0f), glm::vec2(-float(n_grid)/2.0, -float(n_grid)/2.0));
   glm::mat3 S = glm::scale(glm::mat3(1.0f), glm::vec2(20.0f/n_grid));
   glm::mat3 M = S*T;

   //Insert positions
   for(int i=0; i<n_grid-1; i++)
   {
      for (int j=0; j<n_grid-1; j++)
      {
         glm::vec3 p0 = surf(M,i,j);      glm::vec2 t0 = glm::vec2(float(i), float(j))/float(n_grid-1);     glm::vec3 n0 = normal(M,i,j);
         glm::vec3 p1 = surf(M,i+1,j);    glm::vec2 t1 = glm::vec2(float(i+1), float(j))/float(n_grid-1);   glm::vec3 n1 = normal(M,i+1,j);
         glm::vec3 p2 = surf(M,i+1,j+1);  glm::vec2 t2 = glm::vec2(float(i+1), float(j+1))/float(n_grid-1); glm::vec3 n2 = normal(M,i+1,j+1);
         glm::vec3 p3 = surf(M,i,j+1);    glm::vec2 t3 = glm::vec2(float(i), float(j+1))/float(n_grid-1);   glm::vec3 n3 = normal(M,i,j+1);


         //Insert triangle 1
         for (int k = 0; k < 3; k++) surf_verts.push_back(p0[k]); //vertex 0
         for (int k = 0; k < 2; k++) surf_verts.push_back(t0[k]);
         for (int k = 0; k < 3; k++) surf_verts.push_back(n0[k]);

         for (int k = 0; k < 3; k++) surf_verts.push_back(p1[k]); //vertex 1
         for (int k = 0; k < 2; k++) surf_verts.push_back(t1[k]);
         for (int k = 0; k < 3; k++) surf_verts.push_back(n1[k]);

         for (int k = 0; k < 3; k++) surf_verts.push_back(p2[k]); //vertex 2
         for (int k = 0; k < 2; k++) surf_verts.push_back(t2[k]);
         for (int k = 0; k < 3; k++) surf_verts.push_back(n2[k]);

         //Insert triangle 2
         for (int k = 0; k < 3; k++) surf_verts.push_back(p0[k]); //vertex 0
         for (int k = 0; k < 2; k++) surf_verts.push_back(t0[k]);
         for (int k = 0; k < 3; k++) surf_verts.push_back(n0[k]);

         for (int k = 0; k < 3; k++) surf_verts.push_back(p2[k]); //vertex 2
         for (int k = 0; k < 2; k++) surf_verts.push_back(t2[k]);
         for (int k = 0; k < 3; k++) surf_verts.push_back(n2[k]);

         for (int k = 0; k < 3; k++) surf_verts.push_back(p3[k]); //vertex 3
         for (int k = 0; k < 2; k++) surf_verts.push_back(t3[k]);
         for (int k = 0; k < 3; k++) surf_verts.push_back(n3[k]);
      }
   }

   GLuint vbo;
   glGenBuffers(1, &vbo); //Generate vbo to hold vertex attributes for triangle.
   glBindBuffer(GL_ARRAY_BUFFER, vbo); //Specify the buffer where vertex attribute data is stored.
   
   //Upload from main memory to gpu memory.
   glBufferData(GL_ARRAY_BUFFER, sizeof(float)*surf_verts.size(), surf_verts.data(), GL_STATIC_DRAW);

   return vbo;
}


surf_vao create_surf_interleaved_vao(int n)
{
   surf_vao surf;

   //Generate vao id to hold the mapping from attrib variables in shader to memory locations in vbo
   glGenVertexArrays(1, &surf.vao);

   //Binding vao means that bindbuffer, enablevertexattribarray and vertexattribpointer state will be remembered by vao
   glBindVertexArray(surf.vao);

   GLuint vbo = create_surf_interleaved_vbo(n);
   surf.mode = GL_TRIANGLES;
   const int num_triangles = (n - 1)*(n - 1) * 2;
   surf.num_vertices = num_triangles * 3;

   //Separate pos, tex_coord and normal in vbo
   glEnableVertexAttribArray(AttribLocs::pos); 
   glEnableVertexAttribArray(AttribLocs::tex_coord);
   glEnableVertexAttribArray(AttribLocs::normal); 

   //Tell opengl how to get the attribute values out of the vbo (stride and offset).
   const int stride = (3+2+3)*sizeof(float);
   glVertexAttribPointer(AttribLocs::pos, 3, GL_FLOAT, false, stride, BUFFER_OFFSET(0));
   glVertexAttribPointer(AttribLocs::tex_coord, 2, GL_FLOAT, false, stride, BUFFER_OFFSET(3*sizeof(float)));
   glVertexAttribPointer(AttribLocs::normal, 3, GL_FLOAT, false, stride, BUFFER_OFFSET((3+2)*sizeof(float)));

   glBindVertexArray(0); //unbind the vao

   return surf;
}
#pragma endregion

#pragma region Create surface: primitive = GL_TRIANGLES, 3 INDEXED attributes interleaved in 1 VBO
////////////////////////////////////////////////////////////////////////////////
/// Create indexed surf interleaved
/// Places interleaved positions, texture coordinates and normals into the VBO (only unique vertices)
/// Creates an index buffer in GL_TRIANGLES order
////////////////////////////////////////////////////////////////////////////////

GLuint create_indexed_surf_interleaved_vbo(int n_grid)
{
   //Declare a vector to hold GL_POINTS vertices, texture coordinates, and normals 
   const int num_vertices = n_grid*n_grid;

   //Note that we use a std::vector of floats here. This vector needs to hold vec3s and vec2s,
   //so we push each component of the vec3s and vec2s into this container.
   std::vector<float> surf_verts;
   surf_verts.reserve((3+2+3)*num_vertices);

   //Create matrix to transform indices to points
   glm::mat3 T = glm::translate(glm::mat3(1.0f), glm::vec2(-float(n_grid)/2.0, -float(n_grid)/2.0));
   glm::mat3 S = glm::scale(glm::mat3(1.0f), glm::vec2(20.0f/n_grid));
   glm::mat3 M = S*T;

   //Insert vertices: only unique positions, no sharing here. Only indices are repeated
   for(int i=0; i<n_grid; i++)
   {
      for (int j=0; j<n_grid; j++)
      {
         glm::vec3 p = surf(M,i,j);      
         glm::vec2 t = glm::vec2(float(i), float(j))/float(n_grid-1);     
         glm::vec3 n = normal(M,i,j);
        
         //Insert vertex
         for (int k = 0; k < 3; k++) surf_verts.push_back(p[k]);
         for (int k = 0; k < 2; k++) surf_verts.push_back(t[k]);
         for (int k = 0; k < 3; k++) surf_verts.push_back(n[k]);
      }
   }

   GLuint vbo;
   glGenBuffers(1, &vbo); //Generate vbo to hold vertex attributes for triangle.
   glBindBuffer(GL_ARRAY_BUFFER, vbo); //Specify the buffer where vertex attribute data is stored.
   
   //Upload from main memory to gpu memory.
   glBufferData(GL_ARRAY_BUFFER, sizeof(float)*surf_verts.size(), surf_verts.data(), GL_STATIC_DRAW);

   return vbo;
}

GLuint create_triangles_index_buffer(int n_grid)
{
   //Declare a vector to hold GL_POINTS vertices, texture coordinates, and normals 
   const int num_indices = (n_grid-1)*(n_grid-1)*2*3;

   std::vector<unsigned int> surf_indices;
   surf_indices.reserve(num_indices);

   //Insert indices. Consider a grid of squares. We insert the two triangles that make up the square starting 
   // with index idx.
   unsigned int idx = 0;
   for(int i=0; i<n_grid-1; i++)
   {
      for (int j=0; j<n_grid-1; j++)
      {
         //Insert triangle 1
         surf_indices.push_back(idx);
         surf_indices.push_back(idx+n_grid);   
         surf_indices.push_back(idx+n_grid+1);

         //Insert triangle 2
         surf_indices.push_back(idx);
         surf_indices.push_back(idx+n_grid+1);   
         surf_indices.push_back(idx+1);
      
         idx++;
      }
      idx++; //advance index to next row of vertices
   }

   GLuint index_buffer;
   glGenBuffers(1, &index_buffer); 
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer); //GL_ELEMENT_ARRAY_BUFFER is the target for buffers containing indices
   
   //Upload from main memory to gpu memory.
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*surf_indices.size(), surf_indices.data(), GL_STATIC_DRAW);

   return index_buffer;
}

surf_vao create_indexed_surf_interleaved_vao(int n)
{
   surf_vao surf;

   //Generate vao id to hold the mapping from attrib variables in shader to memory locations in vbo
   glGenVertexArrays(1, &surf.vao);

   //Binding vao means that bindbuffer, enablevertexattribarray and vertexattribpointer state will be remembered by vao
   glBindVertexArray(surf.vao);

   GLuint vbo = create_indexed_surf_interleaved_vbo(n);
   surf.mode = GL_TRIANGLES;
   const int num_triangles = (n - 1)*(n - 1) * 2;
   surf.num_indices = num_triangles * 3;
   surf.type = GL_UNSIGNED_INT;

   GLuint index_buffer = create_triangles_index_buffer(n);

   //Separate pos, tex_coord and normal in vbo
   glEnableVertexAttribArray(AttribLocs::pos); 
   glEnableVertexAttribArray(AttribLocs::tex_coord);
   glEnableVertexAttribArray(AttribLocs::normal); 

   //Tell opengl how to get the attribute values out of the vbo (stride and offset).
   const int stride = (3+2+3)*sizeof(float);
   glVertexAttribPointer(AttribLocs::pos, 3, GL_FLOAT, false, stride, BUFFER_OFFSET(0));
   glVertexAttribPointer(AttribLocs::tex_coord, 2, GL_FLOAT, false, stride, BUFFER_OFFSET(3*sizeof(float)));
   glVertexAttribPointer(AttribLocs::normal, 3, GL_FLOAT, false, stride, BUFFER_OFFSET((3+2)*sizeof(float)));

   glBindVertexArray(0); //unbind the vao

   return surf;
}
#pragma endregion

#pragma region Create surface: primitive = GL_TRIANGLE_STRIP, 3 INDEXED attributes interleaved in 1 VBO
////////////////////////////////////////////////////////////////////////////////
/// Create indexed surf interleaved
/// Places interleaved positions, texture coordinates and normals into the VBO (only unique vertices)
/// Creates an index buffer in GL_TRIANGLES order
////////////////////////////////////////////////////////////////////////////////

GLuint create_indexed_strip_surf_interleaved_vbo(int n_grid)
{
    //Declare a vector to hold GL_POINTS vertices, texture coordinates, and normals 
    const int num_vertices = n_grid * n_grid;

    //Note that we use a std::vector of floats here. This vector needs to hold vec3s and vec2s,
    //so we push each component of the vec3s and vec2s into this container.
    std::vector<float> surf_verts;
    surf_verts.reserve((3 + 2 + 3) * num_vertices);

    //Create matrix to transform indices to points
    glm::mat3 T = glm::translate(glm::mat3(1.0f), glm::vec2(-float(n_grid) / 2.0, -float(n_grid) / 2.0));
    glm::mat3 S = glm::scale(glm::mat3(1.0f), glm::vec2(20.0f / n_grid));
    glm::mat3 M = S * T;

    //Insert vertices: only unique positions, no sharing here. Only indices are repeated
    for (int i = 0; i < n_grid; i++)
    {
        for (int j = 0; j < n_grid; j++)
        {
            glm::vec3 p = surf(M, i, j);
            glm::vec2 t = glm::vec2(float(i), float(j)) / float(n_grid - 1);
            glm::vec3 n = normal(M, i, j);

            //Insert vertex
            for (int k = 0; k < 3; k++) surf_verts.push_back(p[k]);
            for (int k = 0; k < 2; k++) surf_verts.push_back(t[k]);
            for (int k = 0; k < 3; k++) surf_verts.push_back(n[k]);
        }
    }

    GLuint vbo;
    glGenBuffers(1, &vbo); //Generate vbo to hold vertex attributes for triangle.
    glBindBuffer(GL_ARRAY_BUFFER, vbo); //Specify the buffer where vertex attribute data is stored.

    //Upload from main memory to gpu memory.
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * surf_verts.size(), surf_verts.data(), GL_STATIC_DRAW);

    return vbo;
}

GLuint create_triangle_strip_index_buffer(int n_grid)
{
 

    std::vector<unsigned int> surf_indices;
    surf_indices.reserve((n_grid - 1) * n_grid * 2 + (n_grid - 2)); // Primitive restart rows


    unsigned int idx = 0;
    for (int i = 0; i < n_grid - 1; i++)
    {
        for (int j = 0; j < n_grid -1; j++)
        {
            surf_indices.push_back(idx + n_grid); 
            surf_indices.push_back(idx);          
            idx++;
        }
        surf_indices.push_back(PRIMITIVE_RESTART_INDEX);
        idx++;

    }


    GLuint index_buffer;
    glGenBuffers(1, &index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * surf_indices.size(), surf_indices.data(), GL_STATIC_DRAW);

    return index_buffer;

}

surf_vao create_indexed_strip_surf_interleaved_vao(int n)
{
    surf_vao surf;

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(PRIMITIVE_RESTART_INDEX);


    //Generate vao id to hold the mapping from attrib variables in shader to memory locations in vbo
    glGenVertexArrays(1, &surf.vao);

    //Binding vao means that bindbuffer, enablevertexattribarray and vertexattribpointer state will be remembered by vao
    glBindVertexArray(surf.vao);

    GLuint vbo = create_indexed_strip_surf_interleaved_vbo(n);
    surf.mode = GL_TRIANGLE_STRIP;
    const int num_triangles = (n - 1) * n * 2 + (n - 2);
    surf.num_indices = num_triangles;
    surf.type = GL_UNSIGNED_INT;

    GLuint index_buffer = create_triangle_strip_index_buffer(n);

    //Separate pos, tex_coord and normal in vbo
    glEnableVertexAttribArray(AttribLocs::pos);
    glEnableVertexAttribArray(AttribLocs::tex_coord);
    glEnableVertexAttribArray(AttribLocs::normal);

    //Tell opengl how to get the attribute values out of the vbo (stride and offset).
    const int stride = (3 + 2 + 3) * sizeof(float);
    glVertexAttribPointer(AttribLocs::pos, 3, GL_FLOAT, false, stride, BUFFER_OFFSET(0));
    glVertexAttribPointer(AttribLocs::tex_coord, 2, GL_FLOAT, false, stride, BUFFER_OFFSET(3 * sizeof(float)));
    glVertexAttribPointer(AttribLocs::normal, 3, GL_FLOAT, false, stride, BUFFER_OFFSET((3 + 2) * sizeof(float)));

    glBindVertexArray(0); //unbind the vao

    return surf;
}
#pragma endregion


