
#ifdef SE_IMPL

// The TResourcePin<BlackboardAsset> body, deliberately isolated from
// BlackboardAsset.tcc: Create instantiates CreateResource<BlackboardAsset>,
// which only compiles in compositions whose resource typelist lists the asset
// (the pool lookup is a compile-time error elsewhere). Spawner-only TUs include
// BlackboardAsset.tcc for the ctors WITHOUT this file; AI compositions get it
// transitively via BlackboardSystem.tcc.

#include <ai/BlackboardAsset.h>
#include <Logging.h>

namespace SE {

H<BlackboardAsset> TResourcePin<BlackboardAsset>::Create(const THolder* pH) {
        if (pH->path() && pH->path()->size() > 0) {
                return CreateResource<BlackboardAsset>(pH->path()->str());
        }
        if (pH->name() && pH->blackboard()) {
                return CreateResource<BlackboardAsset>(pH->name()->str(), pH->blackboard());
        }

        log_w("ResourcePin<BlackboardAsset>: anonymous inline blackboard cannot be pinned");
        return H<BlackboardAsset>::Null();
}

} // namespace SE

#endif // SE_IMPL
