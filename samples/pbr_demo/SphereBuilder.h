#ifndef __PBR_SPHERE_BUILDER_H__
#define __PBR_SPHERE_BUILDER_H__

#include <MeshBuilder.h>
#include <MeshGen.h>
#include <VertexLayout.h>

namespace SE {

inline H<TMesh> CreateSphereMesh(int rings, int sectors) {
        MeshBuilder oBuilder(VertexLayout::PosNormTanUV());
        MeshGen::Sphere(oBuilder, 1.0f, rings, sectors);
        return oBuilder.Upload("pbr_sphere");
}

} // namespace SE

#endif
