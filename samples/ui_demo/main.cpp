
#include <stdint.h>

#define SE_IMPL
#include <GlobalTypes.h>
#include <application.h>
#include "Scene.h"

std::shared_ptr<spdlog::logger> gLogger;


int main(int argc, char ** argv) {

        gLogger = spdlog::stdout_logger_mt("G");
        gLogger->set_level(spdlog::level::debug);

        SE::TEngine::Instance().Init<SE::Config>();

        SE::SysSettings_t    oSettings;
        SE::Camera::Settings oCamSettings;

        oCamSettings.fov        = 45;
        oCamSettings.near_clip  = 0.1;
        oCamSettings.far_clip   = 1000;
        oCamSettings.projection = SE::Camera::Projection::PERSPECTIVE;

        oSettings.clear_flag                 = SE::ClearBuffer::COLOR | SE::ClearBuffer::DEPTH;
        oSettings.oWindowSettings.width      = 1920;
        oSettings.oWindowSettings.height     = 1080;
        oSettings.oWindowSettings.bpp        = 24;
        oSettings.oWindowSettings.fullscreen = 0;
        oSettings.oWindowSettings.title      = "UI Demo";
        oSettings.sResourceDir               = "resource/";
        oSettings.hide_mouse                 = false;
        oSettings.grab_mouse                 = false;

        SE::GetSystem<SE::Config>().sResourceDir = "resource/";

        try {
                SE::Application<SE::Scene> App(oSettings, SE::Scene::Settings{oCamSettings});
        }
        catch (std::exception & ex) {
                log_e("exception: {}", ex.what());
        }
        catch (...) {
                log_e("unknown exception");
        }

        return 0;
}
