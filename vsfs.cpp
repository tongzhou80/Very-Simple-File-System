#include <fstream>
#include <streambuf>
#include <cstdlib>
#include <ctime>
#include <cassert>
#include <cstring>
#include <sstream>
#include <iostream>
#include <vector>
#include "vsfs.hpp"

#define LOG_DEBUG
//using namespace std;
/*
  organization
  inode: 256 bytes
  data block: 4k bytes
 */


/* ===== status =====
   mkfs() is called before any test begins
   when file entry is deleted, set inode id to -1
   after delete a file, the dir's size won't change, because the unused area will be used in the future
   

   ===== to do =====
   add more inode information into pwd_table, such as size, type and so on
   update pwd_table in memory instead of loading modified dir content from disk everytime
*/

template <typename T>
void pp(const T& t){
  std::cerr << t << std::endl;  
}

template <typename T, typename ... Args>
void pp(const T& t, Args ... args){
  std::cerr << t << ' ';
  pp(args...);
}

/* size of an inode and a data block */
int VSFileSystem::isize = 256;
int VSFileSystem::dsize = 4*1024;

VSFileSystem::VSFileSystem() {
  disk_name = "vdisk";
  disk_size = 100*1024*1024;
  cap_0 = VSFileSystem::dsize;
  cap_1 = cap_0 / sizeof(int) * cap_0;
  cap_2 = cap_0 / sizeof(int) * cap_1;
  calcGlobalOffset();
  // printConfig();

  /* use a general stringstream if not read from stdin */
  input_stream = &std::cin;
  disk.open(disk_name, std::fstream::binary | std::fstream::in | std::fstream::out);
  
  /* fd start from 3, 0-2 reserve for std fd */
  fd_cnt = -1;
  // root = mkfs(); // call mkfs will wipe and format the disk
  root = 0; // root defaults to No. 0 inode  

  
  /* by default, open root dir */
  cwd = root;
  loadCwdTable();
}


Inode::Inode() {
  this->type = 0; 
  this->size = 0;
  this->capacity = VSFileSystem::dsize;
  this->addr_0 = -1;
  this->addr_1 = -1;
  this->addr_2 = -1;

  std::strcpy(this->date, "date unknown");
  
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
  std::cout << "---------------------------" << std::endl;
  std::cout << "disk size: " << disk_size << std::endl;
  std::cout << "level 0 capacity: " << cap_0/1024 << "Kb" << std::endl;
  std::cout << "level 1 capacity: " << cap_1/1024/1024 << "Mb" << std::endl;
  std::cout << "level 2 capacity: " << cap_2/1024/1024/1024 << "Gb" << std::endl;
  std::cout << "---------------------------" << std::endl;
}


VSFileSystem::~VSFileSystem() {
  disk.close();
}

char* VSFileSystem::getCurrentTime() {
  time_t rawtime = std::time(0);
  struct tm * timeinfo = std::localtime(&rawtime);
  char* cur_time = std::asctime(timeinfo);
  cur_time[24] = '\0'; // repplace the appending '\n' to '\0'
  return cur_time;
}

/* return a file descriptor */
int VSFileSystem::createFile() {
  //pp("### create new file...");
  int i_id = allocInodeBlock();
  if (i_id == -1) {
    std::cout << "fail to allocate inode, inode region full.\n";
    return -1;
  }

  int d_id = allocDataBlock();
  if (d_id == -1) {
    std::cout << "fail to allocate data block, disk full.\n";
    return -1;
  }

  Inode * node = new Inode();
  node->addr_0 = getDataOffset(d_id);
  std::strcpy(node->date, getCurrentTime());
  writeInode(i_id, node);

  return i_id;
}

int VSFileSystem::newDir() {
  int i_id = createFile();
  Inode* node = readInode(i_id);
  node->type = 1;
  node->size = sizeof(int);
  writeInode(i_id, node);
  putIntAt(node->addr_0, 0);
  addEntryToDir(i_id, ".", i_id);
  addEntryToDir(cwd, "..", i_id);
  loadCwdTable();
  /* now file counter is 2 */
  
  delete node;
  return i_id;
}

// int VSFileSystem::newDir() {
//   pp("create new dir...");
//   int i_id = allocBit(section_offset[1], inum);
//   if (i_id == -1) {
//     std::cout << "fail to allocate inode, inode region full.\n";
//     return -1;
//   }

//   int d_id = allocBit(section_offset[2], dnum);
//   if (d_id == -1) {
//     std::cout << "fail to allocate data block, disk full.\n";
//     return -1;
//   }

//   cout << "allocate inode id: " << i_id << endl;
//   cout << "allocate data block id: " << d_id << endl;
  

//   /* write new node into disk */
//   Inode * node = new Inode();
//   node->type = 1; // set inode type to dir
//   pp("set node type to 1...");
//   int data_offset = getDataOffset(d_id);
//   node->addr_0 = data_offset;

//   DirEntry * e1 = new DirEntry(i_id, 1, ".");
//   DirEntry * e2 = new DirEntry(cwd, 2, "..");

//   cout << "write dir content..." << endl;
//   disk.seekp(data_offset);
//   /* first byte in data block represents file counter */
//   int init_file_cnt = 2;
//   disk.write((char*)&init_file_cnt, sizeof(int));
//   disk << e1;
//   disk << e2;

  
//   cout << "write dir inode..." << endl;
//   /* update size field in inode */
//   int newsize = sizeof(init_file_cnt) + e1->getSize() + e2->getSize();
//   node->size = newsize;
//   writeInode(i_id, node);
  

//   delete node;
//   delete e1;
//   delete e2;
//   return i_id;
// }


int VSFileSystem::registerFD(int i_id) {
  fd_cnt++;
  pp("register fd", fd_cnt, "int fd map");
  std::pair<int, int> index(i_id, 0); // file pointer defaults to 0
  std::pair<int, std::pair<int, int> > map_entry(fd_cnt, index);
  fd_map.insert(map_entry);
  return fd_cnt;
}

int VSFileSystem::releaseFD(int fd) {
  std::map<int, std::pair<int, int> >::iterator iter;
  iter = fd_map.find(fd);
  if (iter == fd_map.end()) {
    std::cout << "unknown file descriptor\n";
    return -1;
  }
  else {
    pp("erase fd", fd, "in fd_map");
    fd_map.erase(iter);
  }

}

/* only support oepn in current dir */
/* initial offset: */
/* open existing file for reading: 0
   open existing file for writing: end of file
   open unexisting file for reading: fileNotExist exception
   open unexisting file for writing: 0
 */
int VSFileSystem::open(const char* file_path, const char* flag) {
  pp("===== open", file_path, flag, "=====");
  int saved_cwd = cwd;
  /* parse file_path to be two parts, file_dir and file_name */
  const char* file_dir = ".";
  const char* file_name = file_path;
  cd(file_dir);

  int f_id = findFileInCurDir(file_name);
  if (f_id != -1) {
    if (std::strcmp(flag, "r") == 0) {
      pp("register fd", fd_cnt, "in fd_map");
      registerFD(f_id);
      std::cout << "SUCCESS, fd = " << fd_cnt << std::endl;
    }
    else if (std::strcmp(flag, "w") == 0) {
      /* to do */
    }
    else {
      std::cout << "invalid flag\n";
      return -1;
    }
  }

  else {
    if (std::strcmp(flag, "r") == 0) {
      std::cout << "file not existed." << std::endl;
      return -1;
    }
    else if (std::strcmp(flag, "w") == 0) {
      f_id = createFile(); // will also register file descriptor
      addEntryToDir(f_id, file_name, cwd);
      loadCwdTable();
      int nfd = registerFD(f_id);
      std::cout << "SUCCESS, fd = " << nfd << std::endl;
    }
    else {
      std::cout << "invalid flag\n";
      return -1;
    }
  }

  changeCwdTo(saved_cwd);
  return fd_cnt;
}

void VSFileSystem::import(const char* source, const char* dest) {
  int fd = open(dest, "w");
  std::ifstream ifs(source);
  char c[2];
  c[1] = '\0';
  while (ifs.get(c[0])) {
    write(fd, c);
  }
  
}

void VSFileSystem::export_(const char* source, const char* dest) {
  std::ofstream ofs(dest);
  std::streambuf* oldCoutStreamBuf = std::cout.rdbuf();
  std::cout.rdbuf(ofs.rdbuf());

  cat(source);

  // Restore old cout.
  std::cout.rdbuf( oldCoutStreamBuf );

  ofs.close();
}


// /* to to do */
// /* only suuport current dir now, but designed to support any path */
// int VSFileSystem::open(const char* filename, const char* flag) {
//   // char filename[256];
//   // char flag[2];
//   // input_stream->getline(filename, 256, ' ');
//   // input_stream->getline(flag, 256);

//   pp("===== open", filename, flag, "=====");
//   int FLAG;

//   if (std::strcmp(flag, "r") == 0) {
//     FLAG = 0;
//   }
//   else if (std::strcmp(flag, "w") == 0) {
//     FLAG = 1;
//     /* update dir inode size field */
//     /* update data block */
//   }
//   else {
//     std::cout << "invalid flag\n";
//     return -1;
//   }


//   /* absolute path */
//   if (filename[0] == '/') {
//     cwd = root;
//   }

//   pp(filename);
  
//   /* relative path */
//   int enter_file = cwd;
//   int enter_dir;
//   char* newfile_name;
//   bool fileExist = true;
//   char * pch;
//   const char * delim = "/";
//   std::vector<char*> newfiles;
//   pch = std::strtok(strdup(filename), delim);

//   while (pch != NULL) {
//     /* find in current working dir */
//     cout << "find inode of " << pch << endl;
//     /* load dir table */
//     /* to do */
    
    
//     std::map<std::string, int>::iterator iter = cwd_table.find(std::string(pch));
//     if (iter != cwd_table.end()) {
//       enter_file = iter->second;
//     }
//     else {
//       fileExist = false;
//       if (FLAG == 0) {
// 	std::cout << "no such file.\n";
// 	return -1;
//       }
//       else if (FLAG == 1) {
// 	newfiles.push_back(pch);
//       }
      
//     }
//     pch = std::strtok(NULL, delim);
//   }

//   if (fileExist) {
//     if (FLAG == 0) {
//       cout << "read: open file node " << enter_file << endl;      
//     }
//     else if (FLAG == 1) {
//       cout << "write: open file node " << enter_file << endl;
//     }
//     else {
//       /* pass */
//     }
    
//     fd_cnt++;
//     cout << "register fd " << fd_cnt << " in fd map" << endl;
//     registerFD(fd_cnt, enter_file);
//     std::cout << "SUCCESS, fd = " << fd_cnt << std::endl;
//     return fd_cnt;
//   }
//   /* if file not exist, create new file */
//   else {
//     if (newfiles.size() > 1) {
//     	std::cout << "dir not exist, please make dir first.\n";
//     	return -1;
//     }
//     else {
//     	/* pass */
//     }
    
//     enter_dir = enter_file;
//     newfile_name = newfiles[0];
//     cout << "create new file " << newfile_name << " in inode(dir) " << enter_dir << endl;
   
//     int nfd = createFile();
//     std::map<int, std::pair<int, int> >::iterator iter;
//     iter = fd_map.find(nfd);
//     int f_inode_id = (iter->second).first;
//     DirEntry * en = new DirEntry(f_inode_id, std::strlen(newfile_name), newfile_name);

//     cout << "update dir data content..." << endl;
//     addEntryToDir(cwd, en);
//     delete en;
//     std::cout << "SUCCESS, fd = " << fd_cnt << std::endl;
//     return nfd;
//   }

//   return -1;
// }

int VSFileSystem::findFileInCurDir(const char* filename) {      
  std::map<std::string, int>::iterator iter = cwd_table.find(std::string(filename));
  if (iter != cwd_table.end()) {
    return iter->second;
  }
  return -1;
}

/* change cwd and load dir table */
void VSFileSystem::changeCwdTo(int dir_id) {
  cwd = dir_id;
  loadCwdTable();
}

int VSFileSystem::cd(const char* dirname) {
  pp("===== cd", dirname, "=====");

  /* absolute path */
  if (dirname[0] == '/') {
    changeCwdTo(root);
    pp("change dir to root");
  }

  /* relative path */
  int enter_file = cwd;
  int enter_dir;
  char* newfile_name;
  bool fileExist = true;
  char * pch;
  const char * delim = "/";
  std::vector<char*> newfiles;
  pch = std::strtok(strdup(dirname), delim);

  while (pch != NULL) {
    /* find in current working dir */
    pp("find inode of", pch);
    /* load dir table */
    /* to do */
    int file_id = findFileInCurDir(pch);
    if (file_id == -1) {
      std::cout << "error: file " << pch << " not found." << std::endl;
      return -1;
    }
    
    pp("change dir to inode", file_id);
    changeCwdTo(file_id);
    pch = std::strtok(NULL, delim);
  }

  pp("===== cd ends =====");
  return 0;
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
    case 0: type = "-"; break;
    case 1: type = "d"; break;
    }
    
    std::cout << type << "\t"
	      << file_name << "\t"
	      << node->date << "\t"
	      << node->size << "Kb" << std::endl;
    delete node;
  }
}

void VSFileSystem::tree() {
  pp("===== tree =====");
  tree_inner(cwd, 0);
}

void VSFileSystem::tree_inner(int dir, int depth) {
  std::map<std::string, int> dir_table;
  loadDirTable(dir, dir_table);
  std::map<std::string, int>::iterator iter;
  Inode * node;
  for(iter = dir_table.begin(); iter != dir_table.end(); ++iter)
  {
    int i_id = iter->second;
    const char* file_name = iter->first.c_str();
    if (std::strcmp(file_name, ".") == 0 || std::strcmp(file_name, "..") == 0)
      continue;
    node = readInode(i_id);
    char* type;
    int size = node->size;

    for(int j = 0; j < depth; j++) {
      std::cout << "|   ";
    }
    
    std::cout << "|---"
	      << file_name << std::endl;

    if (node->type == 1) {
      tree_inner(i_id, depth+1);
    }
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
    std::cout << "unknown file descriptor\n";
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
    std::cout << "unknown file descriptor\n";
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
    std::cout << "allocate data block failed." << std::endl;
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
    std::cout << "file offset over 4Gb, too large to handle.\n";
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
#ifdef DEBUG_writeData
  pp("write data...");
  pp("read node capacity...", capacity);
#endif
  int dsize = VSFileSystem::dsize;
  int capacity = node->capacity;
  int block_left = capacity - f_offset;

  /* file offset within capacity */
  if (block_left > 0) {
    /* check if writing involves more than one block */
    int block_inner_offset = getBlockInnerOffset(f_offset);
#ifdef DEBUG_writeData
    pp("block_inner_offset", block_inner_offset);
#endif
    int disk_addr = calcDiskAddr(node, f_offset);
    /* if involves only current block */
    if (block_inner_offset + len <= dsize) {
#ifdef DEBUG_writeData
      pp("write", len, "bytes to address", disk_addr);
#endif
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
    std::cout << "unknown file descriptor\n";
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

int VSFileSystem::cat(const char* dirname) {
  pp("===== cat =====");
  pp("lookup working dir table...");
  std::map<std::string, int>::iterator iter = cwd_table.find(std::string(dirname));
  if (iter == cwd_table.end()) {
    std::cout << "no such file.\n";
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
    std::cout << "file offset is greater than file size. read nothing." << std::endl;
    return 0;
  }

  int dsize = VSFileSystem::dsize;
  int block_inner_offset = getBlockInnerOffset(f_offset);

#ifdef DEBUG_readData
  pp("block_inner_offset", block_inner_offset);
#endif
  int disk_addr = calcDiskAddr(node, f_offset);
  
  /* if involves only current block */
  if (block_inner_offset + len <= dsize) {  
    /* if reading reaches the end of file, stop */
    len = (len > node->size - f_offset) ? (node->size - f_offset) : len;
#ifdef DEBUG_readData
    pp("read", len, "bytes from address", disk_addr);
#endif
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

#define DEBUG_addEntryToDir
int VSFileSystem::addEntryToDir(int f_id, const char* filename, int dir) {
  
  Inode * dir_node = readInode(dir);
  int nlen = std::strlen(filename);
  writeData(dir_node, dir_node->size, &f_id, sizeof(f_id));
  dir_node->size += sizeof(f_id);
  writeData(dir_node, dir_node->size, &nlen, sizeof(nlen));
  dir_node->size += sizeof(nlen);
  writeData(dir_node, dir_node->size, filename, nlen);
  dir_node->size += nlen;

  /* update file size */
  writeInode(dir, dir_node);

  /* update file counter */
  int f_cnt = getIntAt(dir_node->addr_0);
  putIntAt(dir_node->addr_0, f_cnt+1);

  delete dir_node;

#ifdef DEBUG_addEntryToDir   
  pp("add entry to dir...");
  pp("dir updated size:", dir_node->size);
  pp("dir's now file number: ", f_cnt+1);
#endif

}
// int VSFileSystem::addEntryToDir(int dir, DirEntry* en) {
//   /* treat dir entry as just a block of memory and write it to disk */ 
//   Inode * dir_node = readInode(dir);
//   int buffer_size = en->getSize();
//   char* buffer = new char[buffer_size];
//   std::memcpy(buffer, &en->node_index, sizeof(int));
//   std::memcpy(buffer+sizeof(int), &en->nlen, sizeof(int));
//   std::memcpy(buffer+2*sizeof(int), en->name, en->nlen);

//   /* write directly from the end of file */
//   writeData(dir_node, dir_node->size, buffer, buffer_size);


//   /* update file size */
//   dir_node->size += buffer_size;
//   writeInode(dir, dir_node);

//   /* update file counter */
//   int f_cnt = getIntAt(dir_node->addr_0);
//   putIntAt(dir_node->addr_0, f_cnt+1);


//   /* update cwd_table */
//   loadCwdTable();
  
//   cout << "dir updated size: " << dir_node->size << endl;
//   cout << "dir's now file number: " << f_cnt+1 << endl;
//   delete dir_node;  
// }

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
   don't update dir's size
   update dir's file count
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
  pp("file counter:", file_cnt);

  int ff = 0; // file offset of each entry
  DirEntry * en;
  //disk.seekg(dir->addr_0 + sizeof(int));
  int f_offset_ini = sizeof(int);
  int f_offset = f_offset_ini;

  for (int i = 0; i < file_cnt; i++) {
    en = readEntry(dir, f_offset);
    Address saved_tellg = disk.tellg();
    Inode* entry_node = readInode(en->node_index);
    disk.seekg(saved_tellg);

    int en_id = en->node_index;
    pp("entry id", en_id);
        
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
	  std::cout << "directory must be empty." << std::endl;
	  return;
	}
	else {
	  pp("dir is empty...");
	}
      }

      /* delete its entry in parent dir, set inode id to -1 */
      pp("delete", filename);
      pp("set unused area to -1");
      int buffer = -1;
      writeData(dir, f_offset, &buffer, sizeof(buffer));

      /* !!! after delete file, dir size won't change !!! */
      // /* update dir size */
      // dir->size -= 1;

      /* update dir file count */
      int f_cnt = getIntAt(dir->addr_0);
      putIntAt(dir->addr_0, f_cnt-1);
      
      /* reload cwd table */
      loadCwdTable();

      /* free data block and inode block */
      freeFile(en_id);
      
      delete[] en->name;
      delete en;
      return;
    }

    f_offset += en->getSize();
    delete[] en->name; 
  }
  delete en;

}

/* free both its inode block and data block */
void VSFileSystem::freeFile(int i_id) {
  /* free data blocks first */
  Inode* node = readInode(i_id);
  /* if only use addr_0 */
  if (node->addr_1 == -1) {
    int d_id = getDataIdByOffset(node->addr_0);
    pp("free data id:", d_id);
    freeDataBlock(d_id);
  }
  /* if only use addr_0 and addr_1 */
  else if (node->addr_2 == -1) {
    Address level_1_addr = getIntAt(node->addr_1);
    int level_1_size = node->capacity/dsize - 1;
    for (int i = 0; i < level_1_size; i++) {
      Address block_address = level_1_addr + i*sizeof(Address);
      int d_id = getDataIdByOffset(block_address);
      pp("free data id:", d_id);
      freeDataBlock(d_id);
    }
  }
  /* if use addr_0, addr_1 and addr_2 */
  else {
    
  }

  pp("free inode id", i_id);
  freeInodeBlock(i_id);
}

void VSFileSystem::freeBit(Address start_address, int offset) {
  Address saved_tellp = disk.tellp();
  char buffer = 1;
  dwrite(start_address + offset, &buffer, 1);
  disk.seekp(saved_tellp);
}

void VSFileSystem::freeInodeBlock(int i_id) {
  freeBit(section_offset[1], i_id);
}

void VSFileSystem::freeDataBlock(int d_id) {
  freeBit(section_offset[2], d_id);
}

// int VSFileSystem::getInodeIdByOffset(Address offset) {

// }

int VSFileSystem::getDataIdByOffset(Address offset) {
  return (offset - section_offset[4]) / dsize;
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

char* VSFileSystem::formatRelativePath(char* path) {
  int path_len = std::strlen(path);
  char* rel_path = new char[path_len+2];
  rel_path[0] = '.';
  rel_path[1] = '/';
  std::strcpy(rel_path+2, path);
  return rel_path;
}

/* after parse, file_path become file_dir and return value is file_name */
/* file_path may be modified, so use reference */
char* VSFileSystem::parsePath(char* & file_path) {
  int slash_cnt = 0;
  /* first pass */
  for (int i = 0; i < std::strlen(file_path); i++) {
    if (file_path[i] == '/') {
      slash_cnt++;
    }
  }

  if (slash_cnt == 0) {
    char* old = file_path;
    file_path = formatRelativePath(old);
    slash_cnt = 1;
    delete old;
  }
  // pp("new path", file_path);
  // pp("slash_cnt", slash_cnt);

  /* second pass */
  int slash_p = 0;
  for (int i = 0; i < std::strlen(file_path); i++) {
    if (file_path[i] == '/') {
      slash_p++;
      if (slash_p == slash_cnt) {
	file_path[i] = '\0';
	return file_path+i+1;
      }
    }
  }
}

/* suppose mkdir d1/d2/d3 */
int VSFileSystem::mkdir(const char * name) {
  pp("===== mkdir", name, "=====");
  int saved_cwd = cwd;
  /* parse file_path to be two parts, file_dir and file_name */
  char* file_path = strdup(name);

  char* file_name = parsePath(file_path);
  char* file_dir = file_path;
  pp("------------dir", file_dir);
  pp("------------file", file_name);
  /* cd d1/d2, cwd points to d1/d2*/
  cd(file_dir);
  
  int i_id = newDir();
  if (i_id == -1) {
    return -1;
  }
  /* add entry to d2 */
  addEntryToDir(i_id, file_name, cwd);
  /* update d2 table */
  loadCwdTable();

  /* back to ./ */
  changeCwdTo(saved_cwd);

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

/* read dir entry at a certain file offset */
DirEntry* VSFileSystem::readEntry(Inode* dir, int f_offset) {
  DirEntry * en = new DirEntry(0, 0, "");
  int buffer;
  readData(dir, f_offset, &buffer, sizeof(int));
  en->node_index = buffer;
  f_offset += sizeof(int);
  readData(dir, f_offset, &buffer, sizeof(int));
  en->nlen = buffer;
  f_offset += sizeof(int);
  char* name = new char[en->nlen+1];
  readData(dir, f_offset, name, en->nlen);
  name[en->nlen] = '\0';
  en->name = name; // please delete[] en->name when entry dies

  return en;
}

/* check if en entry exists at a certain file offset */
bool VSFileSystem::checkEntryExist(Inode* dir, int offset) {
  int node_id;
  readData(dir, offset, &node_id, sizeof(node_id));
  if (node_id != -1) {
    return true;
  }
  else {
    return false;
  }
}

#define DEBUG_loadCwdTable
int VSFileSystem::loadCwdTable() {
#ifdef DEBUG_loadCwdTable
  pp("### reload dir...");
  pp("clear cwd_table...");
#endif
  cwd_table.clear();
  Inode * dir = readInode(cwd);
  int dir_start = dir->addr_0;
  /* get file counter */
  int file_cnt = getIntAt(dir_start);
#ifdef DEBUG_loadCwdTable
  pp("file counter:", file_cnt);
#endif
  
  DirEntry * en;
  int f_offset_ini = sizeof(int);
  int f_offset = f_offset_ini;
  for (int i = 0; i < file_cnt; i++) {
#ifdef DEBUG_loadCwdTable
    pp("read entry from file offset:", f_offset);
#endif
    en = readEntry(dir, f_offset);
    while (en->node_index == -1) {
#ifdef DEBUG_loadCwdTable
    pp("skip one deleted entry... move forward");
#endif
      f_offset += en->getSize();
      en = readEntry(dir, f_offset);
    }
    
    f_offset += en->getSize();
    std::pair<std::string, int> file_entry(std::string(en->name), en->node_index);
    cwd_table.insert(file_entry);
#ifdef DEBUG_loadCwdTable
    pp("insert into table:", en->name, en->node_index);
#endif
  }
  delete[] en->name; 
  delete en;
}


void VSFileSystem::loadDirTable(int dir_id, std::map<std::string, int>& dir_table) {
#ifdef DEBUG_loadDirTable
  pp("### load inode", dir, "into to table");
#endif
  Inode * dir = readInode(dir_id);
  int dir_start = dir->addr_0;
  /* get file counter */
  int file_cnt = getIntAt(dir_start);
#ifdef DEBUG_loadDirTable
  pp("file counter:", file_cnt);
#endif
  
  DirEntry * en;
  int f_offset_ini = sizeof(int);
  int f_offset = f_offset_ini;
  for (int i = 0; i < file_cnt; i++) {
#ifdef DEBUG_loadDirTable
    pp("read entry from file offset:", f_offset);
#endif
    en = readEntry(dir, f_offset);
    while (en->node_index == -1) {
#ifdef DEBUG_loadDirTable
    pp("skip one deleted entry... move forward");
#endif
      f_offset += en->getSize();
      en = readEntry(dir, f_offset);
    }
    
    f_offset += en->getSize();
    std::pair<std::string, int> file_entry(std::string(en->name), en->node_index);
    dir_table.insert(file_entry);
#ifdef DEBUG_loadDirTable
    pp("insert into table:", en->name, en->node_index);
#endif
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

#define DEBUG_alloc
int VSFileSystem::allocInodeBlock() {
  int i_id = allocBit(section_offset[1], inum);
#ifdef DEBUG_alloc
  pp("allocate inode id", i_id);
#endif
  return i_id;
}

int VSFileSystem::allocDataBlock() {
  int d_id = allocBit(section_offset[2], dnum);
#ifdef DEBUG_alloc
  pp("allocate data block id", d_id);
#endif
  return d_id;
}

/* accept command line arguments as commands */
/* redirect output to a file, and rpc server will return file content to client */
void VSFileSystem::rpc(int argc, char* argv[]) {
  std::ofstream ofs("tmp");
  std::streambuf* oldCoutStreamBuf = std::cout.rdbuf();
  std::cout.rdbuf(ofs.rdbuf());

  // std::cout << "argc: " << argc << std::endl;
  // std::cout << "argv[1]: " << argv[1] << std::endl;

  char* cmd = strdup(argv[1]);
  const int MAX_LEN = 1024;
  char* op;
  char separator = ' ';
  char *sep_at = strchr(cmd, separator);
  if(sep_at != NULL)
    {
      *sep_at = '\0'; /* overwrite first separator, creating two strings. */
      parseCmd(cmd, sep_at + 1);
    }
  else {
    parseCmd(cmd, NULL);
  }

  
  // Restore old cout.
  std::cout.rdbuf( oldCoutStreamBuf );
  ofs.close();
}

void VSFileSystem::parseCmd(char* op, char* rest) {
  const int MAX_LEN = 1024;
  std::stringstream ss;
  if (rest != NULL) {
    ss.str(rest);
    pp(op, rest);
  }
  
  char delim = ' ';
  
  if (std::strcmp(op, "mkfs") == 0) {
    mkfs();
  }
  else if (std::strcmp(op, "ls") == 0) {
    pp("ls..");
    ls();
  }
  else if (std::strcmp(op, "tree") == 0) {
    tree();
  }
  else if (std::strcmp(op, "mkdir") == 0) {
    char* name = new char[MAX_LEN];
    pp("getline");
    ss.getline(name, MAX_LEN, delim);
    mkdir(name);
  }
  else if (std::strcmp(op, "rmdir") == 0) {
    char* name = new char[MAX_LEN];
    ss.getline(name, MAX_LEN, delim);
    rmdir(name);
  }
  else if (std::strcmp(op, "cat") == 0) {
    char* name = new char[MAX_LEN];
    ss.getline(name, MAX_LEN, delim);
    cat(name);
  }
  else if (std::strcmp(op, "rm") == 0) {
    char* name = new char[MAX_LEN];
    ss.getline(name, MAX_LEN, delim);
    rm(name);
  }
  else if (std::strcmp(op, "cd") == 0) {
    char* name = new char[MAX_LEN];
    ss.getline(name, MAX_LEN, delim);
    cd(name);
  }
  else if (std::strcmp(op, "open") == 0) {
    char* name = new char[MAX_LEN];
    char* flag = new char[MAX_LEN];
    ss.getline(name, MAX_LEN, delim);
    ss.getline(flag, MAX_LEN, delim);
    pp("name", name, "flag", flag);
    open(name, flag);
  }
  else if (std::strcmp(op, "import") == 0) {
    char* src = new char[MAX_LEN];
    char* dest = new char[MAX_LEN];
    ss.getline(src, MAX_LEN, delim);
    ss.getline(dest, MAX_LEN, delim);
    import(src, dest);
  }
  else if (std::strcmp(op, "export") == 0) {
    char* src = new char[MAX_LEN];
    char* dest = new char[MAX_LEN];
    ss.getline(src, MAX_LEN, delim);
    ss.getline(dest, MAX_LEN, delim);
    export_(src, dest);
  }
  else if (std::strcmp(op, "close") == 0) {
    char* fd = new char[MAX_LEN];
    ss.getline(fd, MAX_LEN, delim);
    close(std::atoi(fd));
  }
  else if (std::strcmp(op, "seek") == 0) {
    char* fd = new char[MAX_LEN];
    char* offset = new char[MAX_LEN];
    ss.getline(fd, MAX_LEN, delim);
    ss.getline(offset, MAX_LEN, delim);
    seek(std::atoi(fd), std::atoi(offset));
  }
  else if (std::strcmp(op, "read") == 0) {
    char* fd = new char[MAX_LEN];
    char* size = new char[MAX_LEN];
    ss.getline(fd, MAX_LEN, delim);
    ss.getline(size, MAX_LEN, delim);
    read(std::atoi(fd), std::atoi(size));
  }
  else if (std::strcmp(op, "write") == 0) {
    char* fd = new char[MAX_LEN];
    char* content = new char[MAX_LEN];
    ss.getline(fd, MAX_LEN, delim);

    /* parse quote, get write string content */
    char c;
    ss.get(c);
    int c_p = 0;
    if (c == '"') {
      while (ss.get(c)) {
	if (c == '"')
	  break;
	else if (c == '\\') {
	  ss.get(c);
	  if (c == 'n') {
	    content[c_p] = '\n';
	  }
	}
	else {
	  content[c_p] = c;
	}
	c_p++;
      }
      /* if last character is not " */
      if (c != '"') {
	std::cout << "quote not match" << std::endl;
	return;
      }
    }
    else {
      content[c_p] = c;
      ss.getline(content+1, MAX_LEN, delim); 
    }

    write(std::atoi(fd), content);
  }

  else {
    /* pass */
  }
}

void VSFileSystem::prompt() {
  const int MAX_LEN = 1024;
  char* input = new char[MAX_LEN];
  char* op;

  while(1) {
    //std::cout << "$ ";
    if (std::cin.getline(input, MAX_LEN)) {
      char separator = ' ';
      char *sep_at = strchr(input, separator);
      if(sep_at != NULL)
	{
	  *sep_at = '\0'; /* overwrite first separator, creating two strings. */
	  //printf("first part: '%s'\nsecond part: '%s'\n", input, sep_at + 1);
	  parseCmd(input, sep_at + 1);
	}
      else {
	parseCmd(input, NULL);
      }
    }
    else {
      break;
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
  /* assume cwd (root's parent dir) id is 0 */
  cwd = 0;
  int root_node_id = newDir();
  cwd = root_node_id;
  loadCwdTable();
  return root_node_id;
}

/* virtualize disk I/O in memory */
void VSFileSystem::loadDisk() {
  disk.open(disk_name);
}

void VSFileSystem::calcGlobalOffset() {
  int kb = 1 << 10;
  int mb = 1 << 20;
  
  /* 2M disk configuration */
  // section_offset[0] = 0*kb; // super block
  // section_offset[1] = 2*kb; // inode map
  // section_offset[2] = 4*kb; // data map
  // section_offset[3] = 8*kb; // inode region
  // section_offset[4] = 100*kb; // data region
  // section_offset[5] = 2*mb;

  section_offset[0] = 0*kb; // super block
  section_offset[1] = 256*kb; // inode map
  section_offset[2] = 512*kb; // data map
  section_offset[3] = 1*mb; // inode region
  section_offset[4] = 5*mb; // data region
  section_offset[5] = 100*mb;

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
  fs->ls();
  //fs->prompt();
  delete fs;
  return 0;
}

void testrm() {
  VSFileSystem* fs = new VSFileSystem();
  fs->open("foo", "w");
  //fs->mkdir("dir1");
  // fs->open("bar", "w");
  // fs->rmdir("dir1");
  // fs->ls();
}

void testcd() {
  VSFileSystem* fs = new VSFileSystem();
  fs->open("file1", "w");
  fs->mkdir("dir1");
  fs->mkdir("dir2");
  fs->ls();
  fs->cd("dir1");
  fs->open("f1", "w");
  fs->cd("..");
  fs->open("f2", "w");
  fs->cd("dir2");
  fs->ls();
  fs->mkdir("dir3");
  fs->cd("/");
  fs->tree();
  delete fs;
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

int testopen() {
  VSFileSystem* fs = new VSFileSystem();
  fs->open("foo", "w");
  delete fs;
}

int testPrompt() {
  VSFileSystem* fs = new VSFileSystem();
  fs->prompt();
  delete fs;
}

void testexport() {
  VSFileSystem* fs = new VSFileSystem();
  fs->mkdir("foo");
  //fs->mkdir("foo/foo1");
  //fs->mkdir("foo/foo1/foo2");
  fs->prompt();
  delete fs;
}


int main(int argc, char* argv[]) {
#ifdef LOG_DEBUG
  std::ofstream ofs("log");
  std::streambuf* oldCoutStreamBuf = std::cerr.rdbuf();
  std::cerr.rdbuf(ofs.rdbuf());
#endif

  VSFileSystem* fs = new VSFileSystem();
  // fs->prompt();
  fs->rpc(argc, argv);


  //testexport();
  //testPrompt();
  //testopen();
  //testcd();
  //testrm();
  //testReadWrite();
  //testLevel1();
  

  
  
#ifdef LOG_DEBUG
  std::cerr.rdbuf( oldCoutStreamBuf );
  ofs.close();  
#endif
}
