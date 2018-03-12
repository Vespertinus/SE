// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_SCENETREE_SE_FLATBUFFERS_H_
#define FLATBUFFERS_GENERATED_SCENETREE_SE_FLATBUFFERS_H_

#include "flatbuffers/flatbuffers.h"

#include "Mesh_generated.h"

namespace SE {
namespace FlatBuffers {

struct Entity;

struct Node;

struct Entity FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_NAME = 4,
    VT_DATA = 6
  };
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  const Mesh *data() const {
    return GetPointer<const Mesh *>(VT_DATA);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_NAME) &&
           verifier.Verify(name()) &&
           VerifyOffsetRequired(verifier, VT_DATA) &&
           verifier.VerifyTable(data()) &&
           verifier.EndTable();
  }
};

struct EntityBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(Entity::VT_NAME, name);
  }
  void add_data(flatbuffers::Offset<Mesh> data) {
    fbb_.AddOffset(Entity::VT_DATA, data);
  }
  explicit EntityBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  EntityBuilder &operator=(const EntityBuilder &);
  flatbuffers::Offset<Entity> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<Entity>(end);
    fbb_.Required(o, Entity::VT_NAME);
    fbb_.Required(o, Entity::VT_DATA);
    return o;
  }
};

inline flatbuffers::Offset<Entity> CreateEntity(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    flatbuffers::Offset<Mesh> data = 0) {
  EntityBuilder builder_(_fbb);
  builder_.add_data(data);
  builder_.add_name(name);
  return builder_.Finish();
}

inline flatbuffers::Offset<Entity> CreateEntityDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *name = nullptr,
    flatbuffers::Offset<Mesh> data = 0) {
  return SE::FlatBuffers::CreateEntity(
      _fbb,
      name ? _fbb.CreateString(name) : 0,
      data);
}

struct Node FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_NAME = 4,
    VT_TRANSLATION = 6,
    VT_ROTATION = 8,
    VT_SCALE = 10,
    VT_CHILDREN = 12,
    VT_RENDER_ENTITY = 14
  };
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  const Vec3 *translation() const {
    return GetStruct<const Vec3 *>(VT_TRANSLATION);
  }
  const Vec3 *rotation() const {
    return GetStruct<const Vec3 *>(VT_ROTATION);
  }
  const Vec3 *scale() const {
    return GetStruct<const Vec3 *>(VT_SCALE);
  }
  const flatbuffers::Vector<flatbuffers::Offset<Node>> *children() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Node>> *>(VT_CHILDREN);
  }
  const flatbuffers::Vector<flatbuffers::Offset<Entity>> *render_entity() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Entity>> *>(VT_RENDER_ENTITY);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_NAME) &&
           verifier.Verify(name()) &&
           VerifyField<Vec3>(verifier, VT_TRANSLATION) &&
           VerifyField<Vec3>(verifier, VT_ROTATION) &&
           VerifyField<Vec3>(verifier, VT_SCALE) &&
           VerifyOffset(verifier, VT_CHILDREN) &&
           verifier.Verify(children()) &&
           verifier.VerifyVectorOfTables(children()) &&
           VerifyOffset(verifier, VT_RENDER_ENTITY) &&
           verifier.Verify(render_entity()) &&
           verifier.VerifyVectorOfTables(render_entity()) &&
           verifier.EndTable();
  }
};

struct NodeBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(Node::VT_NAME, name);
  }
  void add_translation(const Vec3 *translation) {
    fbb_.AddStruct(Node::VT_TRANSLATION, translation);
  }
  void add_rotation(const Vec3 *rotation) {
    fbb_.AddStruct(Node::VT_ROTATION, rotation);
  }
  void add_scale(const Vec3 *scale) {
    fbb_.AddStruct(Node::VT_SCALE, scale);
  }
  void add_children(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Node>>> children) {
    fbb_.AddOffset(Node::VT_CHILDREN, children);
  }
  void add_render_entity(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Entity>>> render_entity) {
    fbb_.AddOffset(Node::VT_RENDER_ENTITY, render_entity);
  }
  explicit NodeBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  NodeBuilder &operator=(const NodeBuilder &);
  flatbuffers::Offset<Node> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<Node>(end);
    return o;
  }
};

inline flatbuffers::Offset<Node> CreateNode(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    const Vec3 *translation = 0,
    const Vec3 *rotation = 0,
    const Vec3 *scale = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Node>>> children = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Entity>>> render_entity = 0) {
  NodeBuilder builder_(_fbb);
  builder_.add_render_entity(render_entity);
  builder_.add_children(children);
  builder_.add_scale(scale);
  builder_.add_rotation(rotation);
  builder_.add_translation(translation);
  builder_.add_name(name);
  return builder_.Finish();
}

inline flatbuffers::Offset<Node> CreateNodeDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *name = nullptr,
    const Vec3 *translation = 0,
    const Vec3 *rotation = 0,
    const Vec3 *scale = 0,
    const std::vector<flatbuffers::Offset<Node>> *children = nullptr,
    const std::vector<flatbuffers::Offset<Entity>> *render_entity = nullptr) {
  return SE::FlatBuffers::CreateNode(
      _fbb,
      name ? _fbb.CreateString(name) : 0,
      translation,
      rotation,
      scale,
      children ? _fbb.CreateVector<flatbuffers::Offset<Node>>(*children) : 0,
      render_entity ? _fbb.CreateVector<flatbuffers::Offset<Entity>>(*render_entity) : 0);
}

inline const SE::FlatBuffers::Node *GetNode(const void *buf) {
  return flatbuffers::GetRoot<SE::FlatBuffers::Node>(buf);
}

inline const char *NodeIdentifier() {
  return "SESC";
}

inline bool NodeBufferHasIdentifier(const void *buf) {
  return flatbuffers::BufferHasIdentifier(
      buf, NodeIdentifier());
}

inline bool VerifyNodeBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<SE::FlatBuffers::Node>(NodeIdentifier());
}

inline const char *NodeExtension() {
  return "sesc";
}

inline void FinishNodeBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::Node> root) {
  fbb.Finish(root, NodeIdentifier());
}

}  // namespace FlatBuffers
}  // namespace SE

#endif  // FLATBUFFERS_GENERATED_SCENETREE_SE_FLATBUFFERS_H_
