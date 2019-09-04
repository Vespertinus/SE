
namespace SE {

VertexBuffer::VertexBuffer(const size_t size, const UpdateFreq freq) : vShadowBuffer(size), update_frequency(freq) {

        switch (update_frequency) {

                case UpdateFreq::STATIC:
                        gl_modify_freq = GL_STATIC_DRAW;
                        break;
                case UpdateFreq::DYNAMIC:
                        gl_modify_freq = GL_DYNAMIC_DRAW;
                        break;
                case UpdateFreq::STREAM:
                        gl_modify_freq = GL_STREAM_DRAW;
                        break;
                default:
                        throw(std::runtime_error(fmt::format(
                                                        "wronf update frequency: {}",
                                                        static_cast<uint8_t>(freq))));
        }

        glGenBuffers(1, &gl_id);
}

VertexBuffer::~VertexBuffer() {

        ReleaseGPUData();
}

void VertexBuffer::ReplaceData(void const * const pData, const uint64_t size) {

        if (size != vShadowBuffer.size()) {

                vShadowBuffer.resize(size);
                size_changed = true;
        }

        memcpy(&vShadowBuffer[0], pData, size);
        dirty = true;
}

ret_code_t VertexBuffer::UpdateDataRange(void const * const pData, uint64_t offset, uint64_t size) {

        if ((offset + size) > vShadowBuffer.size() || (offset + size) > MAX_BUFFER_SIZE) {
                log_e("requested offset({}) + size({}) exceed shadeow size ({})", offset, size, vShadowBuffer.size());
                return uWRONG_INPUT_DATA;
        }
        memcpy(&vShadowBuffer[offset], pData, size);
        dirty = true;

        return uSUCCESS;
}

void VertexBuffer::Append(void const * const pData, uint64_t size) {

        vShadowBuffer.reserve(vShadowBuffer.size() + size);
        vShadowBuffer.insert(
                        vShadowBuffer.end(),
                        static_cast<uint8_t const * const>(pData),
                        static_cast<uint8_t const * const>(pData) + size );

        dirty           = true;
        size_changed    = true;
}

void VertexBuffer::UploadToGPU() {

        if (!dirty)  { return; }

        if (!gl_id) {

                glGenBuffers(1, &gl_id);
        }

        //log_d("size_changed: {}", size_changed);//THINK store prev upload size?

        glBindBuffer(GL_ARRAY_BUFFER, gl_id);

        if (size_changed) {

                glBufferData(GL_ARRAY_BUFFER, vShadowBuffer.size(), vShadowBuffer.data(), gl_modify_freq);
                size_changed = false;
        }
        else {
                glBufferSubData(GL_ARRAY_BUFFER, 0, vShadowBuffer.size(), vShadowBuffer.data());
        }

        dirty = false;
}

void VertexBuffer::ReleaseGPUData() {

        if (!gl_id) { return; }

        glDeleteBuffers(1, &gl_id);
        gl_id = 0;
}

void VertexBuffer::Clear() {

        vShadowBuffer.clear();
}

uint32_t VertexBuffer::ID() const {

        return gl_id;
}

size_t VertexBuffer::Size() const {

        return vShadowBuffer.size();
}

}
