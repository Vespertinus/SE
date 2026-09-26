#ifndef APP_WORLD_RESOURCE_PINS_H
#define APP_WORLD_RESOURCE_PINS_H 1

// ---------------------------------------------------------------------------
// ResourcePins — declared-dependency pin specializations for CORE resources
// (Mesh, Material, Skeleton, AudioClip, AnimGraph). Each specialization mirrors
// the EXACT CreateResource flavor (and cache key, including sResourceDir
// prefixes and name-vs-path precedence) that the consuming component ctor uses,
// so the pre-pass warms the cache that instantiation then hits.
// See ResourcePinCore.h for the machinery and where to put app-resource pins.
// ---------------------------------------------------------------------------

#include <world/ResourcePinCore.h>

#include <flatbuffers/flatbuffers.h>
#include <Resources_generated.h>

#include <string>

namespace SE {

// StaticModel (+ AnimatedModel) ctor: path flavor first, inline name+data second.
template <> struct TResourcePin<Mesh> {
        using THolder = FlatBuffers::MeshHolder;
        using TKeyHolder = FlatBuffers::MeshHolderT;   // object type: the union-traits key

        static H<Mesh> Create(const THolder * pH) {
                if (pH->path()) {
                        return CreateResource<Mesh>(pH->path()->str());
                }
                if (pH->name() && pH->mesh()) {
                        return CreateResource<Mesh>(pH->name()->str(), pH->mesh());
                }
                return H<Mesh>::Null();
        }
};

// StaticModel (+ AnimatedModel) ctor: material paths are sResourceDir-relative.
template <> struct TResourcePin<Material> {
        using THolder = FlatBuffers::MaterialHolder;
        using TKeyHolder = FlatBuffers::MaterialHolderT;

        static H<Material> Create(const THolder * pH) {
                const std::string & sResourceDir = ResourceDir();

                if (pH->path()) {
                        return CreateResource<Material>(sResourceDir + pH->path()->str());
                }
                if (pH->name() && pH->material()) {
                        return CreateResource<Material>(pH->name()->str(), pH->material());
                }
                return H<Material>::Null();
        }
};

// AnimatedModel ctor: skeleton paths are sResourceDir-relative.
template <> struct TResourcePin<Skeleton> {
        using THolder = FlatBuffers::SkeletonHolder;
        using TKeyHolder = FlatBuffers::SkeletonHolderT;

        static H<Skeleton> Create(const THolder * pH) {
                const std::string & sResourceDir = ResourceDir();

                if (pH->path()) {
                        return CreateResource<Skeleton>(sResourceDir + pH->path()->str());
                }
                if (pH->name() && pH->skeleton()) {
                        return CreateResource<Skeleton>(pH->name()->str(), pH->skeleton());
                }
                return H<Skeleton>::Null();
        }
};

// AudioEmitter ctor.
template <> struct TResourcePin<AudioClip> {
        using THolder = FlatBuffers::AudioClipHolder;
        using TKeyHolder = FlatBuffers::AudioClipHolderT;

        static H<AudioClip> Create(const THolder * pH) {
                if (pH->path()) {
                        return CreateResource<AudioClip>(pH->path()->str());
                }
                if (pH->name() && pH->clip()) {
                        return CreateResource<AudioClip>(pH->name()->str(), pH->clip());
                }
                return H<AudioClip>::Null();
        }
};

// Animator ctor: path-flavored graphs are keyed by name-if-present-else-path.
template <> struct TResourcePin<AnimGraph> {
        using THolder = FlatBuffers::AnimationGraphHolder;
        using TKeyHolder = FlatBuffers::AnimationGraphHolderT;

        static H<AnimGraph> Create(const THolder * pH) {
                const bool has_path = pH->path() && pH->path()->size() > 0;
                const bool has_name = pH->name() && pH->name()->size() > 0;

                if (has_path) {
                        const std::string sKey = has_name ? pH->name()->str() : pH->path()->str();
                        return CreateResource<AnimGraph>(sKey);
                }
                if (has_name && pH->graph()) {
                        return CreateResource<AnimGraph>(pH->name()->str(), pH->graph());
                }

                log_w("ResourcePin<AnimGraph>: anonymous inline animation graph cannot be pinned");
                return H<AnimGraph>::Null();
        }
};


} // namespace SE

#endif
