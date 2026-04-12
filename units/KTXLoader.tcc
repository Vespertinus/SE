
#include <fstream>
#include <cstring>

namespace SE {

// KTX1 identifier: "\xABKTX 11\xBB\r\n\x1A\n"
static constexpr uint8_t kKTX1Magic[12] = {
        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

// KTX1 file header (immediately follows the 12-byte identifier)
#pragma pack(push, 1)
struct KTX1Header {
        uint8_t  identifier[12];
        uint32_t endianness;
        uint32_t glType;
        uint32_t glTypeSize;
        uint32_t glFormat;
        uint32_t glInternalFormat;
        uint32_t glBaseInternalFormat;
        uint32_t pixelWidth;
        uint32_t pixelHeight;
        uint32_t pixelDepth;
        uint32_t numberOfArrayElements;
        uint32_t numberOfFaces;
        uint32_t numberOfMipmapLevels;
        uint32_t bytesOfKeyValueData;
};
#pragma pack(pop)

static_assert(sizeof(KTX1Header) == 64, "KTX1Header size mismatch");

KTXLoader::KTXLoader(const Settings &) {}

ret_code_t KTXLoader::Load(const std::string & sPath, TextureStock & oTextureStock) {

        // Read the entire file into the owned buffer
        std::ifstream f(sPath, std::ios::binary | std::ios::ate);
        if (!f) {
                log_e("KTXLoader: failed to open '{}'", sPath);
                return uREAD_FILE_ERROR;
        }
        const auto file_size = static_cast<size_t>(f.tellg());
        f.seekg(0);

        oTextureStock.vOwnedData.resize(file_size);
        if (!f.read(reinterpret_cast<char*>(oTextureStock.vOwnedData.data()),
                    static_cast<std::streamsize>(file_size))) {
                log_e("KTXLoader: failed to read '{}'", sPath);
                return uREAD_FILE_ERROR;
        }

        if (file_size < sizeof(KTX1Header)) {
                log_e("KTXLoader: file too small to be KTX1: '{}'", sPath);
                return uWRONG_INPUT_DATA;
        }

        const auto & h = *reinterpret_cast<const KTX1Header*>(oTextureStock.vOwnedData.data());

        if (std::memcmp(h.identifier, kKTX1Magic, 12) != 0) {
                log_e("KTXLoader: not a KTX1 file (bad magic): '{}'", sPath);
                return uWRONG_INPUT_DATA;
        }
        if (h.endianness != 0x04030201u) {
                log_e("KTXLoader: big-endian KTX1 not supported: '{}'", sPath);
                return uWRONG_INPUT_DATA;
        }
        if (h.numberOfFaces != 6) {
                log_e("KTXLoader: expected 6 faces (cubemap), got {}: '{}'",
                      h.numberOfFaces, sPath);
                return uWRONG_INPUT_DATA;
        }
        if (h.numberOfArrayElements != 0) {
                log_e("KTXLoader: texture arrays not supported: '{}'", sPath);
                return uWRONG_INPUT_DATA;
        }

        const uint32_t num_mips = h.numberOfMipmapLevels > 0 ? h.numberOfMipmapLevels : 1;
        const size_t   data_offset = sizeof(KTX1Header) + h.bytesOfKeyValueData;

        if (data_offset >= file_size) {
                log_e("KTXLoader: file truncated before data section: '{}'", sPath);
                return uWRONG_INPUT_DATA;
        }

        oTextureStock.format          = static_cast<int>(h.glFormat);
        oTextureStock.internal_format = static_cast<int>(h.glInternalFormat);

        // cmgen KTX quirk: it writes GL_R11F_G11F_B10F (0x8C3A) into the
        // glType field, but that is an internal-format token, not a valid pixel
        // data type.  The correct packed upload type is
        // GL_UNSIGNED_INT_10F_11F_11F_REV (0x8C3B).
        oTextureStock.gl_type = (h.glType == 0x8C3Au && h.glInternalFormat == 0x8C3Au)
                ? 0x8C3Bu
                : h.glType;
        oTextureStock.width           = h.pixelWidth;
        oTextureStock.height          = h.pixelHeight;
        oTextureStock.num_faces       = 6;
        oTextureStock.num_mips        = num_mips;
        oTextureStock.raw_image       = oTextureStock.vOwnedData.data() + data_offset;
        oTextureStock.raw_image_size  = static_cast<uint32_t>(file_size - data_offset);

        log_d("KTXLoader: loaded '{}' — {}x{}, {} faces, {} mips, "
              "fmt=0x{:04X} internal=0x{:04X} type=0x{:04X}",
              sPath, h.pixelWidth, h.pixelHeight,
              h.numberOfFaces, num_mips,
              h.glFormat, h.glInternalFormat, h.glType);

        return uSUCCESS;
}

} // namespace SE
