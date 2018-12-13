
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <spdlog/sinks/rotating_file_sink.h>

#define SE_IMPL
#include <application.h>
#include "Scene.h"

std::shared_ptr<spdlog::logger> gLogger;

int main(int argc, char **argv) {

        using std::string;

        gLogger = spdlog::stdout_logger_mt("Init");
        gLogger->set_level(spdlog::level::info);

        SE::SysSettings_t oSettings;
        string            sScenePath;
        bool              vdebug;


        try {
                namespace bpo = boost::program_options;

                bpo::options_description desc("Allowed options");

                desc.add_options()
                        ("help", "usage:")
                        ("level",        bpo::value<string>()->default_value("info"),           "log level (debug|info|warn|error)")
                        ("log",          bpo::value<string>()->default_value("stdout"),         "log outout (stdout|stderr|<filename>)")
                        ("scene",        bpo::value<string>(),                                  "SceneTree file for visualisation (<filename.sesc>)")
                        ("ortho",        bpo::value<bool>()->default_value(false),              "orthogonal projection")
                        ("resource",     bpo::value<string>()->default_value("resource/"),      "resource dir (<dir path>)")
                        ("vdebug",       bpo::value<bool>()->default_value(false),              "visual debug helpers")
                        ;

                bpo::variables_map vm;
                bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
                bpo::notify(vm);

                if (vm.count("help") || vm.size() == 0 || !vm.count("scene") ) {
                        std::cout << desc << std::endl;
                        std::cout << "scene file must be set" << std::endl;
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
                if (vm.count("scene") ) {
                        sScenePath  = vm["scene"].as<string>();
                }
                vdebug = vm["vdebug"].as<bool>();

                SE::TEngine::Instance().Init<SE::Config>();

                oSettings.oCamSettings.width 			= 1920;
                oSettings.oCamSettings.height			= 1200;
                oSettings.oCamSettings.up[0]                    = 0;
                oSettings.oCamSettings.up[1]                    = 0;
                oSettings.oCamSettings.up[2]                    = 1;
                oSettings.oCamSettings.oVolume.fov		= 45;
                oSettings.oCamSettings.oVolume.aspect           = (float)oSettings.oCamSettings.width / (float)oSettings.oCamSettings.height;
                oSettings.oCamSettings.oVolume.near_clip	= 0.1;
                oSettings.oCamSettings.oVolume.far_clip         = 2000;
                oSettings.oCamSettings.oVolume.projection       = (vm["ortho"].as<bool>()) ?
                        SE::Frustum::uORTHO :
                        SE::Frustum::uPERSPECTIVE;

                oSettings.clear_flag                            = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
                oSettings.hide_mouse                            = false;
                oSettings.grab_mouse                            = false;

                oSettings.oWindowSettings.width       = oSettings.oCamSettings.width;
                oSettings.oWindowSettings.height      = oSettings.oCamSettings.height;
                oSettings.oWindowSettings.bpp         = 24;
                oSettings.oWindowSettings.fullscreen  = false;
                oSettings.oWindowSettings.title       = "Scene Viewer";
                oSettings.sResourceDir                = vm["resource"].as<string>();

                //TEMP
                SE::GetSystem<SE::Config>().sResourceDir = vm["resource"].as<string>();


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
                SE::Application<SE::TOOLS::Scene> App(oSettings, SE::TOOLS::Scene::Settings{sScenePath, vdebug});
        }
        catch (std::exception & ex) {
                log_e("exception catched = {}", ex.what());
        }
        catch(...) {
                log_e("unknown exception catched");
        }

        return 0;
}


