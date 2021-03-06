// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_SHADERPROGRAM_SE_FLATBUFFERS_H_
#define FLATBUFFERS_GENERATED_SHADERPROGRAM_SE_FLATBUFFERS_H_

#include "flatbuffers/flatbuffers.h"

#include "Common_generated.h"
#include "ShaderComponent_generated.h"

namespace SE {
namespace FlatBuffers {

struct Shader;

struct ShaderProgram;

struct ShaderProgramHolder;

struct Shader FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_NAME = 4,
    VT_DATA = 6
  };
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  const ShaderComponent *data() const {
    return GetPointer<const ShaderComponent *>(VT_DATA);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_NAME) &&
           verifier.Verify(name()) &&
           VerifyOffset(verifier, VT_DATA) &&
           verifier.VerifyTable(data()) &&
           verifier.EndTable();
  }
};

struct ShaderBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(Shader::VT_NAME, name);
  }
  void add_data(flatbuffers::Offset<ShaderComponent> data) {
    fbb_.AddOffset(Shader::VT_DATA, data);
  }
  explicit ShaderBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ShaderBuilder &operator=(const ShaderBuilder &);
  flatbuffers::Offset<Shader> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<Shader>(end);
    fbb_.Required(o, Shader::VT_NAME);
    return o;
  }
};

inline flatbuffers::Offset<Shader> CreateShader(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    flatbuffers::Offset<ShaderComponent> data = 0) {
  ShaderBuilder builder_(_fbb);
  builder_.add_data(data);
  builder_.add_name(name);
  return builder_.Finish();
}

inline flatbuffers::Offset<Shader> CreateShaderDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *name = nullptr,
    flatbuffers::Offset<ShaderComponent> data = 0) {
  return SE::FlatBuffers::CreateShader(
      _fbb,
      name ? _fbb.CreateString(name) : 0,
      data);
}

struct ShaderProgram FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_VERTEX = 4,
    VT_FRAGMENT = 6,
    VT_GEOMETRY = 8
  };
  const Shader *vertex() const {
    return GetPointer<const Shader *>(VT_VERTEX);
  }
  const Shader *fragment() const {
    return GetPointer<const Shader *>(VT_FRAGMENT);
  }
  const Shader *geometry() const {
    return GetPointer<const Shader *>(VT_GEOMETRY);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_VERTEX) &&
           verifier.VerifyTable(vertex()) &&
           VerifyOffsetRequired(verifier, VT_FRAGMENT) &&
           verifier.VerifyTable(fragment()) &&
           VerifyOffset(verifier, VT_GEOMETRY) &&
           verifier.VerifyTable(geometry()) &&
           verifier.EndTable();
  }
};

struct ShaderProgramBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_vertex(flatbuffers::Offset<Shader> vertex) {
    fbb_.AddOffset(ShaderProgram::VT_VERTEX, vertex);
  }
  void add_fragment(flatbuffers::Offset<Shader> fragment) {
    fbb_.AddOffset(ShaderProgram::VT_FRAGMENT, fragment);
  }
  void add_geometry(flatbuffers::Offset<Shader> geometry) {
    fbb_.AddOffset(ShaderProgram::VT_GEOMETRY, geometry);
  }
  explicit ShaderProgramBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ShaderProgramBuilder &operator=(const ShaderProgramBuilder &);
  flatbuffers::Offset<ShaderProgram> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<ShaderProgram>(end);
    fbb_.Required(o, ShaderProgram::VT_VERTEX);
    fbb_.Required(o, ShaderProgram::VT_FRAGMENT);
    return o;
  }
};

inline flatbuffers::Offset<ShaderProgram> CreateShaderProgram(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<Shader> vertex = 0,
    flatbuffers::Offset<Shader> fragment = 0,
    flatbuffers::Offset<Shader> geometry = 0) {
  ShaderProgramBuilder builder_(_fbb);
  builder_.add_geometry(geometry);
  builder_.add_fragment(fragment);
  builder_.add_vertex(vertex);
  return builder_.Finish();
}

struct ShaderProgramHolder FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_SHADER = 4,
    VT_PATH = 6,
    VT_NAME = 8
  };
  const ShaderProgram *shader() const {
    return GetPointer<const ShaderProgram *>(VT_SHADER);
  }
  const flatbuffers::String *path() const {
    return GetPointer<const flatbuffers::String *>(VT_PATH);
  }
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_SHADER) &&
           verifier.VerifyTable(shader()) &&
           VerifyOffset(verifier, VT_PATH) &&
           verifier.Verify(path()) &&
           VerifyOffset(verifier, VT_NAME) &&
           verifier.Verify(name()) &&
           verifier.EndTable();
  }
};

struct ShaderProgramHolderBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_shader(flatbuffers::Offset<ShaderProgram> shader) {
    fbb_.AddOffset(ShaderProgramHolder::VT_SHADER, shader);
  }
  void add_path(flatbuffers::Offset<flatbuffers::String> path) {
    fbb_.AddOffset(ShaderProgramHolder::VT_PATH, path);
  }
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(ShaderProgramHolder::VT_NAME, name);
  }
  explicit ShaderProgramHolderBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ShaderProgramHolderBuilder &operator=(const ShaderProgramHolderBuilder &);
  flatbuffers::Offset<ShaderProgramHolder> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<ShaderProgramHolder>(end);
    return o;
  }
};

inline flatbuffers::Offset<ShaderProgramHolder> CreateShaderProgramHolder(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<ShaderProgram> shader = 0,
    flatbuffers::Offset<flatbuffers::String> path = 0,
    flatbuffers::Offset<flatbuffers::String> name = 0) {
  ShaderProgramHolderBuilder builder_(_fbb);
  builder_.add_name(name);
  builder_.add_path(path);
  builder_.add_shader(shader);
  return builder_.Finish();
}

inline flatbuffers::Offset<ShaderProgramHolder> CreateShaderProgramHolderDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<ShaderProgram> shader = 0,
    const char *path = nullptr,
    const char *name = nullptr) {
  return SE::FlatBuffers::CreateShaderProgramHolder(
      _fbb,
      shader,
      path ? _fbb.CreateString(path) : 0,
      name ? _fbb.CreateString(name) : 0);
}

inline const SE::FlatBuffers::ShaderProgram *GetShaderProgram(const void *buf) {
  return flatbuffers::GetRoot<SE::FlatBuffers::ShaderProgram>(buf);
}

inline const char *ShaderProgramIdentifier() {
  return "SESP";
}

inline bool ShaderProgramBufferHasIdentifier(const void *buf) {
  return flatbuffers::BufferHasIdentifier(
      buf, ShaderProgramIdentifier());
}

inline bool VerifyShaderProgramBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<SE::FlatBuffers::ShaderProgram>(ShaderProgramIdentifier());
}

inline const char *ShaderProgramExtension() {
  return "sesp";
}

inline void FinishShaderProgramBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<SE::FlatBuffers::ShaderProgram> root) {
  fbb.Finish(root, ShaderProgramIdentifier());
}

}  // namespace FlatBuffers
}  // namespace SE

#endif  // FLATBUFFERS_GENERATED_SHADERPROGRAM_SE_FLATBUFFERS_H_
