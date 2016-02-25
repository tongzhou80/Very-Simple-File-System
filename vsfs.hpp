
#ifndef VSFS_H
#define VSFS_H

#include <ctime>
#include <string>
#include <map>

typedef int Address;

class Inode {
public:
  Inode();
  friend std::ostream& operator << (std::ostream& fs, Inode* node);

  /* file type: 0: regular file, 1: directory */
  int type;
  int size;
  int capacity; // track how many bytes left in current 4k block
  int addr_0;
  int addr_1;

  /* to do */
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
  friend std::ostream& operator << (std::ostream& fs, DirEntry* en);

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
  int section_offset[6];
  int inum;
  int dnum;
  long long cap_0;
  long long cap_1;
  long long cap_2;
  
  int fd_cnt;
  /* fd: (inode id, file inner pointer) */
  std::map<int, std::pair<int, int> > fd_map;

  /* inode and data block management */
  int createVirtualDisk();
  void initSuper();
  void calcGlobalOffset();
  void resetImap();
  void resetDmap();
  void resetDisk();
  int allocBit(int start, int len);
  int allocInodeBlock();
  int allocDataBlock();
  void freeFile(int i_id);
  void freeBit(Address start_address, int offset);
  void freeInodeBlock(int i_id);
  void freeDataBlock(int d_id);
  int getInodeOffset(int id);
  int getDataOffset(int id);
  int getDataIdByOffset(Address offset);
  int writeInode(int i_id, Inode * newnode);
  Inode * readInode(int i_id);
  int allocAddr_1(Inode * node);
  int allocLevel1Block(Inode * node);

  /* directory management */
  int newDir();
  void rmDirEntry(int d_id, const char* filename, int f_type);
  int createFile();
  int loadDirTable();
  int incrementDirFileCnt();
  int addEntryToDir(int dir, DirEntry* en);
  int getDirFileNum(Inode* dir_node);
  bool checkDirEmpty(Inode* dir_node);

  int getIntAt(int addr);
  int putIntAt(int addr, int value);
  int writeData(Inode* node, int f_offset, const void * source, int len);
  int readData(Inode* node, int f_offset, void * buffer, int len);
  int registerFD(int fd, int i_id);
  int releaseFD(int fd);
  int calcDiskAddr(Inode* node, int f_offset);
  int getBlockInnerOffset(int offset);
  void printConfig();
  
  /* virtual disk I/O */
  int dwrite(int dest, const void * source, int len);
  int dread(int dest, void * buffer, int len);
  void loadDisk();
public:
  static int isize;
  static int dsize;
  VSFileSystem();
  ~VSFileSystem();
  /* shell */
  void prompt();

  /* file system */
  int execCmd(std::string cmd);

  /* commands implementations */
  int mkfs();
  int mkdir(const char* name);
  int rmdir(const char* name);
  int open(const char* filename, const char* flag);
  int close(int fd);
  int seek(int fd, int offset);
  int read(int fd, int size);
  int write(int fd, const char* str);
  int cat(const char* filename);
  int ls();
  void rm(const char* name);
  void cd(const char* dirname);
};

#endif
