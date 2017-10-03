
#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

#include <spdlog/spdlog.h>

std::shared_ptr<spdlog::logger> gLogger;

#include <application.h>
#include <Scene.h>



int main(int argc, char **argv) {
        
        gLogger = spdlog::stdout_logger_mt("G");
        gLogger->set_level(spdlog::level::debug);

        SE::SysSettings_t oSettings;

        oSettings.oCamSettings.width 			= 1024;
        oSettings.oCamSettings.height			= 768;
        oSettings.oCamSettings.oVolume.fov		= 45;
        oSettings.oCamSettings.oVolume.aspect           = (float)oSettings.oCamSettings.width / (float)oSettings.oCamSettings.height;
        oSettings.oCamSettings.oVolume.near_clip	= 0.1;
        oSettings.oCamSettings.oVolume.far_clip         = 1000;
        oSettings.oCamSettings.oVolume.projection       = SE::Frustum::uPERSPECTIVE;

        oSettings.clear_flag			        = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;

        oSettings.oWindowSettings.width       = oSettings.oCamSettings.width;
        oSettings.oWindowSettings.height      = oSettings.oCamSettings.height;
        oSettings.oWindowSettings.bpp         = 24;
        oSettings.oWindowSettings.fullscreen  = 0;
        oSettings.oWindowSettings.title       = "Simple Engine DEMO (2010.05.13)";
        oSettings.sResourceDir                = "resource/";



        try {

                SE::Application<SE::Scene> App(oSettings, SE::Scene::Settings());
        }
        catch (std::exception & ex) {
                log_e("exception catched = {}", ex.what());
        }
        catch(...) {
                log_e("unknown exception catched");
        }

        return 0;
}
