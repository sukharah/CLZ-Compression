#ifndef CLZ_H
#define CLZ_H

#include <fstream>

namespace CLZ {
  
  bool verify(std::ifstream&);
  
  void pack(std::ifstream&, std::ofstream&);
  
  void unpack(std::ifstream&, std::ofstream&);
};

#endif //CLZ_H