// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_SCENETREE_SE_FLATBUFFERS_H_
#define FLATBUFFERS_GENERATED_SCENETREE_SE_FLATBUFFERS_H_

#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flexbuffers.h"

#include "Mesh_generated.h"
#include "Component_generated.h"
#include "Material_generated.h"
#include "Common_generated.h"
#include "ShaderComponent_generated.h"
#include "ShaderProgram_generated.h"

namespace SE {
namespace FlatBuffers {

struct Node;
struct NodeBuilder;

struct SceneTree;
struct SceneTreeBuilder;

struct Node FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef NodeBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_NAME = 4,
    VT_TRANSLATION = 6,
    VT_ROTATION = 8,
    VT_SCALE = 10,
    VT_COMPONENTS = 12,
    VT_CHILDREN = 14,
    VT_INFO = 16,
    VT_ENABLED = 18
  };
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  const SE::FlatBuffers::Vec3 *translation() const {
    return GetStruct<const SE::FlatBuffers::Vec3 *>(VT_TRANSLATION);
  }
  const SE::FlatBuffers::Vec3 *rotation() const {
    return GetStruct<const SE::FlatBuffers::Vec3 *>(VT_ROTATION);
  }
  const SE::FlatBuffers::Vec3 *scale() const {
    return GetStruct<const SE::FlatBuffers::Vec3 *>(VT_SCALE);
  }
  const flatbuffers::Vector<flatbuffers::Offset<SE::FlatBuffers::Component>> *components() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<SE::FlatBuffers::Component>> *>(VT_COMPONENTS);
  }
  const flatbuffers::Vector<flatbuffers::Offset<SE::FlatBuffers::Node>> *children() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<SE::FlatBuffers::Node>> *>(VT_CHILDREN);
  }
  const flatbuffers::String *info() const {
    return GetPointer<const flatbuffers::String *>(VT_INFO);
  }
  bool enabled() const {
    return GetField<uint8_t>(VT_ENABLED, 1) != 0;
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_NAME) &&
           verifier.VerifyString(name()) &&
           VerifyField<SE::FlatBuffers::Vec3>(verifier, VT_TRANSLATION) &&
           VerifyField<SE::FlatBuffers::Vec3>(verifier, VT_ROTATION) &&
           VerifyField<SE::FlatBuffers::Vec3>(verifier, VT_SCALE) &&
           VerifyOffset(verifier, VT_COMPONENTS) &&
           verifier.VerifyVector(components()) &&
           verifier.VerifyVectorOfTables(components()) &&
           VerifyOffset(verifier, VT_CHILDREN) &&
           verifier.VerifyVector(children()) &&
           verifier.VerifyVectorOfTables(children()) &&
           VerifyOffset(verifier, VT_INFO) &&
           verifier.VerifyString(info()) &&
           VerifyField<uint8_t>(verifier, VT_ENABLED) &&
           verifier.EndTable();
  }
};

struct NodeBuilder {
  typedef Node Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(Node::VT_NAME, name);
  }
  void add_translation(const SE::FlatBuffers::Vec3 *translation) {
    fbb_.AddStruct(Node::VT_TRANSLATION, translation);
  }
  void add_rotation(const SE::FlatBuffers::Vec3 *rotation) {
    fbb_.AddStruct(Node::VT_ROTATION, rotation);
  }
  void add_scale(const SE::FlatBuffers::Vec3 *scale) {
    fbb_.AddStruct(Node::VT_SCALE, scale);
  }
  void add_components(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<SE::FlatBuffers::Component>>> components) {
    fbb_.AddOffset(Node::VT_COMPONENTS, components);
  }
  void add_children(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<SE::FlatBuffers::Node>>> children) {
    fbb_.AddOffset(Node::VT_CHILDREN, children);
  }
  void add_info(flatbuffers::Offset<flatbuffers::String> info) {
    fbb_.AddOffset(Node::VT_INFO, info);
  }
  void add_enabled(bool enabled) {
    fbb_.AddElement<uint8_t>(Node::VT_ENABLED, static_cast<uint8_t>(enabled), 1);
  }
  explicit NodeBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<Node> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<Node>(end);
    fbb_.Required(o, Node::VT_NAME);
    return o;
  }
};

inline flatbuffers::Offset<Node> CreateNode(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    const SE::FlatBuffers::Vec3 *translation = nullptr,
    const SE::FlatBuffers::Vec3 *rotation = nullptr,
    const SE::FlatBuffers::Vec3 *scale = nullptr,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<SE::FlatBuffers::Component>>> components = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<SE::FlatBuffers::Node>>> children = 0,
    flatbuffers::Offset<flatbuffers::String> info = 0,
    bool enabled = true) {
  NodeBuilder builder_(_fbb);
  builder_.add_info(info);
  builder_.add_children(children);
  builder_.add_components(components);
  builder_.add_scale(scale);
  builder_.add_rotation(rotation);
  builder_.add_translation(translation);
  builder_.add_name(name);
  builder_.add_enabled(enabled);
  return builder_.Finish();
}

inline flatbuffers::Offset<Node> CreateNodeDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *name = nullptr,
    const SE::FlatBuffers::Vec3 *translation = nullptr,
    const SE::FlatBuffers::Vec3 *rotation = nullptr,
    const SE::FlatBuffers::Vec3 *scale = nullptr,
    const std::vector<flatbuffers::Offset<SE::FlatBuffers::Component>> *components = nullptr,
    const std::vector<flatbuffers::Offset<SE::FlatBuffers::Node>> *children = nullptr,
    const char *info = nullptr,
    bool enabled = true) {
  auto name__ = name ? _fbb.CreateString(name) : 0;
  auto components__ = components ? _fbb.CreateVector<flatbuffers::Offset<SE::FlatBuffers::Component>>(*components) : 0;
  auto children__ = children ? _fbb.CreateVector<flatbuffers::Offset<SE::FlatBuffers::Node>>(*children) : 0;
  auto info__ = info ? _fbb.CreateString(info) : 0;
  return SE::FlatBuffers::CreateNode(
      _fbb,
      name__,
      translation,
      rotation,
      scale,
      components__,
      children__,
      info__,
      enabled);
}

struct SceneTree FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef SceneTreeBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_ROOT = 4
  };
  const SE::FlatBuffers::Node *root() const {
    return GetPointer<const SE::FlatBuffers::Node *>(VT_ROOT);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_ROOT) &&
           verifier.VerifyTable(root()) &&
           verifier.EndTable();
  }
};

struct SceneTreeBuilder {
  typedef SceneTree Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_root(flatbuffers::Offset<SE::FlatBuffers::Node> root) {
    fbb_.AddOffset(SceneTree::VT_ROOT, root);
  }
  explicit SceneTreeBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<SceneTree> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<SceneTree>(end);
    fbb_.Required(o, SceneTree::VT_ROOT);
    return o;
  }
};

inline flatbuffers::Offset<SceneTree> CreateSceneTree(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<SE::FlatBuffers::Node> root = 0) {
  SceneTreeBuilder builder_(_fbb);
  builder_.add_root(root);
  return builder_.Finish();
}

inline const SE::FlatBuffers::SceneTree *GetSceneTree(const void *buf) {
  return flatbuffers::GetRoot<SE::FlatBuffers::SceneTree>(buf);
}

inline const SE::FlatBuffers::SceneTree *GetSizePrefixedSceneTree(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<SE::FlatBuffers::SceneTree>(buf);
}

inline const char *SceneTreeIdentifier() {
  return "SESC";
}

inline bool SceneTreeBufferHasIdentifier(const void *buf) {
  return flatbuffers::BufferHasIdentifier(
      buf, SceneTreeIdentifier());
}

inline bool VerifySceneTreeBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<SE::FlatBuffers::SceneTree>(SceneTreeIdentifier());
}

inline bool VerifySizePrefixedSceneTreeBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<SE::FlatBuffers::SceneTree>(SceneTreeIdentifier());
}

inline const char *SceneTreeExtension() {
  return "sesc";
}

inline void FinishSceneTreeBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::SceneTree> root) {
  fbb.Finish(root, SceneTreeIdentifier());
}

inline void FinishSizePrefixedSceneTreeBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::SceneTree> root) {
  fbb.FinishSizePrefixed(root, SceneTreeIdentifier());
}

}  // namespace FlatBuffers
}  // namespace SE

#endif  // FLATBUFFERS_GENERATED_SCENETREE_SE_FLATBUFFERS_H_
