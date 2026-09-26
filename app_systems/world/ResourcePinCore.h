
#ifndef APP_WORLD_RESOURCE_PIN_CORE_H
#define APP_WORLD_RESOURCE_PIN_CORE_H 1

// ---------------------------------------------------------------------------
// ResourcePinCore — the TResourcePin primary template + detection machinery,
// deliberately free of pin specializations and heavy includes.
//
// An explicit specialization of a non-template class compiles eagerly in every
// TU that sees it, so WHERE a pin lives decides which compositions pay for it:
//   - core-resource pins (Mesh, Material, ...) stay in ResourcePins.h, which is
//     only included from WorldSystem.tcc (a GlobalTypes context);
//   - opt-in app-resource pins (StateMachineAsset, TerrainAsset) live in the
//     resource's own header and vanish from compositions that omit it.
// WorldSystem picks adapters up from the app's resource typelist automatically
// (the SceneTree::LoadFB loader-map pattern); there is no registration list to
// forget. Resources whose adapter is absent never enter the map; scenes
// declaring them get a load-time warning.
// ---------------------------------------------------------------------------

#include <experimental/type_traits>

#include <ResourceHandle.h>

namespace SE {

template <class R> struct TResourcePin;   // unsupported resource types stay undefined

// Type-level detection only: taking &TResourcePin<T>::Create here would force-
// instantiate every pin's body (and its CreateResource pool) even for resources
// the app composition does not contain. Member addresses are still taken for
// types actually in TResourceList (see WorldSystem::RegisterPin).
template <class T> using TResourcePinCheck       = typename TResourcePin<T>::THolder;
template <class T> using THasResourcePin         = typename std::experimental::is_detected<TResourcePinCheck, T>::type;
template <class T> constexpr bool THasResourcePinVal = std::experimental::is_detected_v<TResourcePinCheck, T>;

/// sResourceDir accessor for pin bodies (defined in WorldSystem.tcc, where
/// Config is complete) — pins must not depend on GetSystem<Config> directly.
const std::string & ResourceDir();

} // namespace SE

#endif
