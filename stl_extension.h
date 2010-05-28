
#ifndef __STL_EXTENSION_H_
#define __STL_EXTENSION_H_ 1

#include <functional>

namespace STD_EXT {

template<typename _Ret, typename _Tp, typename _Arg1, typename _Arg2>
class mem_fun2_t /*: public binary_function<_Tp*, _Arg1, _Ret>*/ {
  public:
    explicit
      mem_fun2_t(_Ret (_Tp::*__pf)(_Arg1, _Arg2))
      : _M_f(__pf) { }

    _Ret
      operator()(_Tp* __p, _Arg1 __x, _Arg2 __y) //const
      { return (__p->*_M_f)(__x, __y); }

  private:
    _Ret (_Tp::*_M_f)(_Arg1, _Arg2);
};



template <class Type, class Ret, class ... Args> class GeneralFunctor {

  Type & oObj;
  Ret (Type::*member)(const Args & ... args);

  public:

  GeneralFunctor(Type & oNewObj, Ret (Type::*ptr)(const Args & ... args)) :
    oObj(oNewObj),
    member(ptr) { ;; }

  Ret operator()(const Args & ... args) {
    return (oObj.*member)(args...);
  }
};



} //namespace STD_EXT

#endif
