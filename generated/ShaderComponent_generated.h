// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_SHADERCOMPONENT_SE_FLATBUFFERS_H_
#define FLATBUFFERS_GENERATED_SHADERCOMPONENT_SE_FLATBUFFERS_H_

#include "flatbuffers/flatbuffers.h"

namespace SE {
namespace FlatBuffers {

struct ShaderComponent;

enum class ShaderType : uint8_t {
  VERTEX = 1,
  FRAGMENT = 2,
  GEOMETRY = 3,
  MIN = VERTEX,
  MAX = GEOMETRY
};

inline const ShaderType (&EnumValuesShaderType())[3] {
  static const ShaderType values[] = {
    ShaderType::VERTEX,
    ShaderType::FRAGMENT,
    ShaderType::GEOMETRY
  };
  return values;
}

inline const char * const *EnumNamesShaderType() {
  static const char * const names[] = {
    "VERTEX",
    "FRAGMENT",
    "GEOMETRY",
    nullptr
  };
  return names;
}

inline const char *EnumNameShaderType(ShaderType e) {
  const size_t index = static_cast<int>(e) - static_cast<int>(ShaderType::VERTEX);
  return EnumNamesShaderType()[index];
}

struct ShaderComponent FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_DEPENDENCIES = 4,
    VT_TYPE = 6,
    VT_SOURCE = 8
  };
  const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>> *dependencies() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>> *>(VT_DEPENDENCIES);
  }
  ShaderType type() const {
    return static_cast<ShaderType>(GetField<uint8_t>(VT_TYPE, 2));
  }
  const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>> *source() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>> *>(VT_SOURCE);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_DEPENDENCIES) &&
           verifier.VerifyVector(dependencies()) &&
           verifier.VerifyVectorOfStrings(dependencies()) &&
           VerifyField<uint8_t>(verifier, VT_TYPE) &&
           VerifyOffsetRequired(verifier, VT_SOURCE) &&
           verifier.VerifyVector(source()) &&
           verifier.VerifyVectorOfStrings(source()) &&
           verifier.EndTable();
  }
};

struct ShaderComponentBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_dependencies(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> dependencies) {
    fbb_.AddOffset(ShaderComponent::VT_DEPENDENCIES, dependencies);
  }
  void add_type(ShaderType type) {
    fbb_.AddElement<uint8_t>(ShaderComponent::VT_TYPE, static_cast<uint8_t>(type), 2);
  }
  void add_source(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> source) {
    fbb_.AddOffset(ShaderComponent::VT_SOURCE, source);
  }
  explicit ShaderComponentBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ShaderComponentBuilder &operator=(const ShaderComponentBuilder &);
  flatbuffers::Offset<ShaderComponent> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<ShaderComponent>(end);
    fbb_.Required(o, ShaderComponent::VT_SOURCE);
    return o;
  }
};

inline flatbuffers::Offset<ShaderComponent> CreateShaderComponent(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> dependencies = 0,
    ShaderType type = ShaderType::FRAGMENT,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> source = 0) {
  ShaderComponentBuilder builder_(_fbb);
  builder_.add_source(source);
  builder_.add_dependencies(dependencies);
  builder_.add_type(type);
  return builder_.Finish();
}

inline flatbuffers::Offset<ShaderComponent> CreateShaderComponentDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<flatbuffers::Offset<flatbuffers::String>> *dependencies = nullptr,
    ShaderType type = ShaderType::FRAGMENT,
    const std::vector<flatbuffers::Offset<flatbuffers::String>> *source = nullptr) {
  return SE::FlatBuffers::CreateShaderComponent(
      _fbb,
      dependencies ? _fbb.CreateVector<flatbuffers::Offset<flatbuffers::String>>(*dependencies) : 0,
      type,
      source ? _fbb.CreateVector<flatbuffers::Offset<flatbuffers::String>>(*source) : 0);
}

inline const SE::FlatBuffers::ShaderComponent *GetShaderComponent(const void *buf) {
  return flatbuffers::GetRoot<SE::FlatBuffers::ShaderComponent>(buf);
}

inline const SE::FlatBuffers::ShaderComponent *GetSizePrefixedShaderComponent(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<SE::FlatBuffers::ShaderComponent>(buf);
}

inline const char *ShaderComponentIdentifier() {
  return "SESL";
}

inline bool ShaderComponentBufferHasIdentifier(const void *buf) {
  return flatbuffers::BufferHasIdentifier(
      buf, ShaderComponentIdentifier());
}

inline bool VerifyShaderComponentBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<SE::FlatBuffers::ShaderComponent>(ShaderComponentIdentifier());
}

inline bool VerifySizePrefixedShaderComponentBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<SE::FlatBuffers::ShaderComponent>(ShaderComponentIdentifier());
}

inline const char *ShaderComponentExtension() {
  return "sesl";
}

inline void FinishShaderComponentBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::ShaderComponent> root) {
  fbb.Finish(root, ShaderComponentIdentifier());
}

inline void FinishSizePrefixedShaderComponentBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::ShaderComponent> root) {
  fbb.FinishSizePrefixed(root, ShaderComponentIdentifier());
}

}  // namespace FlatBuffers
}  // namespace SE

#endif  // FLATBUFFERS_GENERATED_SHADERCOMPONENT_SE_FLATBUFFERS_H_
