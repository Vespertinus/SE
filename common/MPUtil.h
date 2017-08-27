
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

} //namespace MP

#endif
