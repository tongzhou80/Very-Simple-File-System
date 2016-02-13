#include <fstream>
#include <cstdlib>
#include <cstring>
#include <iostream>

void printHelp() {
  std::cout << "-s, -s size\n";
  std::cout << "\tSpecify the size(bytes) of the file system to be created. Default to 100M.\n";
}


int mkfs(int argc, char* argv[]) {
  /* parameters of file system */
  int fs_size = 0;
  bool SET_SIZE = false;

  /* parse command line argument */
  for(int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "-s") == 0)  {
      fs_size = std::atoi(argv[i+1]);
      SET_SIZE = true;
      i++;
    }
    else if (std::strcmp(argv[i], "-h") == 0) {
      printHelp();
      std::exit(0);
    }
    else {
      //
    }
  }

  /* set default parameter value */
  if (!SET_SIZE) {
    fs_size = 1024*1024*100;
  }
  
  std::ofstream ofs("vdisk", std::ios::binary | std::ios::out);
  ofs.seekp(fs_size - 1);
  ofs.write("", 1);
  
}

int main(int argc, char* argv[])
{
  mkfs(argc, argv);
}
