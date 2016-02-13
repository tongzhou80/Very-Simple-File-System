#include <iostream>
using namespace std;

int main(int argc, char* argv[]) {
  char * foo = "foo";
  cout << "argv[1]: " << argv[1];
  cout << "foo:" << foo;
  cout << "foo==argv[1]" << (foo==argv[1]);
}
