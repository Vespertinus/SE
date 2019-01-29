#ifndef __UNIFORM_BUFFER_H__
#define __UNIFORM_BUFFER_H__ 1

#include <unordered_set>

namespace SE {

class UniformBuffer {


        uint32_t                        gl_id;
        /** aligned */
        uint16_t                        block_size;
        uint16_t                        allocated_blocks_cnt;
        std::vector<uint8_t>            vShadowBuffer;
        /** store free block offset */
        std::vector<uint32_t>           vFreeEntryList;
        mutable std::unordered_set<uint16_t>    sDirty;
        mutable bool                    size_changed;

        public:

        UniformBuffer(const uint16_t new_block_size, const uint16_t initial_block_cnt = 10);
        ~UniformBuffer() noexcept;

        ret_code_t SetValue(uint16_t block_id, uint16_t value_offset, const void * pValue, const uint32_t value_size);
        /** temp pointer, could be changed on resize */
        ret_code_t GetValue(uint16_t block_id, uint16_t value_offset, const void *& pValue, const uint32_t value_size);
        void       UploadToDevice() const;
        uint16_t   AllocateBlock();
        void       ReleaseBlock(const uint16_t block_id);
        void       Apply(const uint16_t block_id, const UniformUnitInfo::Type uniform_buffer_unit) const;
        //THINK store unit inside on creation

        std::string Str() const;

        /**
         THINK compactify storage, notify all linked UniformBlock, rewrite block_id for all UniformBLock handles

         option for buffer update rate: GL_DYNAMIC_DRAW or GL_STREAM_DRAW

         */
};

} //namespace SE
#endif

