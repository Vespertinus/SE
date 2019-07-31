
namespace SE {

UniformBuffer::UniformBuffer(const uint16_t new_block_size, const uint16_t initial_block_cnt) :
        block_size(new_block_size),
        allocated_blocks_cnt(initial_block_cnt),
        vShadowBuffer(initial_block_cnt * block_size, 0),
        size_changed(true) {

        int alignment = GetSystem<GraphicsConfig>().GetValue(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);

        if (block_size % alignment != 0) {
                throw(std::runtime_error(fmt::format(
                                                "unaligned block size: {}, alignment: {}",
                                                block_size,
                                                alignment)));
        }

        vFreeEntryList.reserve(initial_block_cnt);
        for (uint32_t i = 0; i < initial_block_cnt; ++i) {
                vFreeEntryList.emplace_back(i);
        }

        glGenBuffers(1, &gl_id);
}

UniformBuffer::~UniformBuffer() noexcept {

        glDeleteBuffers(1, &gl_id);
};

ret_code_t UniformBuffer::SetValue(
                uint16_t block_id,
                uint16_t value_offset,
                const void * pValue,
                const uint32_t value_size) {

        if (block_id >= allocated_blocks_cnt) {
                log_e("wrong block_id: {}, allocated_blocks_cnt: {}, block_size: {}, gl_id: {}",
                                block_id,
                                allocated_blocks_cnt,
                                block_size,
                                gl_id);
                return uWRONG_INPUT_DATA;
        }
        if ((value_offset + value_size) > block_size) {
                log_e("too big value_offset: {}, or value size: {}, block_id: {}, block_size: {}, gl_id: {}",
                                value_offset,
                                value_size,
                                block_id,
                                block_size,
                                gl_id);
                return uWRONG_INPUT_DATA;
        }

        //TODO check that block used, not in free list

        memcpy(&vShadowBuffer[block_id * block_size + value_offset], pValue, value_size);

        sDirty.insert(block_id);
        return uSUCCESS;
}

ret_code_t UniformBuffer::GetValue(
                uint16_t block_id,
                uint16_t value_offset,
                const void *& pValue,
                const uint32_t value_size) {

        if (block_id >= allocated_blocks_cnt) {
                log_e("wrong block_id: {}, allocated_blocks_cnt: {}, block_size: {}, gl_id: {}",
                                block_id,
                                allocated_blocks_cnt,
                                block_size,
                                gl_id);
                return uWRONG_INPUT_DATA;
        }
        if ((value_offset + value_size) > block_size) {
                log_e("too big value_offset: {}, or value size: {}, block_id: {}, block_size: {}, gl_id: {}",
                                value_offset,
                                value_size,
                                block_id,
                                block_size,
                                gl_id);
                return uWRONG_INPUT_DATA;
        }

        //TODO check that block used, not in free list

        pValue = &vShadowBuffer[block_id * block_size + value_offset];
        return uSUCCESS;
}

uint16_t UniformBuffer::AllocateBlock() {

        uint16_t block_id = -1;

        if (vFreeEntryList.size()) {
                block_id = vFreeEntryList.back();
                vFreeEntryList.pop_back();
        }
        else {
                uint16_t old_allocated_cnt = allocated_blocks_cnt;
                allocated_blocks_cnt *= 1.5;
                vShadowBuffer.resize(allocated_blocks_cnt * block_size, 0);
                block_id = old_allocated_cnt;
                for (uint32_t i = old_allocated_cnt + 1; i < allocated_blocks_cnt; ++i) {
                        vFreeEntryList.emplace_back(i);
                }
                size_changed = true;
        }

        return block_id;
}

void UniformBuffer::ReleaseBlock(const uint16_t block_id) {

        if (block_id >= allocated_blocks_cnt) {
                log_e("wrong block_id: {}, allocated_blocks_cnt: {}, block_size: {}, gl_id: {}",
                                block_id,
                                allocated_blocks_cnt,
                                block_size,
                                gl_id);
                return;
        }
        vFreeEntryList.emplace_back(block_id);
        memset(&vShadowBuffer[block_id * block_size], 0, block_size);
}

void UniformBuffer::UploadToDevice() const {

        if (!sDirty.size()) { return; }

        if (size_changed || sDirty.size() > (allocated_blocks_cnt * 0.3) ) {
                //copy full buffer
                GetSystem<GraphicsState>().UploadUniformBufferData(gl_id, vShadowBuffer.size(), vShadowBuffer.data());

                size_changed = false;
        }
        else {
                //copy dirty
                uint32_t buf_offset;
                for (auto block_id : sDirty) {

                        buf_offset = block_id * block_size;
                        GetSystem<GraphicsState>().UploadUniformBufferSubData(
                                        gl_id,
                                        buf_offset,
                                        block_size,
                                        &vShadowBuffer[buf_offset]);
                }
        }

        sDirty.clear();
}


void UniformBuffer::Apply(const uint16_t block_id, const UniformUnitInfo::Type uniform_buffer_unit) const {

        if (sDirty.count(block_id) == 1) {
                UploadToDevice();
        }

        GetSystem<GraphicsState>().BindUniformBufferRange(
                        gl_id,
                        uniform_buffer_unit,
                        block_id * block_size,
                        block_size);
}

}
