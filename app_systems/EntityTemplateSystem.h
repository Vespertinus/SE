
#ifndef APP_ENTITY_TEMPLATE_SYSTEM_H
#define APP_ENTITY_TEMPLATE_SYSTEM_H 1

#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <ComponentLoader.h>
#include <MPTraits.h>
#include <EntityTemplate_generated.h>
#include <EntityTemplateUtility.h>
#include <EntityTemplatePlugin.h>

namespace SE {

/// EntityTemplateSystem — template/recipe system for stamping out entity hierarchies.
/// Templated on the same TComponents... pack as TSceneTree.
/// Included by apps via TCustomSystems; never by TCoreSystems.
///
/// Merge semantics (bake-at-load): a template with base_id is merged into a fresh
/// FlatBuffer once at registration; instantiation always walks the pre-merged buffer
/// and never re-applies overrides. Node names are uniquified per instantiation and a
/// failed instantiation rolls its partial hierarchy back.
template <class ... TComponents> class EntityTemplateSystem {

public:
        using TSceneTreeExact  = SceneTree<TComponents...>;
        using TSceneNodeExact  = SceneNode<TComponents...>;
        using TSceneNode       = std::shared_ptr<TSceneNodeExact>;
        using TSceneNodeWeak   = std::weak_ptr<TSceneNodeExact>;

        static constexpr size_t kMergeBufferMaxSize  = 4 * 1024 * 1024;  // 4 MB — base template merge limit
        static constexpr size_t kMaxTemplateFileSize = 100ull * 1024 * 1024; // per-file load guard (SceneTree precedent)

        EntityTemplateSystem();
        ~EntityTemplateSystem() = default;

        /// Register the active SceneTree. Call after scene init and after scene switches.
        void SetSceneTree(TSceneTreeExact * pTree);

        /// Load a .setm file; register by its id field. Returns uSUCCESS or error.
        /// If the template has a base_id, the base must already be loaded (load order).
        /// Re-loading an already-registered id REPLACES it (warning logged).
        ret_code_t Load(const std::string & sPath);

        /// Load from an in-memory buffer. Useful for unit tests.
        ret_code_t LoadFromMemory(const uint8_t * pData, size_t size,
                        const std::string & sDebugName = "<memory>");

        bool HasTemplate(const std::string & sId) const;
        size_t TemplateCount() const;

        /// Deterministic (sorted) enumeration of registered template ids.
        template <class TFn> void ForEachTemplateId(TFn && fn) const;

        /// Whether the template carries the given authoring tag.
        bool HasTag(const std::string & sId, const std::string & sTag) const;

        /// Instantiate a template by id. Returns weak_ptr to the root node, or expired on failure.
        /// vTranslation/vRotation/vScale/enabled override the root NodeTemplate's authored values.
        /// Node names are uniquified against the live tree; a failure mid-hierarchy is
        /// rolled back (no orphan subtrees, no stale name-map entries).
        /// fnInitializer is called after the full hierarchy is built — use for per-instance setup.
        TSceneNodeWeak Instantiate(
                        const std::string &                              sTemplateId,
                        TSceneNodeWeak                                   pParent       = {},
                        const glm::vec3 &                                vTranslation  = {0.f, 0.f, 0.f},
                        const glm::vec3 &                                vRotation     = {0.f, 0.f, 0.f},
                        const glm::vec3 &                                vScale        = {1.f, 1.f, 1.f},
                        bool                                             enabled       = true,
                        std::function<void(TSceneNodeExact &)>           fnInitializer = {});

        /// Unregister a single template; its buffer is retired. Later loads of the same
        /// id start fresh. Already-merged variants keep their own buffers (no dependency
        /// on the base remains after merge).
        bool Unload(const std::string & sId);

        void UnloadAll();

        std::string Str() const;

private:
        // --- Component loader dispatch (same TMP as SceneTree::Load) ---
        using TFilteredTypes = MP::FilteredTypelist<THasSerialized, TComponents...>;
        using TLoaderVariant = typename MP::Typelist2WrappedTmplPack<
                std::variant, LoadWrapper, TFilteredTypes>::Type;
        using TLoaderTuple   = typename MP::Typelist2WrappedTmplPack<
                std::tuple, LoadWrapper, TFilteredTypes>::Type;
        using TLoaderMap     = std::map<FlatBuffers::ComponentU, TLoaderVariant>;

        using TPostLoadComponents = MP::FilteredTypelistFromTL<THasPostLoad, TFilteredTypes>;
        using TPostLoadVariant    = typename MP::Typelist2WrappedTypeTmplPack<
                std::variant, std::add_pointer, TPostLoadComponents>::Type;
        using TPostLoadVec = std::vector<std::pair<TPostLoadVariant, const void * const>>;

        TLoaderMap mLoaders;
        void BuildLoaders();

        // --- Buffer ownership ---
        // Invariant: one buffer hosts exactly one registered template (each .setm
        // registers a single EntityTemplate root; a merge appends its own buffer).
        // Retired (unloaded/replaced/merged-away) buffers go to a free list for reuse.
        struct OwnedBuffer {
                std::vector<uint8_t> vData;
                size_t               raw_size = 0;  // size before merge (kMergeBufferMaxSize check)
                bool                 live     = true;
        };
        std::vector<OwnedBuffer>                               mBuffers;
        std::vector<size_t>                                    vFreeBuffers;  // retired slots

        // id → const FlatBuffers::EntityTemplate* into one of mBuffers
        std::unordered_map<std::string, const FlatBuffers::EntityTemplate *>   mTemplates;
        // id → owning mBuffers index (base lookup, replace, unload)
        std::unordered_map<std::string, size_t>                                mTemplateBuffers;

        TSceneTreeExact *    pSceneTree { nullptr };

        size_t AllocateBuffer(std::vector<uint8_t> vData, size_t raw_size);
        void   RetireBuffer(size_t idx);

        ret_code_t RegisterBuffer(std::vector<uint8_t> vData,
                        const std::string & sDebugName);

        ret_code_t MergeVariant(const std::string &                    sVariantId,
                        const FlatBuffers::EntityTemplate *    pVariantFB,
                        const FlatBuffers::EntityTemplate *    pBaseFB);

        TSceneNode InstantiateNode(
                        const FlatBuffers::NodeTemplate * pNodeFB,
                        TSceneNode                        pParent,
                        bool                              override_enabled,
                        const glm::vec3 *                 pvOverrideTranslation,
                        const glm::vec3 *                 pvOverrideRotation,
                        const glm::vec3 *                 pvOverrideScale,
                        TPostLoadVec &                    vPostLoad,
                        std::vector<TSceneNode> &         vCreated);
};

// Helper: deduce EntityTemplateSystem<Ts...> from a concrete SceneTree<Ts...> type.
// Forward-declare SceneTree so this header stays self-contained.
template <class...> class SceneTree;

namespace detail {

template <class> struct MakeEntityTemplateSystemT;
template <class... Ts>
struct MakeEntityTemplateSystemT<SceneTree<Ts...>> {
    using Type = EntityTemplateSystem<Ts...>;
};

} // namespace detail

template <class TSceneTreeType> 
using MakeEntityTemplateSystem = typename detail::MakeEntityTemplateSystemT<TSceneTreeType>::Type;

} // namespace SE

#endif
