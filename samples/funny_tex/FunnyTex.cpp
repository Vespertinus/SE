

#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

#define SE_IMPL
#include <GlobalTypes.h>
#include <application.h>
#include "OrthoScene.h"

std::shared_ptr<spdlog::logger> gLogger;


int main(int argc, char **argv) {

        gLogger = spdlog::stdout_logger_mt("G");
        gLogger->set_level(spdlog::level::debug);

        SE::SysSettings_t    oSettings;
        SE::Camera::Settings oCamSettings;

        oCamSettings.fov		        = 45;
        oCamSettings.near_clip                  = 0.1;
        oCamSettings.far_clip                   = 2000;
        oCamSettings.projection                 = SE::Camera::Projection::ORTHO;

        oSettings.clear_flag			= GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;

        oSettings.oWindowSettings.width         = 1920;
        oSettings.oWindowSettings.height        = 1080;
        oSettings.oWindowSettings.bpp           = 24;
        oSettings.oWindowSettings.fullscreen    = 0;
        oSettings.oWindowSettings.title         = "Funny Textures";
        oSettings.sResourceDir                  = "resource/";

        //TEMP
        SE::TEngine::Instance().Init<SE::Config>();
        SE::GetSystem<SE::Config>().sResourceDir = "resource/";

        try {

                SE::Application<FUNNY_TEX::OrthoScene> App(oSettings, FUNNY_TEX::OrthoScene::Settings{oCamSettings});
        }
        catch (std::exception & ex) {
                log_e("exception catched = {}", ex.what());
        }
        catch(...) {
                log_e("unknown exception catched");
        }

        return 0;
}
