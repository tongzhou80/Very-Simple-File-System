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

 */

template <class T>
void p(T foo) {
  std::cout << foo << '\n';
}

Inode::Inode() {
  this->type = 0; 
  this->size = 0;
  this->block_left_size = dsize;
  this->addr_0 = 0;
  
  this->mode = 664;
  this->uid = 0;
  this->gid = 0;
}

std::ostream& operator << (std::ostream& fs, const Inode& node) {
  fs.write((char*)&node.type, sizeof(node.type));
  fs.write((char*)&node.size, sizeof(node.size));
  fs.write((char*)&node.addr_0, sizeof(node.addr_0));
  
  fs.flush();
  return fs;
  // return fs << node.type
  // 	    << node.file_cnt
  // 	    << node.size
  // 	    << node.addr_0
  // 	    << "mark";
}

std::ostream& operator << (std::ostream& fs, const DirEntry& en) {
  fs.write((char*)&en.node_index, sizeof(en.node_index));
  fs.write((char*)&en.nlen, sizeof(en.nlen));
  fs.write(en.name, en.nlen*sizeof(int));
  fs.flush();
}

/* write to data block of inode i_id */
/* a wrap of memcpy */
int VSFileSystem::writeData(int i_id, const void * source, int len) {
  Inode * node = getInode(i_id);
  int data_block_start = node->addr_0;
  int capacity = node->block_left_size;
  /* memcpy */
  if (len <= capacity) {
    std::memcpy(data_block_start, source, len);
    node->block_left_size = capacity - len;
    updateInode
  }
  else {
    /* allocate new block */
    /* recursive call writeData */
  }

  /* update capacity */
  delete node;
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
  /* size of an inode and a data block */
  isize = 256;
  dsize = 4*1024;

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

  Inode * node = new Inode();
  int node_offset = getInodeOffset(i_id);
  disk.seekp(node_offset);
  disk << *node;
  disk.flush();

  /* maintain a file descriptor and fd map */
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
  p(filename);
  p(flag);

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


  
  if (FLAG == 1) {
    if (newfiles.size() > 1) {
      std::cerr << "directory not exist, please make directory first.\n";
      return -1;
    }
    
    enter_dir = enter_file;
    newfile_name = newfiles[0];
    int nfd = createFile();
    addFileToDir(enter_dir, nfd, newfile_name);
  }

  // if reading existing file
  
  /* create new file */
}

int VSFileSystem::addFileToDir(int dir, int fd, char* name) {
  Inode * dir_node = getInode(dir);
  std::map<int, std::pair<int, int> >::iterator iter;
  iter = f_index.find(fd);
  DirEntry * e = new DirEntry(fd, (iter->second).first, name);
  cout << dir_node->size;
  delete dir_node;
}

int VSFileSystem::incrementDirFileCnt() {
  
}

// int VSFileSystem::getAddress_0(int node_id) {
//   int inode_offset = getInodeOffset(node_id);
//   char buffer[sizeof(int)];

//   /* get file data address */
//   disk.seekg(inode_offset);
//   disk.read(buffer, (int)sizeof(int));
//   disk.read(buffer, (int)sizeof(int));
//   disk.read(buffer, (int)sizeof(int));
//   int address_0 = *((int*)buffer);
//   return address_0;
// }

Inode * VSFileSystem::getInode(int id) {
  Inode * node = new Inode();
  int inode_offset = getInodeOffset(id);
  char buffer[sizeof(int)];

  /* get file data address */
  disk.seekg(inode_offset);
  disk.read(buffer, (int)sizeof(int));
  node->type = *((int*)buffer);
  disk.read(buffer, (int)sizeof(int));
  node->size = *((int*)buffer);
  disk.read(buffer, (int)sizeof(int));
  node->addr_0 = *((int*)buffer);
  return node;
}

int VSFileSystem::loadDirTable() {
  int dir_start = getInode(cwd)->addr_0;
  //int dir_start = getAddress_0(cwd);
  disk.seekg(dir_start);
  char buffer[sizeof(int)];
  disk.read(buffer, (int)sizeof(int));
  int file_cnt = *((int*)buffer);
  p(file_cnt);
  for (int i = 0; i < file_cnt; i++) {
    disk.read(buffer, (int)sizeof(int));
    int node_index = *((int*)buffer);
    disk.read(buffer, (int)sizeof(int));
    int name_len = *((int*)buffer);
    disk.read(buffer, name_len*sizeof(int));
    char* f_name = buffer;
    std::pair<std::string, int> file_entry(std::string(f_name), node_index);
    cwd_table.insert(file_entry);
    p("insert:");
    std::cout << f_name << " " << node_index << "\n";
  }

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

  /* write new node into disk */
  Inode * node = new Inode();
  node->type = 1; // set inode type to dir
  int node_offset = getInodeOffset(i_id);
  int data_offset = getDataOffset(d_id);
  node->addr_0 = data_offset;
  p("add:");
  p(node->addr_0);
  p("node:");
  p(node_offset);
  disk.seekp(node_offset);
  disk << *node;
  
  DirEntry * e1 = new DirEntry(i_id, 1, ".");
  DirEntry * e2 = new DirEntry(i_id, 2, "..");
  disk.seekp(data_offset);
  /* first byte in data block represents file counter */
  int init_file_cnt = 2;
  disk.write((char*)&init_file_cnt, sizeof(int));
  disk << *e1;
  disk << *e2;

  /* update size field in inode */
  int newsize = e1->getSize() + e2->getSize();
  char* buffer = (char*)&newsize;
  node->size = newsize;
  updateInode(i_id, node, sizeof(*node));

  delete node;
  delete e1;
  delete e2;
  return i_id;
}

int VSFileSystem::updateInode1(int node_offset, int inner_offset, char* buffer, std::size_t num) {
  disk.seekp(node_offset + inner_offset);
  disk.write(buffer, num);
  disk.flush();
}

int VSFileSystem::updateInode(int i_id, Inode * newnode) {
  int offset = getInodeOffset(i_id);
  
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
