
#include <iostream>
#include <unordered_set>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <Logging.h>
#include <spdlog/sinks/rotating_file_sink.h>

#define SE_IMPL
#include <GeometryUtil.h>
#include <StrID.h>

#include "OBJReader.h"
#include "FlatBuffersMeshWriter.h"

#include "FBXReader.h"
#include "GLTFReader.h"
#include "FlatBuffersSceneTreeWriter.h"
#include "FlatBuffersAnimWriter.h"
#include "SceneImport.h"


std::shared_ptr<spdlog::logger> gLogger;

const SE::TOOLS::MeshData * GetMesh(const SE::TOOLS::NodeData & oRoot);

static void CollectSkeletons(const SE::TOOLS::NodeData & oNode,
                              std::unordered_set<SE::TOOLS::Skeleton*> & oOut) {
        for (const auto & comp : oNode.vComponents) {
                if (const auto * pModel = std::get_if<SE::TOOLS::ModelData>(&comp)) {
                        if (pModel->pSkin && pModel->pSkin->pSkeleton) {
                                oOut.insert(pModel->pSkin->pSkeleton);
                        }
                }
        }
        for (const auto & child : oNode.vChildren) {
                CollectSkeletons(child, oOut);
        }
}


int main(int argc, char **argv) {

        using std::string;
        using namespace SE::TOOLS;

        gLogger = spdlog::stdout_logger_mt("Init");
        gLogger->set_level(spdlog::level::info);

        ImportCtx               oCtx{};
        string                  sInput;
        string                  sOutput;
        bool                    to_scene;
        bool                    to_mesh;
        SE::TOOLS::SceneImportManifest oManifest;

        try {
                namespace bpo = boost::program_options;

                bpo::options_description desc("Allowed options");

                desc.add_options()
                        ("help", "usage:")
                        ("level",        bpo::value<string>()->default_value("info"),           "log level (debug|info|warn|error)")
                        ("log",          bpo::value<string>()->default_value("stdout"),         "log outout (stdout|stderr|<filename>)")
                        ("input",        bpo::value<string>(),                                  "input file, Mesh or Scene (<filename>)")
                        ("output",       bpo::value<string>(),                                  "output file destination (<filename>)")
                        ("skip_normals", bpo::value<bool>()->default_value(false),              "does not write normals to output file")
                        ("skip_material", bpo::value<bool>()->default_value(false),             "does not import materials")
                        ("flip_yz",      bpo::value<bool>()->default_value(false),              "flip yz axes, depend on what axis might be up")
                        ("cut_path",     bpo::value<string>(),                                  "regex for cuting imported paths (<search substr>)")
                        ("replace",      bpo::value<string>(),                                  "string for replacing cuted path part (<path>)")
                        ("to_scene",     bpo::value<bool>()->default_value(false),              "write mesh as scene")
                        ("to_mesh",      bpo::value<bool>()->default_value(false),              "write first mesh inside scene as mesh file")
                        ("info_prop",    bpo::value<bool>()->default_value(true),               "import custom data from fbx node property ('info') as string")
                        ("blendshapes",  bpo::value<bool>()->default_value(false),              "import blend shapes (morph target)")
                        ("disable_nodes", bpo::value<bool>()->default_value(false),             "all scene nodes stored in disabled state")
                        ("skin",           bpo::value<bool>()->default_value(false),            "import mesh skin")
                        ("animations",     bpo::value<bool>()->default_value(false),            "export animation clips to .seak files (requires --skin)")
                        ("anim_rate",      bpo::value<float>()->default_value(30.0f),           "animation sample rate in fps (default 30)")
                        ("skeleton_inline", bpo::value<bool>()->default_value(false),           "embed skeleton inline in scene file instead of a separate .sesk")
                        ("inline_all",      bpo::value<bool>()->default_value(false),           "embed all resources (textures, clips, skeleton) in a single .sesc prefab")
                        ;

                bpo::variables_map vm;
                bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
                bpo::notify(vm);

                if (vm.count("help") ||
                                vm.size() == 0 ||
                                !vm.count("input") ||
                                (vm.count("replace") && !vm.count("cut_path"))
                   ) {
                        std::cout << desc << std::endl;
                        std::cout << "input file must be set" << std::endl;
                        return 1;
                }

                {
                        const string & log_type = vm["log"].as<string>();
                        if (log_type == "stdout") {
                                gLogger = spdlog::stdout_logger_mt("G");
                        }
                        else if (log_type == "stderr") {
                                gLogger = spdlog::stderr_logger_mt("G");
                        }
                        else {
                                gLogger = spdlog::rotating_logger_mt("G", log_type, 1024 * 1024 * 1024, 10);
                        }
                }
                {
                        const string & level = vm["level"].as<string>();
                        if (level == "debug") {
                                gLogger->set_level(spdlog::level::debug);
                        }
                        else if (level == "info") {
                                gLogger->set_level(spdlog::level::info);
                        }
                        else if (level == "warn") {
                                gLogger->set_level(spdlog::level::warn);
                        }
                        else if (level == "error") {
                                gLogger->set_level(spdlog::level::err);
                        }
                        else {
                                log_w("unknown log level: '{}'", level);
                        }

                }
                if (vm.count("output") ) {
                        sOutput = vm["output"].as<string>();
                }
                if (vm.count("input") ) {
                        sInput  = vm["input"].as<string>();
                }
                if (vm.count("cut_path") ) {
                        oCtx.oCutPath = vm["cut_path"].as<string>();
                }
                if (vm.count("replace") ) {
                        oCtx.sReplace = vm["replace"].as<string>();
                }

                oCtx.skip_normals               = vm["skip_normals"].as<bool>();
                oCtx.skip_material              = vm["skip_material"].as<bool>();
                oCtx.flip_yz                    = vm["flip_yz"].as<bool>();
                to_scene                        = vm["to_scene"].as<bool>();
                to_mesh                         = vm["to_mesh"].as<bool>();
                oCtx.import_info_prop           = vm["info_prop"].as<bool>();
                oCtx.import_blend_shapes        = vm["blendshapes"].as<bool>();
                oCtx.disable_nodes              = vm["disable_nodes"].as<bool>();
                oCtx.import_skin                = vm["skin"].as<bool>();
                oCtx.import_animations          = vm["animations"].as<bool>();
                oCtx.anim_sample_rate           = vm["anim_rate"].as<float>();
                oCtx.skeleton_inline            = vm["skeleton_inline"].as<bool>();
                oCtx.inline_all                 = vm["inline_all"].as<bool>();

                // --- Sidecar (.sceneimport): load, then re-apply explicit CLI overrides ---
                // 1. Copy CLI-resolved values into manifest as starting point
                oManifest.skip_normals    = oCtx.skip_normals;
                oManifest.skip_material   = oCtx.skip_material;
                oManifest.flip_yz         = oCtx.flip_yz;
                oManifest.to_scene        = to_scene;
                oManifest.to_mesh         = to_mesh;
                oManifest.info_prop       = oCtx.import_info_prop;
                oManifest.blendshapes     = oCtx.import_blend_shapes;
                oManifest.disable_nodes   = oCtx.disable_nodes;
                oManifest.skin            = oCtx.import_skin;
                oManifest.animations      = oCtx.import_animations;
                oManifest.anim_rate       = oCtx.anim_sample_rate;
                oManifest.skeleton_inline = oCtx.skeleton_inline;
                oManifest.inline_all      = oCtx.inline_all;
                if (vm.count("cut_path"))  oManifest.cut_path = vm["cut_path"].as<string>();
                if (vm.count("replace"))   oManifest.replace  = vm["replace"].as<string>();

                // 2. Sidecar overrides (only fields present in JSON)
                SE::TOOLS::ReadManifest(SE::TOOLS::SidecarPath(sInput), oManifest);

                // 3. Re-apply any CLI flags that were explicitly set (not just defaulted)
                if (!vm["skip_normals"].defaulted())    oManifest.skip_normals    = vm["skip_normals"].as<bool>();
                if (!vm["skip_material"].defaulted())   oManifest.skip_material   = vm["skip_material"].as<bool>();
                if (!vm["flip_yz"].defaulted())         oManifest.flip_yz         = vm["flip_yz"].as<bool>();
                if (!vm["to_scene"].defaulted())        oManifest.to_scene        = vm["to_scene"].as<bool>();
                if (!vm["to_mesh"].defaulted())         oManifest.to_mesh         = vm["to_mesh"].as<bool>();
                if (!vm["info_prop"].defaulted())       oManifest.info_prop       = vm["info_prop"].as<bool>();
                if (!vm["blendshapes"].defaulted())     oManifest.blendshapes     = vm["blendshapes"].as<bool>();
                if (!vm["disable_nodes"].defaulted())   oManifest.disable_nodes   = vm["disable_nodes"].as<bool>();
                if (!vm["skin"].defaulted())            oManifest.skin            = vm["skin"].as<bool>();
                if (!vm["animations"].defaulted())      oManifest.animations      = vm["animations"].as<bool>();
                if (!vm["anim_rate"].defaulted())       oManifest.anim_rate       = vm["anim_rate"].as<float>();
                if (!vm["skeleton_inline"].defaulted()) oManifest.skeleton_inline = vm["skeleton_inline"].as<bool>();
                if (!vm["inline_all"].defaulted())      oManifest.inline_all      = vm["inline_all"].as<bool>();
                if (vm.count("cut_path"))               oManifest.cut_path        = vm["cut_path"].as<string>();
                if (vm.count("replace"))                oManifest.replace         = vm["replace"].as<string>();

                // 4. Apply manifest back to oCtx
                oCtx.skip_normals         = oManifest.skip_normals;
                oCtx.skip_material        = oManifest.skip_material;
                oCtx.flip_yz              = oManifest.flip_yz;
                to_scene                  = oManifest.to_scene;
                to_mesh                   = oManifest.to_mesh;
                oCtx.import_info_prop     = oManifest.info_prop;
                oCtx.import_blend_shapes  = oManifest.blendshapes;
                oCtx.disable_nodes        = oManifest.disable_nodes;
                oCtx.import_skin          = oManifest.skin;
                oCtx.import_animations    = oManifest.animations;
                oCtx.anim_sample_rate     = oManifest.anim_rate;
                oCtx.skeleton_inline      = oManifest.skeleton_inline;
                oCtx.inline_all           = oManifest.inline_all;
                // inline_all forces scene output with skeleton embedded
                if (oCtx.inline_all) {
                        to_scene             = true;
                        oManifest.to_scene   = true;
                        oCtx.skeleton_inline = true;
                        oManifest.skeleton_inline = true;
                }
                if (!oManifest.cut_path.empty()) oCtx.oCutPath = std::regex(oManifest.cut_path);
                oCtx.sReplace             = oManifest.replace;
                // Per-clip settings
                for (auto & [name, cs] : oManifest.clips) {
                        oCtx.mClipSettings[name].loop = cs.loop;
                }
        }
        catch (std::exception & ex) {
                log_e("parsing input exception catched, reason: '{}'", ex.what());
                return -1;
        }
        catch(...) {
                log_e("parsing input unknown exception catched");
                return -1;
        }

        try {
                SE::ret_code_t          err_code;
                boost::filesystem::path oPath(sInput);
                string                  sExt = oPath.extension().string();
                std::transform(sExt.begin(), sExt.end(), sExt.begin(), ::tolower);
                if (sOutput.empty()) {
                        sOutput = oPath.stem().string();
                }
                oCtx.sPackName = "hs:" + std::to_string(SE::StrID(sInput)) + "|";

                if (sExt == ".obj") {
                        log_d("file '{}' ext '{}', call OBJLoader", sInput, sExt);
                        auto pMesh = std::make_unique<MeshData>();
                        ModelData oModelData;
                        oModelData.pMesh = pMesh.get();
                        err_code = ReadOBJ(sInput, oModelData, oCtx);
                        if (err_code) {
                                throw (std::runtime_error("Loading failed, err_code = " + std::to_string(err_code)) );
                        }

                        log_i("Imported: node cnt: {}, mesh cnt: {}, shape cnt: {}, total triangles cnt: {}, total_vertices_cnt: {}, materials cnt: {}, textures cnt: {}",
                                        oCtx.node_cnt,
                                        oCtx.mesh_cnt,
                                        oCtx.shape_cnt,
                                        oCtx.total_triangles_cnt,
                                        oCtx.total_vertices_cnt,
                                        oCtx.material_cnt,
                                        oCtx.textures_cnt);

                        if (to_scene) {
                                NodeData oRoot,
                                         oChild;
                                oRoot.sName = "RootNode";
                                oRoot.vScale = glm::vec3(1, 1, 1);

                                oChild.sName = "obj";
                                oChild.vScale = glm::vec3(1, 1, 1);
                                oChild.vComponents.emplace_back(std::move(oModelData));
                                oRoot.vChildren.emplace_back(std::move(oChild));

                                err_code = WriteSceneTree(sOutput + ".sesc", oRoot, oCtx);
                        }
                        else {
                                err_code = WriteMesh(sOutput + ".sems", *oModelData.pMesh);
                        }
                        if (err_code) {
                                throw (std::runtime_error("Write failed, err_code = " + std::to_string(err_code)) );
                        }

                }
                else if (sExt == ".fbx") {
                        FBXReader oReader;
                        NodeData oRoot;
                        err_code = oReader.ReadScene(sInput, oRoot, oCtx);
                        if (err_code) {
                                throw (std::runtime_error("Loading failed, err_code = " + std::to_string(err_code)) );
                        }

                        log_i("Imported: node cnt: {}, mesh cnt: {}, shape cnt: {}, total triangles cnt: {}, total_vertices_cnt: {}, materials cnt: {}, textures cnt: {}",
                                        oCtx.node_cnt,
                                        oCtx.mesh_cnt,
                                        oCtx.shape_cnt,
                                        oCtx.total_triangles_cnt,
                                        oCtx.total_vertices_cnt,
                                        oCtx.material_cnt,
                                        oCtx.textures_cnt);

                        if (to_mesh) {
                                auto * pMeshData = GetMesh(oRoot);
                                if (!pMeshData) {
                                        throw(std::runtime_error("failed to find mesh inside scene"));
                                }
                                err_code = WriteMesh(sOutput + ".sems", *pMeshData);
                        }
                        else {
                                // Write skeleton files before scene (separate mode only)
                                if (!oCtx.skeleton_inline) {
                                        std::unordered_set<SE::TOOLS::Skeleton*> oSkels;
                                        CollectSkeletons(oRoot, oSkels);
                                        int skelIdx = 0;
                                        for (auto * pSkel : oSkels) {
                                                string sFull = sOutput
                                                        + (oSkels.size() > 1
                                                                ? "_skeleton_" + std::to_string(skelIdx++)
                                                                : "_skeleton")
                                                        + ".sesk";
                                                err_code = WriteSkeleton(sFull, *pSkel);
                                                if (err_code) {
                                                        throw(std::runtime_error("WriteSkeleton failed"));
                                                }
                                                string sRel = sFull;
                                                oCtx.FixPath(sRel);
                                                oCtx.mSkeletonPaths[pSkel] = sRel;
                                        }
                                }
                                if (!oCtx.inline_all) {
                                        oCtx.vAnimClipPaths.resize(oCtx.vAnimClips.size());
                                        for (size_t i = 0; i < oCtx.vAnimClips.size(); ++i) {
                                                string sClipName = oCtx.vAnimClips[i].sName;
                                                std::replace_if(sClipName.begin(), sClipName.end(),
                                                        [](char c){ return c == '/' || c == '\\' || c == ':' || c == ' '; }, '_');
                                                string sClipPath = sOutput + "_" + sClipName + ".seak";
                                                err_code = WriteAnimClip(sClipPath, oCtx.vAnimClips[i]);
                                                if (err_code) {
                                                        throw (std::runtime_error("WriteAnimClip failed, err_code = " + std::to_string(err_code)));
                                                }
                                                string sRelPath = sClipPath;
                                                oCtx.FixPath(sRelPath);
                                                oCtx.vAnimClipPaths[i] = sRelPath;
                                        }
                                }

                                err_code = WriteSceneTree(sOutput + ".sesc", oRoot, oCtx);
                        }

                        if (err_code) {
                                throw (std::runtime_error("Write failed, err_code = " + std::to_string(err_code)) );
                        }

                }
                else if (sExt == ".gltf" || sExt == ".glb" || sExt == ".bin") {
                        GLTFReader oReader;
                        NodeData oRoot;
                        err_code = oReader.ReadScene(sInput, oRoot, oCtx);
                        if (err_code) {
                                throw (std::runtime_error("Loading failed, err_code = " + std::to_string(err_code)) );
                        }

                        log_i("Imported: node cnt: {}, mesh cnt: {}, shape cnt: {}, total triangles cnt: {}, total_vertices_cnt: {}, materials cnt: {}, textures cnt: {}",
                                        oCtx.node_cnt,
                                        oCtx.mesh_cnt,
                                        oCtx.shape_cnt,
                                        oCtx.total_triangles_cnt,
                                        oCtx.total_vertices_cnt,
                                        oCtx.material_cnt,
                                        oCtx.textures_cnt);

                        if (to_mesh) {
                                auto * pMeshData = GetMesh(oRoot);
                                if (!pMeshData) {
                                        throw(std::runtime_error("failed to find mesh inside scene"));
                                }
                                err_code = WriteMesh(sOutput + ".sems", *pMeshData);
                        }
                        else {
                                // Write skeleton files before scene (separate mode only)
                                if (!oCtx.skeleton_inline) {
                                        std::unordered_set<SE::TOOLS::Skeleton*> oSkels;
                                        CollectSkeletons(oRoot, oSkels);
                                        int skelIdx = 0;
                                        for (auto * pSkel : oSkels) {
                                                string sFull = sOutput
                                                        + (oSkels.size() > 1
                                                                ? "_skeleton_" + std::to_string(skelIdx++)
                                                                : "_skeleton")
                                                        + ".sesk";
                                                err_code = WriteSkeleton(sFull, *pSkel);
                                                if (err_code) {
                                                        throw(std::runtime_error("WriteSkeleton failed"));
                                                }
                                                string sRel = sFull;
                                                oCtx.FixPath(sRel);
                                                oCtx.mSkeletonPaths[pSkel] = sRel;
                                        }
                                }
                                // Compute clip paths / write .seak files before WriteSceneTree
                                // so SerializeAnimatorComponent can reference them.
                                if (!oCtx.inline_all) {
                                        oCtx.vAnimClipPaths.resize(oCtx.vAnimClips.size());
                                        for (size_t i = 0; i < oCtx.vAnimClips.size(); ++i) {
                                                string sClipName = oCtx.vAnimClips[i].sName;
                                                std::replace_if(sClipName.begin(), sClipName.end(),
                                                        [](char c){ return c == '/' || c == '\\' || c == ':' || c == ' '; }, '_');
                                                string sClipPath = sOutput + "_" + sClipName + ".seak";
                                                err_code = WriteAnimClip(sClipPath, oCtx.vAnimClips[i]);
                                                if (err_code) {
                                                        throw (std::runtime_error("WriteAnimClip failed, err_code = " + std::to_string(err_code)));
                                                }
                                                string sRelPath = sClipPath;
                                                oCtx.FixPath(sRelPath);
                                                oCtx.vAnimClipPaths[i] = sRelPath;
                                        }
                                }

                                err_code = WriteSceneTree(sOutput + ".sesc", oRoot, oCtx);
                        }

                        if (err_code) {
                                throw (std::runtime_error("Write failed, err_code = " + std::to_string(err_code)) );
                        }
                }
                else {
                        log_e("unsupported file extension: '{}'", sExt);
                        throw ("unsupported extension: " + sExt);
                }
        }
        catch (std::exception & ex) {
                log_e("convert: exception catched, reason: '{}'", ex.what());
                return -1;
        }
        catch(...) {
                log_e("convert: unknown exception catched");
                return -1;
        }

        SE::TOOLS::WriteManifest(SE::TOOLS::SidecarPath(sInput), oManifest);

        return 0;
}

const SE::TOOLS::MeshData * GetMesh(const SE::TOOLS::NodeData & oRoot) {

        for (auto & oComponent : oRoot.vComponents) {
                if(auto pModelComponent = std::get_if<SE::TOOLS::ModelData>(&oComponent)) {
                        return pModelComponent->pMesh;
                }
        }

        for (auto & oChild : oRoot.vChildren) {

                if (auto * pMesh = GetMesh(oChild)) {
                        return pMesh;
                }
        }

        return nullptr;
}

