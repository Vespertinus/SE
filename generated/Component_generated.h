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

struct AnimatedModel FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_MESH = 4,
    VT_MATERIAL = 6,
    VT_TBO = 8
  };
  const MeshHolder *mesh() const {
    return GetPointer<const MeshHolder *>(VT_MESH);
  }
  const MaterialHolder *material() const {
    return GetPointer<const MaterialHolder *>(VT_MATERIAL);
  }
  const TextureHolder *tbo() const {
    return GetPointer<const TextureHolder *>(VT_TBO);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_MESH) &&
           verifier.VerifyTable(mesh()) &&
           VerifyOffsetRequired(verifier, VT_MATERIAL) &&
           verifier.VerifyTable(material()) &&
           VerifyOffsetRequired(verifier, VT_TBO) &&
           verifier.VerifyTable(tbo()) &&
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
  void add_tbo(flatbuffers::Offset<TextureHolder> tbo) {
    fbb_.AddOffset(AnimatedModel::VT_TBO, tbo);
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
    fbb_.Required(o, AnimatedModel::VT_MATERIAL);
    fbb_.Required(o, AnimatedModel::VT_TBO);
    return o;
  }
};

inline flatbuffers::Offset<AnimatedModel> CreateAnimatedModel(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<MeshHolder> mesh = 0,
    flatbuffers::Offset<MaterialHolder> material = 0,
    flatbuffers::Offset<TextureHolder> tbo = 0) {
  AnimatedModelBuilder builder_(_fbb);
  builder_.add_tbo(tbo);
  builder_.add_material(material);
  builder_.add_mesh(mesh);
  return builder_.Finish();
}

struct AppComponent FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_DATA = 4
  };
  const flatbuffers::Vector<uint8_t> *data() const {
    return GetPointer<const flatbuffers::Vector<uint8_t> *>(VT_DATA);
  }
  flexbuffers::Reference data_flexbuffer_root() const {
    auto v = data();
    return flexbuffers::GetRoot(v->Data(), v->size());
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_DATA) &&
           verifier.Verify(data()) &&
           verifier.EndTable();
  }
};

struct AppComponentBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
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
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> data = 0) {
  AppComponentBuilder builder_(_fbb);
  builder_.add_data(data);
  return builder_.Finish();
}

inline flatbuffers::Offset<AppComponent> CreateAppComponentDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<uint8_t> *data = nullptr) {
  return SE::FlatBuffers::CreateAppComponent(
      _fbb,
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