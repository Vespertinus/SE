
#ifndef __RENDER_COMMAND_H__
#define __RENDER_COMMAND_H__ 1

class GeometryEntity;
class Material;

namespace SE {

class RenderCommand {

        const GeometryEntity  * pGeom;
        const Material        * pMaterial;
        const glm::mat4 & oWorldTransform;

        //uint64_t      sort_key;

        void    UpdateKey();

        public:
        RenderCommand(const GeometryEntity * pNewGeom, const Material * pNewMaterial, const glm::mat4 & oTransform);

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



