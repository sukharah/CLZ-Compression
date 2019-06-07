#include "CLZ.h"

#include <iostream>
#include <climits>

#include <queue>

#include "CLZHashTable.h"

bool CLZ::verify(std::ifstream& infile) {
  int bits = 0, bit_count = 0;
  
  const int BUFFER_SIZE = 4096;
  
  char buffer[BUFFER_SIZE];
  int buffer_size = 0;
  int buffer_ofs = 0;
  
  infile.read(buffer, 16);
  bool result = true;
  int max_len = 0;
  int max_delta = 0;
  if (infile.gcount() == 16) {
    int signature = (static_cast<int>(buffer[0]) & 0xff) << 24 | (static_cast<int>(buffer[1]) & 0xff) << 16 | (static_cast<int>(buffer[2]) & 0xff) << 8 | (static_cast<int>(buffer[3]) & 0xff);
    int expected_size = (static_cast<int>(buffer[4]) & 0xff) << 24 | (static_cast<int>(buffer[5]) & 0xff) << 16 | (static_cast<int>(buffer[6]) & 0xff) << 8 | (static_cast<int>(buffer[7]) & 0xff);
    int decomp_size = 0;
    if (signature == 0x434c5a00) {
      bool exit = false;
      char c;
      while (!exit) {
        if (buffer_ofs >= buffer_size) {
          if (!infile.eof()) {
            infile.read(buffer, BUFFER_SIZE);
            buffer_size = infile.gcount();
            buffer_ofs = 0;
            if (buffer_size) {
              c = buffer[buffer_ofs++];
            } else
              exit = true;
          } else
            exit = true;
        } else {
          c = buffer[buffer_ofs++];
        }
        if (!exit) {
          if (bit_count == 0) {
            bits = static_cast<int>(c);
            bit_count = 8;
          } else {
            if (bits & 1) { // distance length code
              int window_delta = static_cast<int>(c) & 0xff;
              if (buffer_ofs >= buffer_size) {
                if (!infile.eof()) {
                  infile.read(buffer, BUFFER_SIZE);
                  buffer_size = infile.gcount();
                  buffer_ofs = 0;
                  if (buffer_size) {
                    c = buffer[buffer_ofs++];
                  } else
                    result = false;
                } else
                  result = false;
              } else {
                c = buffer[buffer_ofs++];
              }
              if (result) {
                int length = static_cast<int>(c) & 0xff;
                window_delta |= length << 4 & 0xf00;
                length = (length & 0x0f) + 3;
                window_delta = window_delta - 0x1000;
                int window_pos = decomp_size + window_delta;
                if (window_pos < 0)
                  result = false;
                if (result) {
                  decomp_size += length;
                  
                  if (length > max_len)
                    max_len = length;
                  if (window_delta < max_delta)
                    max_delta = window_delta;
                }
              }
            } else { // raw byte value
              ++decomp_size;
            }
            bits >>= 1;
            --bit_count;
          }
        }
        exit |= !result;
      }
      if (decomp_size != expected_size)
        result = false;
    } else
      result = false;
  } else
    result = false;
  return result;
}
  
void CLZ::pack(std::ifstream& infile, std::ofstream& outfile) {
  const size_t WINDOW_SIZE = 4096;
  const size_t ARRAY_SIZE = 16384;
  
  const size_t MAX_HEDGE = 18;
  
  const size_t BUFFER_SIZE = 16384;
  const size_t BUFFER_MAX_OUT = BUFFER_SIZE - 18;
  
  // hashing constants
  const size_t PRIME_A = 37,
               PRIME_B = 54059,
               PRIME_C = 76963;
  
  CLZHashTable hash_tables[MAX_HEDGE - 2];
  for (size_t i = 0; i < MAX_HEDGE - 2; ++i)
    hash_tables[i].setStrLen(i + 3);

  // indexed by substring start offset % HASH_BUFFER_SIZE
  const int HASH_BUFFER_SIZE = MAX_HEDGE * 2;
  size_t hashes[HASH_BUFFER_SIZE][MAX_HEDGE + 1];
  
  for (size_t i = 0; i < HASH_BUFFER_SIZE; ++i) {
    hashes[i][0] = PRIME_A;
  }
  
  std::queue<std::pair<CLZHashTable::Node*, size_t>> clzqueue[MAX_HEDGE - 1];
  
  char buffer[BUFFER_SIZE];
  buffer[0] = 0;
  size_t num_entries = 0;
  size_t buffer_ofs = 1;
  size_t buffer_type_ofs = 0;
  
  char window[ARRAY_SIZE];
  size_t window_ofs = 0;
  size_t decomp_size = 0;
  size_t hedge_size = 0;
  
  window[0] = 'C';
  window[1] = 'L';
  window[2] = 'Z';
  outfile.write(window, 16);
  
  bool condition = true;
  bool skip_substr = false;
  int deltaB = 0;
  size_t longestB = 0;
  size_t prev_hash_ofs = 0;
  size_t last_decomp = 0;
  while (condition) {
    if (hedge_size < MAX_HEDGE + 1 && !infile.eof()) {
      size_t diff = (ARRAY_SIZE - WINDOW_SIZE) - hedge_size;
      size_t winpos = (window_ofs + hedge_size) % ARRAY_SIZE;
      size_t rem = std::min(ARRAY_SIZE - winpos, diff);
      size_t amt = 0;
      if (rem) {
        infile.read(window + winpos, rem);
        amt = infile.gcount();
      }
      if (!infile.eof() && diff > rem) {
        rem = diff - rem;
        infile.read(window, rem);
        amt += infile.gcount();
      }
      hedge_size += amt;
    }
    condition = (hedge_size > 0);
    if (condition) {
      int deltaA = 0;
      size_t longestA = 0;
      
      // calculate hashes for new substrings in hedge
      size_t hash_adv_ofs = window_ofs + std::min(hedge_size, MAX_HEDGE + 1);
      while (prev_hash_ofs < hash_adv_ofs) {
        size_t hx = static_cast<size_t>(window[prev_hash_ofs % ARRAY_SIZE]) * PRIME_C;
        for (size_t j = 1; j <= MAX_HEDGE; ++j) {
          size_t hashofs = (HASH_BUFFER_SIZE + prev_hash_ofs - j + 1) % HASH_BUFFER_SIZE;
          hashes[hashofs][j] = (hashes[hashofs][j - 1] * PRIME_B) ^ hx;
        }
        ++prev_hash_ofs;
      }
      
      // add all substrings with last_decomp as last byte to pool
      while (last_decomp < decomp_size) {
        for (size_t j = 3; j <= std::min(last_decomp + 1, MAX_HEDGE); ++j) {
          size_t start_ofs = last_decomp - j + 1;
          size_t hash_idx = start_ofs % HASH_BUFFER_SIZE;
          size_t hash = hashes[hash_idx][j];
          std::pair<CLZHashTable::Node*, size_t> prev = hash_tables[j - 3].addNode(window, ARRAY_SIZE, start_ofs, hash);
          clzqueue[j - 3].emplace(prev.first, start_ofs);
        }
        ++last_decomp;
      }
      
      if (!skip_substr) {
        longestA = 0;
        size_t maxsub = std::min(std::min(1 + decomp_size, MAX_HEDGE), hedge_size);
        for (size_t j = maxsub; !longestA && j >= 3; --j) {
          size_t hash = hashes[window_ofs % HASH_BUFFER_SIZE][j];
          size_t prev = hash_tables[j - 3].getLast(window, ARRAY_SIZE, window_ofs, hash);
          if (prev != window_ofs) {
            longestA = j;
            deltaA = window_ofs - prev;
          }
        }
      } else {
        deltaA = deltaB;
        longestA = longestB;
        skip_substr = false;
      }
      
      for (size_t j = 3; j <= std::min(last_decomp + 1, MAX_HEDGE); ++j) {
        size_t start_ofs = last_decomp - j + 1;
        size_t hash_idx = start_ofs % HASH_BUFFER_SIZE;
        size_t hash = hashes[hash_idx][j];
        std::pair<CLZHashTable::Node*, size_t> prev = hash_tables[j - 3].addNode(window, ARRAY_SIZE, start_ofs, hash);
        clzqueue[j - 3].emplace(prev.first, start_ofs);
      }
      ++last_decomp;
      
      longestB = 0;
      deltaB = 0;
      {
        size_t maxsub = std::min(2 + decomp_size, MAX_HEDGE);
        for (size_t j = maxsub; !longestB && j >= 3; --j) {
          size_t hash = hashes[(window_ofs + 1) % HASH_BUFFER_SIZE][j];
          size_t prev = hash_tables[j - 3].getLast(window, ARRAY_SIZE, window_ofs + 1, hash);
          if (prev != window_ofs + 1) {
            longestB = j;
            deltaB = window_ofs + 1 - prev;
          }
        }
      }
      
      if ((longestA + 3 - 2) < (longestB + 3 - 3) || longestA < 3)
        longestA = 1;
      
      // drop dictionary entries once they dip below the window
      for (size_t q = 0; q < MAX_HEDGE - 2; ++q) {
        while (!clzqueue[q].empty() && (decomp_size + longestA) - clzqueue[q].front().second > WINDOW_SIZE) {
          hash_tables[q].removeNode(clzqueue[q].front());
          clzqueue[q].pop();
        }
      }
      
      if (longestA >= 3) {
        buffer[buffer_type_ofs] |= 1 << num_entries++;
        deltaA = -deltaA;
        
        buffer[buffer_ofs] = deltaA & 0xff;
        buffer[buffer_ofs + 1] = (deltaA >> 4 & 0xf0) | ((longestA - 3) & 0x0f);
        buffer_ofs += 2;

        window_ofs += longestA;
        hedge_size -= longestA;
        decomp_size += longestA;
      } else {
        buffer[buffer_ofs++] = window[window_ofs++ % ARRAY_SIZE];
        ++num_entries;
        
        --hedge_size;
        ++decomp_size;
        skip_substr = true;
      }
      if (num_entries >= 8) {
        num_entries = 0;
        
        if (buffer_ofs >= BUFFER_MAX_OUT) {
          outfile.write(buffer, buffer_ofs);
          buffer_ofs = 0;
        }
        
        buffer_type_ofs = buffer_ofs++;
        buffer[buffer_type_ofs] = 0;
      }
    }
  }
  
  if (buffer_ofs > 1) {
    if (num_entries == 0)
      outfile.write(buffer, buffer_ofs - 1);
    else
      outfile.write(buffer, buffer_ofs);
  }
  
  buffer[0] = window_ofs >> 24;
  buffer[1] = window_ofs >> 16;
  buffer[2] = window_ofs >> 8;
  buffer[3] = window_ofs;
  buffer[7] = buffer[6] = buffer[5] = buffer[4] = 0;
  
  outfile.seekp(4, std::ios::beg);
  outfile.write(buffer, 8);
  outfile.write(buffer, 4);
}


void CLZ::pack2(std::ifstream& infile, std::ofstream& outfile) {
  infile.seekg(0, std::ios::end);
  size_t decomp_size = infile.tellg();
  
  char* array = new char[std::max(16u, decomp_size)];
  
  array[0] = 'C';
  array[1] = 'L';
  array[2] = 'Z';
  array[3] = 0;
  
  array[12] = array[4] = decomp_size >> 24;
  array[13] = array[5] = decomp_size >> 16;
  array[14] = array[6] = decomp_size >> 8;
  array[15] = array[7] = decomp_size;
  
  array[11] = array[10] = array[9] = array[8] = 0;
  
  outfile.write(array, 16);
  
  infile.seekg(0, std::ios::beg);
  
  infile.read(array, decomp_size);
  
  const size_t MAX_SUB = 18;
  const size_t WINDOW_SIZE = 4096;
  
  // hashing constants
  const size_t PRIME_A = 37,
               PRIME_B = 54059,
               PRIME_C = 76963;
  
  CLZHashTable hash_tables[MAX_SUB - 2];
  for (size_t i = 0; i < MAX_SUB - 2; ++i)
    hash_tables[i].setStrLen(i + 3);

  // indexed by substring start offset % HASH_BUFFER_SIZE
  const int HASH_BUFFER_SIZE = MAX_SUB * 2;
  size_t hashes[HASH_BUFFER_SIZE][MAX_SUB + 1];
  
  for (size_t i = 0; i < HASH_BUFFER_SIZE; ++i) {
    hashes[i][0] = PRIME_A;
  }
  
  std::queue<std::pair<CLZHashTable::Node*, size_t>> clzqueue[MAX_SUB - 1];
  
  int* comp_bits = new int[decomp_size + 1];
  int* dlpair = new int[decomp_size + 1];
  comp_bits[0] = 0;
  for (size_t i = 1; i <= decomp_size; ++i)
    comp_bits[i] = INT_MAX;
  
  // calculate initial hashes
  for (size_t i = 0; i < std::min(MAX_SUB - 1, decomp_size); ++i) {
    size_t hx = static_cast<size_t>(array[i]) * PRIME_C;
    for (size_t j = 1; j <= std::min(decomp_size, MAX_SUB); ++j) {
      size_t hashofs = (HASH_BUFFER_SIZE + i - j + 1) % HASH_BUFFER_SIZE;
      hashes[hashofs][j] = (hashes[hashofs][j - 1] * PRIME_B) ^ hx;
    }
  }
  
  // get minimum cost compression
  for (size_t i = 0; i < decomp_size; ++i) {
    
    // update hashes
    if (i + MAX_SUB - 1 < decomp_size) {
      size_t ofs = i + MAX_SUB - 1;
      size_t hx = static_cast<size_t>(array[ofs]) * PRIME_C;
      for (size_t j = 1; j <= std::min(decomp_size, MAX_SUB); ++j) {
        size_t hashofs = (HASH_BUFFER_SIZE + ofs - j + 1) % HASH_BUFFER_SIZE;
        hashes[hashofs][j] = (hashes[hashofs][j - 1] * PRIME_B) ^ hx;
      }
    }
    
    // add strings to pool
    for (size_t j = 3; j <= std::min(i, MAX_SUB); ++j) {
      size_t start_ofs = i - j;
      size_t hash_idx = start_ofs % HASH_BUFFER_SIZE;
      size_t hash = hashes[hash_idx][j];
      std::pair<CLZHashTable::Node*, size_t> prev = hash_tables[j - 3].addNode(array, decomp_size, start_ofs, hash);
      clzqueue[j - 3].emplace(prev.first, start_ofs);
    }

    int current_dlpairs[MAX_SUB];
    size_t longest = 0;
    size_t maxsub = std::min(std::min(MAX_SUB, decomp_size - i), i);
    for (size_t j = maxsub; !longest && j >= 3; --j) {
      size_t hash = hashes[i % HASH_BUFFER_SIZE][j];
      size_t prev = hash_tables[j - 3].getLast(array, decomp_size, i, hash);
      if (prev != i) {
        for (size_t t = std::max(longest, 2u); t < j; ++t) {
          current_dlpairs[t] = i - prev;
        }
        longest = j;
      }
    }
    // remove old dictionary entries
    for (size_t q = 0; q < MAX_SUB - 2; ++q) {
      while (!clzqueue[q].empty() && (i + 1 - clzqueue[q].front().second) > WINDOW_SIZE) {
        hash_tables[q].removeNode(clzqueue[q].front());
        clzqueue[q].pop();
      }
    }

    if (comp_bits[i + 1] > comp_bits[i] + 9) {
      comp_bits[i + 1] = comp_bits[i] + 9;
      dlpair[i + 1] = 1;
    }
    
    for (size_t t = 3; t <= longest; ++t) {
      if (comp_bits[i + t] > comp_bits[i] + 17) {
        comp_bits[i + t] = comp_bits[i] + 17;
        dlpair[i + t] = -current_dlpairs[t - 1] << 16 | t;
      }
    }
  }

  // align with start of each block
  int ofs = decomp_size;
  int del = dlpair[ofs];
  while (ofs > 0) {
    ofs -= del & 0xffff;
    int temp = dlpair[ofs];
    dlpair[ofs] = del;
    del = temp;
  }
  
  size_t comp_size = (comp_bits[decomp_size] + 7) / 8;
  char* buffer = new char[comp_size];
  
  size_t buffer_ofs = 0;
  size_t buffer_type_ofs = 0;
  size_t num_entries = 0;
  if (comp_size) {
    buffer[buffer_ofs++] = 0;
  }
  
  size_t aofs = 0;
  while (aofs < decomp_size) {
    del = dlpair[aofs];
    
    size_t len = del & 0xffff;
    if (len < 3) {
      buffer[buffer_ofs++] = array[aofs];
      ++num_entries;
      ++aofs;
    } else {
      buffer[buffer_type_ofs] |= 1 << num_entries;
      buffer[buffer_ofs] = del >> 16;
      buffer[buffer_ofs + 1] = (del >> 20 & 0xf0) | ((len - 3) & 0x0f);
      buffer_ofs += 2;
      aofs += len;
      ++num_entries;
    }
    if (num_entries == 8) {
      num_entries = 0;
      if (buffer_ofs < comp_size - 1) {
        buffer_type_ofs = buffer_ofs++;
        buffer[buffer_type_ofs] = 0;
      }
    }
  }
  outfile.write(buffer, buffer_ofs);
  
  delete[] buffer;
  delete[] array;
  delete[] comp_bits;
  delete[] dlpair;
}
  
void CLZ::unpack(std::ifstream& infile, std::ofstream& outfile) {
  const int WINDOW_SIZE = 4096;
  
  char window[WINDOW_SIZE];
  
  int window_ofs = 0;
  
  infile.seekg(16, std::ios::beg);
  int bits = 0, bit_count = 0;
  
  char c;
  
  const int BUFFER_SIZE = 4096;
  char buffer_in[BUFFER_SIZE];
  int buffer_size = 0;
  int buffer_ofs = 0;
  
  bool exit = false;
  
  while (!exit) {
    if (buffer_ofs >= buffer_size) {
      if (!infile.eof()) {
        infile.read(buffer_in, BUFFER_SIZE);
        buffer_size = infile.gcount();
        buffer_ofs = 0;
        if (buffer_ofs < buffer_size)
          c = buffer_in[buffer_ofs++];
        else
          exit = true;
      } else
        exit = true;
    } else {
      c = buffer_in[buffer_ofs++];
    }
    if (!exit) {
      if (bit_count == 0) {
        bits = static_cast<int>(c);
        bit_count = 8;
      } else {
        if (bits & 1) {
          int window_delta = static_cast<int>(c) & 0xff;
          
          if (buffer_ofs >= buffer_size) {
            if (!infile.eof()) {
              infile.read(buffer_in, BUFFER_SIZE);
              buffer_size = infile.gcount();
              buffer_ofs = 0;
              if (buffer_ofs < buffer_size)
                c = buffer_in[buffer_ofs++];
              else
                exit = true;
            } else
              exit = true;
          } else {
            c = buffer_in[buffer_ofs++];
          }
          
          int length = static_cast<int>(c) & 0xff;
          window_delta |= length << 4 & 0xf00;
          length = (length & 0x0f) + 3;
          
          if (window_ofs + length >= WINDOW_SIZE) {
            for (int i = window_ofs; i < WINDOW_SIZE; ++i) {
              window[i] = window[(window_delta + i) % WINDOW_SIZE];
            }
            outfile.write(window, WINDOW_SIZE);
            window_ofs = window_ofs + length - WINDOW_SIZE;
            for (int i = 0; i < window_ofs; ++i) {
              window[i] = window[(i + window_delta) % WINDOW_SIZE];
            }
          } else {
            for (int i = 0; i < length; ++i) {
              window[window_ofs + i] = window[(window_ofs + i + window_delta) % WINDOW_SIZE];
            }
            window_ofs += length;
          }
        } else {
          window[window_ofs++] = c;
          if (window_ofs == WINDOW_SIZE) {
            outfile.write(window, WINDOW_SIZE);
            window_ofs = 0;
          }
        }
        bits >>= 1;
        --bit_count;
      }
    }
  }
  if (window_ofs)
    outfile.write(window, window_ofs);
}