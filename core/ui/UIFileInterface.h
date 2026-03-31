#ifndef __UI_FILE_INTERFACE_H__
#define __UI_FILE_INTERFACE_H__ 1

#ifdef SE_UI_ENABLED

#include <string>
#include <RmlUi/Core/FileInterface.h>

namespace SE {

class UIFileInterface : public Rml::FileInterface {

        std::string sBaseDir;

public:
        explicit UIFileInterface(const std::string & base_dir);

        Rml::FileHandle Open  (const Rml::String & path) override;
        void            Close (Rml::FileHandle file)      override;
        size_t          Read  (void * pBuffer, size_t size, Rml::FileHandle file) override;
        bool            Seek  (Rml::FileHandle file, long offset, int origin)     override;
        size_t          Tell  (Rml::FileHandle file)      override;
        size_t          Length(Rml::FileHandle file)      override;
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_FILE_INTERFACE_H__
