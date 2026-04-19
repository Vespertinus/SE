
#ifdef SE_IMPL
#ifndef ENTITY_TEMPLATE_PATCHER_IMPL_INCLUDED
#define ENTITY_TEMPLATE_PATCHER_IMPL_INCLUDED
#include <EntityTemplateUtility.tcc>
#endif
#endif

#include <fstream>
#include <flatbuffers/flatbuffers.h>

#include <CommonEvents.h>
#include <Logging.h>
#include <MPUtil.h>

namespace SE {

// ---------------------------------------------------------------------------
// BFS helper: find a NodeTemplateT by name in an unpacked tree
// ---------------------------------------------------------------------------

static FlatBuffers::NodeTemplateT * FindTemplateNodeByName(
                FlatBuffers::NodeTemplateT & root,
                std::string_view name) {

        // BFS using a simple index-based traversal to avoid iterator invalidation
        std::vector<FlatBuffers::NodeTemplateT *> queue;
        queue.push_back(&root);

        for (size_t i = 0; i < queue.size(); ++i) {
                FlatBuffers::NodeTemplateT * pNode = queue[i];
                if (pNode->name == name) return pNode;
                for (auto & pChild : pNode->children) {
                        if (pChild) queue.push_back(pChild.get());
                }
        }
        return nullptr;
}

// ---------------------------------------------------------------------------
// BuildLoaders
// ---------------------------------------------------------------------------

template <class ... TComponents> void EntityTemplateSystem<TComponents...>::BuildLoaders() {

        using TFilteredTypes = MP::FilteredTypelist<THasSerialized, TComponents...>;
        static_assert(!std::is_same<MP::TypelistWrapper<>, TFilteredTypes>::value,
                        "EntityTemplateSystem: TComponents pack has no serializable components");

        using TLoaderTuple = typename MP::Typelist2WrappedTmplPack<
                std::tuple, LoadWrapper, TFilteredTypes>::Type;

        TLoaderTuple oLoaders;

        auto InitMap = [this](auto & oLoader) {
                using TExactSerialized = typename std::decay<decltype(oLoader)>::type::TExactSerialized;
                FlatBuffers::ComponentU key = FlatBuffers::ComponentUTraits<TExactSerialized>::enum_value;
                mLoaders.emplace(key, oLoader);
        };

        MP::TupleForEach(oLoaders, InitMap);
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

template <class ... TComponents> EntityTemplateSystem<TComponents...>::EntityTemplateSystem() {
        BuildLoaders();
}

// ---------------------------------------------------------------------------
// SetSceneTree
// ---------------------------------------------------------------------------

template <class ... TComponents> void EntityTemplateSystem<TComponents...>::SetSceneTree(TSceneTreeExact * pTree) {
    pSceneTree = pTree;
}

// ---------------------------------------------------------------------------
// RegisterBuffer — internal; verifies, stores, handles variant merge
// ---------------------------------------------------------------------------

template <class ... TComponents> ret_code_t EntityTemplateSystem<TComponents...>::RegisterBuffer(
        std::vector<uint8_t>   vData,
        const std::string &    sDebugName) {

        flatbuffers::Verifier oVerifier(vData.data(), vData.size());
        if (!FlatBuffers::VerifyEntityTemplateBuffer(oVerifier)) {
                log_e("EntityTemplateSystem: FlatBuffer verification failed for '{}'", sDebugName);
                return uWRONG_INPUT_DATA;
        }

        const auto * pTemplateFB = FlatBuffers::GetEntityTemplate(vData.data());
        if (!pTemplateFB || !pTemplateFB->id()) {
                log_e("EntityTemplateSystem: missing id field in '{}'", sDebugName);
                return uWRONG_INPUT_DATA;
        }

        const std::string sId = pTemplateFB->id()->str();
        if (mTemplates.count(sId)) {
                log_w("EntityTemplateSystem: template '{}' already registered, skipping '{}'",
                                sId, sDebugName);
                return uSUCCESS;
        }

        // Store buffer and register pointer
        size_t idx = mBuffers.size();
        size_t buf_size = vData.size();
        mBuffers.push_back({ std::move(vData), buf_size });
        const auto * pFB = FlatBuffers::GetEntityTemplate(mBuffers.back().vData.data());
        mTemplates.emplace(sId, pFB);

        // If this template has a base_id, perform the merge now
        if (pFB->base_id() && pFB->base_id()->size() > 0) {
                const std::string sBaseId = pFB->base_id()->str();
                auto baseIt = mTemplates.find(sBaseId);
                if (baseIt == mTemplates.end()) {
                        log_e("EntityTemplateSystem: template '{}' references base '{}' not yet loaded; "
                                        "load base templates before variants",
                                        sId, sBaseId);
                        mTemplates.erase(sId);
                        mBuffers.resize(idx);
                        return uWRONG_INPUT_DATA;
                }

                // Find the base buffer size to enforce the merge limit
                size_t base_buf_size = 0;
                for (const auto & buf : mBuffers) {
                        const auto * pStart = reinterpret_cast<const uint8_t *>(buf.vData.data());
                        const auto * pBaseRaw = reinterpret_cast<const uint8_t *>(baseIt->second);
                        if (pBaseRaw >= pStart && pBaseRaw < pStart + buf.vData.size()) {
                                base_buf_size = buf.raw_size;
                                break;
                        }
                }
                if (base_buf_size > kMergeBufferMaxSize) {
                        log_e("EntityTemplateSystem: base '{}' buffer ({} bytes) exceeds {} MB limit",
                                        sBaseId, base_buf_size, kMergeBufferMaxSize / (1024 * 1024));
                        mTemplates.erase(sId);
                        mBuffers.resize(idx);
                        return uWRONG_INPUT_DATA;
                }

                if (auto res = MergeVariant(sId, pFB, baseIt->second); res != uSUCCESS) {
                        mTemplates.erase(sId);
                        mBuffers.resize(idx);
                        return res;
                }
        }

        log_d("EntityTemplateSystem: registered template '{}'", sId);
        return uSUCCESS;
}

// ---------------------------------------------------------------------------
// MergeVariant
// ---------------------------------------------------------------------------

template <class ... TComponents> ret_code_t EntityTemplateSystem<TComponents...>::MergeVariant(
                const std::string &                    sVariantId,
                const FlatBuffers::EntityTemplate *    pVariantFB,
                const FlatBuffers::EntityTemplate *    pBaseFB) {

        // Unpack base root to mutable object tree
        std::unique_ptr<FlatBuffers::NodeTemplateT> pRootT(pBaseFB->root()->UnPack());
        if (!pRootT) {
                log_e("EntityTemplateSystem: failed to unpack base root for variant '{}'", sVariantId);
                return uWRONG_INPUT_DATA;
        }

        // Apply each NodeOverride from the variant
        if (const auto * pOverrides = pVariantFB->overrides()) {
                for (flatbuffers::uoffset_t i = 0; i < pOverrides->size(); ++i) {
                        const auto * pOvr = pOverrides->Get(i);
                        if (!pOvr->node_name()) continue;

                        std::string_view target_name = pOvr->node_name()->c_str();
                        FlatBuffers::NodeTemplateT * pTarget = FindTemplateNodeByName(*pRootT, target_name);
                        if (!pTarget) {
                                log_e("EntityTemplateSystem: NodeOverride target '{}' not found in base '{}'",
                                                target_name, pBaseFB->id() ? pBaseFB->id()->c_str() : "?");
                                return uWRONG_INPUT_DATA;
                        }

                        // Apply transform overrides
                        if (pOvr->has_translation() && pOvr->translation()) {
                                const auto * v = pOvr->translation();
                                pTarget->translation = std::make_unique<FlatBuffers::Vec3>(v->x(), v->y(), v->z());
                        }
                        if (pOvr->has_rotation() && pOvr->rotation()) {
                                const auto * v = pOvr->rotation();
                                pTarget->rotation = std::make_unique<FlatBuffers::Vec3>(v->x(), v->y(), v->z());
                        }
                        if (pOvr->has_scale() && pOvr->scale()) {
                                const auto * v = pOvr->scale();
                                pTarget->scale = std::make_unique<FlatBuffers::Vec3>(v->x(), v->y(), v->z());
                        }
                        if (pOvr->has_enabled()) {
                                pTarget->enabled = pOvr->enabled();
                        }

                        // Apply field overrides — dispatch to each component's static ApplyField
                        if (const auto * pFieldOvrs = pOvr->field_overrides()) {
                                // Build a loader tuple once for compile-time component type iteration
                                TLoaderTuple oLoaderTuple;

                                for (flatbuffers::uoffset_t j = 0; j < pFieldOvrs->size(); ++j) {
                                        const auto * pFO = pFieldOvrs->Get(j);
                                        if (!pFO->path() ||
                                                        pFO->value_type() == FlatBuffers::FieldValueU::NONE) continue;

                                        std::string_view full_path = pFO->path()->c_str();
                                        auto dot = full_path.find('.');
                                        std::string_view comp_name = full_path.substr(0, dot);
                                        std::string_view field_path = (dot != std::string_view::npos)
                                                ? full_path.substr(dot + 1) : "";

                                        FlatBuffers::ComponentU comp_type =
                                                EntityTemplateUtility::ComponentNameToEnum(comp_name);
                                        if (comp_type == FlatBuffers::ComponentU::NONE) {
                                                log_e("EntityTemplateSystem: unknown component '{}' in path '{}'",
                                                                comp_name, full_path);
                                                continue;
                                        }

                                        FlatBuffers::ComponentT * pCompT = nullptr;
                                        for (auto & pComp : pTarget->components) {
                                                if (pComp && pComp->component.type == comp_type) {
                                                        pCompT = pComp.get();
                                                        break;
                                                }
                                        }
                                        if (!pCompT) {
                                                log_e("EntityTemplateSystem: node '{}' has no '{}' component",
                                                                pTarget->name, comp_name);
                                                return uWRONG_INPUT_DATA;
                                        }

                                        bool applied = false;
                                        auto try_apply = [&](auto & oLoader) {
                                                if (applied) return;
                                                using TComp = typename std::decay_t<decltype(oLoader)>::TExactComponent;
                                                using TSerial = typename TComp::TSerialized;
                                                constexpr FlatBuffers::ComponentU key =
                                                        FlatBuffers::ComponentUTraits<TSerial>::enum_value;
                                                if (key != comp_type) return;

                                                if constexpr (THasApplyFieldVal<TComp>) {
                                                        using TTObj = typename TSerial::NativeTableType;
                                                        auto * pTObj = static_cast<TTObj *>(pCompT->component.value);
                                                        TComp::ApplyField(*pTObj, field_path, *pFO);
                                                        applied = true;
                                                } else {
                                                        log_w("EntityTemplateSystem: component '{}' has no ApplyField — "
                                                                        "field override '{}' skipped", comp_name, full_path);
                                                        applied = true; // suppress repeated warnings
                                                }
                                        };
                                        MP::TupleForEach(oLoaderTuple, try_apply);
                                }
                        }
                }
        }

        // Repack the merged tree to a new FlatBuffer
        flatbuffers::FlatBufferBuilder fbb;
        auto oRootOffset = FlatBuffers::NodeTemplate::Pack(fbb, pRootT.get());

        // Build the EntityTemplate: use variant's id, tags; merged root
        auto oIdOffset = fbb.CreateString(sVariantId);
        flatbuffers::Offset<flatbuffers::String> oTagsOffset;
        std::vector<flatbuffers::Offset<flatbuffers::String>> tag_offsets;

        if (pVariantFB->tags()) {
                for (flatbuffers::uoffset_t t = 0; t < pVariantFB->tags()->size(); ++t) {
                        tag_offsets.push_back(fbb.CreateString(pVariantFB->tags()->Get(t)->c_str()));
                }
        }
        auto oTagsVecOffset = fbb.CreateVector(tag_offsets);

        FlatBuffers::EntityTemplateBuilder builder(fbb);
        builder.add_id(oIdOffset);
        builder.add_root(oRootOffset);
        builder.add_tags(oTagsVecOffset);
        auto oTemplateOffset = builder.Finish();

        FlatBuffers::FinishEntityTemplateBuffer(fbb, oTemplateOffset);

        // Store merged buffer and update mTemplates[variantId]
        auto pBuf = fbb.GetBufferPointer();
        size_t buf_size = fbb.GetSize();
        std::vector<uint8_t> vMerged(pBuf, pBuf + buf_size);

        size_t merged_idx = mBuffers.size();
        mBuffers.push_back({ std::move(vMerged), buf_size });
        mMergeCache[sVariantId] = merged_idx;

        // Replace the template pointer with one pointing into the merged buffer
        mTemplates[sVariantId] = FlatBuffers::GetEntityTemplate(mBuffers.back().vData.data());

        log_d("EntityTemplateSystem: merged variant '{}' from base '{}'",
                        sVariantId, pBaseFB->id() ? pBaseFB->id()->c_str() : "?");
        return uSUCCESS;
}

// ---------------------------------------------------------------------------
// Load / LoadFromMemory
// ---------------------------------------------------------------------------

template <class ... TComponents> ret_code_t EntityTemplateSystem<TComponents...>::Load(const std::string & sPath) {

        std::ifstream oInput(sPath, std::ios::binary | std::ios::ate);

        if (!oInput.is_open()) {
                log_e("EntityTemplateSystem::Load: cannot open '{}'", sPath);
                return uWRONG_INPUT_DATA;
        }

        const auto file_size = static_cast<size_t>(oInput.tellg());
        oInput.seekg(0);

        std::vector<uint8_t> vData(file_size);
        oInput.read(reinterpret_cast<char *>(vData.data()),
                        static_cast<std::streamsize>(file_size));
        oInput.close();

        return RegisterBuffer(std::move(vData), sPath);
}

template <class ... TComponents> ret_code_t EntityTemplateSystem<TComponents...>::LoadFromMemory(
                const uint8_t *    pData,
                size_t             size,
                const std::string & sDebugName) {

        std::vector<uint8_t> vData(pData, pData + size);
        return RegisterBuffer(std::move(vData), sDebugName);
}

// ---------------------------------------------------------------------------
// HasTemplate / UnloadAll
// ---------------------------------------------------------------------------

template <class ... TComponents> bool EntityTemplateSystem<TComponents...>::HasTemplate(const std::string & sId) const {
        return mTemplates.count(sId) > 0;
}

template <class ... TComponents> void EntityTemplateSystem<TComponents...>::UnloadAll() {

        mTemplates.clear();
        mMergeCache.clear();
        mBuffers.clear();
}

template <class ... TComponents> std::string EntityTemplateSystem<TComponents...>::Str() const {

        size_t total_bytes = 0;
        for (const auto & buf : mBuffers) {
                total_bytes += buf.vData.size();
        }

        return fmt::format(
                "EntityTemplateSystem: {} template(s), {} buffer(s) ({} bytes total), {} merged variant(s)",
                mTemplates.size(),
                mBuffers.size(),
                total_bytes,
                mMergeCache.size());
}

// ---------------------------------------------------------------------------
// InstantiateNode — mirrors SceneTree::LoadNode
// ---------------------------------------------------------------------------

template <class ... TComponents> typename EntityTemplateSystem<TComponents...>::TSceneNode EntityTemplateSystem<TComponents...>::InstantiateNode(
                const FlatBuffers::NodeTemplate * pNodeFB,
                TSceneNode                        pParent,
                bool                              override_enabled,
                const glm::vec3 *                 pvOverrideTranslation,
                const glm::vec3 *                 pvOverrideRotation,
                const glm::vec3 *                 pvOverrideScale,
                TPostLoadVec &                    vPostLoad) {

        se_assert(pNodeFB);
        se_assert(pSceneTree);

        if (!pNodeFB->name() || pNodeFB->name()->size() == 0) {
                log_e("EntityTemplateSystem::InstantiateNode: node has no name — authoring error");
                return nullptr;
        }
        std::string_view sName = pNodeFB->name()->c_str();

        TSceneNode pNode = pSceneTree->Create(pParent, sName, override_enabled);
        if (!pNode) {
                log_e("EntityTemplateSystem::InstantiateNode: failed to create node '{}'", sName);
                return nullptr;
        }

        // Apply transform — caller overrides apply to root node only (nullptr = use authored)
        auto ReadVec3 = [](const FlatBuffers::Vec3 * p) -> glm::vec3 {
                if (!p) return {0.f, 0.f, 0.f};
                return {p->x(), p->y(), p->z()};
        };

        pNode->SetPos     (pvOverrideTranslation ? *pvOverrideTranslation
                        : ReadVec3(pNodeFB->translation()));
        pNode->SetRotation(pvOverrideRotation    ? *pvOverrideRotation
                        : ReadVec3(pNodeFB->rotation()));
        pNode->SetScale   (pvOverrideScale       ? *pvOverrideScale
                        : ReadVec3(pNodeFB->scale()));

        if (pNodeFB->info()) {
                pNode->SetCustomInfo(pNodeFB->info()->c_str());
        }

        // Load components — same dispatch as SceneTree::LoadNode
        if (const auto * pComponentsFB = pNodeFB->components()) {
                size_t cnt = pComponentsFB->Length();
                for (size_t i = 0; i < cnt; ++i) {
                        const auto * pCurFB = pComponentsFB->Get(i);
                        auto it = mLoaders.find(pCurFB->component_type());
                        if (it == mLoaders.end()) {
                                log_e("EntityTemplateSystem::InstantiateNode: unknown component '{}' on node '{}'",
                                                FlatBuffers::EnumNameComponentU(pCurFB->component_type()), sName);
                                return nullptr;
                        }

                        ret_code_t res = std::visit([pCurFB, &pNode, &vPostLoad](auto & oLoader) {
                                        return oLoader.Load(pCurFB->component(), pNode, vPostLoad);
                                        }, it->second);

                        if (res != uSUCCESS) {
                                log_e("EntityTemplateSystem::InstantiateNode: component '{}' failed on node '{}'",
                                                FlatBuffers::EnumNameComponentU(pCurFB->component_type()), sName);
                                return nullptr;
                        }
                }
        }

        // Recurse into children; they use authored values (no caller override)
        if (const auto * pChildrenFB = pNodeFB->children()) {
                size_t cnt = pChildrenFB->Length();
                for (size_t i = 0; i < cnt; ++i) {
                        const auto * pChildFB = pChildrenFB->Get(i);
                        bool child_enabled = pChildFB->enabled();
                        if (!InstantiateNode(pChildFB, pNode, child_enabled,
                                                nullptr, nullptr, nullptr, vPostLoad)) {
                                return nullptr;
                        }
                }
        }

        return pNode;
}

// ---------------------------------------------------------------------------
// Instantiate
// ---------------------------------------------------------------------------

template <class ... TComponents> typename EntityTemplateSystem<TComponents...>::TSceneNodeWeak EntityTemplateSystem<TComponents...>::Instantiate(
                const std::string &                              sTemplateId,
                TSceneNodeWeak                                   pParentWeak,
                const glm::vec3 &                                vTranslation,
                const glm::vec3 &                                vRotation,
                const glm::vec3 &                                vScale,
                bool                                             enabled,
                std::function<void(TSceneNodeExact &)>           fnInitializer) {

        if (!pSceneTree) {
                log_e("EntityTemplateSystem::Instantiate: no SceneTree set");
                return {};
        }

        auto it = mTemplates.find(sTemplateId);
        if (it == mTemplates.end()) {
                log_e("EntityTemplateSystem::Instantiate: unknown template '{}'", sTemplateId);
                return {};
        }

        const auto * pTemplateFB = it->second;
        const auto * pRootFB = pTemplateFB->root();
        if (!pRootFB) {
                log_e("EntityTemplateSystem::Instantiate: template '{}' has null root", sTemplateId);
                return {};
        }

        TSceneNode pParent = pParentWeak.lock();
        if (!pParent) pParent = pSceneTree->GetRoot();

        TPostLoadVec vPostLoad;

        TSceneNode pRootNode = InstantiateNode(
                        pRootFB, pParent, enabled, &vTranslation, &vRotation, &vScale, vPostLoad);

        if (!pRootNode) {
                log_e("EntityTemplateSystem::Instantiate: InstantiateNode failed for '{}'", sTemplateId);
                return {};
        }

        // Run PostLoad callbacks (same pattern as SceneTree::Load)
        for (auto & oEntry : vPostLoad) {
                std::visit([&oEntry, &sTemplateId](auto * pComponent) {

                                using TExactSerialized =
                                typename std::remove_pointer<decltype(pComponent)>::type::TSerialized;
                                const TExactSerialized * pSerialized =
                                static_cast<const TExactSerialized *>(oEntry.second);

                                if (auto res = pComponent->PostLoad(pSerialized); res != uSUCCESS) {
                                        log_e("EntityTemplateSystem: PostLoad failed for template '{}'", sTemplateId);
                                }
                                }, oEntry.first);
        }

        if (fnInitializer) {
                fnInitializer(*pRootNode);
        }

        TSceneNodeWeak hWeak { pRootNode };
        GetSystem<EventManager>().TriggerEvent(EEntitySpawned{ hWeak });
        return hWeak;
}

} // namespace SE
