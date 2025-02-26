#ifndef __CUBE_H__
#define __CUBE_H__

#include "LoadMesh.h"

struct CubeData : public MeshData
{
   void CreateCube();
   void DrawMesh() override;
};


#endif