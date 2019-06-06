#include "CLZHashTable.h"

CLZHashTable::CLZHashTable(size_t buckets) : num_buckets(_makeprime(buckets)) {
  this->buckets = new Node*[this->num_buckets];
  for (size_t i = 0; i < this->num_buckets; ++i)
    this->buckets[i] = nullptr;
}

CLZHashTable::~CLZHashTable() {
  this->clear();
  
  delete[] this->buckets;
  
  while (!this->alloc_pool.empty()) {
    delete this->alloc_pool.top();
    this->alloc_pool.pop();
  }
}

void CLZHashTable::setStrLen(size_t str_len) {
  this->str_len = str_len;
}

void CLZHashTable::clear() {
  for (size_t i = 0; i < this->num_buckets; ++i) {
    Node* node = this->buckets[i];
    while (node) {
      Node* temp = node->next;
      delete node;
      node = temp;
    }
    this->buckets[i] = nullptr;
  }
}

void CLZHashTable::removeNode(std::pair<Node*, size_t> const& pair) {
  Node* const& nodeptr = pair.first;
  size_t ofs = pair.second;
  if (nodeptr->last_index == ofs) {
    int idx = nodeptr->hash % this->num_buckets;
    if (nodeptr->next) {
      nodeptr->next->prev = nodeptr->prev;
    }
    if (nodeptr->prev) {
      nodeptr->prev->next = nodeptr->next;
    } else {
      this->buckets[idx] = nodeptr->next;
    }
    this->alloc_pool.push(nodeptr);
  }
}

std::pair<CLZHashTable::Node*, size_t> CLZHashTable::addNode(char* const window, size_t window_size, size_t start_ofs, size_t hash) {
  int idx = hash % this->num_buckets;
  Node* prev = nullptr, * next = this->buckets[idx];
  
  while (next && !(hash == next->hash && this->_isequal(window, window_size, start_ofs, next->last_index))) {
    prev = next;
    next = next->next;
  }
  size_t prev_index = start_ofs;
  if (next == nullptr) {
    if (this->alloc_pool.empty()) {
      next = new Node;
    } else {
      next = this->alloc_pool.top();
      this->alloc_pool.pop();
    }
    next->prev = prev;
    next->hash = hash;
    if (prev) {
      prev->next = next;
    } else {
      this->buckets[idx] = next;
    }
    next->next = nullptr;
  } else
    prev_index = next->last_index;
  next->last_index = start_ofs;
  
  return std::pair<CLZHashTable::Node*, size_t>(next, prev_index);
}

size_t CLZHashTable::getLast(char* const window, size_t window_size, size_t def_ofs, size_t hash) const {
  int idx = hash % this->num_buckets;
  Node* next = this->buckets[idx];
  
  while (next && !(hash == next->hash && this->_isequal(window, window_size, def_ofs, next->last_index))) {
    next = next->next;
  }
  
  return next ? next->last_index : def_ofs;
}

size_t CLZHashTable::_modexp(size_t x, size_t e, size_t m) {
  typedef unsigned long long int ull;
  ull xull = static_cast<ull>(x),
      mull = static_cast<ull>(m),
      result = 1;
  while (e) {
    if (e & 1)
      result = result * xull % mull;
    e >>= 1;
    xull = xull * xull % mull;
  }
  return static_cast<size_t>(result);
}

size_t CLZHashTable::_makeprime(size_t s) {
  s |= 1;
  while (!_isprime(s))
    s += 2;
  return s;
}

bool CLZHashTable::_isprime(size_t n) {
  bool prime = true;
  size_t const test_nums[] = {2, 7, 61}; // enough for all unsigned 32 bit numbers
  size_t const length = sizeof(test_nums)/sizeof(*test_nums);
  for (size_t idx = 0, test_num; prime && idx < length && (test_num = test_nums[idx]) < n; ++idx) {
    size_t const n1 = n - 1;
    size_t r = (n1 ^ (n1 - 1));
    r ^= r >> 1;
    size_t const d = n1 / r;
    
    size_t x = _modexp(test_num, d, n);
    if (x != 1 && x != n1)  {
      prime = false;
      while (!prime && (r >>= 1) > 1) {
        x = x * x % n;
        if (x == n1)
          prime = true;
      }
    }
  }
  return prime;
}

bool CLZHashTable::_isequal(char* const window, size_t window_size, size_t ofs1, size_t ofs2) const {
  bool matches = true;
  for (size_t i = 0; matches && i < this->str_len; ++i) {
    matches &= (window[(ofs1 + i) % window_size] == window[(ofs2 + i) % window_size]);
  }
  return matches;
}