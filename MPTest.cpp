
#include <iostream>
#include <fstream>

#include <Typelist.h>

#include <MPUtil.h>

struct A1 {

  typedef int TChild;

  void Action() { fprintf(stderr, "A1\n"); }
};

struct A2 {

  typedef double TChild;

  void Action() { fprintf(stderr, "A2\n"); }
};

struct A3 {

  typedef char TChild;

  void Action() { fprintf(stderr, "A3\n"); }
};



typedef LOKI_TYPELIST_3(A1, A2, A3) TestList1;

int main() {

  typedef MP::InnerSearch<TestList1, float>::Result TResult;

  TResult obj;
  obj.Action();

  return 0;
}
