


#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

#include <opencv2/opencv.hpp>

#define SE_IMPL
#include <OffScreenApplication.h>
#include "Scene.h"

std::shared_ptr<spdlog::logger> gLogger;


int main(int argc, char **argv) {

        gLogger = spdlog::stdout_logger_mt("G");
        gLogger->set_level(spdlog::level::debug);

        std::vector<GLubyte> vRenderBuffer;

        SE::SysSettings_t oSettings(vRenderBuffer);

        oSettings.oCamSettings.width 			= 512;
        oSettings.oCamSettings.height			= 512;
        oSettings.oCamSettings.up[0]                    = 0;
        oSettings.oCamSettings.up[1]                    = 0;
        oSettings.oCamSettings.up[2]                    = 1;
        oSettings.oCamSettings.oVolume.fov		= 90;
        oSettings.oCamSettings.oVolume.aspect           = (float)oSettings.oCamSettings.width / (float)oSettings.oCamSettings.height;
        oSettings.oCamSettings.oVolume.near_clip	= 0.1;
        oSettings.oCamSettings.oVolume.far_clip         = 100;
        oSettings.oCamSettings.oVolume.projection       = SE::Frustum::uORTHO;
        //oSettings.oCamSettings.oVolume.projection       = SE::Frustum::uPERSPECTIVE;

        oSettings.clear_flag			        = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
        oSettings.sResourceDir                          = "resource/";

        log_d("{}: settings: output width = {}, height = {}, clip near = {}, far = {}",
                        argv[0],
                        oSettings.oCamSettings.width,
                        oSettings.oCamSettings.height,
                        oSettings.oCamSettings.oVolume.near_clip,
                        oSettings.oCamSettings.oVolume.far_clip);

        try {

                SE::OffScreenApplication<SAMPLES::Scene> App(oSettings, SAMPLES::Scene::Settings());
                App.Run();

                cv::Mat oMat(oSettings.oCamSettings.width, oSettings.oCamSettings.height, CV_8UC4);
                oMat.data = &vRenderBuffer[0];

                cv::Mat oResMat(oSettings.oCamSettings.width, oSettings.oCamSettings.height, CV_8UC4);
                cv::flip(oMat, oResMat, 0);

                cv::imwrite("./output.png", oResMat);
        }
        catch (std::exception & ex) {
                log_e("main: exception catched, reason: '{}'", ex.what());
        }
        catch(...) {
                log_e("main: unknown exception catched");
        }

        return 0;
}
