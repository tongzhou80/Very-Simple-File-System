#include "FSShell.hpp"

void FSShell::prompt() {
  while(1) {
    std::cout << "$ ";
    std::getline(std::cin, curCmd);
    exec(curCmd);
    //std::cout << "cmd: " << curCmd << '\n';
  }
}

int FSShell::exec(std::string cmd) {
  std::cout << "execute cmd...\n";
  return 0;
}

int main() {
  FSShell* shell = new FSShell();
  shell->prompt();}
