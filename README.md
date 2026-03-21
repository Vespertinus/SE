
# SimpleEngine

simple rendering engine for inhouse usage.
support rendering using OpenGL on GPU and on CPU with OSMesa

C++17 required (at least gcc7)

Dependency:
 * General:
   * [Loki: library written by Andrei Alexandrescu as part of his book Modern C++ Design (TODO - need to switch on boost hana or spirit)](http://loki-lib.sourceforge.net/)
   * [Boost: widely known C++ libraries pack](http://www.boost.org/)
   * [glm:  OpenGL Mathematics](https://glm.g-truc.net)
   * [spdlog: fast C++ logging library](https://github.com/gabime/spdlog)
   * [OpenCV: Open Source Computer Vision Library](https://opencv.org/)
   * OpenGL: usually shiped with graphics drivers 
     * or with open source realisation - mesa
   * [FlatBuffers: memory efficient cross platform serialization library](https://github.com/google/flatbuffers)
 * GPU enabled build only:
   * [SDL: Simple DirectMedia Layer](https://github.com/libsdl-org/SDL)
 * CPU enabled build only:
   * [OSMesa: part of Mesa 3D Graphics Library](https://www.mesa3d.org/)
 * Tools:
   * [Autodesk fbxsdk: for fbx importing. Tested with FBX SDK 2018](https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2018-0)
   * [tinyobjloader: wavefront obj (and mtl) format parser](https://github.com/syoyo/tinyobjloader)
   * [tinygltf: tiny glTF library(loader/saver)](https://github.com/syoyo/tinygltf)
   * [nlohmann/json: JSON for Modern C++](https://github.com/nlohmann/json)
 * Physics (optional):
   * [JoltPhysics: multi-core friendly rigid body physics and collision detection library](https://github.com/jrouwe/JoltPhysics)
 * Audio (optional):
   * [OpenAL Soft: cross-platform 3D audio API implementation](https://github.com/kcat/openal-soft)
   * [libopus: Opus audio codec library](https://opus-codec.org/)
   * [dr_libs: single file audio decoding libraries (FLAC, MP3, WAV)](https://github.com/mackron/dr_libs)
   * [stb_vorbis: Ogg Vorbis audio decoder](https://github.com/nothings/stb)

