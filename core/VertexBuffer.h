#ifndef VERTEX_BUFFER_H
#define VERTEX_BUFFER_H

namespace SE {

class VertexBuffer {

        static const uint64_t MAX_BUFFER_SIZE = 1024 * 1024 * 1024;

        public:

        enum class UpdateFreq : uint8_t {
                STATIC  = 1,
                DYNAMIC = 2,
                STREAM  = 3
        };

        private:

        uint32_t                        gl_id{};
        uint32_t                        gl_modify_freq;
        std::vector<uint8_t>            vShadowBuffer;
        mutable bool                    size_changed{true};
        mutable bool                    dirty{false};
        UpdateFreq                      update_frequency;

        public:

        VertexBuffer(const size_t size = 0, const UpdateFreq freq = UpdateFreq::DYNAMIC);
        ~VertexBuffer();

        void       ReplaceData(void const * const pData, const uint64_t size);
        ret_code_t UpdateDataRange(void const * const pData, uint64_t offset, uint64_t size);
        void       Append(void const * const pData, uint64_t size);
        void       Clear();
        void       UploadToGPU();
        void     * GetData(const uint64_t offset);
        void       ReleaseGPUData();

        uint32_t   ID() const;
        size_t     Size() const;
        std::string Str() const;
};

} //namespace SE
#endif


