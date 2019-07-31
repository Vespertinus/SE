
#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

#define SE_IMPL
#include <GlobalTypes.h>
#include <application.h>
#include "Scene.h"

std::shared_ptr<spdlog::logger> gLogger;


int main(int argc, char **argv) {

        gLogger = spdlog::stdout_logger_mt("G");
        gLogger->set_level(spdlog::level::debug);

        SE::SysSettings_t    oSettings;
        SE::Camera::Settings oCamSettings;

        oCamSettings.fov        = 90;
        oCamSettings.near_clip  = 0.1;
        oCamSettings.far_clip   = 1000;
        oCamSettings.projection = SE::Camera::Projection::ORTHO;
        //oCamSettings.projection       = SE::Camera::Projection::PERSPECTIVE;

        oSettings.clear_flag			        = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;

        oSettings.oWindowSettings.width       = 512;
        oSettings.oWindowSettings.height      = 512;
        oSettings.oWindowSettings.bpp         = 24;
        oSettings.oWindowSettings.fullscreen  = 0;
        oSettings.oWindowSettings.title       = "Off screen rendering test";
        oSettings.sResourceDir                = "resource/";

        try {

                SE::Application<SAMPLES::Scene> App(oSettings, SAMPLES::Scene::Settings{oCamSettings});
        }
        catch (std::exception & ex) {
                log_e("exception catched = {}", ex.what());
        }
        catch(...) {
                log_e("unknown exception catched");
        }

        return 0;
}
