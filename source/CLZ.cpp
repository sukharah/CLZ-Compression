#include "CLZ.h"

#include <iostream>

bool CLZ::verify(std::ifstream& infile) {
  int bits = 0, bit_count = 0;
  
  char header[16];
  infile.read(header, 16);
  bool result = true;
  int max_len = 0;
  int max_delta = 0;
  if (infile.gcount() == 16) {
    int signature = (static_cast<int>(header[0]) & 0xff) << 24 | (static_cast<int>(header[1]) & 0xff) << 16 | (static_cast<int>(header[2]) & 0xff) << 8 | (static_cast<int>(header[3]) & 0xff);
    int expected_size = (static_cast<int>(header[4]) & 0xff) << 24 | (static_cast<int>(header[5]) & 0xff) << 16 | (static_cast<int>(header[6]) & 0xff) << 8 | (static_cast<int>(header[7]) & 0xff);
    int decomp_size = 0;
    if (signature == 0x434c5a00) {
      char c;
      while (infile.get(c) && result) {
        if (bit_count == 0) {
          bits = static_cast<int>(c);
          bit_count = 8;
        } else {
          if (bits & 1) { // distance length code
            int window_delta = static_cast<int>(c) & 0xff;
            if (!infile.get(c))
              result = false;
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
      if (decomp_size != expected_size)
        result = false;
    } else
      result = false;
  } else
    result = false;
  return result;
}
  
void CLZ::pack(std::ifstream& infile, std::ofstream& outfile) {
  const int WINDOW_SIZE = 4096;
  const int ARRAY_SIZE = 8192;
  
  const int MAX_HEDGE = 18;
  
  outfile << "CLZ";
  for (int i = 0; i < 13; ++i)
    outfile.put(0);
  
  char buffer[18];
  buffer[0] = 0;
  int num_entries = 0;
  int buffer_ofs = 1;
  
  char window[ARRAY_SIZE];
  int window_ofs = 0;
  int decomp_size = 0;
  int hedge_size = 0;
  
  int comp_size = 0;
  
  char c;
  bool exit = false;
  while (!exit) {
    bool condition;
    if (infile.get(c)) {
      window[(window_ofs + hedge_size++) % ARRAY_SIZE] = c;
      ++decomp_size;
      condition = (hedge_size == MAX_HEDGE);
    } else {
      condition = (hedge_size > 0);
      exit = !condition;
    }
    if (condition) {
      int delta = 0;
      int longest = 0;
      for (int i = 1; i < std::min(decomp_size, WINDOW_SIZE) - hedge_size; ++i) {
        int l = 0;
        for (int j = 0; j < std::min(i, hedge_size) && window[(window_ofs - i + j + ARRAY_SIZE) % ARRAY_SIZE] == window[(window_ofs + j) % ARRAY_SIZE]; ++j) {
          ++l;
        }
        if (l > longest) {
          longest = l;
          delta = i;
        }
      }
      if (longest >= 3) {
        buffer[0] |= 1 << num_entries++;
        delta = -delta;
        
        buffer[buffer_ofs] = delta;
        buffer[buffer_ofs + 1] = (delta >> 4 & 0xf0) | ((longest - 3) & 0x0f);
        buffer_ofs += 2;
        
        comp_size += 2;
        window_ofs += longest;
        hedge_size -= longest;
      } else {
        buffer[buffer_ofs++] = window[window_ofs++ % ARRAY_SIZE];
        ++num_entries;
        
        --hedge_size;
        ++comp_size;
      }
      if (num_entries >= 8) {
        outfile.write(buffer, buffer_ofs);
        buffer_ofs = 1;
        num_entries = 0;
        buffer[0] = 0;
      }
      
    }
  }
  
  if (num_entries) {
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
  
void CLZ::unpack(std::ifstream& infile, std::ofstream& outfile) {
  const int WINDOW_SIZE = 4096;
  
  char window[WINDOW_SIZE];
  
  int window_ofs = 0;
  
  infile.seekg(16, std::ios::beg);
  int bits = 0, bit_count = 0;
  
  char c;
  
  while (infile.get(c)) {
    if (bit_count == 0) {
      bits = static_cast<int>(c);
      bit_count = 8;
    } else {
      if (bits & 1) {
        int window_delta = static_cast<int>(c) & 0xff;
        infile.get(c);
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
        }
      }
      bits >>= 1;
      --bit_count;
    }
  }
  if (window_ofs)
    outfile.write(window, window_ofs);
  outfile.flush();
}