#ifndef CLZ_HASHTABLE_H
#define CLZ_HASHTABLE_H

#include <stack>

class CLZHashTable {
public:
  struct Node {
  private:
    size_t hash;
    size_t last_index;
    Node * prev, * next;
    friend class CLZHashTable;
  };

private:
  size_t str_len;
  
  std::stack<Node*> alloc_pool;
  
  size_t num_buckets;
  Node** buckets;
  
  static size_t _modexp(size_t, size_t, size_t);
  static size_t _makeprime(size_t);
  static bool _isprime(size_t);
  bool _isequal(char* const, size_t, size_t, size_t) const;
public:
  CLZHashTable(size_t num_buckets = 4097);
  ~CLZHashTable();
  
  std::pair<Node*, size_t> addNode(char* const window, size_t window_size, size_t start_ofs, size_t hash);
  size_t getLast(char* const window, size_t window_size, size_t def_ofs, size_t hash) const;
  void removeNode(std::pair<Node*, size_t> const&);
  void setStrLen(size_t);
  void clear();
};


#endif