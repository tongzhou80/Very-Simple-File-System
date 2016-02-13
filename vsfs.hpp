
#ifndef VSFS_H
#define VSFS_H

#include <ctime>
#include <string>
#include <map>

class Inode {
public:
  Inode();
  friend std::ostream& operator << (std::ostream& fs, const Inode& node);

  /* file type: 0: regular file, 1: directory */
  int type;
  int size;
  int block_left_size; // track how many bytes left in current 4k block
  int addr_0;

  /* to do */
  int addr_1;
  int addr_2;
  int mode;
  int uid;
  int gid;
  struct tm * time;
  struct tm * ctime;
  struct tm * mtime;

  /* move file count to data region */
  //int file_cnt; // type 1 only
  };

class DirEntry {
public:
  DirEntry(int id, int nlen, const char* name);
  std::size_t getSize();
  friend std::ostream& operator << (std::ostream& fs, const DirEntry& en);

  int node_index;
  int nlen;
  const char * name;

  /* to do */
  //int rec_bytes;
  //int str_len;
};

class VSFileSystem {
private:
  const char* disk_name;
  int disk_size;
  std::fstream disk;
  std::istream * input_stream;
  int cwd;
  int root;
  /* cwd_table is contains pairs of file name and its fd */
  std::map<std::string, int> cwd_table;
  int block_offset[6];
  static int const isize = 256;
  static int const dsize = 4*1024;
  int inum;
  int dnum;
  int fd_cnt;
  std::map<int, std::pair<int, int> > f_index;
  
  int createVirtualDisk();
  void initSuper();
  void calcOffset();
  void resetImap();
  void resetDmap();
  int allocBit(int start, int len);
  int getInodeOffset(int id);
  int getDataOffset(int id);
  int updateInode(int i_id, Inode * newnode);
  int createFile();
  int loadDirTable();
  int incrementDirFileCnt();
  int addFileToDir(int dir, int fd, char* name);
  //int getAddress_0(int node_id);
  Inode * getInode(int id);
  int writeData(int i_id, const void * source, int len);
  
  /* virtual disk I/O */
  void dwrite();
  void dread();
  void loadDisk();
public:
  VSFileSystem();
  ~VSFileSystem();
  /* shell */
  void prompt();

  /* file system */
  int execCmd(std::string cmd);

  /* commands implementations */
  int mkfs();
  int mkdir();
  int open();
  // int read();
  // int write();
};

#endif
