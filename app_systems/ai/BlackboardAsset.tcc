
#ifdef SE_IMPL

#include <ai/BlackboardAsset.h>
#include <Logging.h>

#include <Blackboard_generated.h>
#include <flatbuffers/flatbuffers.h>

#include <fstream>

namespace SE {

BlackboardAsset::BlackboardAsset(const std::string& sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                throw std::runtime_error(fmt::format("BlackboardAsset: failed to open '{}'", sName));
        }

        const size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> raw(sz);
        f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(sz));

        flatbuffers::Verifier verifier(raw.data(), raw.size());
        if (!SE::FlatBuffers::VerifyBlackboardDefinitionBuffer(verifier)) {
                throw std::runtime_error(
                                fmt::format("BlackboardAsset: FlatBuffer verify failed for '{}'", sName));
        }

        Build(SE::FlatBuffers::GetBlackboardDefinition(raw.data()));
        size = static_cast<uint32_t>(sz);

        log_d("BlackboardAsset: loaded '{}' ({} bytes, {} keys)", sName, sz, vKeys.size());
}

BlackboardAsset::BlackboardAsset(const std::string& sName, rid_t rid, const void* pData, size_t sz)
        : ResourceHolder(rid, sName) {

        if (!pData || sz == 0) {
                throw std::runtime_error(fmt::format("BlackboardAsset: empty byte payload for '{}'", sName));
        }

        flatbuffers::Verifier verifier(static_cast<const uint8_t*>(pData), sz);
        if (!SE::FlatBuffers::VerifyBlackboardDefinitionBuffer(verifier)) {
                throw std::runtime_error(
                                fmt::format("BlackboardAsset: FlatBuffer verify failed for '{}'", sName));
        }

        Build(SE::FlatBuffers::GetBlackboardDefinition(pData));
        size = static_cast<uint32_t>(sz);

        log_d("BlackboardAsset: built from bytes '{}' ({} bytes, {} keys)", sName, sz, vKeys.size());
}

BlackboardAsset::BlackboardAsset(const std::string& sName, rid_t rid,
                                 const FlatBuffers::BlackboardDefinition* pDef)
        : ResourceHolder(rid, sName) {

        Build(pDef);
        log_d("BlackboardAsset: built inline '{}' ({} keys)", sName, vKeys.size());
}

void BlackboardAsset::Build(const FlatBuffers::BlackboardDefinition* pDef) {

        if (!pDef) {
                throw std::runtime_error(fmt::format("BlackboardAsset '{}': null definition", sName));
        }

        if (pDef->id()) {
                id = StrID(pDef->id()->str());
        }

        if (!pDef->keys() || pDef->keys()->size() == 0) {
                throw std::runtime_error(
                                fmt::format("BlackboardAsset '{}': no keys declared", sName));
        }

        // Per-type running indices: the slot table partitions the SoA value arrays.
        uint16_t float_count = 0;
        uint16_t bool_count  = 0;
        uint16_t int_count   = 0;
        uint16_t vec3_count  = 0;
        uint16_t entity_count = 0;

        vKeys.reserve(pDef->keys()->size());

        for (const auto* pKey : *pDef->keys()) {
                if (!pKey || !pKey->name() || pKey->name()->size() == 0) {
                        throw std::runtime_error(
                                        fmt::format("BlackboardAsset '{}': key with no name", sName));
                }

                SKey k;
                k.s_name = pKey->name()->str();
                k.key    = StrID(k.s_name);

                for (const auto& existing : vKeys) {
                        if (existing.key == k.key) {
                                throw std::runtime_error(fmt::format(
                                                "BlackboardAsset '{}': duplicate key '{}'", sName,
                                                k.s_name));
                        }
                }

                switch (pKey->type()) {
                        case FlatBuffers::EBType::Float:  k.slot = { EBBType::Float,  float_count++ };  break;
                        case FlatBuffers::EBType::Bool:   k.slot = { EBBType::Bool,   bool_count++ };   break;
                        case FlatBuffers::EBType::Int:    k.slot = { EBBType::Int,    int_count++ };    break;
                        case FlatBuffers::EBType::Vec3:   k.slot = { EBBType::Vec3,   vec3_count++ };   break;
                        case FlatBuffers::EBType::Entity: k.slot = { EBBType::Entity, entity_count++ }; break;
                        default:
                                throw std::runtime_error(fmt::format(
                                                "BlackboardAsset '{}': key '{}' has unknown type", sName,
                                                k.s_name));
                }

                using VU = FlatBuffers::BBValueU;
                const auto e_type = pKey->type();

                // Authored defaults; type must agree with the declared key type.
                switch (pKey->default_value_type()) {
                        case VU::Float: {
                                const auto* p = pKey->default_value_as_Float();
                                if (e_type != FlatBuffers::EBType::Float) {
                                        throw std::runtime_error(fmt::format(
                                                        "BlackboardAsset '{}': key '{}' float default on non-float key",
                                                        sName, k.s_name));
                                }
                                if (p) k.f_default = p->value();
                                break;
                        }
                        case VU::Bool: {
                                const auto* p = pKey->default_value_as_Bool();
                                if (e_type != FlatBuffers::EBType::Bool) {
                                        throw std::runtime_error(fmt::format(
                                                        "BlackboardAsset '{}': key '{}' bool default on non-bool key",
                                                        sName, k.s_name));
                                }
                                if (p) k.b_default = p->value();
                                break;
                        }
                        case VU::Int: {
                                const auto* p = pKey->default_value_as_Int();
                                if (e_type != FlatBuffers::EBType::Int) {
                                        throw std::runtime_error(fmt::format(
                                                        "BlackboardAsset '{}': key '{}' int default on non-int key",
                                                        sName, k.s_name));
                                }
                                if (p) k.i_default = p->value();
                                break;
                        }
                        case VU::Vec3: {
                                const auto* p = pKey->default_value_as_Vec3();
                                if (e_type != FlatBuffers::EBType::Vec3) {
                                        throw std::runtime_error(fmt::format(
                                                        "BlackboardAsset '{}': key '{}' vec3 default on non-vec3 key",
                                                        sName, k.s_name));
                                }
                                if (p) k.v_default = glm::vec3(p->x(), p->y(), p->z());
                                break;
                        }
                        case VU::NONE:
                                break;   // absent = zero value of the declared type
                        default:
                                throw std::runtime_error(fmt::format(
                                                "BlackboardAsset '{}': key '{}' has unknown default variant",
                                                sName, k.s_name));
                }

                vKeys.push_back(std::move(k));
        }
}

std::string BlackboardAsset::Str() const {
        return fmt::format("BlackboardAsset['{}' keys={}]", sName, vKeys.size());
}

} // namespace SE

#endif // SE_IMPL
