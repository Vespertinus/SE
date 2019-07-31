
#ifndef __MP_UTIL_H__
#define __MP_UTIL_H__ 1

#include <loki/Typelist.h>
#include <variant>

namespace MP {

template <class TList, template < class > class Holder> struct ExtendList;
/*
template <> struct ExtendList<Loki::NullType, Loki::NullType> {
	typedef Loki::NullType TExtendedList;
};
*/
template <class Head, template <class> class Holder> struct ExtendList
  <Loki::Typelist<Head, Loki::NullType>, Holder > {
	
  typedef Loki::Typelist< Holder<Head>, Loki::NullType> TExtendedList;
};

template <class Head, class Tail, template <class> class Holder> struct ExtendList
  <Loki::Typelist<Head, Tail>, Holder > {

	typedef Loki::Typelist< Holder<Head>, typename ExtendList<Tail, Holder > ::TExtendedList > TExtendedList;
};



template <class T, class V> struct IsEqual {
  enum { value = false };
};

template <class T> struct IsEqual<T, T> {
  enum {value=true};
};



template <class TList, class T, bool Equal> struct SearchByChildType;

template <class Head, class Tail, class T, bool Equal> struct SearchByChildType <Loki::Typelist <Head, Tail>, T, Equal> {

  typedef typename SearchByChildType<Tail, T, IsEqual<T, typename Tail::Head::TChild >::value >::Result Result;
};

template <class Head, class Tail, class T> struct SearchByChildType<Loki::Typelist<Head, Tail>, T, true> {

  typedef Head Result;
};

template < class TList, class T> struct InnerSearch;

template < class Head, class Tail, class T> struct InnerSearch <Loki::Typelist<Head, Tail>, T> {

  typedef typename SearchByChildType<Loki::Typelist<Head, Tail>, T, IsEqual<T, typename Head::TChild >::value >::Result Result;
};



template <class TList, class T, bool Equal> struct ContainByChildType;

template <class Head, class Tail, class T, bool Equal> struct ContainByChildType <Loki::Typelist <Head, Tail>, T, Equal> {

                  enum { value = ContainByChildType<Tail, T, IsEqual<T, typename Tail::Head::TChild >::value >::value };
};

template <class Head, class Tail, class T> struct ContainByChildType<Loki::Typelist<Head, Tail>, T, true> {

                  enum { value = true };
};

template <class Head, class T> struct ContainByChildType<Loki::Typelist<Head, Loki::NullType>, T, false> {
                  enum { value = false };
};

template < class TList, class T> struct InnerContain;

template < class Head, class Tail, class T> struct InnerContain <Loki::Typelist<Head, Tail>, T> {

                  enum { value = ContainByChildType<Loki::Typelist<Head, Tail>, T, IsEqual<T, typename Head::TChild >::value >::value };
};


//TEMP remove after switching from loki to hana
template <typename ... T> struct MakeTL;
template <typename H, typename ... T> struct MakeTL<H, T...> {

                typedef Loki::Typelist<H, typename MakeTL<T...>::TL > TL;
};
template <typename H> struct MakeTL<H> {

                typedef Loki::Typelist<H, Loki::NullType> TL;
};

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template <class ... Args, class TVariant> auto Visit(TVariant && oVar, Args && ... args) {

        return std::visit(overloaded { args... }, oVar);
}


template <class TTuple, typename TAction, std::size_t ... Indices>
constexpr void TupleForEachImpl(TTuple && oTuple, TAction && oAction, std::index_sequence<Indices...>) {
        using TProxy = int[];
        (void)TProxy{1,
                (oAction(std::get<Indices>(std::forward<TTuple>(oTuple))), void(), int{})...
        };
}

template <typename TTuple, typename TAction>
constexpr void TupleForEach(TTuple && oTuple, TAction && oAction) {

        constexpr std::size_t size = std::tuple_size<std::remove_reference_t<TTuple>>::value;

        TupleForEachImpl(std::forward<TTuple>(oTuple), std::forward<TAction>(oAction), std::make_index_sequence<size>{});
}


template <int32_t key, int32_t value> struct IPair {
        static const int        k = key,
                                v = value;
};

template <class...> struct Int2IntDict;

template <> struct Int2IntDict<> {
        template<int32_t> struct Get {
                static const int value = 0;
        };
};

template<int32_t key, int32_t val, class ... rest> struct Int2IntDict<IPair<key, val>, rest...> {
        template<int32_t key2> struct Get {
                static const int32_t value =
                        (key2 == key) ?
                        val :
                        Int2IntDict<rest...>::template Get<key2>::value;
        };
};



template <class T, uint32_t val> struct TPair {
        using Type = T;
        static const uint32_t value = val;
};

template < class ...> struct Type2IntDict;

template < class THolder, class ... rest> struct Type2IntDict<THolder, rest ...> {

        template <class T, bool = std::is_same<T, typename THolder::Type>::value > struct Get {
                static const uint32_t value = THolder::value;
        };
        template <class T> struct Get<T, false> {
                static const uint32_t value = Type2IntDict<rest ...>::template Get<T>::value;
        };
};



template <class... T> struct TypelistWrapper {};

template<template <class ...> class TContainer, class ... T> struct Typelist2TmplPack;

template<template <class ...> class TContainer, class ... T> struct Typelist2TmplPack<TContainer, TypelistWrapper<T...> > {
            using Type = TContainer<T...>;
};



template<template <class ...> class TContainer, template <class> class TWrapper, class ... T> struct Typelist2WrappedTmplPack;

template<template <class ...> class TContainer, template <class> class TWrapper, class ... T> struct Typelist2WrappedTmplPack<TContainer, TWrapper, TypelistWrapper<T...> > {
            static_assert(sizeof...(T) > 0);
            using Type = TContainer<TWrapper<T> ... >;
};



template <class ... T1s, class ... T2s> constexpr auto TypelistConcatenate(TypelistWrapper<T1s...>, TypelistWrapper<T2s...>) {
        return TypelistWrapper<T1s..., T2s...>{};
}

template <template <class> class TCondition, class TResult> constexpr auto TypelistFilter(TResult result, TypelistWrapper<>) {
        return result;
}

template <template <class> class TCondition, class TResult, class T, class ... Ts>
        constexpr auto TypelistFilter(TResult result, TypelistWrapper<T, Ts...>) {

        if constexpr (TCondition<T>{}) {
                return TypelistFilter<TCondition>(TypelistConcatenate(result, TypelistWrapper<T>{}), TypelistWrapper<Ts...>{});
        }
        else {
                return TypelistFilter<TCondition>(result, TypelistWrapper<Ts...>{});
        }
}

template <template <class> class TCondition, class ... Types>
        using FilteredTypelist = std::decay_t<decltype(TypelistFilter<TCondition>(TypelistWrapper<>{}, TypelistWrapper<Types...>{}))>;


} //namespace MP

#endif
