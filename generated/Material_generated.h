// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_MATERIAL_SE_FLATBUFFERS_H_
#define FLATBUFFERS_GENERATED_MATERIAL_SE_FLATBUFFERS_H_

#include "flatbuffers/flatbuffers.h"

#include "Common_generated.h"
#include "ShaderComponent_generated.h"
#include "ShaderProgram_generated.h"

namespace SE {
namespace FlatBuffers {

struct TextureStock;

struct StoreTexture2D;

struct StoreTextureBuffer;

struct TextureHolder;

struct ShaderVariable;

struct Material;

struct MaterialHolder;

enum class StoreSettings : uint8_t {
  NONE = 0,
  StoreTexture2D = 1,
  StoreTextureBuffer = 2,
  MIN = NONE,
  MAX = StoreTextureBuffer
};

inline const StoreSettings (&EnumValuesStoreSettings())[3] {
  static const StoreSettings values[] = {
    StoreSettings::NONE,
    StoreSettings::StoreTexture2D,
    StoreSettings::StoreTextureBuffer
  };
  return values;
}

inline const char * const *EnumNamesStoreSettings() {
  static const char * const names[] = {
    "NONE",
    "StoreTexture2D",
    "StoreTextureBuffer",
    nullptr
  };
  return names;
}

inline const char *EnumNameStoreSettings(StoreSettings e) {
  const size_t index = static_cast<int>(e);
  return EnumNamesStoreSettings()[index];
}

template<typename T> struct StoreSettingsTraits {
  static const StoreSettings enum_value = StoreSettings::NONE;
};

template<> struct StoreSettingsTraits<StoreTexture2D> {
  static const StoreSettings enum_value = StoreSettings::StoreTexture2D;
};

template<> struct StoreSettingsTraits<StoreTextureBuffer> {
  static const StoreSettings enum_value = StoreSettings::StoreTextureBuffer;
};

bool VerifyStoreSettings(flatbuffers::Verifier &verifier, const void *obj, StoreSettings type);
bool VerifyStoreSettingsVector(flatbuffers::Verifier &verifier, const flatbuffers::Vector<flatbuffers::Offset<void>> *values, const flatbuffers::Vector<uint8_t> *types);

enum class TextureUnit : uint8_t {
  UNIT_DIFFUSE = 0,
  UNIT_NORMAL = 1,
  UNIT_SPECULAR = 2,
  UNIT_ENV = 3,
  UNIT_SHADOW = 4,
  UNIT_BUFFER = 5,
  UNIT_CUSTOM = 7,
  MIN = UNIT_DIFFUSE,
  MAX = UNIT_CUSTOM
};

inline const TextureUnit (&EnumValuesTextureUnit())[7] {
  static const TextureUnit values[] = {
    TextureUnit::UNIT_DIFFUSE,
    TextureUnit::UNIT_NORMAL,
    TextureUnit::UNIT_SPECULAR,
    TextureUnit::UNIT_ENV,
    TextureUnit::UNIT_SHADOW,
    TextureUnit::UNIT_BUFFER,
    TextureUnit::UNIT_CUSTOM
  };
  return values;
}

inline const char * const *EnumNamesTextureUnit() {
  static const char * const names[] = {
    "UNIT_DIFFUSE",
    "UNIT_NORMAL",
    "UNIT_SPECULAR",
    "UNIT_ENV",
    "UNIT_SHADOW",
    "UNIT_BUFFER",
    "",
    "UNIT_CUSTOM",
    nullptr
  };
  return names;
}

inline const char *EnumNameTextureUnit(TextureUnit e) {
  const size_t index = static_cast<int>(e);
  return EnumNamesTextureUnit()[index];
}

struct TextureStock FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_IMAGE = 4,
    VT_FORMAT = 6,
    VT_INTERNAL_FORMAT = 8,
    VT_WIDTH = 10,
    VT_HEIGHT = 12
  };
  const flatbuffers::Vector<uint8_t> *image() const {
    return GetPointer<const flatbuffers::Vector<uint8_t> *>(VT_IMAGE);
  }
  int32_t format() const {
    return GetField<int32_t>(VT_FORMAT, 0);
  }
  int32_t internal_format() const {
    return GetField<int32_t>(VT_INTERNAL_FORMAT, 0);
  }
  uint32_t width() const {
    return GetField<uint32_t>(VT_WIDTH, 0);
  }
  uint32_t height() const {
    return GetField<uint32_t>(VT_HEIGHT, 0);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_IMAGE) &&
           verifier.Verify(image()) &&
           VerifyField<int32_t>(verifier, VT_FORMAT) &&
           VerifyField<int32_t>(verifier, VT_INTERNAL_FORMAT) &&
           VerifyField<uint32_t>(verifier, VT_WIDTH) &&
           VerifyField<uint32_t>(verifier, VT_HEIGHT) &&
           verifier.EndTable();
  }
};

struct TextureStockBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_image(flatbuffers::Offset<flatbuffers::Vector<uint8_t>> image) {
    fbb_.AddOffset(TextureStock::VT_IMAGE, image);
  }
  void add_format(int32_t format) {
    fbb_.AddElement<int32_t>(TextureStock::VT_FORMAT, format, 0);
  }
  void add_internal_format(int32_t internal_format) {
    fbb_.AddElement<int32_t>(TextureStock::VT_INTERNAL_FORMAT, internal_format, 0);
  }
  void add_width(uint32_t width) {
    fbb_.AddElement<uint32_t>(TextureStock::VT_WIDTH, width, 0);
  }
  void add_height(uint32_t height) {
    fbb_.AddElement<uint32_t>(TextureStock::VT_HEIGHT, height, 0);
  }
  explicit TextureStockBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  TextureStockBuilder &operator=(const TextureStockBuilder &);
  flatbuffers::Offset<TextureStock> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<TextureStock>(end);
    fbb_.Required(o, TextureStock::VT_IMAGE);
    return o;
  }
};

inline flatbuffers::Offset<TextureStock> CreateTextureStock(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> image = 0,
    int32_t format = 0,
    int32_t internal_format = 0,
    uint32_t width = 0,
    uint32_t height = 0) {
  TextureStockBuilder builder_(_fbb);
  builder_.add_height(height);
  builder_.add_width(width);
  builder_.add_internal_format(internal_format);
  builder_.add_format(format);
  builder_.add_image(image);
  return builder_.Finish();
}

inline flatbuffers::Offset<TextureStock> CreateTextureStockDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<uint8_t> *image = nullptr,
    int32_t format = 0,
    int32_t internal_format = 0,
    uint32_t width = 0,
    uint32_t height = 0) {
  return SE::FlatBuffers::CreateTextureStock(
      _fbb,
      image ? _fbb.CreateVector<uint8_t>(*image) : 0,
      format,
      internal_format,
      width,
      height);
}

struct StoreTexture2D FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_WRAP = 4,
    VT_MIN_FILTER = 6,
    VT_MAG_FILTER = 8,
    VT_MIPMAP = 10
  };
  int32_t wrap() const {
    return GetField<int32_t>(VT_WRAP, 0);
  }
  int32_t min_filter() const {
    return GetField<int32_t>(VT_MIN_FILTER, 0);
  }
  int32_t mag_filter() const {
    return GetField<int32_t>(VT_MAG_FILTER, 0);
  }
  bool mipmap() const {
    return GetField<uint8_t>(VT_MIPMAP, 0) != 0;
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<int32_t>(verifier, VT_WRAP) &&
           VerifyField<int32_t>(verifier, VT_MIN_FILTER) &&
           VerifyField<int32_t>(verifier, VT_MAG_FILTER) &&
           VerifyField<uint8_t>(verifier, VT_MIPMAP) &&
           verifier.EndTable();
  }
};

struct StoreTexture2DBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_wrap(int32_t wrap) {
    fbb_.AddElement<int32_t>(StoreTexture2D::VT_WRAP, wrap, 0);
  }
  void add_min_filter(int32_t min_filter) {
    fbb_.AddElement<int32_t>(StoreTexture2D::VT_MIN_FILTER, min_filter, 0);
  }
  void add_mag_filter(int32_t mag_filter) {
    fbb_.AddElement<int32_t>(StoreTexture2D::VT_MAG_FILTER, mag_filter, 0);
  }
  void add_mipmap(bool mipmap) {
    fbb_.AddElement<uint8_t>(StoreTexture2D::VT_MIPMAP, static_cast<uint8_t>(mipmap), 0);
  }
  explicit StoreTexture2DBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  StoreTexture2DBuilder &operator=(const StoreTexture2DBuilder &);
  flatbuffers::Offset<StoreTexture2D> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<StoreTexture2D>(end);
    return o;
  }
};

inline flatbuffers::Offset<StoreTexture2D> CreateStoreTexture2D(
    flatbuffers::FlatBufferBuilder &_fbb,
    int32_t wrap = 0,
    int32_t min_filter = 0,
    int32_t mag_filter = 0,
    bool mipmap = false) {
  StoreTexture2DBuilder builder_(_fbb);
  builder_.add_mag_filter(mag_filter);
  builder_.add_min_filter(min_filter);
  builder_.add_wrap(wrap);
  builder_.add_mipmap(mipmap);
  return builder_.Finish();
}

struct StoreTextureBuffer FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           verifier.EndTable();
  }
};

struct StoreTextureBufferBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  explicit StoreTextureBufferBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  StoreTextureBufferBuilder &operator=(const StoreTextureBufferBuilder &);
  flatbuffers::Offset<StoreTextureBuffer> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<StoreTextureBuffer>(end);
    return o;
  }
};

inline flatbuffers::Offset<StoreTextureBuffer> CreateStoreTextureBuffer(
    flatbuffers::FlatBufferBuilder &_fbb) {
  StoreTextureBufferBuilder builder_(_fbb);
  return builder_.Finish();
}

struct TextureHolder FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_STOCK = 4,
    VT_PATH = 6,
    VT_NAME = 8,
    VT_STORE_TYPE = 10,
    VT_STORE = 12,
    VT_UNIT = 14
  };
  const TextureStock *stock() const {
    return GetPointer<const TextureStock *>(VT_STOCK);
  }
  const flatbuffers::String *path() const {
    return GetPointer<const flatbuffers::String *>(VT_PATH);
  }
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  StoreSettings store_type() const {
    return static_cast<StoreSettings>(GetField<uint8_t>(VT_STORE_TYPE, 0));
  }
  const void *store() const {
    return GetPointer<const void *>(VT_STORE);
  }
  template<typename T> const T *store_as() const;
  const StoreTexture2D *store_as_StoreTexture2D() const {
    return store_type() == StoreSettings::StoreTexture2D ? static_cast<const StoreTexture2D *>(store()) : nullptr;
  }
  const StoreTextureBuffer *store_as_StoreTextureBuffer() const {
    return store_type() == StoreSettings::StoreTextureBuffer ? static_cast<const StoreTextureBuffer *>(store()) : nullptr;
  }
  TextureUnit unit() const {
    return static_cast<TextureUnit>(GetField<uint8_t>(VT_UNIT, 0));
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_STOCK) &&
           verifier.VerifyTable(stock()) &&
           VerifyOffset(verifier, VT_PATH) &&
           verifier.Verify(path()) &&
           VerifyOffset(verifier, VT_NAME) &&
           verifier.Verify(name()) &&
           VerifyField<uint8_t>(verifier, VT_STORE_TYPE) &&
           VerifyOffset(verifier, VT_STORE) &&
           VerifyStoreSettings(verifier, store(), store_type()) &&
           VerifyField<uint8_t>(verifier, VT_UNIT) &&
           verifier.EndTable();
  }
};

template<> inline const StoreTexture2D *TextureHolder::store_as<StoreTexture2D>() const {
  return store_as_StoreTexture2D();
}

template<> inline const StoreTextureBuffer *TextureHolder::store_as<StoreTextureBuffer>() const {
  return store_as_StoreTextureBuffer();
}

struct TextureHolderBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_stock(flatbuffers::Offset<TextureStock> stock) {
    fbb_.AddOffset(TextureHolder::VT_STOCK, stock);
  }
  void add_path(flatbuffers::Offset<flatbuffers::String> path) {
    fbb_.AddOffset(TextureHolder::VT_PATH, path);
  }
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(TextureHolder::VT_NAME, name);
  }
  void add_store_type(StoreSettings store_type) {
    fbb_.AddElement<uint8_t>(TextureHolder::VT_STORE_TYPE, static_cast<uint8_t>(store_type), 0);
  }
  void add_store(flatbuffers::Offset<void> store) {
    fbb_.AddOffset(TextureHolder::VT_STORE, store);
  }
  void add_unit(TextureUnit unit) {
    fbb_.AddElement<uint8_t>(TextureHolder::VT_UNIT, static_cast<uint8_t>(unit), 0);
  }
  explicit TextureHolderBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  TextureHolderBuilder &operator=(const TextureHolderBuilder &);
  flatbuffers::Offset<TextureHolder> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<TextureHolder>(end);
    return o;
  }
};

inline flatbuffers::Offset<TextureHolder> CreateTextureHolder(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<TextureStock> stock = 0,
    flatbuffers::Offset<flatbuffers::String> path = 0,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    StoreSettings store_type = StoreSettings::NONE,
    flatbuffers::Offset<void> store = 0,
    TextureUnit unit = TextureUnit::UNIT_DIFFUSE) {
  TextureHolderBuilder builder_(_fbb);
  builder_.add_store(store);
  builder_.add_name(name);
  builder_.add_path(path);
  builder_.add_stock(stock);
  builder_.add_unit(unit);
  builder_.add_store_type(store_type);
  return builder_.Finish();
}

inline flatbuffers::Offset<TextureHolder> CreateTextureHolderDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<TextureStock> stock = 0,
    const char *path = nullptr,
    const char *name = nullptr,
    StoreSettings store_type = StoreSettings::NONE,
    flatbuffers::Offset<void> store = 0,
    TextureUnit unit = TextureUnit::UNIT_DIFFUSE) {
  return SE::FlatBuffers::CreateTextureHolder(
      _fbb,
      stock,
      path ? _fbb.CreateString(path) : 0,
      name ? _fbb.CreateString(name) : 0,
      store_type,
      store,
      unit);
}

struct ShaderVariable FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_NAME = 4,
    VT_FLOAT_VAL = 6,
    VT_VEC2_VAL = 8,
    VT_VEC3_VAL = 10,
    VT_VEC4_VAL = 12,
    VT_UVEC2_VAL = 14,
    VT_UVEC3_VAL = 16,
    VT_UVEC4_VAL = 18
  };
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  float float_val() const {
    return GetField<float>(VT_FLOAT_VAL, 0.0f);
  }
  const Vec2 *vec2_val() const {
    return GetStruct<const Vec2 *>(VT_VEC2_VAL);
  }
  const Vec3 *vec3_val() const {
    return GetStruct<const Vec3 *>(VT_VEC3_VAL);
  }
  const Vec4 *vec4_val() const {
    return GetStruct<const Vec4 *>(VT_VEC4_VAL);
  }
  const UVec2 *uvec2_val() const {
    return GetStruct<const UVec2 *>(VT_UVEC2_VAL);
  }
  const UVec3 *uvec3_val() const {
    return GetStruct<const UVec3 *>(VT_UVEC3_VAL);
  }
  const UVec4 *uvec4_val() const {
    return GetStruct<const UVec4 *>(VT_UVEC4_VAL);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_NAME) &&
           verifier.Verify(name()) &&
           VerifyField<float>(verifier, VT_FLOAT_VAL) &&
           VerifyField<Vec2>(verifier, VT_VEC2_VAL) &&
           VerifyField<Vec3>(verifier, VT_VEC3_VAL) &&
           VerifyField<Vec4>(verifier, VT_VEC4_VAL) &&
           VerifyField<UVec2>(verifier, VT_UVEC2_VAL) &&
           VerifyField<UVec3>(verifier, VT_UVEC3_VAL) &&
           VerifyField<UVec4>(verifier, VT_UVEC4_VAL) &&
           verifier.EndTable();
  }
};

struct ShaderVariableBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(ShaderVariable::VT_NAME, name);
  }
  void add_float_val(float float_val) {
    fbb_.AddElement<float>(ShaderVariable::VT_FLOAT_VAL, float_val, 0.0f);
  }
  void add_vec2_val(const Vec2 *vec2_val) {
    fbb_.AddStruct(ShaderVariable::VT_VEC2_VAL, vec2_val);
  }
  void add_vec3_val(const Vec3 *vec3_val) {
    fbb_.AddStruct(ShaderVariable::VT_VEC3_VAL, vec3_val);
  }
  void add_vec4_val(const Vec4 *vec4_val) {
    fbb_.AddStruct(ShaderVariable::VT_VEC4_VAL, vec4_val);
  }
  void add_uvec2_val(const UVec2 *uvec2_val) {
    fbb_.AddStruct(ShaderVariable::VT_UVEC2_VAL, uvec2_val);
  }
  void add_uvec3_val(const UVec3 *uvec3_val) {
    fbb_.AddStruct(ShaderVariable::VT_UVEC3_VAL, uvec3_val);
  }
  void add_uvec4_val(const UVec4 *uvec4_val) {
    fbb_.AddStruct(ShaderVariable::VT_UVEC4_VAL, uvec4_val);
  }
  explicit ShaderVariableBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ShaderVariableBuilder &operator=(const ShaderVariableBuilder &);
  flatbuffers::Offset<ShaderVariable> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<ShaderVariable>(end);
    fbb_.Required(o, ShaderVariable::VT_NAME);
    return o;
  }
};

inline flatbuffers::Offset<ShaderVariable> CreateShaderVariable(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    float float_val = 0.0f,
    const Vec2 *vec2_val = 0,
    const Vec3 *vec3_val = 0,
    const Vec4 *vec4_val = 0,
    const UVec2 *uvec2_val = 0,
    const UVec3 *uvec3_val = 0,
    const UVec4 *uvec4_val = 0) {
  ShaderVariableBuilder builder_(_fbb);
  builder_.add_uvec4_val(uvec4_val);
  builder_.add_uvec3_val(uvec3_val);
  builder_.add_uvec2_val(uvec2_val);
  builder_.add_vec4_val(vec4_val);
  builder_.add_vec3_val(vec3_val);
  builder_.add_vec2_val(vec2_val);
  builder_.add_float_val(float_val);
  builder_.add_name(name);
  return builder_.Finish();
}

inline flatbuffers::Offset<ShaderVariable> CreateShaderVariableDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *name = nullptr,
    float float_val = 0.0f,
    const Vec2 *vec2_val = 0,
    const Vec3 *vec3_val = 0,
    const Vec4 *vec4_val = 0,
    const UVec2 *uvec2_val = 0,
    const UVec3 *uvec3_val = 0,
    const UVec4 *uvec4_val = 0) {
  return SE::FlatBuffers::CreateShaderVariable(
      _fbb,
      name ? _fbb.CreateString(name) : 0,
      float_val,
      vec2_val,
      vec3_val,
      vec4_val,
      uvec2_val,
      uvec3_val,
      uvec4_val);
}

struct Material FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_SHADER = 4,
    VT_TEXTURES = 6,
    VT_VARIABLES = 8
  };
  const ShaderProgramHolder *shader() const {
    return GetPointer<const ShaderProgramHolder *>(VT_SHADER);
  }
  const flatbuffers::Vector<flatbuffers::Offset<TextureHolder>> *textures() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<TextureHolder>> *>(VT_TEXTURES);
  }
  const flatbuffers::Vector<flatbuffers::Offset<ShaderVariable>> *variables() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<ShaderVariable>> *>(VT_VARIABLES);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_SHADER) &&
           verifier.VerifyTable(shader()) &&
           VerifyOffset(verifier, VT_TEXTURES) &&
           verifier.Verify(textures()) &&
           verifier.VerifyVectorOfTables(textures()) &&
           VerifyOffset(verifier, VT_VARIABLES) &&
           verifier.Verify(variables()) &&
           verifier.VerifyVectorOfTables(variables()) &&
           verifier.EndTable();
  }
};

struct MaterialBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_shader(flatbuffers::Offset<ShaderProgramHolder> shader) {
    fbb_.AddOffset(Material::VT_SHADER, shader);
  }
  void add_textures(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<TextureHolder>>> textures) {
    fbb_.AddOffset(Material::VT_TEXTURES, textures);
  }
  void add_variables(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<ShaderVariable>>> variables) {
    fbb_.AddOffset(Material::VT_VARIABLES, variables);
  }
  explicit MaterialBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  MaterialBuilder &operator=(const MaterialBuilder &);
  flatbuffers::Offset<Material> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<Material>(end);
    fbb_.Required(o, Material::VT_SHADER);
    return o;
  }
};

inline flatbuffers::Offset<Material> CreateMaterial(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<ShaderProgramHolder> shader = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<TextureHolder>>> textures = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<ShaderVariable>>> variables = 0) {
  MaterialBuilder builder_(_fbb);
  builder_.add_variables(variables);
  builder_.add_textures(textures);
  builder_.add_shader(shader);
  return builder_.Finish();
}

inline flatbuffers::Offset<Material> CreateMaterialDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<ShaderProgramHolder> shader = 0,
    const std::vector<flatbuffers::Offset<TextureHolder>> *textures = nullptr,
    const std::vector<flatbuffers::Offset<ShaderVariable>> *variables = nullptr) {
  return SE::FlatBuffers::CreateMaterial(
      _fbb,
      shader,
      textures ? _fbb.CreateVector<flatbuffers::Offset<TextureHolder>>(*textures) : 0,
      variables ? _fbb.CreateVector<flatbuffers::Offset<ShaderVariable>>(*variables) : 0);
}

struct MaterialHolder FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_MATERIAL = 4,
    VT_PATH = 6,
    VT_NAME = 8
  };
  const Material *material() const {
    return GetPointer<const Material *>(VT_MATERIAL);
  }
  const flatbuffers::String *path() const {
    return GetPointer<const flatbuffers::String *>(VT_PATH);
  }
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_MATERIAL) &&
           verifier.VerifyTable(material()) &&
           VerifyOffset(verifier, VT_PATH) &&
           verifier.Verify(path()) &&
           VerifyOffset(verifier, VT_NAME) &&
           verifier.Verify(name()) &&
           verifier.EndTable();
  }
};

struct MaterialHolderBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_material(flatbuffers::Offset<Material> material) {
    fbb_.AddOffset(MaterialHolder::VT_MATERIAL, material);
  }
  void add_path(flatbuffers::Offset<flatbuffers::String> path) {
    fbb_.AddOffset(MaterialHolder::VT_PATH, path);
  }
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(MaterialHolder::VT_NAME, name);
  }
  explicit MaterialHolderBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  MaterialHolderBuilder &operator=(const MaterialHolderBuilder &);
  flatbuffers::Offset<MaterialHolder> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<MaterialHolder>(end);
    return o;
  }
};

inline flatbuffers::Offset<MaterialHolder> CreateMaterialHolder(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<Material> material = 0,
    flatbuffers::Offset<flatbuffers::String> path = 0,
    flatbuffers::Offset<flatbuffers::String> name = 0) {
  MaterialHolderBuilder builder_(_fbb);
  builder_.add_name(name);
  builder_.add_path(path);
  builder_.add_material(material);
  return builder_.Finish();
}

inline flatbuffers::Offset<MaterialHolder> CreateMaterialHolderDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<Material> material = 0,
    const char *path = nullptr,
    const char *name = nullptr) {
  return SE::FlatBuffers::CreateMaterialHolder(
      _fbb,
      material,
      path ? _fbb.CreateString(path) : 0,
      name ? _fbb.CreateString(name) : 0);
}

inline bool VerifyStoreSettings(flatbuffers::Verifier &verifier, const void *obj, StoreSettings type) {
  switch (type) {
    case StoreSettings::NONE: {
      return true;
    }
    case StoreSettings::StoreTexture2D: {
      auto ptr = reinterpret_cast<const StoreTexture2D *>(obj);
      return verifier.VerifyTable(ptr);
    }
    case StoreSettings::StoreTextureBuffer: {
      auto ptr = reinterpret_cast<const StoreTextureBuffer *>(obj);
      return verifier.VerifyTable(ptr);
    }
    default: return false;
  }
}

inline bool VerifyStoreSettingsVector(flatbuffers::Verifier &verifier, const flatbuffers::Vector<flatbuffers::Offset<void>> *values, const flatbuffers::Vector<uint8_t> *types) {
  if (!values || !types) return !values && !types;
  if (values->size() != types->size()) return false;
  for (flatbuffers::uoffset_t i = 0; i < values->size(); ++i) {
    if (!VerifyStoreSettings(
        verifier,  values->Get(i), types->GetEnum<StoreSettings>(i))) {
      return false;
    }
  }
  return true;
}

inline const SE::FlatBuffers::Material *GetMaterial(const void *buf) {
  return flatbuffers::GetRoot<SE::FlatBuffers::Material>(buf);
}

inline const SE::FlatBuffers::Material *GetSizePrefixedMaterial(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<SE::FlatBuffers::Material>(buf);
}

inline const char *MaterialIdentifier() {
  return "SEMT";
}

inline bool MaterialBufferHasIdentifier(const void *buf) {
  return flatbuffers::BufferHasIdentifier(
      buf, MaterialIdentifier());
}

inline bool VerifyMaterialBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<SE::FlatBuffers::Material>(MaterialIdentifier());
}

inline bool VerifySizePrefixedMaterialBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<SE::FlatBuffers::Material>(MaterialIdentifier());
}

inline const char *MaterialExtension() {
  return "semt";
}

inline void FinishMaterialBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::Material> root) {
  fbb.Finish(root, MaterialIdentifier());
}

inline void FinishSizePrefixedMaterialBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::Material> root) {
  fbb.FinishSizePrefixed(root, MaterialIdentifier());
}

}  // namespace FlatBuffers
}  // namespace SE

#endif  // FLATBUFFERS_GENERATED_MATERIAL_SE_FLATBUFFERS_H_