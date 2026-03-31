#ifdef SE_UI_ENABLED

#include <fstream>
#include <cstring>

namespace SE {

UIFileInterface::UIFileInterface(const std::string & base_dir)
        : sBaseDir(base_dir) {
}

Rml::FileHandle UIFileInterface::Open(const Rml::String & path) {

        // Guard against double-prepending sBaseDir.
        // RmlUi may pass an already-prefixed path on reloads or cache reuse.
        std::string full_path;
        if (!sBaseDir.empty() && path.compare(0, sBaseDir.size(), sBaseDir) == 0) {
                full_path = path;
                log_d("already-prefixed path: {}", path);
        }
        else {
                full_path = sBaseDir + path;
        }

        log_d("UIFileInterface: open '{}' -> '{}'", path, full_path);
        auto * pStream = new std::fstream(full_path, std::ios::in | std::ios::binary);
        if (!pStream->is_open()) {
                delete pStream;
                log_w("UIFileInterface: failed to open '{}'", full_path);
                return {};
        }
        return reinterpret_cast<Rml::FileHandle>(pStream);
}

void UIFileInterface::Close(Rml::FileHandle file) {

        if (!file) return;
        delete reinterpret_cast<std::fstream *>(file);
}

size_t UIFileInterface::Read(void * pBuffer, size_t size, Rml::FileHandle file) {

        if (!file) return 0;
        auto * pStream = reinterpret_cast<std::fstream *>(file);
        pStream->read(static_cast<char *>(pBuffer), static_cast<std::streamsize>(size));
        return static_cast<size_t>(pStream->gcount());
}

bool UIFileInterface::Seek(Rml::FileHandle file, long offset, int origin) {

        if (!file) return false;
        auto * pStream = reinterpret_cast<std::fstream *>(file);
        std::ios::seekdir dir;
        switch (origin) {
                case SEEK_SET: dir = std::ios::beg; break;
                case SEEK_CUR: dir = std::ios::cur; break;
                case SEEK_END: dir = std::ios::end; break;
                default:       return false;
        }
        pStream->seekg(offset, dir);
        return !pStream->fail();
}

size_t UIFileInterface::Tell(Rml::FileHandle file) {

        if (!file) return 0;
        auto * pStream = reinterpret_cast<std::fstream *>(file);
        return static_cast<size_t>(pStream->tellg());
}

size_t UIFileInterface::Length(Rml::FileHandle file) {

        if (!file) return 0;
        auto * pStream = reinterpret_cast<std::fstream *>(file);
        const auto cur = pStream->tellg();
        pStream->seekg(0, std::ios::end);
        const auto end = pStream->tellg();
        pStream->seekg(cur, std::ios::beg);
        return static_cast<size_t>(end);
}

} // namespace SE

#endif // SE_UI_ENABLED
