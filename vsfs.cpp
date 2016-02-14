#include <fstream>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iostream>
#include <vector>
#include "vsfs.hpp"
using namespace std;
/*
  organization
  inode: 256 bytes
  data block: 4k bytes
 */

template <class T>
void p(T foo) {
  std::cout << foo << '\n';
}

/* size of an inode and a data block */
int VSFileSystem::isize = 256;
int VSFileSystem::dsize = 4*1024;

Inode::Inode() {
  this->type = 0; 
  this->size = 0;
  this->block_left_size = VSFileSystem::dsize;
  this->addr_0 = 0;
  
  this->mode = 664;
  this->uid = 0;
  this->gid = 0;
}

// std::ostream& operator << (std::ostream& fs, const Inode& node) {
//   fs.write((char*)&node.type, sizeof(node.type));
//   fs.write((char*)&node.size, sizeof(node.size));
//   fs.write((char*)&node.addr_0, sizeof(node.addr_0));
  
//   fs.flush();
//   return fs;
// }
std::ostream& operator << (std::ostream& fs, Inode* node) {
  fs.write((char*)node, sizeof(*node));
  fs.flush();
  return fs;
}

std::istream& operator >> (std::istream& fs, Inode* node) {
  fs.read((char*)node, sizeof(*node));
  return fs;
}

std::ostream& operator << (std::ostream& fs, DirEntry* en) {
  fs.write((char*)&(en->node_index), sizeof(en->node_index));
  fs.write((char*)&(en->nlen), sizeof(en->nlen));
  fs.write(en->name, en->nlen);
  //fs.write((char*)en, sizeof(*en));
  fs.flush();
  return fs;
}

std::istream& operator >> (std::istream& fs, DirEntry* en) {
  int buffer;
  fs.read((char*)&buffer, sizeof(buffer));
  en->node_index = buffer;
  fs.read((char*)&buffer, sizeof(buffer));
  en->nlen = buffer;

  char* name = new char[en->nlen+1];
  fs.read(name, en->nlen);
  en->name = name; // please delete[] en->name when entry dies
  //fs.read((char*)en, sizeof(*en));
  return fs;
}


DirEntry::DirEntry(int id, int nlen, const char *name) {
  this->node_index = id;
  //rec_bytes = bytes;
  //str_len = slen;
  this->nlen = nlen;
  this->name = name;
}

std::size_t DirEntry::getSize() {
  return sizeof(node_index)
    + sizeof(nlen)
    + nlen;
}


VSFileSystem::VSFileSystem() {
  disk_name = "vdisk";
  disk_size = 2*1024*1024;

  /* start from 3, 0-2 reserve for std fd */
  fd_cnt = 2;

  /* use a general stringstream if not read from stdin */
  input_stream = &std::cin;
  disk.open(disk_name, std::fstream::binary | std::fstream::in | std::fstream::out);

  /* debug */
  mkfs();
  root = mkdir();
  cwd = root;
  loadDirTable();
  open();
}

VSFileSystem::~VSFileSystem() {
  disk.close();
}

/* return a file descriptor */
int VSFileSystem::createFile() {
  int i_id = allocBit(block_offset[1], inum);
  if (i_id == -1) {
    std::cerr << "fail to allocate inode, inode region full.\n";
    return -1;
  }

  int d_id = allocBit(block_offset[2], dnum);
  if (d_id == -1) {
    std::cerr << "fail to allocate data block, disk full.\n";
    return -1;
  }

  cout << "allocate inode id: " << i_id << endl;
  cout << "allocate data block id: " << d_id << endl;
  Inode * node = new Inode();
  node->addr_0 = getDataOffset(d_id);
  writeInode(i_id, node);

  /* increment file descriptor and register in fd map */
  fd_cnt++;
  std::pair<int, int> index(i_id, d_id);
  std::pair<int, std::pair<int, int> > map_entry(fd_cnt, index);
  f_index.insert(map_entry);
  return fd_cnt;
}

/* flags:
   'r': read
   'w': write
   return a file descriptor
 */

//int VSFileSystem

int VSFileSystem::open() {
  char filename[256];
  char flag[2];
  int FLAG;
  input_stream->getline(filename, 256, ' ');
  input_stream->getline(flag, 256);
  cout << "file name: " << filename << endl;
  cout << "flag: " << flag << endl;

  if (std::strcmp(flag, "r") == 0) {
    FLAG = 0;
  }
  else if (std::strcmp(flag, "w") == 0) {
    FLAG = 1;
    /* update dir inode size field */
    /* update data block */
  }
  else {
    std::cerr << "unvalid flag\n";
    return -1;
  }


  /* absolute path */
  if (filename[0] == '/') {
    cwd = root;
  }

  
  /* relative path */
  int enter_file = cwd;
  int enter_dir;
  char* newfile_name;
  bool fileExist = true;
  char * pch;
  const char * delim = "/";
  std::vector<char*> newfiles;
  pch = std::strtok(filename, delim);
  while (pch != NULL) {
    /* find in current working dir */
    cout << "find inode of " << pch << endl;
    /* load dir table */

    





    
    std::map<std::string, int>::iterator iter = cwd_table.find(std::string(pch));
    if (iter != cwd_table.end()) {
      enter_file = iter->second;
    }
    else {
      fileExist = false;
      if (FLAG == 0) {
	std::cerr << "no such file.\n";
	return -1;
      }
      else if (FLAG == 1) {
	newfiles.push_back(pch);
      }
      
    }
    pch = std::strtok(NULL, delim);
  }


  /* if write */
  if (FLAG == 1) {
    if (newfiles.size() > 1) {
      std::cerr << "directory not exist, please make directory first.\n";
      return -1;
    }
    
    enter_dir = enter_file;
    newfile_name = newfiles[0];
    cout << "create new file " << newfile_name << " in inode(dir) " << enter_dir << endl;
   
    int nfd = createFile();
    cout << "update dir data content..." << endl;
    addFileToDir(enter_dir, nfd, newfile_name);
    return nfd;
  }
  /* if read */
  else if (FLAG == 0) {
    
  }

  // if reading existing file
  
  /* create new file */
}

/* write to data block of inode i_id */
int VSFileSystem::writeData(int i_id, const void * source, int len) {
  Inode * node = readInode(i_id);
  int data_block_start = node->addr_0;
  int capacity = node->block_left_size;
  if (len <= capacity) {
    disk.seekp(data_block_start);
    disk.write((char*)source, len);
    node->block_left_size = capacity - len;
    //updateInode
  }
  else {
    /* allocate new block */
    /* recursive call writeData */
  }

  /* update capacity */
  delete node;
}


int VSFileSystem::addFileToDir(int dir, int fd, char* name) {
  Inode * dir_node = readInode(dir);
  std::map<int, std::pair<int, int> >::iterator iter;
  iter = f_index.find(fd);
  int f_inode_id = (iter->second).first;
  DirEntry * e = new DirEntry(f_inode_id, std::strlen(name), name);
  cout << "dir current size: " << dir_node->size << endl;

  int write_p = dir_node->addr_0 + dir_node->size;
  //cout << "write to place " << write_p << endl;
  disk.seekp(write_p);
  disk << e;
  delete e;
  delete dir_node;
}

int VSFileSystem::incrementDirFileCnt() {
  
}

Inode * VSFileSystem::readInode(int i_id) {
  Inode * node = new Inode();
  int offset = getInodeOffset(i_id);
  disk.seekg(offset);
  disk >> node;
  //disk.read((char*)node, sizeof(*node));
  return node;
}

int VSFileSystem::mkdir() {
  int i_id = allocBit(block_offset[1], inum);
  if (i_id == -1) {
    std::cerr << "fail to allocate inode, inode region full.\n";
    return -1;
  }

  int d_id = allocBit(block_offset[2], dnum);
  if (d_id == -1) {
    std::cerr << "fail to allocate data block, disk full.\n";
    return -1;
  }

  cout << "allocate inode id: " << i_id << endl;
  cout << "allocate data block id: " << d_id << endl;
  

  /* write new node into disk */
  Inode * node = new Inode();
  node->type = 1; // set inode type to dir
  int data_offset = getDataOffset(d_id);
  node->addr_0 = data_offset;

  DirEntry * e1 = new DirEntry(i_id, 1, ".");
  DirEntry * e2 = new DirEntry(i_id, 2, "..");

  cout << "write dir content..." << endl;
  disk.seekp(data_offset);
  /* first byte in data block represents file counter */
  int init_file_cnt = 2;
  disk.write((char*)&init_file_cnt, sizeof(int));
  disk << e1;
  disk << e2;

  
  cout << "write dir inode..." << endl;
  /* update size field in inode */
  int newsize = sizeof(init_file_cnt) + e1->getSize() + e2->getSize();
  node->size = newsize;
  writeInode(i_id, node);
  

  /* debug */
  // DirEntry * t = new DirEntry(0, 0, "");
  // disk.seekg(data_offset);
  // disk.read((char*)t, sizeof(*t));
  // cout << "test read\n";
  // cout << t->name << '\n';


  delete node;
  delete e1;
  delete e2;
  return i_id;
}


int VSFileSystem::loadDirTable() {
  cout << "load current dir table into memory..." << endl;
  Inode * dir = readInode(cwd);
  int dir_start = dir->addr_0;

  cout << "get file counter..." << endl;
  /* get file counter */
  disk.seekg(dir_start);
  char buffer[sizeof(int)];
  disk.read(buffer, (int)sizeof(int));
  int file_cnt = *((int*)buffer);
  cout << "file counter:" << " " << file_cnt << "\n";

  DirEntry * en = new DirEntry(0, 0, "");
  for (int i = 0; i < file_cnt; i++) {
    disk >> en;
    std::pair<std::string, int> file_entry(std::string(en->name), en->node_index);
    cwd_table.insert(file_entry);
    cout << "insert to table:" << endl;
    std::cout << en->name << " " << en->node_index << "\n";
  }
  delete[] en->name; 
  delete en;
}


int VSFileSystem::writeInode(int i_id, Inode * newnode) {
  int offset = getInodeOffset(i_id);
  disk.seekp(offset);
  disk << newnode;
  //disk.write((char*)newnode, sizeof(*newnode));
}


//int VSFileSystem::addDirEntry

int VSFileSystem::getInodeOffset(int id) {
  return block_offset[3] + isize * id;
}

int VSFileSystem::getDataOffset(int id) {
  return block_offset[4] + dsize * id;
}


/*
  on success, return index of allocated inode or data block
  on fail, return -1
 */
int VSFileSystem::allocBit(int start, int len) {
  disk.seekg(start);
  char * buffer = new char[1];
  for (int i = 0; i < len; i++) {
    disk.read(buffer, 1);
    if (*buffer == 0) {
      /* set allocated bit to 1 */
      char value = 1;
      disk.seekp(start + i);
      disk.write(&value, 1);
      return i;
      break;
    }
  }
  delete [] buffer;
  return -1;
}

void VSFileSystem::prompt() {
  const int MAX_LEN = 1024;
  char* input = new char[MAX_LEN];
  char* op;
  
  while(1) {
    std::cout << "$ ";
    std::cin.getline(input, MAX_LEN);
    op = std::strtok(input, " \n");
    if (std::strcmp(op, "mkfs") == 0) {
      mkfs();
    }
    else {
      std::cout << "command not supported\n";
    }
  }
}

int VSFileSystem::execCmd(std::string cmd) {
  /* parse cmd */
  
  return 0;
}

int VSFileSystem::mkfs() {
  //char* disk_name = createVirtualDisk();

  calcOffset();
  initSuper();
  resetImap();
  resetDmap();
}

/* virtualize disk I/O in memory */
void VSFileSystem::loadDisk() {
  disk.open(disk_name);
}

void VSFileSystem::calcOffset() {
  int kb = 1 << 10;
  int mb = 1 << 20;
  block_offset[0] = 0*kb;
  block_offset[1] = 2*kb;
  block_offset[2] = 4*kb;
  block_offset[3] = 8*kb;
  block_offset[4] = 100*kb;
  block_offset[5] = 2*mb;

  inum = (block_offset[4] - block_offset[3])/isize;
  dnum = (block_offset[5] - block_offset[4])/dsize;
  //p(inum);
  //p(dnum);
}

void VSFileSystem::resetImap() {
  disk.seekp(block_offset[1]);
  int len = block_offset[2] - block_offset[1];
  for (int i = 0; i < len; i++) {
    disk.write("", 1);
  }
  disk.flush();
}

void VSFileSystem::resetDmap() {
  disk.seekp(block_offset[2]);
  int len = block_offset[3] - block_offset[2];
  for (int i = 0; i < len; i++) {
    disk.write("", 1);
  }
  disk.flush();
}

void VSFileSystem::initSuper() {
  disk.seekp(block_offset[0]);
  const char* magic = "Very Simple File System";
  disk << magic;
  //disk.write(magic, 1024);
  disk.flush();
}

int VSFileSystem::createVirtualDisk() {
  std::cout << "creating virtual disk...\n";
  /* if use disk_name instead of a string literal, the disk size will be unexpected */
  std::ofstream ofs("vdisk", std::ios::binary | std::ios::out);
  p(disk_size);
  for (int i = 0; i < disk_size; i++) {
    ofs.write("", 1);
  }
  ofs.close();
  return 0;
}


int main() {
  VSFileSystem* fs = new VSFileSystem();
  //fs->prompt();
  //delete fs;
  return 0;
}
