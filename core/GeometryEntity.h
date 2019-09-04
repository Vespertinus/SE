#ifndef __GEOMETRY_ENTITY_H__
#define __GEOMETRY_ENTITY_H__ 1


namespace SE {

class GeometryEntity {

        static const uint32_t WITHOUT_INDEX = -1;

        uint32_t        vao_id;
        uint32_t        primitive_type;
        /** -1 for rendering without index, using DrawArrays */
        uint32_t        index_type;
        uint32_t        start;
        uint32_t        count;

        public:

        GeometryEntity(const uint32_t new_vao_id,
                       const uint32_t vertex_count,
                       const uint32_t vertex_start,
                       const uint32_t new_primitive_type);

        GeometryEntity(const uint32_t new_vao_id,
                       const uint32_t index_count,
                       const uint32_t index_start,
                       const uint32_t index_type,
                       const uint32_t new_primitive_type);

        void Draw() const;
        uint32_t GetKey();
        //Getters if needed
        //Set...
        std::string Str() const;
        //uint32_t GetPrimitiveCnt() const;//TODO
        uint32_t GetIndicesCnt() const;
        uint32_t GetVAO() const;//THINK
        void     SetVAO(const uint32_t new_vao_id);
        void     SetRange(const uint32_t vertex_count, const uint32_t vertex_start);
};

} //namespace SE
#endif




