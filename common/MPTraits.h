
#ifndef MP_TRAITS_H
#define MP_TRAITS_H

#include <experimental/type_traits>

namespace MP {

template<typename T> using TNameCheck = decltype( &T::Name );

template<typename T> constexpr bool HasName = std::experimental::is_detected_v< TNameCheck, T>;
static const std::string sDefault{"<undefined>"};

template <class T> const std::string & GetName(T & oItem) {


        if constexpr (HasName< typename std::decay<T>::type >) {
                return oItem.Name();
        }

        return sDefault;//typeid().name();string_view
}

}

#endif
