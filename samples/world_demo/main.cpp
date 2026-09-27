#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#define SE_IMPL
#include <GlobalTypes.h>
#include <application.h>
#include "Scene.h"

std::shared_ptr<spdlog::logger> gLogger;

int main(int argc, char ** argv) {

        gLogger = spdlog::stdout_logger_mt("WDemo");
        gLogger->set_level(spdlog::level::debug);

        namespace bpo = boost::program_options;

        bpo::options_description desc("world_demo options");
        desc.add_options()
                ("help",         "print this help and exit")
                ("world",        bpo::value<std::string>()->default_value("resource/world/world_demo.seworld"),
                                 "world file (.seworld); the _no_streaming variant runs the same content with streaming off")
                ("ibl_irr",      bpo::value<std::string>()->default_value(""), "irradiance cubemap KTX file (<path>.ktx)")
                ("ibl_ld",       bpo::value<std::string>()->default_value(""), "prefiltered specular IBL KTX file (<path>.ktx)")
                ("ibl_scale",    bpo::value<float>()->default_value(1.0f),     "IBL intensity multiplier (default 1.0)")
                ("ibl_rotation", bpo::value<float>()->default_value(0.0f),     "IBL yaw rotation in degrees")
                ("fullscreen",   "launch in fullscreen desktop mode instead of a window")
                ;

        // The world path may also be given positionally.
        bpo::positional_options_description pos;
        pos.add("world", 1);

        bpo::variables_map vm;
        try {
                bpo::store(bpo::command_line_parser(argc, argv).options(desc).positional(pos).run(), vm);
                bpo::notify(vm);
        }
        catch (const bpo::error & ex) {
                log_e("world_demo: {}", ex.what());
                std::cout << desc << std::endl;
                return 2;
        }

        if (vm.count("help")) {
                std::cout << desc << std::endl;
                std::cout
                        << "Controls:\n"
                        << "  W/S/A/D move   Space jump   Shift sprint   mouse look (RMB or ESC unlocks)\n"
                        << "  E              interact (open/close the nearest door)\n"
                        << "  F3             debug panels   G LOD overlay   B navmesh   P stress-spawn\n"
                        << "  ESC            unlock mouse, second press quits\n"
                        << "\nWalk into the bell arch to ring it (villagers investigate), enter the west\n"
                        << "camp to raise the alarm (guards give chase). Doors: press E to open — the inn\n"
                        << "loads its interior, the tower crypt swaps the streamed world.\n"
                        << std::endl;
                return 0;
        }

        const std::string sWorld      = vm["world"].as<std::string>();
        const std::string sIblIrr     = vm["ibl_irr"].as<std::string>();
        const std::string sIblLd      = vm["ibl_ld"].as<std::string>();
        const float       ibl_scale    = vm["ibl_scale"].as<float>();
        const float       ibl_rotation = vm["ibl_rotation"].as<float>();

        SE::TEngine::Instance().Init<SE::Config>();

        SE::SysSettings_t    oSettings;
        SE::Camera::Settings oCamSettings;

        oCamSettings.fov        = 60.0f;
        oCamSettings.near_clip  = 0.1f;
        oCamSettings.far_clip   = 4000.0f;
        oCamSettings.projection = SE::Camera::Projection::PERSPECTIVE;

        oSettings.clear_flag                 = SE::ClearBuffer::COLOR | SE::ClearBuffer::DEPTH;
        oSettings.oWindowSettings.width      = 1920;
        oSettings.oWindowSettings.height     = 1200;
        oSettings.oWindowSettings.bpp        = 24;
        oSettings.oWindowSettings.fullscreen = vm.count("fullscreen") ? 1 : 0;
        oSettings.oWindowSettings.vsync      = true;
        oSettings.oWindowSettings.title      = "World Demo";
        oSettings.sResourceDir               = "resource/";

        SE::GetSystem<SE::Config>().sResourceDir = "resource/";

        try {
                SE::Scene::Settings oSceneSettings;
                oSceneSettings.oCamSettings  = oCamSettings;
                oSceneSettings.sWorldPath    = sWorld;
                oSceneSettings.sIblIrrPath   = sIblIrr;
                oSceneSettings.sIblLdPath    = sIblLd;
                oSceneSettings.ibl_scale     = ibl_scale;
                oSceneSettings.ibl_rotation  = ibl_rotation;

                SE::Application<SE::Scene> App(oSettings, oSceneSettings);
        }
        catch (std::exception & ex) {
                log_e("exception caught = {}", ex.what());
        }
        catch (...) {
                log_e("unknown exception caught");
        }

        return 0;
}
