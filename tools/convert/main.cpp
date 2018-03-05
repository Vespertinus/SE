
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <Logging.h>

#include "OBJReader.h"
#include "FlatBuffersMeshWriter.h"

#include "FBXReader.h"
#include "FlatBuffersSceneTreeWriter.h"


std::shared_ptr<spdlog::logger> gLogger;

int main(int argc, char **argv) {

        using std::string;
        using namespace SE::TOOLS;

        gLogger = spdlog::stdout_logger_mt("Init");
        gLogger->set_level(spdlog::level::info);

        ImportCtx       oCtx{};
        string          sInput;
        string          sOutput;

        try {
                namespace bpo = boost::program_options;

                bpo::options_description desc("Allowed options");

                desc.add_options()
                        ("help", "usage:")
                        ("level",        bpo::value<string>()->default_value("info"),           "log level (debug|info|warn|error)")
                        ("log",          bpo::value<string>()->default_value("stdout"),         "log outout (stdout|stderr|<filename>)")
                        ("input",        bpo::value<string>(),                                  "input file, Mesh or Scene (<filename>)")
                        ("output",       bpo::value<string>()->default_value("output.sem"),     "output file destination (<filename>)")
                        ("skip_normals", bpo::value<bool>()->default_value(false),              "write normals in output file")
                        ("cut_path",     bpo::value<string>(),                                  "substring for cuting imported paths (<search substr>)")
                        ("replace",      bpo::value<string>(),                                  "string for replacing cuted path part (<path>)")
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
                {
                        sOutput = vm["output"].as<string>();
                }
                if (vm.count("input") ) {
                        sInput  = vm["input"].as<string>();
                }
                {
                        oCtx.skip_normals = vm["skip_normals"].as<bool>();
                }
                if (vm.count("cut_path") ) {
                        oCtx.sCutPath = vm["cut_path"].as<string>();
                }
                if (vm.count("replace") ) {
                        oCtx.sReplace = vm["replace"].as<string>();
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

                if (sExt == ".obj") {
                        log_d("file '{}' ext '{}', call OBJLoader", sInput, sExt);
                        MeshData oMeshData;
                        err_code = ReadOBJ(sInput, oMeshData, oCtx);
                        if (err_code) {
                                throw (std::runtime_error("Loading failed, err_code = " + std::to_string(err_code)) );
                        }

                        log_i("Imported: node cnt: {}, mesh cnt: {}, total triangles cnt: {}, textures cnt: {}",
                                        oCtx.node_cnt,
                                        oCtx.mesh_cnt,
                                        oCtx.total_triangles_cnt,
                                        oCtx.textures_cnt);

                        err_code = WriteMesh("test.sems", oMeshData);
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

                        log_i("Imported: node cnt: {}, mesh cnt: {}, total triangles cnt: {}, textures cnt: {}",
                                        oCtx.node_cnt,
                                        oCtx.mesh_cnt,
                                        oCtx.total_triangles_cnt,
                                        oCtx.textures_cnt);

                        err_code = WriteSceneTree("test.sesc", oRoot);
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
