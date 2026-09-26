
// ---------------------------------------------------------------------------
// terrain_bake — offline terrain baker
//
// Consumes a JSON config describing a heightmap source (png16 / raw16 /
// procedural), bakes the tile grid + LOD chain via the shared TerrainBake
// library (the same code the runtime tests exercise), and writes one .setile
// per tile plus the .seterrain manifest. Byte-deterministic.
//
// Usage: terrain_bake <config.json> [--level L] [--log DEST]
//
// Config example (resource/terrain/demo.json):
// {
//     "id": "demo",
//     "output": "resource/terrain/demo",
//     "source": { "type": "procedural", "size": 513, "seed": 7,
//                 "base_height": 20.0, "amplitude": 90.0 },
//     "tile_size": 512.0,
//     "lod0_cells": 128,
//     "lod_count": 5,
//     "origin": [0, 0, 0],
//     "material_path": "material/terrain.semt",
//     "uv_scale": 0.02,
//     "physics": { "friction": 0.8, "max_slope_deg": 50.0 }
// }
//
// Heightmap conventions (matching World Machine / Gaea / Unreal exports):
//   - HORIZONTAL detail: meters-per-pixel = world_span / (image_size - 1).
//     There is no fixed standard; 1-2 m/px is typical for gameplay terrain
//     (finer detail is wasted — terrain LOD flattens it). The demo world runs
//     640 m over 513 px = 1.25 m/px.
//   - VERTICAL detail: png16/raw16 pixels are NORMALIZED 0..65535; the source
//     keys "height_scale" / "height_offset" map them to meters
//     (meters = raw * height_scale + height_offset). E.g. a ±32 m range over
//     16 bits gives ~1 mm vertical steps. The legacy raw-meters behavior is
//     the default (scale 1.0, offset 0.0).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <terrain/TerrainBake.h>
#include <terrain/TerrainBake.tcc>
#include "HeightmapSource.h"

#include <Logging.h>
#include <boost/program_options.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

std::shared_ptr<spdlog::logger> gLogger;

namespace po = boost::program_options;
using nlohmann::json;

int main(int argc, char * argv[]) {
        gLogger = spdlog::stdout_logger_mt("Init");

        po::options_description oDesc("terrain_bake — bake heightmaps into tiled terrain assets\n\nUsage");
        oDesc.add_options()
                ("help,h", "print help")
                ("config", po::value<std::string>()->required(), "path to bake config (JSON)")
                ("level",  po::value<std::string>()->default_value("info"), "log level: debug|info|warn|error")
                ("log",    po::value<std::string>()->default_value("stdout"), "log destination: stdout|stderr|<file>");

        po::positional_options_description oPos;
        oPos.add("config", 1);

        po::variables_map oVm;
        try {
                po::store(po::command_line_parser(argc, argv).options(oDesc).positional(oPos).run(), oVm);
                if (oVm.count("help")) { std::cout << oDesc << "\n"; return 0; }
                po::notify(oVm);
        }
        catch (const std::exception & ex) {
                std::cerr << ex.what() << "\n" << oDesc << "\n";
                return 1;
        }

        // log routing (house convention)
        {
                const std::string sLevel = oVm["level"].as<std::string>();
                gLogger->set_level(spdlog::level::from_str(sLevel));
                const std::string sLog = oVm["log"].as<std::string>();
                if (sLog == "stderr") {
                        gLogger = spdlog::stderr_logger_mt("Init");
                } else if (sLog != "stdout") {
                        gLogger = spdlog::basic_logger_mt("Init", sLog);
                }
                gLogger->set_level(spdlog::level::from_str(sLevel));
        }

        // ---- config ---------------------------------------------------------
        const std::string sConfigPath = oVm["config"].as<std::string>();
        json oJson;
        {
                std::ifstream f(sConfigPath);
                if (!f.is_open()) {
                        log_e("terrain_bake: cannot open config '{}'", sConfigPath);
                        return 1;
                }
                f >> oJson;
        }

        SE::TerrainBake::SConfig oCfg;
        oCfg.id = oJson.value("id", "terrain");
        oCfg.tile_size = oJson.value("tile_size", 512.0f);
        oCfg.lod0_cells = oJson.value("lod0_cells", 128u);
        oCfg.lod_count = oJson.value("lod_count", 5u);
        oCfg.material_path = oJson.value("material_path", "");
        oCfg.uv_scale = oJson.value("uv_scale", 0.05f);
        if (oJson.contains("origin") && oJson["origin"].is_array() && oJson["origin"].size() == 3) {
                oCfg.origin = glm::vec3(oJson["origin"][0].get<float>(),
                                        oJson["origin"][1].get<float>(),
                                        oJson["origin"][2].get<float>());
        }
        if (oJson.contains("physics")) {
                oCfg.friction = oJson["physics"].value("friction", 0.8f);
                oCfg.max_slope_deg = oJson["physics"].value("max_slope_deg", 50.0f);
        }

        // ---- source heights ---------------------------------------------------
        se_terrain_bake::SSourceDesc oSrc;
        if (oJson.contains("source")) {
                const json & jSrc = oJson["source"];
                oSrc.type = jSrc.value("type", "procedural");
                oSrc.path = jSrc.value("path", "");
                oSrc.width = jSrc.value("width", 0u);
                oSrc.height = jSrc.value("height", 0u);
                oSrc.seed = jSrc.value("seed", uint64_t(1));
                oSrc.size = jSrc.value("size", 513u);
                oSrc.frequency = jSrc.value("frequency", 3.0f);
                oSrc.octaves = jSrc.value("octaves", 5u);
                oSrc.lacunarity = jSrc.value("lacunarity", 2.0f);
                oSrc.gain = jSrc.value("gain", 0.5f);
                oSrc.base_height = jSrc.value("base_height", 20.0f);
                oSrc.amplitude = jSrc.value("amplitude", 60.0f);
                // png16/raw16 exchange formats are normalized 0..65535 (World
                // Machine / Gaea exports); scale maps them to meters.
                oSrc.height_scale = jSrc.value("height_scale", 1.0f);
                oSrc.height_offset = jSrc.value("height_offset", 0.0f);
        }

        std::vector<float> vHeights;
        uint32_t src_w = 0, src_h = 0;
        if (!se_terrain_bake::LoadHeightmap(oSrc, vHeights, src_w, src_h)) {
                return 1;
        }
        oCfg.source_width = src_w;
        oCfg.source_height = src_h;

        // ---- bake -------------------------------------------------------------
        SE::TerrainBake::SOutput oOut;
        if (!SE::TerrainBake::Bake(vHeights.data(), oCfg, oOut)) {
                return 1;
        }

        // ---- write ------------------------------------------------------------
        const std::string sOutDir = oJson.value("output", "resource/terrain/" + oCfg.id);
        std::filesystem::create_directories(sOutDir);

        for (const auto & oTile : oOut.vTiles) {
                const std::string sTilePath = sOutDir + "/" + oCfg.id + "_tile_"
                                + std::to_string(oTile.x) + "_" + std::to_string(oTile.y) + ".setile";
                std::ofstream f(sTilePath, std::ios::binary | std::ios::trunc);
                if (!f.is_open() || !f.write(reinterpret_cast<const char *>(oTile.vBytes.data()),
                                             std::streamsize(oTile.vBytes.size()))) {
                        log_e("terrain_bake: failed to write '{}'", sTilePath);
                        return 1;
                }
        }

        const std::string sManifestPath = sOutDir + "/" + oCfg.id + ".seterrain";
        {
                std::ofstream f(sManifestPath, std::ios::binary | std::ios::trunc);
                if (!f.is_open() || !f.write(reinterpret_cast<const char *>(oOut.vManifest.data()),
                                             std::streamsize(oOut.vManifest.size()))) {
                        log_e("terrain_bake: failed to write '{}'", sManifestPath);
                        return 1;
                }
        }

        log_i("terrain_bake: wrote {} tiles + manifest to '{}' ({}x{} tiles, {} LODs, "
              "heights {:.1f}..{:.1f} m)",
              oOut.vTiles.size(), sOutDir, oOut.tiles_x, oOut.tiles_y,
              oCfg.lod_count, oOut.height_min, oOut.height_max);
        return 0;
}
