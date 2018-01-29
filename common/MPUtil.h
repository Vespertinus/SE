
#ifndef __MP_UTIL_H__
#define __MP_UTIL_H__ 1

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


} //namespace MP

#endif
