#include <fstream>
#include <cstdlib>
#include <cassert>
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


/* ===== status =====
   mkfs() is called before any test begins
   in dir data content, set removed subdir's inode to be -1
   skip those entry whose inode id is -1 when disk >> dir entry

   ===== to do =====
   update pwd_table in memory instead of loading modified dir content from disk everytime
*/

template <typename T>
void pp(const T& t){
  std::cout << t << std::endl;  
}

template <typename T, typename ... Args>
void pp(const T& t, Args ... args){
  std::cout << t << ' ';
  pp(args...);
}

/* size of an inode and a data block */
int VSFileSystem::isize = 256;
int VSFileSystem::dsize = 4*1024;

VSFileSystem::VSFileSystem() {
  disk_name = "vdisk";
  disk_size = 2*1024*1024;
  cap_0 = VSFileSystem::dsize;
  cap_1 = cap_0 / sizeof(int) * cap_0;
  cap_2 = cap_0 / sizeof(int) * cap_1;
  calcGlobalOffset();
  printConfig();

  /* use a general stringstream if not read from stdin */
  input_stream = &std::cin;
  disk.open(disk_name, std::fstream::binary | std::fstream::in | std::fstream::out);

  
  /* fd start from 3, 0-2 reserve for std fd */
  fd_cnt = 2;
  root = mkfs(); // call mkfs will wipe and format the disk
  //  root = 0; // root defaults to No. 0 inode  

  
  /* by default, open root dir */
  cwd = root;
  loadDirTable();
  
  /* debug */

  // open("foo", "w");
  // open("bar", "w");
  // write(3, "good restraut!");
  // cat("foo");
  // seek(3, 4);
  // write(3, "hello");
  // seek(3, 0);
  // read(3, 12);
  // close(3);
  // close(3);
  // ls();
}


Inode::Inode() {
  this->type = 0; 
  this->size = 0;
  this->capacity = VSFileSystem::dsize;
  this->addr_0 = -1;
  this->addr_1 = -1;
  this->addr_2 = -1;
  
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

  /* skip deleted node */
  if (en->node_index == -1) {
    pp("skip one deleted node...");
    int cur_p = fs.tellg();
    fs.seekg(cur_p + en->nlen);
    en = NULL;
  }
  else {
    char* name = new char[en->nlen+1];
    fs.read(name, en->nlen);
    name[en->nlen] = '\0';
    en->name = name; // please delete[] en->name when entry dies
  
  }
  
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

void VSFileSystem::printConfig() {
  std::cout << "disk size: " << disk_size << std::endl;
  std::cout << "level 0 capacity: " << cap_0/1024 << "Kb" << std::endl;
  std::cout << "level 1 capacity: " << cap_1/1024/1024 << "Mb" << std::endl;
  std::cout << "level 2 capacity: " << cap_2/1024/1024/1024 << "Gb" << std::endl;
}


VSFileSystem::~VSFileSystem() {
  disk.close();
}

/* return a file descriptor */
int VSFileSystem::createFile() {
  int i_id = allocInodeBlock();
  if (i_id == -1) {
    std::cerr << "fail to allocate inode, inode region full.\n";
    return -1;
  }

  int d_id = allocDataBlock();
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
  registerFD(fd_cnt, i_id);
  // std::pair<int, int> index(i_id, 0); // file pointer defaults to 0
  // std::pair<int, std::pair<int, int> > map_entry(fd_cnt, index);
  // fd_map.insert(map_entry);
  return fd_cnt;
}

int VSFileSystem::registerFD(int fd, int i_id) {
  pp("register fd", fd, "int fd map");
  std::pair<int, int> index(i_id, 0); // file pointer defaults to 0
  std::pair<int, std::pair<int, int> > map_entry(fd, index);
  fd_map.insert(map_entry);
  return fd;
}

int VSFileSystem::releaseFD(int fd) {
  std::map<int, std::pair<int, int> >::iterator iter;
  iter = fd_map.find(fd);
  if (iter == fd_map.end()) {
    std::cerr << "unknown file descriptor\n";
    return -1;
  }
  else {
    pp("erase fd", fd, "in fd_map");
    fd_map.erase(iter);
  }

}

/* to to do */
/* only suuport current dir now, but designed to support any path */
int VSFileSystem::open(const char* filename, const char* flag) {
  // char filename[256];
  // char flag[2];
  // input_stream->getline(filename, 256, ' ');
  // input_stream->getline(flag, 256);

  pp("===== open", filename, flag, "=====");
  int FLAG;

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

  pp(filename);
  
  /* relative path */
  int enter_file = cwd;
  int enter_dir;
  char* newfile_name;
  bool fileExist = true;
  char * pch;
  const char * delim = "/";
  std::vector<char*> newfiles;
  pch = std::strtok(strdup(filename), delim);

  while (pch != NULL) {
    /* find in current working dir */
    cout << "find inode of " << pch << endl;
    /* load dir table */
    /* to do */
    
    
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

  if (fileExist) {
    if (FLAG == 0) {
      cout << "read: open file node " << enter_file << endl;      
    }
    else if (FLAG == 1) {
      cout << "write: open file node " << enter_file << endl;
    }
    else {
      /* pass */
    }
    
    fd_cnt++;
    cout << "register fd " << fd_cnt << " in fd map" << endl;
    registerFD(fd_cnt, enter_file);
    std::cout << "SUCCESS, fd = " << fd_cnt << std::endl;
    return fd_cnt;
  }
  /* if file not exist, create new file */
  else {
    if (newfiles.size() > 1) {
    	std::cerr << "dir not exist, please make dir first.\n";
    	return -1;
    }
    else {
    	/* pass */
    }
    
    enter_dir = enter_file;
    newfile_name = newfiles[0];
    cout << "create new file " << newfile_name << " in inode(dir) " << enter_dir << endl;
   
    int nfd = createFile();
    std::map<int, std::pair<int, int> >::iterator iter;
    iter = fd_map.find(nfd);
    int f_inode_id = (iter->second).first;
    DirEntry * en = new DirEntry(f_inode_id, std::strlen(newfile_name), newfile_name);

    cout << "update dir data content..." << endl;
    addEntryToDir(cwd, en);
    delete en;
    std::cout << "SUCCESS, fd = " << fd_cnt << std::endl;
    return nfd;
  }

  return -1;
}

int VSFileSystem::ls() {
  pp("===== ls =====");
  std::map<std::string, int>::iterator iter;
  Inode * node;
  for(iter = cwd_table.begin(); iter != cwd_table.end(); ++iter)
  {
    int i_id = iter->second;
    const char* file_name = iter->first.c_str();
    node = readInode(i_id);
    char* type;
    int size = node->size;
    switch(node->type) {
    case 0: type = "file"; break;
    case 1: type = "dir"; break;
    }
    
    std::cout << type << " " << file_name << " " << size << "Kb" << std::endl;
    delete node;
  }
}

int VSFileSystem::close(int fd) {
  pp("===== close", fd, "=====");
  releaseFD(fd);
  return 0;
}

int VSFileSystem::seek(int fd, int offset) {
  pp("===== seek", fd, offset, "=====");
  std::map<int, std::pair<int, int> >::iterator iter;
  iter = fd_map.find(fd);
  if (iter == fd_map.end()) {
    std::cerr << "unknown file descriptor\n";
    return -1;
  }
  else {
    /* set file offset */
    (iter->second).second = offset;
  }
  return 0;
}

int VSFileSystem::write(int fd, const char* str) {
  pp("===== write", fd, str, "======");
  
  std::map<int, std::pair<int, int> >::iterator iter;
  iter = fd_map.find(fd);
  if (iter == fd_map.end()) {
    std::cerr << "unknown file descriptor\n";
    return -1;
  }
  else {
    int i_id = (iter->second).first;
    int f_offset = (iter->second).second;
    pp("file offset:", f_offset);
    pp("write string...");

    Inode* node = readInode(i_id);
    int str_len = std::strlen(str);
    writeData(node, f_offset, str, str_len);

    /* update file size */
    if (f_offset + str_len <= node->size) {
      /* don't update size */
    }
    else {
      node->size = f_offset + str_len;
      pp("update file size to", node->size);
    }
    writeInode(i_id, node); // node has been updated in writeData()

    /* update file offset */
    (iter->second).second += str_len;
    pp("update file offset to:", (iter->second).second);

    delete node;
  }
}

int VSFileSystem::allocAddr_1(Inode * node) {
  int d_id = allocDataBlock();
  if (d_id == -1) {
    std::cerr << "allocate data block failed." << std::endl;
  }

  node->addr_1 = getDataOffset(d_id);
}

int VSFileSystem::calcDiskAddr(Inode * node, int f_offset) {
  int data_block_start = node->addr_0;
  int disk_addr;

  /* cap_0 also denotes one data block */
  
  /* if only use addr_0 */
  if (f_offset < cap_0) {
    disk_addr = node->addr_0 + f_offset;
  }
  /* if use addr_0 and addr_1 */
  else if (f_offset < cap_1) {
    int addr_1_0 = getIntAt(node->addr_1);
    int offset_1 = f_offset - cap_0;
    disk_addr = addr_1_0 + offset_1/cap_0 * sizeof(int) + offset_1%cap_0;
  }
  /* if use three level address */
  else if (f_offset < cap_2) {
    
  }
  else {
    std::cerr << "file offset over 4Gb, too large to handle.\n";
  }

  return disk_addr;
  delete node;
}

int VSFileSystem::dwrite(int dest, const void * source, int len) {
  disk.seekp(dest);
  disk.write((char*)source, len);
  disk.flush();
}

int VSFileSystem::getBlockInnerOffset(int offset) {
  return offset % VSFileSystem::dsize;
}

/* write to data block of inode i_id */
int VSFileSystem::writeData(Inode* node, int f_offset, const void * source, int len) {
  int dsize = VSFileSystem::dsize;
  int capacity = node->capacity;
  pp("read node capacity...", capacity);
  int block_left = capacity - f_offset;

  /* file offset within capacity */
  if (block_left > 0) {
    /* check if writing involves more than one block */
    int block_inner_offset = getBlockInnerOffset(f_offset);
    pp("block_inner_offset", block_inner_offset);

    int disk_addr = calcDiskAddr(node, f_offset);
    /* if involves only current block */
    if (block_inner_offset + len <= dsize) {
      pp("write", len, "bytes to address", disk_addr);
      dwrite(disk_addr, source, len);
    }
    /* if need new data block */
    else {
      int write_len = dsize - block_inner_offset;
      pp("write", write_len, "bytes to address", disk_addr);
      dwrite(disk_addr, source, write_len);
      writeData(node, f_offset+write_len, ((char*)source)+write_len, len-write_len);
    }

  }
  else if (f_offset == capacity) {
    /* allocate new block */
    /* check if use level 1 address */
    if (node->addr_1 == -1) {
      pp("init address level 1...");
      allocAddr_1(node);
    }
    /* check if use level 1 address */
    /* to do */

    pp("allocat new data block in level 1 space...");
    allocLevel1Block(node);
    pp("update node capacity to", node->capacity+dsize);
    node->capacity += dsize;

    writeData(node, f_offset, source, len);
  }

}

int VSFileSystem::allocLevel1Block(Inode * node) {
  int dsize = VSFileSystem::dsize;
  int level1_block_index = (node->capacity - 1*dsize) / dsize;
  int new_block_pter = node->addr_1 + level1_block_index * sizeof(int);
  int d_id = allocDataBlock();
  putIntAt(new_block_pter, getDataOffset(d_id));

  return 0;
}

int VSFileSystem::read(int fd, int size) {
  pp("===== read", fd, size, "=====");
  std::map<int, std::pair<int, int> >::iterator iter;
  iter = fd_map.find(fd);
  if (iter == fd_map.end()) {
    std::cerr << "unknown file descriptor\n";
    return -1;
  }
  
  int i_id = (iter->second).first;
  int f_offset = (iter->second).second;
  Inode* node = readInode(i_id);

  int len = size;
  // if (f_offset + size > node->size) {
  //   len = node->size - f_offset;
  //   std::cout << "read " << len << " reaches end of file" << std::endl;
  // }

  char* buffer = new char[len+1];
  readData(node, f_offset, buffer, len);
  buffer[len] = '\0';
  std::cout << buffer << std::endl;

  /* update file offset */
  (iter->second).second += size;
  pp("update file offset to:", (iter->second).second);

  delete[] buffer;
  delete node;
}

int VSFileSystem::cat(const char* filename) {
  pp("===== cat =====");
  pp("lookup working dir table...");
  std::map<std::string, int>::iterator iter = cwd_table.find(std::string(filename));
  if (iter == cwd_table.end()) {
    std::cerr << "no such file.\n";
  }
  else {
    int i_id = iter->second;
    pp("file inode id:", i_id);
    Inode * node = readInode(i_id);
    int file_size = node->size;
    pp("file size:", node->size);

    char* content = new char[file_size+1];
    readData(node, 0, content, file_size);
    content[file_size] = '\0';

    /* cout the whole stirng may stop print as long as it encounters \0*/
    for (int i = 0; i < file_size; i++) {
      std::cout << content[i];
    }
    std::cout << std::endl;
    delete[] content;
    delete node;
  }
}

int VSFileSystem::readData(Inode* node, int f_offset, void * buffer, int len) {
  /* boundry check */
  if (f_offset >= node->size) {
    std::cerr << "file offset is greater than file size. read nothing." << std::endl;
    return 0;
  }

  int dsize = VSFileSystem::dsize;
  int block_inner_offset = getBlockInnerOffset(f_offset);
  pp("block_inner_offset", block_inner_offset);
  int disk_addr = calcDiskAddr(node, f_offset);

  
  /* if involves only current block */
  if (block_inner_offset + len <= dsize) {  
    /* if reading reaches the end of file, stop */
    len = (len > node->size - f_offset) ? (node->size - f_offset) : len;
    pp("read", len, "bytes from address", disk_addr);
    dread(disk_addr, buffer, len);
  }
  /* if read more than one data block */
  else {
    pp("read to next data block");
    int read_len = dsize - block_inner_offset;
    dread(disk_addr, buffer, read_len);
    readData(node, f_offset+read_len, ((char*)buffer)+read_len, len-read_len);
  }

}

int VSFileSystem::dread(int dest, void * buffer, int len) {
  disk.seekg(dest);
  disk.read((char*)buffer, len);
}

int VSFileSystem::addEntryToDir(int dir, DirEntry* en) {
  /* treat dir entry as just a block of memory and write it to disk */ 
  Inode * dir_node = readInode(dir);
  int buffer_size = en->getSize();
  char* buffer = new char[buffer_size];
  std::memcpy(buffer, &en->node_index, sizeof(int));
  std::memcpy(buffer+sizeof(int), &en->nlen, sizeof(int));
  std::memcpy(buffer+2*sizeof(int), en->name, en->nlen);

  /* write directly from the end of file */
  writeData(dir_node, dir_node->size, buffer, buffer_size);


  /* update file size */
  dir_node->size += buffer_size;
  writeInode(dir, dir_node);

  /* update file counter */
  int f_cnt = getIntAt(dir_node->addr_0);
  putIntAt(dir_node->addr_0, f_cnt+1);


  /* update cwd_table */
  loadDirTable();
  
  cout << "dir updated size: " << dir_node->size << endl;
  cout << "dir's now file number: " << f_cnt+1 << endl;
  delete dir_node;  
}

int VSFileSystem::getDirFileNum(Inode* dir_node) {
  Address saved_tellg = disk.tellg();
  int num = getIntAt(dir_node->addr_0);
  disk.seekg(saved_tellg);
  return num;
}

bool VSFileSystem::checkDirEmpty(Inode* dir_node) {
  assert(dir_node->type == 1);
  int file_num = getDirFileNum(dir_node);
  assert(file_num >= 2);
  pp("file num:", file_num);

  /* if only contains "." and ".." */
  if (file_num == 2) {
    return true;
  }
  else {
    return false;
  }
}


/* general file rm:
   reset its inode block
   reset its data block(maybe more than 1)
   remove its entry in current dir
   if in current dir, delete entry in pwd_table

   if file, delete
   if dir, check if empty and delete
*/

/* parameters:
   d_id: inode if of the directory that contains the file
   filename: name of the file that is to be deleted
   f_t: tyoe of the file that is to be deleted

 */

void VSFileSystem::rm(const char* name) {
  rmDirEntry(cwd, name, 1);
}

int VSFileSystem::rmdir(const char* name) {
  pp("===== rmdir", name, "=====");
  rm(name);
  return 0;
}


void VSFileSystem::rmDirEntry(int d_id, const char* filename, int f_t) {
  Inode * dir = readInode(cwd);
  /* get file counter */
  int file_cnt = getDirFileNum(dir);
  cout << "file counter:" << " " << file_cnt << "\n";

  int ff = 0; // file offset of each entry
  DirEntry * en = new DirEntry(0, 0, "");
  disk.seekg(dir->addr_0 + sizeof(int));
  for (int i = 0; i < file_cnt; i++) {
    disk >> en;
    int en_id = en->node_index;
    pp("entry id", en_id);
    Address saved_tellg = disk.tellg();
    Inode* entry_node = readInode(en_id);
    disk.seekg(saved_tellg);
    
    // pp(en->node_index);
    // pp(en->nlen);
    // pp(en->name);
    // pp(entry_node->type);
    // pp("current file offset:", ff);
    // pp("");


    /* if find the file that is to be deleted */
    if (std::strcmp(en->name, filename) == 0) {
      if (entry_node->type == 1) {
	if (!checkDirEmpty(entry_node)) {
	  std::cerr << "directory must be empty." << std::endl;
	  return;
	}
	else {
	  pp("dir is empty...");
	}
      }

      
      pp("delete", filename);
      int buffer = -1;
      pp("set entry inode field to -1");
      writeData(dir, ff, &buffer, sizeof(int));
      delete[] en->name;
      delete en;
      return;
    }

    ff += en->getSize();
    delete[] en->name; 
  }
  delete en;

}


void VSFileSystem::freeBit(Address start_address, int offset) {
  Address saved_tellp = disk.tellp();
  char buffer = 1;
  dwrite(start_address + offset, &buffer, 1);
  disk.seekp(saved_tellg);
}

void VSFileSystem::freeInodeBlock(int i_id) {
  freeBit(section_offset[1], i_id);
}

void VSFileSystem::freeInodeBlock(int d_id) {
  freeBit(section_offset[2], i_id);
}

int VSFileSystem::getInodeIdByOffset() {

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

int VSFileSystem::mkdir(const char * name) {
  pp("===== mkdir", name, "=====");
  int i_id = newDir();
  if (i_id == -1) {
    return -1;
  }
  DirEntry* en = new DirEntry(i_id, std::strlen(name), name);
  addEntryToDir(cwd, en);
  delete en;
}

int VSFileSystem::newDir() {
  int i_id = allocBit(section_offset[1], inum);
  if (i_id == -1) {
    std::cerr << "fail to allocate inode, inode region full.\n";
    return -1;
  }

  int d_id = allocBit(section_offset[2], dnum);
  if (d_id == -1) {
    std::cerr << "fail to allocate data block, disk full.\n";
    return -1;
  }

  cout << "allocate inode id: " << i_id << endl;
  cout << "allocate data block id: " << d_id << endl;
  

  /* write new node into disk */
  Inode * node = new Inode();
  node->type = 1; // set inode type to dir
  pp("set node type to 1...");
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

/* side effect: move file offset one byte forward */
int VSFileSystem::getIntAt(int addr) {
  int buffer;
  disk.seekg(addr);
  disk.read((char*)&buffer, sizeof(buffer));
  return buffer;
}

int VSFileSystem::putIntAt(int addr, int value) {
  disk.seekp(addr);
  disk.write((char*)&value, sizeof(value));
  disk.flush();
}

int VSFileSystem::loadDirTable() {
  cout << "load current dir table into memory..." << endl;
  Inode * dir = readInode(cwd);
  int dir_start = dir->addr_0;
  pp("read from address", dir_start);
  cout << "get file counter..." << endl;
  /* get file counter */
  int file_cnt = getIntAt(dir_start);
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
  return section_offset[3] + isize * id;
}

int VSFileSystem::getDataOffset(int id) {
  return section_offset[4] + dsize * id;
}


/*
  on success, return index of allocated inode or data block
  on fail, return -1
 */
int VSFileSystem::allocBit(int start, int len) {
  disk.seekg(start);
  char buffer;
  for (int i = 0; i < len; i++) {
    disk.read(&buffer, 1);
    //pp(start+i, (int)buffer);
    if (buffer == 0) {
      /* set allocated bit to 1 */
      char value = 1;
      disk.seekp(start + i);
      disk.write(&value, 1);
      return i;
      break;
    }
  }
  return -1;
}

int VSFileSystem::allocInodeBlock() {
  int i_id = allocBit(section_offset[1], inum);
  return i_id;
}

int VSFileSystem::allocDataBlock() {
  int d_id = allocBit(section_offset[2], dnum);
  return d_id;
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
  resetDisk();
  initSuper();
  //resetImap();
  //resetDmap();
  int root_node_id = newDir();
  return root_node_id;
}

/* virtualize disk I/O in memory */
void VSFileSystem::loadDisk() {
  disk.open(disk_name);
}

void VSFileSystem::calcGlobalOffset() {
  int kb = 1 << 10;
  int mb = 1 << 20;
  section_offset[0] = 0*kb; // super block
  section_offset[1] = 2*kb; // inode map
  section_offset[2] = 4*kb; // data map
  section_offset[3] = 8*kb; // inode region
  section_offset[4] = 100*kb; // data region
  section_offset[5] = 2*mb;

  inum = (section_offset[4] - section_offset[3])/isize;
  dnum = (section_offset[5] - section_offset[4])/dsize;
  pp("number of inodes:", inum);
  pp("number of data blocks:", dnum);
  //p(dnum);
}

void VSFileSystem::resetDisk() {
  disk.seekp(0);
  char zero = 0;
  for (int i = 0; i < disk_size; i++) {
    disk.write(&zero, 1);
  }
  disk.flush();
}

void VSFileSystem::resetImap() {
  disk.seekp(section_offset[1]);
  int len = section_offset[2] - section_offset[1];
  for (int i = 0; i < len; i++) {
    disk.write("", 1);
  }
  disk.flush();
}

void VSFileSystem::resetDmap() {
  disk.seekp(section_offset[2]);
  int len = section_offset[3] - section_offset[2];
  for (int i = 0; i < len; i++) {
    disk.write("", 1);
  }
  disk.flush();
}

void VSFileSystem::initSuper() {
  disk.seekp(section_offset[0]);
  const char* magic = "Very Simple File System";
  disk << magic;
  //disk.write(magic, 1024);
  disk.flush();
}

int VSFileSystem::createVirtualDisk() {
  std::cout << "creating virtual disk...\n";
  /* if use disk_name instead of a string literal, the disk size will be unexpected */
  std::ofstream ofs("vdisk", std::ios::binary | std::ios::out);
  pp(disk_size);
  for (int i = 0; i < disk_size; i++) {
    ofs.write("", 1);
  }
  ofs.close();
  return 0;
}


int testReadWrite() {
  VSFileSystem* fs = new VSFileSystem();
  fs->open("foo", "w");
  fs->open("bar", "w");
  fs->write(3, "good restraut!");
  fs->cat("foo");
  fs->seek(3, 4);
  fs->close(3);
  fs->ls();
  fs->mkdir("new foo");
  fs->ls();
  fs->rmdir("new foo");
  //fs->prompt();
  delete fs;
  return 0;

}

int testLevel1() {
  VSFileSystem* fs = new VSFileSystem();
  fs->open("foo", "w");
  fs->seek(3, 4*1024);
  fs->write(3, "new block");
  fs->seek(3, 0);
  fs->read(3, 4*1024+10);
  delete fs;
}


int main() {
  testReadWrite();
  //testLevel1();
}
