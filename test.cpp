#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "vsfs.hpp"
using namespace std;

template <class T>
void p(T foo) {
  std::cout << foo << '\n';
}

void testStrlen() {
  char * s = "123";
  cout << std::strlen(s);
}

void teststrtok() {
  char str[] = "/sample/string";
  char * pch;
  pch = std::strtok(str, "/");
  while(pch!=NULL) {
    std::cout << pch << '\n';
    pch = std::strtok(NULL, "/");
  }

}

void testWrite() {
  //fstream fs("vdisk", std::fstream::binary);
  fstream fs;
  fs.open("vdisk", std::fstream::binary | std::fstream::in | std::fstream::out);
  int i = 99;
  fs << i;
  //fs.write("AAA", 4);
  //fs.write((char*)&i, 4);
  fs.flush();
  fs.close();

}

void time() {
  time_t rawtime = std::time(0);
  struct tm * timeinfo = std::localtime(&rawtime);
  std::cout << "XXX" << std::asctime(timeinfo) << "XXX";
  cout << strlen(asctime(timeinfo));
}

void foo() {
  int disk_size = 1024*1024;
  std::ofstream ofs("vdisk", std::ios::binary | std::ios::out);
  ofs.seekp(disk_size - 1);
  ofs.write("", 1);
  std::cout << "resetting disk...\n";

  std::vector<char> empty(1024, 0);
  int size_in_kb = disk_size / 1024;
  p("kb:");
  p(size_in_kb);
  for(int i = 0; i < size_in_kb; i++)
  {
    if (!ofs.write(&empty[0], empty.size()))
      {
	std::cerr << "problem writing to file" << std::endl;
	exit(255);
      }
  }

}

void bar() {
  std::ofstream ofs("vdisk", std::ios::binary | std::ios::out);
  ofs.seekp(100);
  ofs.write("foo", 3);
  ofs.close();

}

int main() {
  // char * p = "hh";
  // p[0] = 'a'; // undefined
  //foo();
  time();
  char * a = "123";
  cout << strlen(a);
}
