// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_COMPONENT_SE_FLATBUFFERS_H_
#define FLATBUFFERS_GENERATED_COMPONENT_SE_FLATBUFFERS_H_

#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flexbuffers.h"

#include "Common_generated.h"
#include "Material_generated.h"
#include "Mesh_generated.h"
#include "ShaderComponent_generated.h"
#include "ShaderProgram_generated.h"

namespace SE {
namespace FlatBuffers {

struct StaticModel;

struct BindSQT;

struct Joint;

struct Skeleton;

struct SkeletonHolder;

struct CharacterShell;

struct CharacterShellHolder;

struct AnimatedModel;

struct AppComponent;

struct Component;

enum class ComponentU : uint8_t {
  NONE = 0,
  StaticModel = 1,
  AnimatedModel = 2,
  AppComponent = 3,
  MIN = NONE,
  MAX = AppComponent
};

inline const ComponentU (&EnumValuesComponentU())[4] {
  static const ComponentU values[] = {
    ComponentU::NONE,
    ComponentU::StaticModel,
    ComponentU::AnimatedModel,
    ComponentU::AppComponent
  };
  return values;
}

inline const char * const *EnumNamesComponentU() {
  static const char * const names[] = {
    "NONE",
    "StaticModel",
    "AnimatedModel",
    "AppComponent",
    nullptr
  };
  return names;
}

inline const char *EnumNameComponentU(ComponentU e) {
  const size_t index = static_cast<int>(e);
  return EnumNamesComponentU()[index];
}

template<typename T> struct ComponentUTraits {
  static const ComponentU enum_value = ComponentU::NONE;
};

template<> struct ComponentUTraits<StaticModel> {
  static const ComponentU enum_value = ComponentU::StaticModel;
};

template<> struct ComponentUTraits<AnimatedModel> {
  static const ComponentU enum_value = ComponentU::AnimatedModel;
};

template<> struct ComponentUTraits<AppComponent> {
  static const ComponentU enum_value = ComponentU::AppComponent;
};

bool VerifyComponentU(flatbuffers::Verifier &verifier, const void *obj, ComponentU type);
bool VerifyComponentUVector(flatbuffers::Verifier &verifier, const flatbuffers::Vector<flatbuffers::Offset<void>> *values, const flatbuffers::Vector<uint8_t> *types);

struct StaticModel FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_MESH = 4,
    VT_MATERIAL = 6
  };
  const MeshHolder *mesh() const {
    return GetPointer<const MeshHolder *>(VT_MESH);
  }
  const MaterialHolder *material() const {
    return GetPointer<const MaterialHolder *>(VT_MATERIAL);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_MESH) &&
           verifier.VerifyTable(mesh()) &&
           VerifyOffset(verifier, VT_MATERIAL) &&
           verifier.VerifyTable(material()) &&
           verifier.EndTable();
  }
};

struct StaticModelBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_mesh(flatbuffers::Offset<MeshHolder> mesh) {
    fbb_.AddOffset(StaticModel::VT_MESH, mesh);
  }
  void add_material(flatbuffers::Offset<MaterialHolder> material) {
    fbb_.AddOffset(StaticModel::VT_MATERIAL, material);
  }
  explicit StaticModelBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  StaticModelBuilder &operator=(const StaticModelBuilder &);
  flatbuffers::Offset<StaticModel> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<StaticModel>(end);
    fbb_.Required(o, StaticModel::VT_MESH);
    return o;
  }
};

inline flatbuffers::Offset<StaticModel> CreateStaticModel(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<MeshHolder> mesh = 0,
    flatbuffers::Offset<MaterialHolder> material = 0) {
  StaticModelBuilder builder_(_fbb);
  builder_.add_material(material);
  builder_.add_mesh(mesh);
  return builder_.Finish();
}

struct BindSQT FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_BIND_ROT = 4,
    VT_BIND_POS = 6,
    VT_BIND_SCALE = 8
  };
  const Vec4 *bind_rot() const {
    return GetStruct<const Vec4 *>(VT_BIND_ROT);
  }
  const Vec3 *bind_pos() const {
    return GetStruct<const Vec3 *>(VT_BIND_POS);
  }
  const Vec3 *bind_scale() const {
    return GetStruct<const Vec3 *>(VT_BIND_SCALE);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<Vec4>(verifier, VT_BIND_ROT) &&
           VerifyField<Vec3>(verifier, VT_BIND_POS) &&
           VerifyField<Vec3>(verifier, VT_BIND_SCALE) &&
           verifier.EndTable();
  }
};

struct BindSQTBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_bind_rot(const Vec4 *bind_rot) {
    fbb_.AddStruct(BindSQT::VT_BIND_ROT, bind_rot);
  }
  void add_bind_pos(const Vec3 *bind_pos) {
    fbb_.AddStruct(BindSQT::VT_BIND_POS, bind_pos);
  }
  void add_bind_scale(const Vec3 *bind_scale) {
    fbb_.AddStruct(BindSQT::VT_BIND_SCALE, bind_scale);
  }
  explicit BindSQTBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  BindSQTBuilder &operator=(const BindSQTBuilder &);
  flatbuffers::Offset<BindSQT> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<BindSQT>(end);
    return o;
  }
};

inline flatbuffers::Offset<BindSQT> CreateBindSQT(
    flatbuffers::FlatBufferBuilder &_fbb,
    const Vec4 *bind_rot = 0,
    const Vec3 *bind_pos = 0,
    const Vec3 *bind_scale = 0) {
  BindSQTBuilder builder_(_fbb);
  builder_.add_bind_scale(bind_scale);
  builder_.add_bind_pos(bind_pos);
  builder_.add_bind_rot(bind_rot);
  return builder_.Finish();
}

struct Joint FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_NAME = 4,
    VT_PARENT_INDEX = 6
  };
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  uint8_t parent_index() const {
    return GetField<uint8_t>(VT_PARENT_INDEX, 0);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_NAME) &&
           verifier.VerifyString(name()) &&
           VerifyField<uint8_t>(verifier, VT_PARENT_INDEX) &&
           verifier.EndTable();
  }
};

struct JointBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(Joint::VT_NAME, name);
  }
  void add_parent_index(uint8_t parent_index) {
    fbb_.AddElement<uint8_t>(Joint::VT_PARENT_INDEX, parent_index, 0);
  }
  explicit JointBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  JointBuilder &operator=(const JointBuilder &);
  flatbuffers::Offset<Joint> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<Joint>(end);
    fbb_.Required(o, Joint::VT_NAME);
    return o;
  }
};

inline flatbuffers::Offset<Joint> CreateJoint(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    uint8_t parent_index = 0) {
  JointBuilder builder_(_fbb);
  builder_.add_name(name);
  builder_.add_parent_index(parent_index);
  return builder_.Finish();
}

inline flatbuffers::Offset<Joint> CreateJointDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *name = nullptr,
    uint8_t parent_index = 0) {
  return SE::FlatBuffers::CreateJoint(
      _fbb,
      name ? _fbb.CreateString(name) : 0,
      parent_index);
}

struct Skeleton FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_JOINTS = 4
  };
  const flatbuffers::Vector<flatbuffers::Offset<Joint>> *joints() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Joint>> *>(VT_JOINTS);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_JOINTS) &&
           verifier.VerifyVector(joints()) &&
           verifier.VerifyVectorOfTables(joints()) &&
           verifier.EndTable();
  }
};

struct SkeletonBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_joints(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Joint>>> joints) {
    fbb_.AddOffset(Skeleton::VT_JOINTS, joints);
  }
  explicit SkeletonBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  SkeletonBuilder &operator=(const SkeletonBuilder &);
  flatbuffers::Offset<Skeleton> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<Skeleton>(end);
    fbb_.Required(o, Skeleton::VT_JOINTS);
    return o;
  }
};

inline flatbuffers::Offset<Skeleton> CreateSkeleton(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Joint>>> joints = 0) {
  SkeletonBuilder builder_(_fbb);
  builder_.add_joints(joints);
  return builder_.Finish();
}

inline flatbuffers::Offset<Skeleton> CreateSkeletonDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<flatbuffers::Offset<Joint>> *joints = nullptr) {
  return SE::FlatBuffers::CreateSkeleton(
      _fbb,
      joints ? _fbb.CreateVector<flatbuffers::Offset<Joint>>(*joints) : 0);
}

struct SkeletonHolder FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_SKELETON = 4,
    VT_PATH = 6,
    VT_NAME = 8
  };
  const Skeleton *skeleton() const {
    return GetPointer<const Skeleton *>(VT_SKELETON);
  }
  const flatbuffers::String *path() const {
    return GetPointer<const flatbuffers::String *>(VT_PATH);
  }
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_SKELETON) &&
           verifier.VerifyTable(skeleton()) &&
           VerifyOffset(verifier, VT_PATH) &&
           verifier.VerifyString(path()) &&
           VerifyOffset(verifier, VT_NAME) &&
           verifier.VerifyString(name()) &&
           verifier.EndTable();
  }
};

struct SkeletonHolderBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_skeleton(flatbuffers::Offset<Skeleton> skeleton) {
    fbb_.AddOffset(SkeletonHolder::VT_SKELETON, skeleton);
  }
  void add_path(flatbuffers::Offset<flatbuffers::String> path) {
    fbb_.AddOffset(SkeletonHolder::VT_PATH, path);
  }
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(SkeletonHolder::VT_NAME, name);
  }
  explicit SkeletonHolderBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  SkeletonHolderBuilder &operator=(const SkeletonHolderBuilder &);
  flatbuffers::Offset<SkeletonHolder> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<SkeletonHolder>(end);
    return o;
  }
};

inline flatbuffers::Offset<SkeletonHolder> CreateSkeletonHolder(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<Skeleton> skeleton = 0,
    flatbuffers::Offset<flatbuffers::String> path = 0,
    flatbuffers::Offset<flatbuffers::String> name = 0) {
  SkeletonHolderBuilder builder_(_fbb);
  builder_.add_name(name);
  builder_.add_path(path);
  builder_.add_skeleton(skeleton);
  return builder_.Finish();
}

inline flatbuffers::Offset<SkeletonHolder> CreateSkeletonHolderDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<Skeleton> skeleton = 0,
    const char *path = nullptr,
    const char *name = nullptr) {
  return SE::FlatBuffers::CreateSkeletonHolder(
      _fbb,
      skeleton,
      path ? _fbb.CreateString(path) : 0,
      name ? _fbb.CreateString(name) : 0);
}

struct CharacterShell FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_SKELETON_ROOT_NODE = 4,
    VT_SKELETON = 6
  };
  const flatbuffers::String *skeleton_root_node() const {
    return GetPointer<const flatbuffers::String *>(VT_SKELETON_ROOT_NODE);
  }
  const SkeletonHolder *skeleton() const {
    return GetPointer<const SkeletonHolder *>(VT_SKELETON);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_SKELETON_ROOT_NODE) &&
           verifier.VerifyString(skeleton_root_node()) &&
           VerifyOffsetRequired(verifier, VT_SKELETON) &&
           verifier.VerifyTable(skeleton()) &&
           verifier.EndTable();
  }
};

struct CharacterShellBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_skeleton_root_node(flatbuffers::Offset<flatbuffers::String> skeleton_root_node) {
    fbb_.AddOffset(CharacterShell::VT_SKELETON_ROOT_NODE, skeleton_root_node);
  }
  void add_skeleton(flatbuffers::Offset<SkeletonHolder> skeleton) {
    fbb_.AddOffset(CharacterShell::VT_SKELETON, skeleton);
  }
  explicit CharacterShellBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  CharacterShellBuilder &operator=(const CharacterShellBuilder &);
  flatbuffers::Offset<CharacterShell> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<CharacterShell>(end);
    fbb_.Required(o, CharacterShell::VT_SKELETON_ROOT_NODE);
    fbb_.Required(o, CharacterShell::VT_SKELETON);
    return o;
  }
};

inline flatbuffers::Offset<CharacterShell> CreateCharacterShell(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> skeleton_root_node = 0,
    flatbuffers::Offset<SkeletonHolder> skeleton = 0) {
  CharacterShellBuilder builder_(_fbb);
  builder_.add_skeleton(skeleton);
  builder_.add_skeleton_root_node(skeleton_root_node);
  return builder_.Finish();
}

inline flatbuffers::Offset<CharacterShell> CreateCharacterShellDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *skeleton_root_node = nullptr,
    flatbuffers::Offset<SkeletonHolder> skeleton = 0) {
  return SE::FlatBuffers::CreateCharacterShell(
      _fbb,
      skeleton_root_node ? _fbb.CreateString(skeleton_root_node) : 0,
      skeleton);
}

struct CharacterShellHolder FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_SHELL = 4,
    VT_PATH = 6,
    VT_NAME = 8
  };
  const CharacterShell *shell() const {
    return GetPointer<const CharacterShell *>(VT_SHELL);
  }
  const flatbuffers::String *path() const {
    return GetPointer<const flatbuffers::String *>(VT_PATH);
  }
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_SHELL) &&
           verifier.VerifyTable(shell()) &&
           VerifyOffset(verifier, VT_PATH) &&
           verifier.VerifyString(path()) &&
           VerifyOffset(verifier, VT_NAME) &&
           verifier.VerifyString(name()) &&
           verifier.EndTable();
  }
};

struct CharacterShellHolderBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_shell(flatbuffers::Offset<CharacterShell> shell) {
    fbb_.AddOffset(CharacterShellHolder::VT_SHELL, shell);
  }
  void add_path(flatbuffers::Offset<flatbuffers::String> path) {
    fbb_.AddOffset(CharacterShellHolder::VT_PATH, path);
  }
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(CharacterShellHolder::VT_NAME, name);
  }
  explicit CharacterShellHolderBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  CharacterShellHolderBuilder &operator=(const CharacterShellHolderBuilder &);
  flatbuffers::Offset<CharacterShellHolder> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<CharacterShellHolder>(end);
    return o;
  }
};

inline flatbuffers::Offset<CharacterShellHolder> CreateCharacterShellHolder(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<CharacterShell> shell = 0,
    flatbuffers::Offset<flatbuffers::String> path = 0,
    flatbuffers::Offset<flatbuffers::String> name = 0) {
  CharacterShellHolderBuilder builder_(_fbb);
  builder_.add_name(name);
  builder_.add_path(path);
  builder_.add_shell(shell);
  return builder_.Finish();
}

inline flatbuffers::Offset<CharacterShellHolder> CreateCharacterShellHolderDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<CharacterShell> shell = 0,
    const char *path = nullptr,
    const char *name = nullptr) {
  return SE::FlatBuffers::CreateCharacterShellHolder(
      _fbb,
      shell,
      path ? _fbb.CreateString(path) : 0,
      name ? _fbb.CreateString(name) : 0);
}

struct AnimatedModel FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_MESH = 4,
    VT_MATERIAL = 6,
    VT_BLENDSHAPES = 8,
    VT_BLENDSHAPES_WEIGHTS = 10,
    VT_SHELL = 12,
    VT_JOINTS_INDEXES = 14,
    VT_JOINTS_INV_BIND_POSE = 16,
    VT_MESH_BIND_POS = 18
  };
  const MeshHolder *mesh() const {
    return GetPointer<const MeshHolder *>(VT_MESH);
  }
  const MaterialHolder *material() const {
    return GetPointer<const MaterialHolder *>(VT_MATERIAL);
  }
  const TextureHolder *blendshapes() const {
    return GetPointer<const TextureHolder *>(VT_BLENDSHAPES);
  }
  const flatbuffers::Vector<float> *blendshapes_weights() const {
    return GetPointer<const flatbuffers::Vector<float> *>(VT_BLENDSHAPES_WEIGHTS);
  }
  const CharacterShellHolder *shell() const {
    return GetPointer<const CharacterShellHolder *>(VT_SHELL);
  }
  const flatbuffers::Vector<uint8_t> *joints_indexes() const {
    return GetPointer<const flatbuffers::Vector<uint8_t> *>(VT_JOINTS_INDEXES);
  }
  const flatbuffers::Vector<flatbuffers::Offset<BindSQT>> *joints_inv_bind_pose() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<BindSQT>> *>(VT_JOINTS_INV_BIND_POSE);
  }
  const BindSQT *mesh_bind_pos() const {
    return GetPointer<const BindSQT *>(VT_MESH_BIND_POS);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_MESH) &&
           verifier.VerifyTable(mesh()) &&
           VerifyOffset(verifier, VT_MATERIAL) &&
           verifier.VerifyTable(material()) &&
           VerifyOffset(verifier, VT_BLENDSHAPES) &&
           verifier.VerifyTable(blendshapes()) &&
           VerifyOffset(verifier, VT_BLENDSHAPES_WEIGHTS) &&
           verifier.VerifyVector(blendshapes_weights()) &&
           VerifyOffset(verifier, VT_SHELL) &&
           verifier.VerifyTable(shell()) &&
           VerifyOffset(verifier, VT_JOINTS_INDEXES) &&
           verifier.VerifyVector(joints_indexes()) &&
           VerifyOffset(verifier, VT_JOINTS_INV_BIND_POSE) &&
           verifier.VerifyVector(joints_inv_bind_pose()) &&
           verifier.VerifyVectorOfTables(joints_inv_bind_pose()) &&
           VerifyOffset(verifier, VT_MESH_BIND_POS) &&
           verifier.VerifyTable(mesh_bind_pos()) &&
           verifier.EndTable();
  }
};

struct AnimatedModelBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_mesh(flatbuffers::Offset<MeshHolder> mesh) {
    fbb_.AddOffset(AnimatedModel::VT_MESH, mesh);
  }
  void add_material(flatbuffers::Offset<MaterialHolder> material) {
    fbb_.AddOffset(AnimatedModel::VT_MATERIAL, material);
  }
  void add_blendshapes(flatbuffers::Offset<TextureHolder> blendshapes) {
    fbb_.AddOffset(AnimatedModel::VT_BLENDSHAPES, blendshapes);
  }
  void add_blendshapes_weights(flatbuffers::Offset<flatbuffers::Vector<float>> blendshapes_weights) {
    fbb_.AddOffset(AnimatedModel::VT_BLENDSHAPES_WEIGHTS, blendshapes_weights);
  }
  void add_shell(flatbuffers::Offset<CharacterShellHolder> shell) {
    fbb_.AddOffset(AnimatedModel::VT_SHELL, shell);
  }
  void add_joints_indexes(flatbuffers::Offset<flatbuffers::Vector<uint8_t>> joints_indexes) {
    fbb_.AddOffset(AnimatedModel::VT_JOINTS_INDEXES, joints_indexes);
  }
  void add_joints_inv_bind_pose(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<BindSQT>>> joints_inv_bind_pose) {
    fbb_.AddOffset(AnimatedModel::VT_JOINTS_INV_BIND_POSE, joints_inv_bind_pose);
  }
  void add_mesh_bind_pos(flatbuffers::Offset<BindSQT> mesh_bind_pos) {
    fbb_.AddOffset(AnimatedModel::VT_MESH_BIND_POS, mesh_bind_pos);
  }
  explicit AnimatedModelBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  AnimatedModelBuilder &operator=(const AnimatedModelBuilder &);
  flatbuffers::Offset<AnimatedModel> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<AnimatedModel>(end);
    fbb_.Required(o, AnimatedModel::VT_MESH);
    return o;
  }
};

inline flatbuffers::Offset<AnimatedModel> CreateAnimatedModel(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<MeshHolder> mesh = 0,
    flatbuffers::Offset<MaterialHolder> material = 0,
    flatbuffers::Offset<TextureHolder> blendshapes = 0,
    flatbuffers::Offset<flatbuffers::Vector<float>> blendshapes_weights = 0,
    flatbuffers::Offset<CharacterShellHolder> shell = 0,
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> joints_indexes = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<BindSQT>>> joints_inv_bind_pose = 0,
    flatbuffers::Offset<BindSQT> mesh_bind_pos = 0) {
  AnimatedModelBuilder builder_(_fbb);
  builder_.add_mesh_bind_pos(mesh_bind_pos);
  builder_.add_joints_inv_bind_pose(joints_inv_bind_pose);
  builder_.add_joints_indexes(joints_indexes);
  builder_.add_shell(shell);
  builder_.add_blendshapes_weights(blendshapes_weights);
  builder_.add_blendshapes(blendshapes);
  builder_.add_material(material);
  builder_.add_mesh(mesh);
  return builder_.Finish();
}

inline flatbuffers::Offset<AnimatedModel> CreateAnimatedModelDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<MeshHolder> mesh = 0,
    flatbuffers::Offset<MaterialHolder> material = 0,
    flatbuffers::Offset<TextureHolder> blendshapes = 0,
    const std::vector<float> *blendshapes_weights = nullptr,
    flatbuffers::Offset<CharacterShellHolder> shell = 0,
    const std::vector<uint8_t> *joints_indexes = nullptr,
    const std::vector<flatbuffers::Offset<BindSQT>> *joints_inv_bind_pose = nullptr,
    flatbuffers::Offset<BindSQT> mesh_bind_pos = 0) {
  return SE::FlatBuffers::CreateAnimatedModel(
      _fbb,
      mesh,
      material,
      blendshapes,
      blendshapes_weights ? _fbb.CreateVector<float>(*blendshapes_weights) : 0,
      shell,
      joints_indexes ? _fbb.CreateVector<uint8_t>(*joints_indexes) : 0,
      joints_inv_bind_pose ? _fbb.CreateVector<flatbuffers::Offset<BindSQT>>(*joints_inv_bind_pose) : 0,
      mesh_bind_pos);
}

struct AppComponent FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_SUB_TYPE = 4,
    VT_DATA = 6
  };
  uint16_t sub_type() const {
    return GetField<uint16_t>(VT_SUB_TYPE, 0);
  }
  const flatbuffers::Vector<uint8_t> *data() const {
    return GetPointer<const flatbuffers::Vector<uint8_t> *>(VT_DATA);
  }
  flexbuffers::Reference data_flexbuffer_root() const {
    return flexbuffers::GetRoot(data()->Data(), data()->size());
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<uint16_t>(verifier, VT_SUB_TYPE) &&
           VerifyOffset(verifier, VT_DATA) &&
           verifier.VerifyVector(data()) &&
           verifier.EndTable();
  }
};

struct AppComponentBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_sub_type(uint16_t sub_type) {
    fbb_.AddElement<uint16_t>(AppComponent::VT_SUB_TYPE, sub_type, 0);
  }
  void add_data(flatbuffers::Offset<flatbuffers::Vector<uint8_t>> data) {
    fbb_.AddOffset(AppComponent::VT_DATA, data);
  }
  explicit AppComponentBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  AppComponentBuilder &operator=(const AppComponentBuilder &);
  flatbuffers::Offset<AppComponent> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<AppComponent>(end);
    return o;
  }
};

inline flatbuffers::Offset<AppComponent> CreateAppComponent(
    flatbuffers::FlatBufferBuilder &_fbb,
    uint16_t sub_type = 0,
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> data = 0) {
  AppComponentBuilder builder_(_fbb);
  builder_.add_data(data);
  builder_.add_sub_type(sub_type);
  return builder_.Finish();
}

inline flatbuffers::Offset<AppComponent> CreateAppComponentDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    uint16_t sub_type = 0,
    const std::vector<uint8_t> *data = nullptr) {
  return SE::FlatBuffers::CreateAppComponent(
      _fbb,
      sub_type,
      data ? _fbb.CreateVector<uint8_t>(*data) : 0);
}

struct Component FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_COMPONENT_TYPE = 4,
    VT_COMPONENT = 6
  };
  ComponentU component_type() const {
    return static_cast<ComponentU>(GetField<uint8_t>(VT_COMPONENT_TYPE, 0));
  }
  const void *component() const {
    return GetPointer<const void *>(VT_COMPONENT);
  }
  template<typename T> const T *component_as() const;
  const StaticModel *component_as_StaticModel() const {
    return component_type() == ComponentU::StaticModel ? static_cast<const StaticModel *>(component()) : nullptr;
  }
  const AnimatedModel *component_as_AnimatedModel() const {
    return component_type() == ComponentU::AnimatedModel ? static_cast<const AnimatedModel *>(component()) : nullptr;
  }
  const AppComponent *component_as_AppComponent() const {
    return component_type() == ComponentU::AppComponent ? static_cast<const AppComponent *>(component()) : nullptr;
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<uint8_t>(verifier, VT_COMPONENT_TYPE) &&
           VerifyOffsetRequired(verifier, VT_COMPONENT) &&
           VerifyComponentU(verifier, component(), component_type()) &&
           verifier.EndTable();
  }
};

template<> inline const StaticModel *Component::component_as<StaticModel>() const {
  return component_as_StaticModel();
}

template<> inline const AnimatedModel *Component::component_as<AnimatedModel>() const {
  return component_as_AnimatedModel();
}

template<> inline const AppComponent *Component::component_as<AppComponent>() const {
  return component_as_AppComponent();
}

struct ComponentBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_component_type(ComponentU component_type) {
    fbb_.AddElement<uint8_t>(Component::VT_COMPONENT_TYPE, static_cast<uint8_t>(component_type), 0);
  }
  void add_component(flatbuffers::Offset<void> component) {
    fbb_.AddOffset(Component::VT_COMPONENT, component);
  }
  explicit ComponentBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ComponentBuilder &operator=(const ComponentBuilder &);
  flatbuffers::Offset<Component> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<Component>(end);
    fbb_.Required(o, Component::VT_COMPONENT);
    return o;
  }
};

inline flatbuffers::Offset<Component> CreateComponent(
    flatbuffers::FlatBufferBuilder &_fbb,
    ComponentU component_type = ComponentU::NONE,
    flatbuffers::Offset<void> component = 0) {
  ComponentBuilder builder_(_fbb);
  builder_.add_component(component);
  builder_.add_component_type(component_type);
  return builder_.Finish();
}

inline bool VerifyComponentU(flatbuffers::Verifier &verifier, const void *obj, ComponentU type) {
  switch (type) {
    case ComponentU::NONE: {
      return true;
    }
    case ComponentU::StaticModel: {
      auto ptr = reinterpret_cast<const StaticModel *>(obj);
      return verifier.VerifyTable(ptr);
    }
    case ComponentU::AnimatedModel: {
      auto ptr = reinterpret_cast<const AnimatedModel *>(obj);
      return verifier.VerifyTable(ptr);
    }
    case ComponentU::AppComponent: {
      auto ptr = reinterpret_cast<const AppComponent *>(obj);
      return verifier.VerifyTable(ptr);
    }
    default: return false;
  }
}

inline bool VerifyComponentUVector(flatbuffers::Verifier &verifier, const flatbuffers::Vector<flatbuffers::Offset<void>> *values, const flatbuffers::Vector<uint8_t> *types) {
  if (!values || !types) return !values && !types;
  if (values->size() != types->size()) return false;
  for (flatbuffers::uoffset_t i = 0; i < values->size(); ++i) {
    if (!VerifyComponentU(
        verifier,  values->Get(i), types->GetEnum<ComponentU>(i))) {
      return false;
    }
  }
  return true;
}

inline const SE::FlatBuffers::Component *GetComponent(const void *buf) {
  return flatbuffers::GetRoot<SE::FlatBuffers::Component>(buf);
}

inline const SE::FlatBuffers::Component *GetSizePrefixedComponent(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<SE::FlatBuffers::Component>(buf);
}

inline bool VerifyComponentBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<SE::FlatBuffers::Component>(nullptr);
}

inline bool VerifySizePrefixedComponentBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<SE::FlatBuffers::Component>(nullptr);
}

inline void FinishComponentBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::Component> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedComponentBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::Component> root) {
  fbb.FinishSizePrefixed(root);
}

}  // namespace FlatBuffers
}  // namespace SE

#endif  // FLATBUFFERS_GENERATED_COMPONENT_SE_FLATBUFFERS_H_
