#include "SceneComposer.h"
#include "Manifest.h"

#include <Logging.h>
#include <flatbuffers/flatbuffers.h>

#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace SE::TOOLS {

// ---------------------------------------------------------------------------
// File-scope helpers
// ---------------------------------------------------------------------------

static std::unique_ptr<SE::FlatBuffers::SceneTreeT> LoadSceneFile(const std::string& path) {

        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs)
                throw std::runtime_error("compose: cannot open scene '" + path + "'");

        const auto file_size = static_cast<std::size_t>(ifs.tellg());
        ifs.seekg(0, std::ios::beg);

        std::vector<uint8_t> buf(file_size);
        ifs.read(reinterpret_cast<char*>(buf.data()), file_size);

        flatbuffers::Verifier verifier(buf.data(), file_size);
        if (!SE::FlatBuffers::VerifySceneTreeBuffer(verifier))
                throw std::runtime_error("compose: bad FlatBuffer in '" + path + "'");

        auto* pRoot = SE::FlatBuffers::GetSceneTree(buf.data());
        return std::unique_ptr<SE::FlatBuffers::SceneTreeT>(pRoot->UnPack());
}

static void PrintNodeT(const SE::FlatBuffers::NodeT& node, int indent) {
        std::string comps;
        for (const auto& pC : node.components)
                if (pC) comps += " [" + std::string(SE::FlatBuffers::EnumNameComponentU(pC->component.type)) + "]";
        log_i("{:>{}}'{}'{}", "", indent * 2, node.name, comps);
        for (const auto& pChild : node.children)
                if (pChild) PrintNodeT(*pChild, indent + 1);
}

// Generic typed component accessor using ComponentUUnionTraits
template<typename TComp>
static TComp* GetTyped(SE::FlatBuffers::ComponentUUnion& u) {
        return u.type == SE::FlatBuffers::ComponentUUnionTraits<TComp>::enum_value
                ? reinterpret_cast<TComp*>(u.value) : nullptr;
}

static SE::FlatBuffers::ComponentT* GetOrAdd(SE::FlatBuffers::NodeT& node, const ComponentDef& def) {

        for (auto& pC : node.components)
                if (pC && pC->component.type == def.type_tag) return pC.get();
        auto pC = std::make_unique<SE::FlatBuffers::ComponentT>();
        def.make(pC->component);
        node.components.push_back(std::move(pC));
        return node.components.back().get();
}

template<typename TComp, typename THolder>
FieldApplier SceneComposer::MakeApplier(std::unique_ptr<THolder> TComp::* member) {
        return [member](SE::FlatBuffers::ComponentT& c, const AssetRef& r, SceneComposer& sc) {
                (GetTyped<TComp>(c.component)->*member) = sc.template Resolve<THolder>(r);
        };
}

template<typename TComp, typename THolder>
ArrayFieldApplier SceneComposer::MakeArrayApplier(
                std::vector<std::unique_ptr<THolder>> TComp::* member) {

        return [member](SE::FlatBuffers::ComponentT& c,
                        const std::vector<AssetRef>& refs, SceneComposer& sc) {
                auto* p = GetTyped<TComp>(c.component);
                (p->*member).clear();
                for (const auto& r : refs) (p->*member).push_back(sc.template Resolve<THolder>(r));
        };
}

// ---------------------------------------------------------------------------
// Resolve<THolder> specializations
// ---------------------------------------------------------------------------

template<>
std::unique_ptr<SE::FlatBuffers::AnimationGraphHolderT>
SceneComposer::Resolve(const AssetRef& ref) {

        if (ref.IsNamed())
                return Resolve<SE::FlatBuffers::AnimationGraphHolderT>(ResolveNamed(ref.named));

        if (ref.IsPath()) {
                auto h = std::make_unique<SE::FlatBuffers::AnimationGraphHolderT>();
                h->path = ref.path;
                return h;
        }

        if (ref.IsSceneRef()) {
                auto& tree = LoadOrGetScene(ref.scene_ref.scene);
                if (!tree.root)
                        throw std::runtime_error("compose: scene '" + ref.scene_ref.scene + "' has no root");
                auto* pNode = FindNode(*tree.root, ref.scene_ref.node);
                if (!pNode)
                        throw std::runtime_error("compose: node '" + ref.scene_ref.node +
                                        "' not found in '" + ref.scene_ref.scene + "'");
                for (auto& pComp : pNode->components) {
                        if (!pComp) continue;
                        if (auto* pAnim = pComp->component.AsAnimator()) {
                                if (pAnim->animation_graph) {
                                        auto holder = std::make_unique<SE::FlatBuffers::AnimationGraphHolderT>(
                                                        *pAnim->animation_graph);
                                        if (holder->name.empty())
                                                holder->name = ref.scene_ref.scene + "|" + ref.scene_ref.node + "|AnimGraph";
                                        return holder;
                                }
                        }
                }
                throw std::runtime_error("compose: no Animator component on node '" +
                                ref.scene_ref.node + "' in '" + ref.scene_ref.scene + "'");
        }

        throw std::runtime_error("compose: empty AnimationGraph asset reference");
}

template<>
std::unique_ptr<SE::FlatBuffers::MeshHolderT>
SceneComposer::Resolve(const AssetRef& ref) {
        if (ref.IsNamed())
                return Resolve<SE::FlatBuffers::MeshHolderT>(ResolveNamed(ref.named));

        if (ref.IsPath()) {
                auto h = std::make_unique<SE::FlatBuffers::MeshHolderT>();
                h->path = ref.path;
                return h;
        }

        if (ref.IsSceneRef()) {
                auto& tree = LoadOrGetScene(ref.scene_ref.scene);
                if (!tree.root) throw std::runtime_error("compose: scene has no root");
                auto* pNode = FindNode(*tree.root, ref.scene_ref.node);
                if (!pNode) throw std::runtime_error("compose: node not found: " + ref.scene_ref.node);
                for (auto& pComp : pNode->components) {
                        if (!pComp) continue;
                        if (auto* p = pComp->component.AsAnimatedModel()) {
                                if (p->mesh) {
                                        auto holder = std::make_unique<SE::FlatBuffers::MeshHolderT>(*p->mesh);
                                        if (holder->name.empty())
                                                holder->name = ref.scene_ref.scene + "|" + ref.scene_ref.node + "|Mesh";
                                        return holder;
                                }
                        }
                        if (auto* p = pComp->component.AsStaticModel()) {
                                if (p->mesh) {
                                        auto holder = std::make_unique<SE::FlatBuffers::MeshHolderT>(*p->mesh);
                                        if (holder->name.empty())
                                                holder->name = ref.scene_ref.scene + "|" + ref.scene_ref.node + "|Mesh";
                                        return holder;
                                }
                        }
                }
                throw std::runtime_error("compose: no mesh component on node: " + ref.scene_ref.node);
        }

        throw std::runtime_error("compose: empty Mesh asset reference");
}

template<>
std::unique_ptr<SE::FlatBuffers::SkeletonHolderT>
SceneComposer::Resolve(const AssetRef& ref) {
        if (ref.IsNamed())
                return Resolve<SE::FlatBuffers::SkeletonHolderT>(ResolveNamed(ref.named));

        if (ref.IsPath()) {
                auto h = std::make_unique<SE::FlatBuffers::SkeletonHolderT>();
                h->path = ref.path;
                return h;
        }

        if (ref.IsSceneRef()) {
                auto& tree = LoadOrGetScene(ref.scene_ref.scene);
                if (!tree.root) throw std::runtime_error("compose: scene has no root");
                auto* pNode = FindNode(*tree.root, ref.scene_ref.node);
                if (!pNode) throw std::runtime_error("compose: node not found: " + ref.scene_ref.node);
                for (auto& pComp : pNode->components) {
                        if (!pComp) continue;
                        if (auto* p = pComp->component.AsAnimatedModel()) {
                                if (p->skeleton) {
                                        auto holder = std::make_unique<SE::FlatBuffers::SkeletonHolderT>(*p->skeleton);
                                        if (holder->name.empty())
                                                holder->name = ref.scene_ref.scene + "|" + ref.scene_ref.node + "|Skeleton";
                                        return holder;
                                }
                        }
                }
                throw std::runtime_error("compose: no AnimatedModel skeleton on node: " + ref.scene_ref.node);
        }

        throw std::runtime_error("compose: empty Skeleton asset reference");
}

template<>
std::unique_ptr<SE::FlatBuffers::MaterialHolderT>
SceneComposer::Resolve(const AssetRef& ref) {
        if (ref.IsNamed())
                return Resolve<SE::FlatBuffers::MaterialHolderT>(ResolveNamed(ref.named));
        if (ref.IsPath()) {
                auto h = std::make_unique<SE::FlatBuffers::MaterialHolderT>();
                h->path = ref.path;
                return h;
        }
        throw std::runtime_error("compose: MaterialHolder ref must be a path or @name");
}

template<>
std::unique_ptr<SE::FlatBuffers::TextureHolderT>
SceneComposer::Resolve(const AssetRef& ref) {
        if (ref.IsNamed())
                return Resolve<SE::FlatBuffers::TextureHolderT>(ResolveNamed(ref.named));
        if (ref.IsPath()) {
                auto h = std::make_unique<SE::FlatBuffers::TextureHolderT>();
                h->path = ref.path;
                return h;
        }
        throw std::runtime_error("compose: TextureHolder ref must be a path or @name");
}

template<>
std::unique_ptr<SE::FlatBuffers::AudioClipHolderT>
SceneComposer::Resolve(const AssetRef& ref) {
        if (ref.IsNamed())
                return Resolve<SE::FlatBuffers::AudioClipHolderT>(ResolveNamed(ref.named));
        if (ref.IsPath()) {
                auto h = std::make_unique<SE::FlatBuffers::AudioClipHolderT>();
                h->path = ref.path;
                return h;
        }
        throw std::runtime_error("compose: AudioClipHolder ref must be a path or @name");
}

template<>
std::unique_ptr<SE::FlatBuffers::StateMachineHolderT>
SceneComposer::Resolve(const AssetRef& ref) {
        if (ref.IsNamed())
                return Resolve<SE::FlatBuffers::StateMachineHolderT>(ResolveNamed(ref.named));
        if (ref.IsPath()) {
                auto h = std::make_unique<SE::FlatBuffers::StateMachineHolderT>();
                h->path = ref.path;
                return h;
        }
        throw std::runtime_error("compose: StateMachineHolder ref must be a path or @name");
}

// ---------------------------------------------------------------------------
// Component registry
// ---------------------------------------------------------------------------

const std::unordered_map<std::string, ComponentDef>& SceneComposer::GetRegistry() {
        using namespace SE::FlatBuffers;
        static const auto s_reg = []() {
                std::unordered_map<std::string, ComponentDef> reg;
                auto add = [&reg](ComponentDef def) {
                        reg[EnumNameComponentU(def.type_tag)] = std::move(def);
                };

                // StaticModel
                { ComponentDef d;
                        d.type_tag = ComponentUUnionTraits<StaticModelT>::enum_value;
                        d.make = [](auto& u) { u.Set(StaticModelT{}); };
                        d.fields["mesh"]            = MakeApplier<StaticModelT, MeshHolderT>(&StaticModelT::mesh);
                        d.array_fields["materials"] = MakeArrayApplier<StaticModelT, MaterialHolderT>(&StaticModelT::materials);
                        add(std::move(d)); }

                // AnimatedModel
                { ComponentDef d;
                        d.type_tag = ComponentUUnionTraits<AnimatedModelT>::enum_value;
                        d.make = [](auto& u) { u.Set(AnimatedModelT{}); };
                        d.fields["mesh"]            = MakeApplier<AnimatedModelT, MeshHolderT>(&AnimatedModelT::mesh);
                        d.fields["skeleton"]        = MakeApplier<AnimatedModelT, SkeletonHolderT>(&AnimatedModelT::skeleton);
                        d.fields["blendshapes"]     = MakeApplier<AnimatedModelT, TextureHolderT>(&AnimatedModelT::blendshapes);
                        d.array_fields["materials"] = MakeArrayApplier<AnimatedModelT, MaterialHolderT>(&AnimatedModelT::materials);
                        add(std::move(d)); }

                // AudioEmitter
                { ComponentDef d;
                        d.type_tag = ComponentUUnionTraits<AudioEmitterT>::enum_value;
                        d.make = [](auto& u) { u.Set(AudioEmitterT{}); };
                        d.fields["clip"] = MakeApplier<AudioEmitterT, AudioClipHolderT>(&AudioEmitterT::clip);
                        add(std::move(d)); }

                // Animator
                { ComponentDef d;
                        d.type_tag = ComponentUUnionTraits<AnimatorT>::enum_value;
                        d.make = [](auto& u) { u.Set(AnimatorT{}); };
                        d.fields["animation_graph"] = MakeApplier<AnimatorT, AnimationGraphHolderT>(&AnimatorT::animation_graph);
                        add(std::move(d)); }

                // StateMachine
                { ComponentDef d;
                        d.type_tag = ComponentUUnionTraits<StateMachineT>::enum_value;
                        d.make = [](auto& u) { u.Set(StateMachineT{}); };
                        d.fields["hms"] = MakeApplier<StateMachineT, StateMachineHolderT>(&StateMachineT::hms);
                        add(std::move(d)); }

                return reg;
        }();
        return s_reg;
}

// ---------------------------------------------------------------------------
// SceneComposer
// ---------------------------------------------------------------------------

SceneComposer::SceneComposer(const Manifest& manifest, bool print_scenes)
        : m_manifest(manifest), m_print_scenes(print_scenes) {}

        SE::FlatBuffers::SceneTreeT&
        SceneComposer::LoadOrGetScene(const std::string& path) {
                auto it = m_scene_cache.find(path);
                if (it != m_scene_cache.end())
                        return *it->second;
                auto tree = LoadSceneFile(path);
                auto* pRaw = tree.get();
                m_scene_cache[path] = std::move(tree);
                if (m_print_scenes)
                        PrintSceneTree(*pRaw, path);
                return *pRaw;
        }

void SceneComposer::PrintSceneTree(const SE::FlatBuffers::SceneTreeT& tree,
                const std::string& path) const {
        log_i("compose: scene '{}':", path);
        if (tree.root) PrintNodeT(*tree.root, 1);
        else           log_i("  (no root)");
}

SE::FlatBuffers::NodeT*
SceneComposer::FindNode(SE::FlatBuffers::NodeT& root, const std::string& slash_path) {
        if (slash_path.empty() || slash_path == root.name)
                return &root;

        // Strip leading segment if it matches root name
        std::string remaining = slash_path;
        const auto sep = slash_path.find('|');
        if (sep != std::string::npos) {
                const std::string first = slash_path.substr(0, sep);
                if (first == root.name)
                        remaining = slash_path.substr(sep + 1);
        }

        // Descend into children
        const auto next_sep = remaining.find('|');
        const std::string head = (next_sep == std::string::npos)
                ? remaining
                : remaining.substr(0, next_sep);
        const std::string tail = (next_sep == std::string::npos)
                ? ""
                : remaining.substr(next_sep + 1);

        for (auto& pChild : root.children) {
                if (pChild && pChild->name == head) {
                        if (tail.empty()) return pChild.get();
                        return FindNode(*pChild, tail);
                }
        }
        return nullptr;
}

// ---------------------------------------------------------------------------
// Asset resolution
// ---------------------------------------------------------------------------

const AssetRef& SceneComposer::ResolveNamed(const std::string& name) const {
        auto it = m_manifest.assets.find(name);
        if (it == m_manifest.assets.end())
                throw std::runtime_error("compose: unknown named asset '@" + name + "'");
        return it->second;
}

// ---------------------------------------------------------------------------
// Component manipulation
// ---------------------------------------------------------------------------

void SceneComposer::RemoveComponent(SE::FlatBuffers::NodeT& node,
                const std::string& comp_type) {
        const auto& reg = GetRegistry();
        auto it = reg.find(comp_type);
        if (it == reg.end())
                throw std::runtime_error("compose: unknown component type '" + comp_type + "' in remove_components");
        const auto tag = it->second.type_tag;
        auto& v = node.components;
        v.erase(std::remove_if(v.begin(), v.end(),
                                [tag](const std::unique_ptr<SE::FlatBuffers::ComponentT>& p) {
                                return p && p->component.type == tag;
                                }), v.end());
}

void SceneComposer::ApplyComponentSpec(SE::FlatBuffers::NodeT& node,
                const std::string&      comp_type,
                const ComponentSpec&    spec) {
        const auto& reg = GetRegistry();
        auto it = reg.find(comp_type);
        if (it == reg.end())
                throw std::runtime_error("compose: unsupported component type '" + comp_type + "'");
        const auto& def = it->second;
        auto* pComp = GetOrAdd(node, def);
        for (const auto& [field, ref] : spec.fields) {
                auto fit = def.fields.find(field);
                if (fit == def.fields.end())
                        throw std::runtime_error("compose: unknown field '" + field +
                                        "' for component '" + comp_type + "'");
                fit->second(*pComp, ref, *this);
        }
        for (const auto& [field, refs] : spec.array_fields) {
                auto fit = def.array_fields.find(field);
                if (fit == def.array_fields.end())
                        throw std::runtime_error("compose: unknown array field '" + field +
                                        "' for component '" + comp_type + "'");
                fit->second(*pComp, refs, *this);
        }
}

// ---------------------------------------------------------------------------
// Node building
// ---------------------------------------------------------------------------

std::unique_ptr<SE::FlatBuffers::NodeT>
SceneComposer::BuildNode(const NodeSpec& spec) {
        std::unique_ptr<SE::FlatBuffers::NodeT> pNode;

        if (!spec.from.IsEmpty()) {
                const AssetRef& from = spec.from;
                const AssetRef& resolved = from.IsNamed() ? ResolveNamed(from.named) : from;

                if (!resolved.IsSceneRef())
                        throw std::runtime_error("compose: 'from' must be a scene reference");

                auto& tree = LoadOrGetScene(resolved.scene_ref.scene);
                if (!tree.root)
                        throw std::runtime_error("compose: source scene '" +
                                        resolved.scene_ref.scene + "' has no root");

                auto* pSrc = FindNode(*tree.root, resolved.scene_ref.node);
                if (!pSrc)
                        throw std::runtime_error("compose: 'from' node '" + resolved.scene_ref.node +
                                        "' not found in '" + resolved.scene_ref.scene + "'");
                pNode = std::make_unique<SE::FlatBuffers::NodeT>(*pSrc);
        } else {
                pNode = std::make_unique<SE::FlatBuffers::NodeT>();
                pNode->translation = std::make_unique<SE::FlatBuffers::Vec3>(0.f, 0.f, 0.f);
                pNode->rotation    = std::make_unique<SE::FlatBuffers::Vec3>(0.f, 0.f, 0.f);
                pNode->scale       = std::make_unique<SE::FlatBuffers::Vec3>(1.f, 1.f, 1.f);
        }

        if (!spec.name.empty())
                pNode->name = spec.name;

        if (spec.translation) {
                pNode->translation = std::make_unique<SE::FlatBuffers::Vec3>(
                                (*spec.translation)[0], (*spec.translation)[1], (*spec.translation)[2]);
        }
        if (spec.rotation) {
                pNode->rotation = std::make_unique<SE::FlatBuffers::Vec3>(
                                (*spec.rotation)[0], (*spec.rotation)[1], (*spec.rotation)[2]);
        }
        if (spec.scale) {
                pNode->scale = std::make_unique<SE::FlatBuffers::Vec3>(
                                (*spec.scale)[0], (*spec.scale)[1], (*spec.scale)[2]);
        }

        for (const auto& rc : spec.remove_components)
                RemoveComponent(*pNode, rc);

        for (const auto& [comp_type, comp_spec] : spec.components)
                ApplyComponentSpec(*pNode, comp_type, comp_spec);

        for (const auto& child_spec : spec.children)
                pNode->children.push_back(BuildNode(child_spec));

        return pNode;
}

// ---------------------------------------------------------------------------
// HolderDeduplicator + custom Pack (Fix 3)
// ---------------------------------------------------------------------------

struct HolderDeduplicator {

        flatbuffers::FlatBufferBuilder& fbb;

        std::unordered_map<std::string, flatbuffers::Offset<SE::FlatBuffers::AnimationGraphHolder>> m_anim_graphs;
        std::unordered_map<std::string, flatbuffers::Offset<SE::FlatBuffers::MeshHolder>>           m_meshes;
        std::unordered_map<std::string, flatbuffers::Offset<SE::FlatBuffers::SkeletonHolder>>       m_skeletons;
        std::unordered_map<std::string, flatbuffers::Offset<SE::FlatBuffers::MaterialHolder>>       m_materials;
        std::unordered_map<std::string, flatbuffers::Offset<SE::FlatBuffers::TextureHolder>>        m_textures;
        std::unordered_map<std::string, flatbuffers::Offset<SE::FlatBuffers::AudioClipHolder>>      m_audio_clips;
        std::unordered_map<std::string, flatbuffers::Offset<SE::FlatBuffers::StateMachineHolder>>   m_state_machines;

        explicit HolderDeduplicator(flatbuffers::FlatBufferBuilder& b) : fbb(b) {}

        template<typename THolder, typename TFbHolder>
                flatbuffers::Offset<TFbHolder> Lookup(
                                const THolder* ptr,
                                std::unordered_map<std::string, flatbuffers::Offset<TFbHolder>>& cache)
                {
                        if (!ptr) return 0;
                        if (!ptr->name.empty()) {
                                auto it = cache.find(ptr->name);
                                if (it != cache.end()) return it->second;
                        }
                        if (!ptr->path.empty()) {
                                auto it = cache.find(ptr->path);
                                if (it != cache.end()) return it->second;
                        }
                        auto off = TFbHolder::Pack(fbb, ptr);
                        if (!ptr->name.empty()) cache[ptr->name] = off;
                        if (!ptr->path.empty()) cache[ptr->path] = off;
                        return off;
                }

        flatbuffers::Offset<SE::FlatBuffers::AnimationGraphHolder>
                GetAnimGraph(const SE::FlatBuffers::AnimationGraphHolderT* p) { return Lookup(p, m_anim_graphs); }

        flatbuffers::Offset<SE::FlatBuffers::MeshHolder>
                GetMesh(const SE::FlatBuffers::MeshHolderT* p)                { return Lookup(p, m_meshes); }

        flatbuffers::Offset<SE::FlatBuffers::SkeletonHolder>
                GetSkeleton(const SE::FlatBuffers::SkeletonHolderT* p)        { return Lookup(p, m_skeletons); }

        flatbuffers::Offset<SE::FlatBuffers::MaterialHolder>
                GetMaterial(const SE::FlatBuffers::MaterialHolderT* p)        { return Lookup(p, m_materials); }

        flatbuffers::Offset<SE::FlatBuffers::TextureHolder>
                GetTexture(const SE::FlatBuffers::TextureHolderT* p)          { return Lookup(p, m_textures); }

        flatbuffers::Offset<SE::FlatBuffers::AudioClipHolder>
                GetAudioClip(const SE::FlatBuffers::AudioClipHolderT* p)      { return Lookup(p, m_audio_clips); }

        flatbuffers::Offset<SE::FlatBuffers::StateMachineHolder>
                GetStateMachine(const SE::FlatBuffers::StateMachineHolderT* p){ return Lookup(p, m_state_machines); }
};

static flatbuffers::Offset<SE::FlatBuffers::Component>
PackComponent(flatbuffers::FlatBufferBuilder& fbb,
                const SE::FlatBuffers::ComponentT& comp,
                HolderDeduplicator& dedup) {
        using namespace SE::FlatBuffers;
        switch (comp.component.type) {
                case ComponentU::StaticModel: {
                                                      const auto* sm = comp.component.AsStaticModel();
                                                      auto mesh_off = dedup.GetMesh(sm->mesh.get());
                                                      std::vector<flatbuffers::Offset<MaterialHolder>> mats;
                                                      mats.reserve(sm->materials.size());
                                                      for (const auto& pMat : sm->materials)
                                                              mats.push_back(dedup.GetMaterial(pMat.get()));
                                                      auto mats_vec = mats.empty() ? 0 : fbb.CreateVector(mats);
                                                      auto sm_off   = CreateStaticModel(fbb, mesh_off, mats_vec);
                                                      return CreateComponent(fbb, ComponentU::StaticModel, sm_off.Union());
                                              }
                case ComponentU::AnimatedModel: {
                                                        const auto* am = comp.component.AsAnimatedModel();
                                                        auto mesh_off      = dedup.GetMesh(am->mesh.get());
                                                        std::vector<flatbuffers::Offset<MaterialHolder>> mats;
                                                        mats.reserve(am->materials.size());
                                                        for (const auto& pMat : am->materials)
                                                                mats.push_back(dedup.GetMaterial(pMat.get()));
                                                        auto mats_vec      = mats.empty() ? 0 : fbb.CreateVector(mats);
                                                        auto blend_off     = dedup.GetTexture(am->blendshapes.get());
                                                        auto bw_off        = am->blendshapes_weights.empty() ? 0 : fbb.CreateVector(am->blendshapes_weights);
                                                        auto skel_off      = dedup.GetSkeleton(am->skeleton.get());
                                                        auto skel_root_off = am->skeleton_root_node.empty() ? 0 : fbb.CreateString(am->skeleton_root_node);
                                                        auto ji_off        = am->joints_indexes.empty() ? 0 : fbb.CreateVector(am->joints_indexes);

                                                        std::vector<flatbuffers::Offset<BindSQT>> jibp_offs;
                                                        jibp_offs.reserve(am->joints_inv_bind_pose.size());
                                                        for (const auto& pBind : am->joints_inv_bind_pose)
                                                                jibp_offs.push_back(BindSQT::Pack(fbb, pBind.get()));
                                                        auto jibp_off = jibp_offs.empty() ? 0 : fbb.CreateVector(jibp_offs);

                                                        auto mbp_off = am->mesh_bind_pos ? BindSQT::Pack(fbb, am->mesh_bind_pos.get()) : flatbuffers::Offset<BindSQT>(0);

                                                        auto am_off = CreateAnimatedModel(fbb,
                                                                        mesh_off, mats_vec, blend_off, bw_off,
                                                                        skel_off, skel_root_off,
                                                                        ji_off, jibp_off, mbp_off);
                                                        return CreateComponent(fbb, ComponentU::AnimatedModel, am_off.Union());
                                                }
                case ComponentU::AudioEmitter: {
                                                       const auto* ae = comp.component.AsAudioEmitter();
                                                       auto clip_off  = dedup.GetAudioClip(ae->clip.get());
                                                       auto ae_off    = CreateAudioEmitter(fbb, clip_off,
                                                                       ae->loop, ae->spatial, ae->auto_play,
                                                                       ae->gain, ae->pitch,
                                                                       ae->ref_dist, ae->max_dist, ae->rolloff, ae->bus);
                                                       return CreateComponent(fbb, ComponentU::AudioEmitter, ae_off.Union());
                                               }
                case ComponentU::Animator: {
                                                   const auto* an = comp.component.AsAnimator();
                                                   auto graph_off = dedup.GetAnimGraph(an->animation_graph.get());
                                                   auto an_off    = CreateAnimator(fbb, graph_off);
                                                   return CreateComponent(fbb, ComponentU::Animator, an_off.Union());
                                           }
                case ComponentU::StateMachine: {
                                                       const auto* sm = comp.component.AsStateMachine();
                                                       auto hms_off   = dedup.GetStateMachine(sm->hms.get());
                                                       auto sm_off    = CreateStateMachine(fbb, hms_off, sm->tick_interval);
                                                       return CreateComponent(fbb, ComponentU::StateMachine, sm_off.Union());
                                               }
                default:
                                               return Component::Pack(fbb, &comp);
        }
}

static flatbuffers::Offset<SE::FlatBuffers::Node>
PackNode(flatbuffers::FlatBufferBuilder& fbb,
                const SE::FlatBuffers::NodeT& node,
                HolderDeduplicator& dedup) {
        using namespace SE::FlatBuffers;

        auto name_off = fbb.CreateString(node.name);

        const Vec3* trans = node.translation.get();
        const Vec3* rot   = node.rotation.get();
        const Vec3* scale = node.scale.get();

        std::vector<flatbuffers::Offset<Component>> comp_offsets;
        comp_offsets.reserve(node.components.size());
        for (const auto& pC : node.components)
                if (pC) comp_offsets.push_back(PackComponent(fbb, *pC, dedup));
        auto comps_vec = comp_offsets.empty() ? 0 : fbb.CreateVector(comp_offsets);

        std::vector<flatbuffers::Offset<Node>> child_offsets;
        child_offsets.reserve(node.children.size());
        for (const auto& pChild : node.children)
                if (pChild) child_offsets.push_back(PackNode(fbb, *pChild, dedup));
        auto children_vec = child_offsets.empty() ? 0 : fbb.CreateVector(child_offsets);

        auto info_off = node.info.empty() ? 0 : fbb.CreateString(node.info);

        return CreateNode(fbb, name_off, trans, rot, scale, comps_vec, children_vec, info_off, node.enabled);
}

static int CountNodes(const SE::FlatBuffers::NodeT& node) {
        int n = 1;
        for (const auto& pChild : node.children)
                if (pChild) n += CountNodes(*pChild);
        return n;
}

static flatbuffers::Offset<SE::FlatBuffers::SceneTree> PackSceneTree(
                flatbuffers::FlatBufferBuilder& fbb,
                const SE::FlatBuffers::SceneTreeT& tree) {

        HolderDeduplicator dedup(fbb);
        auto root_off = tree.root ? PackNode(fbb, *tree.root, dedup) : 0;

        const int node_count = tree.root ? CountNodes(*tree.root) : 0;
        log_d("packed {} nodes; dedup cache sizes — "
                        "anim_graphs={}, meshes={}, skeletons={}, materials={}, "
                        "textures={}, audio_clips={}, state_machines={}",
                        node_count,
                        dedup.m_anim_graphs.size(),
                        dedup.m_meshes.size(),
                        dedup.m_skeletons.size(),
                        dedup.m_materials.size(),
                        dedup.m_textures.size(),
                        dedup.m_audio_clips.size(),
                        dedup.m_state_machines.size());

        return SE::FlatBuffers::CreateSceneTree(fbb, root_off);
}

// ---------------------------------------------------------------------------
// Compose (Fix 2: build nodes before applying overrides)
// ---------------------------------------------------------------------------

bool SceneComposer::Compose() {

        try {
                std::unique_ptr<SE::FlatBuffers::SceneTreeT> oTree;

                if (!m_manifest.base_scene.empty()) {
                        auto& src = LoadOrGetScene(m_manifest.base_scene);
                        oTree = std::make_unique<SE::FlatBuffers::SceneTreeT>(src);
                } else {
                        oTree = std::make_unique<SE::FlatBuffers::SceneTreeT>();
                }

                // Build nodes section first so overrides can target them
                if (!m_manifest.nodes.empty()) {
                        if (!oTree->root) {
                                if (m_manifest.nodes.size() == 1) {
                                        oTree->root = BuildNode(m_manifest.nodes.front());
                                } else {
                                        auto pRoot = std::make_unique<SE::FlatBuffers::NodeT>();
                                        pRoot->name = "Root";
                                        for (const auto& spec : m_manifest.nodes)
                                                pRoot->children.push_back(BuildNode(spec));
                                        oTree->root = std::move(pRoot);
                                }
                        } else {
                                for (const auto& spec : m_manifest.nodes)
                                        oTree->root->children.push_back(BuildNode(spec));
                        }
                }

                // Apply overrides after nodes are built
                if (!m_manifest.overrides.empty() && !oTree->root)
                        throw std::runtime_error("compose: 'overrides' specified but scene has no root");
                for (const auto& ov : m_manifest.overrides) {
                        auto* pNode = FindNode(*oTree->root, ov.node);
                        if (!pNode)
                                throw std::runtime_error("compose: override node '" + ov.node +
                                                "' not found in output scene");
                        for (const auto& [comp_type, comp_spec] : ov.components)
                                ApplyComponentSpec(*pNode, comp_type, comp_spec);
                        for (const auto& rc : ov.remove_components)
                                RemoveComponent(*pNode, rc);
                }

                if (!oTree->root)
                        throw std::runtime_error("compose: output scene has no root node");

                flatbuffers::FlatBufferBuilder oBuilder(1 << 20);
                auto tree_offset = PackSceneTree(oBuilder, *oTree);
                SE::FlatBuffers::FinishSceneTreeBuffer(oBuilder, tree_offset);

                const uint8_t* pBuf  = oBuilder.GetBufferPointer();
                const size_t   nSize = oBuilder.GetSize();

                std::ofstream ofs(m_manifest.output, std::ios::binary);
                if (!ofs) {
                        log_e("cannot write output '{}'", m_manifest.output);
                        return false;
                }
                ofs.write(reinterpret_cast<const char*>(pBuf), nSize);
                log_i("wrote '{}' ({} bytes)", m_manifest.output, nSize);
                return true;
        }
        catch (const std::exception& e) {
                log_e("got exception: {}", e.what());
                return false;
        }
}

} // namespace SE::TOOLS
