#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

#include <opencv2/opencv.hpp>

#define SE_IMPL
#include <GlobalTypes.h>
#include <OffScreenApplication.h>
#include "Scene.h"

std::shared_ptr<spdlog::logger> gLogger;


int main(int argc, char **argv) {

        gLogger = spdlog::stdout_logger_mt("G");
        gLogger->set_level(spdlog::level::debug);

        std::vector<GLubyte>    vRenderBuffer;
        SE::SysSettings_t       oSettings(vRenderBuffer);
        SE::Camera::Settings    oCamSettings;


        oSettings.width                 = 512;
        oSettings.height                = 512;
        oCamSettings.fov                = 90;
        oCamSettings.near_clip          = 0.1;
        oCamSettings.far_clip           = 100;
        oCamSettings.projection         = SE::Camera::Projection::ORTHO;
        //oCamSettings.projection       = SE::Camera::Projection::PERSPECTIVE;

        oSettings.clear_flag			        = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
        oSettings.sResourceDir                          = "resource/";

        log_d("{}: settings: output width = {}, height = {}, clip near = {}, far = {}",
                        argv[0],
                        oSettings.width,
                        oSettings.height,
                        oCamSettings.near_clip,
                        oCamSettings.far_clip);

        try {

                SE::OffScreenApplication<SAMPLES::Scene> App(oSettings, SAMPLES::Scene::Settings{oCamSettings});
                App.Run();

                cv::Mat oMat(oSettings.width, oSettings.height, CV_8UC4);
                oMat.data = &vRenderBuffer[0];

                cv::Mat oResMat(oSettings.width, oSettings.height, CV_8UC4);
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
