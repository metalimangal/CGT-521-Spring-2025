#ifndef __SURF_H__
#define __SURF_H__

#include <windows.h>
#include <GL/glew.h>

struct surf_vao
{
   GLuint vao = -1;
   GLenum mode = GL_NONE;     //Primitive mode, e.g. GL_POINTS, GL_TRIANGLES, etc...
   GLuint num_vertices = 0;   //Number of vertices to draw with glDrawArrays

   void DrawArrays() 
   {
      glDrawArrays(mode, 0, num_vertices);
   }

   GLuint num_indices = 0; //Number of indices to draw with glDrawElements
   GLenum type = GL_NONE;  //Type of indices, e.g. GL_UNSIGNED_SHORT, GL_UNSIGNED_INT
   void* indices = 0;      //Offset, in bytes, from the beginning of the index buffer to start drawing from         

   void DrawElements()
   {
      glDrawElements(mode, num_indices, type, indices);
   }

   void Draw()
   {
      if(num_indices > 0)
      {
         DrawElements();
      }
      else
      {
         DrawArrays();
      }
   }
};

surf_vao create_surf_points_vao(int n_grid);
surf_vao create_surf_triangles_vao(int n_grid);
surf_vao create_surf_separate_points_vao(int n_grid);

surf_vao create_surf_separate_vao(int n_grid);
surf_vao create_surf_interleaved_vao(int n_grid);

surf_vao create_indexed_surf_interleaved_vao(int n_grid);

#endif
