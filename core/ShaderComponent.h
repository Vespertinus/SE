#ifndef __SHADER_COMPONENT_H__
#define __SHADER_COMPONENT_H__ 1

#include <ShaderComponent_generated.h>

namespace SE {

//TODO store shader source in yaml and convert to flatbuffers via tools/convert

class ShaderComponent : public ResourceHolder {

        std::vector <ShaderComponent *> vDependencies;
        uint32_t                        gl_id;
        SE::FlatBuffers::ShaderType     type;

        void Load(const SE::FlatBuffers::ShaderComponent * pShader);

        public:
        ShaderComponent(const std::string & sName,
                        const rid_t         new_rid);
        ShaderComponent(const std::string & sName,
                        const rid_t         new_rid,
                        const SE::FlatBuffers::ShaderComponent * pShader);
        ~ShaderComponent() noexcept;

        uint32_t Get() const;
        FlatBuffers::ShaderType GetType() const;
        const std::vector <ShaderComponent *> & GetDependencies() const;

};


} //namespace SE
#endif
