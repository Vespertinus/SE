namespace SE {

GeometryEntity::GeometryEntity(
                const uint32_t new_vao_id,
                const uint32_t vertex_count,
                const uint32_t vertex_start,
                const uint32_t new_primitive_type) :
                        vao_id(new_vao_id),
                        primitive_type(new_primitive_type),
                        index_type(WITHOUT_INDEX),
                        start(vertex_start),
                        count(vertex_count) {
}

GeometryEntity::GeometryEntity(
                const uint32_t new_vao_id,
                const uint32_t index_count,
                const uint32_t index_start,
                const uint32_t new_index_type,
                const uint32_t new_primitive_type) :
                        vao_id(new_vao_id),
                        primitive_type(new_primitive_type),
                        index_type(new_index_type),
                        start(index_start),
                        count(index_count) {
}

void GeometryEntity::Draw() const {

        if (index_type != WITHOUT_INDEX) {
                /*
                log_d("vao: {}, primitive_type: {}, index_type: {}, start: {}, count: {}",
                                vao_id,
                                primitive_type,
                                index_type,
                                start,
                                count);
                 */

                TGraphicsState::Instance().Draw(vao_id, primitive_type, index_type, start, count);
        }
        else {
                TGraphicsState::Instance().DrawArrays(vao_id, primitive_type, start, count);
        }
}

uint32_t GeometryEntity::GetIndicesCnt() const {

        return count;
}

}
