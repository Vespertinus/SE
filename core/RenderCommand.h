
#ifndef __RENDER_COMMAND_H__
#define __RENDER_COMMAND_H__ 1

#include <Transform.h>

class GeometryEntity;
class Material;

namespace SE {

class RenderCommand {

        const GeometryEntity  * pGeom;
        const Material        * pMaterial;
        const Transform       & oTransform;

        //uint64_t      sort_key;

        void    UpdateKey();

        public:
        RenderCommand(const GeometryEntity * pNewGeom,
                      const Material * pNewMaterial,
                      const Transform & oNewTransform);

        void            Draw() const;
        //Update?
        uint64_t        GetSortKey() const;
        //SetMaterial SetGeom SetTransform;?

};


} //namespace SE

#ifdef SE_IMPL
#include <RenderCommand.tcc>
#endif

#endif



