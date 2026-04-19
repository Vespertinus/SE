
#ifndef APP_ENTITY_TEMPLATE_PATCHER_H
#define APP_ENTITY_TEMPLATE_PATCHER_H 1

#include <string>
#include <string_view>
#include <vector>

#include <Component_generated.h>

namespace SE {

struct PathSegment {
        std::string name;
        int         index = -1;   // >= 0 → numeric array index
        std::string key;          // non-empty → lookup by .name field in array
};

// Path parsing and component name → enum lookup used by EntityTemplateSystem.
// Override application is dispatched via THasApplyField (ComponentLoader.h) directly
// in EntityTemplateSystem::MergeVariant — no navigator classes needed.
class EntityTemplateUtility {

public:
        EntityTemplateUtility() = default;

        static std::vector<PathSegment> ParsePath(std::string_view path);
        static FlatBuffers::ComponentU  ComponentNameToEnum(std::string_view name);
};

} // namespace SE

#endif
