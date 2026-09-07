#include "NormalizeSkel.h"

#include <Logging.h>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <stdexcept>

std::shared_ptr<spdlog::logger> gLogger;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

// ---------------------------------------------------------------------------
// mode: generate-canonical
// Copies a reference skeleton as-is to produce the canonical .sesk.
// ---------------------------------------------------------------------------
static int ModeGenerateCanonical(const std::string& ref_path, const std::string& out_path) {
        auto skel = SE::TOOLS::NormSkel::LoadSkeleton(ref_path);
        SE::TOOLS::NormSkel::SaveSkeleton(out_path, *skel);
        log_i("normalize_skel: canonical skeleton written to '{}'", out_path);
        return 0;
}

// ---------------------------------------------------------------------------
// mode: add-mask
// Adds (or replaces) a named bone mask in a .sesk skeleton. Every bone whose
// name does NOT start with one of the --exclude-bones prefixes gets weight 1;
// excluded bones are omitted from the entry list (lookup pads with 0).
// Replaces the asset-pipeline gap where layers referenced masks no skeleton
// defined (silently degrading to all-bones overrides).
// ---------------------------------------------------------------------------
static int ModeAddMask(
                const std::string& skel_path,
                const std::string& mask_name,
                const std::vector<std::string>& vExcludePrefixes) {

        auto skel = SE::TOOLS::NormSkel::LoadSkeleton(skel_path);

        // Replace an existing mask of the same name
        skel->masks.erase(
                        std::remove_if(skel->masks.begin(), skel->masks.end(),
                                [&mask_name](const auto& pMask) { return pMask && pMask->name == mask_name; }),
                        skel->masks.end());

        const auto is_excluded = [&](const std::string& bone_name) {
                for (const auto& prefix : vExcludePrefixes) {
                        if (bone_name.compare(0, prefix.size(), prefix) == 0) return true;
                }
                return false;
        };

        auto pMask = std::make_unique<SE::FlatBuffers::BoneMaskT>();
        pMask->name = mask_name;
        uint16_t included = 0;
        for (uint16_t i = 0; i < skel->bones.size(); ++i) {
                const auto& pBone = skel->bones[i];
                if (!pBone || is_excluded(pBone->name)) continue;
                auto pEntry = std::make_unique<SE::FlatBuffers::BoneMaskEntryT>();
                pEntry->bone_index = i;
                pEntry->weight     = 1.0f;
                pMask->entries.push_back(std::move(pEntry));
                ++included;
        }
        skel->masks.push_back(std::move(pMask));

        SE::TOOLS::NormSkel::SaveSkeleton(skel_path, *skel);
        log_i("normalize_skel: mask '{}' written to '{}' ({}/{} bones included)",
                        mask_name, skel_path, included, skel->bones.size());
        return 0;
}

// ---------------------------------------------------------------------------
// mode: character
// Normalizes a character model's skeleton to the canonical bind pose.
// Does NOT modify animation clips.
// ---------------------------------------------------------------------------
static int ModeCharacter(const std::string& scene_path, const std::string& canon_path,
                const std::string& resource_root) {
        auto canonical = SE::TOOLS::NormSkel::LoadSkeleton(canon_path);
        const auto canon_map = SE::TOOLS::NormSkel::BuildCanonicalMap(*canonical);

        auto scene = SE::TOOLS::NormSkel::LoadScene(scene_path);

        // Collect all unique skeleton paths from AnimatedModel components
        std::unordered_map<std::string, std::string> skel_paths; // asset-relative path → same
        std::function<void(const SE::FlatBuffers::NodeT&)> collect;
        collect = [&](const SE::FlatBuffers::NodeT& node) {
                for (const auto& pComp : node.components) {
                        if (!pComp) continue;
                        const auto* pAM = pComp->component.AsAnimatedModel();
                        if (!pAM || !pAM->skeleton) continue;
                        const auto& holder = *pAM->skeleton;
                        if (!holder.path.empty())
                                skel_paths[holder.path] = holder.path;
                }
                for (const auto& pChild : node.children)
                        if (pChild) collect(*pChild);
        };
        if (scene->root) collect(*scene->root);

        if (skel_paths.empty()) {
                log_e("normalize_skel: no path-referenced skeletons found in '{}'", scene_path);
                return 1;
        }

        for (const auto& [skel_path, _] : skel_paths) {
                log_i("normalize_skel: normalizing skeleton '{}'", skel_path);
                const auto fs_path = resource_root + "/" + skel_path;
                auto skel = SE::TOOLS::NormSkel::LoadSkeleton(fs_path);
                SE::TOOLS::NormSkel::NormalizeSkeletonBones(*skel, canon_map);
                SE::TOOLS::NormSkel::UpdateSceneInvBinds(*scene, skel_path, *skel);
                SE::TOOLS::NormSkel::SaveSkeleton(fs_path, *skel);
        }

        SE::TOOLS::NormSkel::SaveScene(scene_path, *scene);
        return 0;
}

// ---------------------------------------------------------------------------
// mode: anim-source
// Normalizes the animation source skeleton AND retargets all referenced clips
// to be expressed in canonical bind-pose space.
// ---------------------------------------------------------------------------
static int ModeAnimSource(const std::string& scene_path, const std::string& canon_path,
                const std::string& resource_root) {
        auto canonical = SE::TOOLS::NormSkel::LoadSkeleton(canon_path);
        const auto canon_map = SE::TOOLS::NormSkel::BuildCanonicalMap(*canonical);

        auto scene = SE::TOOLS::NormSkel::LoadScene(scene_path);

        // Collect skeleton paths (same as character mode)
        std::vector<std::string> skel_paths;
        std::function<void(const SE::FlatBuffers::NodeT&)> collect;
        collect = [&](const SE::FlatBuffers::NodeT& node) {
                for (const auto& pComp : node.components) {
                        if (!pComp) continue;
                        const auto* pAM = pComp->component.AsAnimatedModel();
                        if (!pAM || !pAM->skeleton) continue;
                        const auto& holder = *pAM->skeleton;
                        if (!holder.path.empty()) skel_paths.push_back(holder.path);
                }
                for (const auto& pChild : node.children)
                        if (pChild) collect(*pChild);
        };
        if (scene->root) collect(*scene->root);

        // Deduplicate
        std::sort(skel_paths.begin(), skel_paths.end());
        skel_paths.erase(std::unique(skel_paths.begin(), skel_paths.end()), skel_paths.end());

        if (skel_paths.empty()) {
                log_e("normalize_skel: no path-referenced skeletons found in '{}'", scene_path);
                return 1;
        }

        for (const auto& skel_path : skel_paths) {
                log_i("normalize_skel: processing anim source skeleton '{}'", skel_path);

                const auto fs_path = resource_root + "/" + skel_path;
                auto skel = SE::TOOLS::NormSkel::LoadSkeleton(fs_path);

                // Build correction BEFORE normalizing (uses original source bind rotations)
                const auto correction = SE::TOOLS::NormSkel::BuildCorrectionMap(*skel, canon_map);

                // Extract source bind positions for translation delta conversion
                SE::TOOLS::NormSkel::BindPosVec src_bind_pos;
                src_bind_pos.reserve(skel->bones.size());
                for (const auto& pBone : skel->bones) {
                        if (pBone && pBone->bind_pos)
                                src_bind_pos.emplace_back(pBone->bind_pos->x(),
                                                pBone->bind_pos->y(),
                                                pBone->bind_pos->z());
                        else
                                src_bind_pos.emplace_back(0.0f, 0.0f, 0.0f);
                }

                // Retarget all clips found in the scene's animation graphs
                SE::TOOLS::NormSkel::RetargetSceneClips(*scene, correction, src_bind_pos, resource_root);

                // Normalize the skeleton itself and update scene inv binds
                SE::TOOLS::NormSkel::NormalizeSkeletonBones(*skel, canon_map);
                SE::TOOLS::NormSkel::UpdateSceneInvBinds(*scene, skel_path, *skel);
                SE::TOOLS::NormSkel::SaveSkeleton(fs_path, *skel);
        }

        SE::TOOLS::NormSkel::SaveScene(scene_path, *scene);
        return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
        po::options_description general("Options");
        general.add_options()
                ("help,h",      "show help")
                ("ref-skel",    po::value<std::string>(), "path to canonical .sesk file")
                ("output",      po::value<std::string>(), "output path (generate-canonical only)")
                ("mask-name",   po::value<std::string>(), "mask name (add-mask mode)")
                ("exclude-bones", po::value<std::string>()->default_value(""),
                 "comma-separated bone-name prefixes to exclude from the mask (add-mask mode)")
                ("resource-root", po::value<std::string>()->default_value("resource"),
                 "resource directory prefix prepended to asset paths stored in scene files");

        po::options_description hidden;
        hidden.add_options()
                ("mode",        po::value<std::string>(), "mode")
                ("input",       po::value<std::string>(), "input file");

        po::positional_options_description pos;
        pos.add("mode",  1);
        pos.add("input", 1);

        po::options_description all;
        all.add(general).add(hidden);

        po::variables_map vm;
        try {
                po::store(po::command_line_parser(argc, argv).options(all).positional(pos).run(), vm);
                po::notify(vm);
        } catch (const std::exception& e) {
                std::cerr << "normalize_skel: " << e.what() << "\n";
                return 1;
        }

        gLogger = spdlog::stdout_logger_mt("G");
        gLogger->set_level(spdlog::level::info);

        if (vm.count("help") || !vm.count("mode") || !vm.count("input")) {
                std::cout <<
                        "Usage:\n"
                        "  normalize_skel generate-canonical <ref.sesk> --output <canonical.sesk>\n"
                        "  normalize_skel character          <scene.sesc> --ref-skel <canonical.sesk>\n"
                        "  normalize_skel anim-source        <scene.sesc> --ref-skel <canonical.sesk>\n"
                        "  normalize_skel add-mask           <skel.sesk> --mask-name <name> [--exclude-bones a,b,c]\n"
                        "\n"
                        "Modes:\n"
                        "  generate-canonical  Copy a reference skeleton as the canonical profile.\n"
                        "  character           Normalize a character model's skeleton bind pose (no clip changes).\n"
                        "  anim-source         Normalize skeleton AND retarget all referenced animation clips.\n"
                        "  add-mask            Add (or replace) a named bone mask; bones not matching any\n"
                        "                      --exclude-bones prefix get weight 1.\n"
                        "\n"
                        << general << "\n";
                return 0;
        }

        const auto mode          = vm["mode"].as<std::string>();
        const auto input         = vm["input"].as<std::string>();
        const auto resource_root = vm["resource-root"].as<std::string>();

        try {
                if (mode == "generate-canonical") {
                        if (!vm.count("output")) {
                                std::cerr << "normalize_skel: --output required for generate-canonical\n";
                                return 1;
                        }
                        return ModeGenerateCanonical(input, vm["output"].as<std::string>());
                }

                if (mode == "add-mask") {
                        if (!vm.count("mask-name")) {
                                std::cerr << "normalize_skel: --mask-name required for add-mask\n";
                                return 1;
                        }
                        std::vector<std::string> vExcludes;
                        const std::string sExcludes = vm["exclude-bones"].as<std::string>();
                        size_t pos = 0;
                        while (pos < sExcludes.size()) {
                                size_t comma = sExcludes.find(',', pos);
                                std::string token = sExcludes.substr(
                                                pos, comma == std::string::npos ? std::string::npos : comma - pos);
                                if (!token.empty()) vExcludes.push_back(token);
                                if (comma == std::string::npos) break;
                                pos = comma + 1;
                        }
                        return ModeAddMask(input, vm["mask-name"].as<std::string>(), vExcludes);
                }

                if (mode == "character" || mode == "anim-source") {
                        if (!vm.count("ref-skel")) {
                                std::cerr << "normalize_skel: --ref-skel required for mode '" << mode << "'\n";
                                return 1;
                        }
                        const auto& ref = vm["ref-skel"].as<std::string>();
                        return (mode == "character")
                                ? ModeCharacter(input, ref, resource_root)
                                : ModeAnimSource(input, ref, resource_root);
                }

                std::cerr << "normalize_skel: unknown mode '" << mode
                        << "' (expected: generate-canonical, character, anim-source)\n";
                return 1;

        } catch (const std::exception& e) {
                log_e("normalize_skel: {}", e.what());
                return 1;
        }
}
