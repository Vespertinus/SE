
#include <iostream>

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
#include "FlatBuffersSceneTreeWriter.h"


std::shared_ptr<spdlog::logger> gLogger;

const SE::TOOLS::MeshData * GetMesh(const SE::TOOLS::NodeData & oRoot);


int main(int argc, char **argv) {

        using std::string;
        using namespace SE::TOOLS;

        gLogger = spdlog::stdout_logger_mt("Init");
        gLogger->set_level(spdlog::level::info);

        ImportCtx       oCtx{};
        string          sInput;
        string          sOutput;
        bool            to_scene;
        bool            to_mesh;

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
                        ("skin",        bpo::value<bool>()->default_value(false),               "import mesh skin")
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
                        oCtx.sCutPath = vm["cut_path"].as<string>();
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

                        log_i("Imported: node cnt: {}, mesh cnt: {}, total triangles cnt: {}, total_vertices_cnt: {}, materials cnt: {}, textures cnt: {}",
                                        oCtx.node_cnt,
                                        oCtx.mesh_cnt,
                                        oCtx.total_triangles_cnt,
                                        oCtx.total_vertices_cnt,
                                        oCtx.material_cnt,
                                        oCtx.textures_cnt);

                        if (to_scene) {
                                NodeData oRoot,
                                         oChild;
                                oRoot.sName = "RootNode";
                                oRoot.scale = glm::vec3(1, 1, 1);

                                oChild.sName = "obj";
                                oChild.scale = glm::vec3(1, 1, 1);
                                oChild.vComponents.emplace_back(std::move(oModelData));
                                oRoot.vChildren.emplace_back(std::move(oChild));

                                err_code = WriteSceneTree(sOutput + ".sesc", oRoot);
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

                        log_i("Imported: node cnt: {}, mesh cnt: {}, total triangles cnt: {}, total_vertices_cnt: {}, materials cnt: {}, textures cnt: {}",
                                        oCtx.node_cnt,
                                        oCtx.mesh_cnt,
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
                                err_code = WriteSceneTree(sOutput + ".sesc", oRoot);
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

