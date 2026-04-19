
#ifndef CORE_COMPONENT_LOADER_H
#define CORE_COMPONENT_LOADER_H 1

#include <experimental/type_traits>
#include <ErrCode.h>
#include <Logging.h>

namespace SE {

template <class T> using TSerializedCheck = typename T::TSerialized;
template <class T> using THasSerialized   = typename std::experimental::is_detected<TSerializedCheck, T>::type;

template <class T> using TPostLoadCheck = decltype( &T::PostLoad );
template <class T> using THasPostLoad   = typename std::experimental::is_detected<TPostLoadCheck, T>::type;
template <class T> constexpr bool THasPostLoadVal = std::experimental::is_detected_v<TPostLoadCheck, T>;

template <class T> using TApplyFieldCheck = decltype( &T::ApplyField );
template <class T> using THasApplyField   = typename std::experimental::is_detected<TApplyFieldCheck, T>::type;
template <class T> constexpr bool THasApplyFieldVal = std::experimental::is_detected_v<TApplyFieldCheck, T>;

template <class TComponent> struct LoadWrapper {

        using TExactComponent  = TComponent;
        using TExactSerialized = typename TComponent::TSerialized;

        template <class TNode, class TPostLoadVec> ret_code_t Load(
                        const void * const pData,
                        TNode & pNode,
                        TPostLoadVec & vPostLoadComponents) const {

                const TExactSerialized * pSerialized = static_cast<const TExactSerialized *>(pData);
                auto res = pNode->template CreateComponent<TComponent>(pSerialized);

                if constexpr (THasPostLoadVal<TComponent>) {

                        if (res != uSUCCESS) { return res; }

                        auto * pComponent = pNode->template GetComponent<TComponent>();
                        se_assert(pComponent);

                        vPostLoadComponents.emplace_back(pComponent, pSerialized);
                }

                return res;
        }
};

} // namespace SE

#endif
