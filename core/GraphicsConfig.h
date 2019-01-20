#ifndef __GRAPHICS_CONFIG_H__
#define __GRAPHICS_CONFIG_H__ 1

#include <unordered_set>

#include <GLUtil.h>

namespace SE {

class GraphicsConfig {

        //uniform buffer management shared_ptr...
        int32_t                                 gl_major,
                                                gl_minor;
        std::string                             sVendor;
        std::string                             sRenderer;
        std::unordered_map<uint32_t, uint32_t>  mVariables;
        std::unordered_set<std::string>         mExtensions;

        public:

        GraphicsConfig();

        void            PrintConfiguration() const;
        void            PrintExtensions() const;
        void            PrintGLInfo() const;
        uint32_t        GetValue(const uint32_t key) const;
        bool            CheckExtension(const std::string & sExt) const;
        bool            VersionSupport(const int32_t major, const int32_t minor) const;
        void            FillVariables();

};

} // namespace SE

#endif
