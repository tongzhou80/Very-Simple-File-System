
#ifndef MY_SHELL_H
#define MY_SHELL_H

#include <iostream>
#include <string>

class FSShell {
public:
  std::string curCmd;
  
  void prompt();
  int exec(std::string cmd);
};

#endif
