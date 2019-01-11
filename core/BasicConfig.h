
#ifndef __CONFIG_H__
#define __CONFIG_H__ 1

namespace SE {
/*
 TODO
 temporary configuration stub

 rewrite on confetti like configuration parser generator
 from .tmpl to struct
 */
struct Config {

        /** configuration render surface size,
            actual inside TGraphicsState */
        uint32_t                width           = 1024,
                                height          = 768;

        std::string             sResourceDir    = "resource/";


};


} //namespace SE

#endif




