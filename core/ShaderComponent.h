#ifndef __SHADER_COMPONENT_H__
#define __SHADER_COMPONENT_H__ 1

#include <ShaderComponent_generated.h>

namespace SE {

//TODO store shader source in yaml and convert to flatbuffers via tools/convert

class ShaderComponent : public ResourceHolder {

        public:

        struct Settings {

                //TODO incapsulate resource path in some obj
                //std::string_view        sDependenciesDir;
                //char                    splitter;
                std::string        sDependenciesDir;
        };

        private:

        std::vector <ShaderComponent *> vDependencies;
        uint32_t                        gl_id;
        SE::FlatBuffers::ShaderType     type;

        void Load(const SE::FlatBuffers::ShaderComponent * pShader, const Settings & oSettings);

        public:
        ShaderComponent(const std::string & sName,
                        const rid_t         new_rid,
                        const Settings    & oSettings =  {});
        ShaderComponent(const std::string & sName,
                        const rid_t         new_rid,
                        const SE::FlatBuffers::ShaderComponent * pShader,
                        const Settings   & oSettings = {});
        ~ShaderComponent() noexcept;

        uint32_t Get() const;
        FlatBuffers::ShaderType GetType() const;
        const std::vector <ShaderComponent *> & GetDependencies() const;

};


} //namespace SE

#ifdef SE_IMPL
#include <ShaderComponent.tcc>
#endif

#endif
