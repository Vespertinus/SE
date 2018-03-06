
#include "Common.h"


namespace SE {
namespace TOOLS {

void ImportCtx::FixPath(std::string & sPath) {

        if (sCutPath.empty()) { return; }

        if (auto pos = sPath.rfind(sCutPath); pos != std::string::npos) {
                sPath = sReplace + sPath.substr(pos + sCutPath.size());
        }
}


}
}
