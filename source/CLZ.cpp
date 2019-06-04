#include "CLZ.h"

#include <iostream>

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
  const int WINDOW_SIZE = 4096;
  const int ARRAY_SIZE = 16384; // 8192
  
  const int MAX_HEDGE = 18;
  
  outfile << "CLZ";
  for (int i = 0; i < 13; ++i)
    outfile.put(0);
  
  
  const int BUFFER_SIZE = 16384; // 4096
  const int BUFFER_MAX_OUT = BUFFER_SIZE - 18;
  
  char buffer[BUFFER_SIZE];
  buffer[0] = 0;
  int num_entries = 0;
  int buffer_ofs = 1;
  int buffer_type_ofs = 0;
  
  char window[ARRAY_SIZE];
  int window_ofs = 0;
  int decomp_size = 0;
  int hedge_size = 0;
  
  //char c;
  bool condition = true;
  while (condition) {
    if (hedge_size < MAX_HEDGE && !infile.eof()) {
      int diff = (ARRAY_SIZE - WINDOW_SIZE) - hedge_size;
      int winpos = (window_ofs + hedge_size) % ARRAY_SIZE;
      int rem = std::min(ARRAY_SIZE - winpos, diff);
      int amt = 0;
      if (rem) {
        infile.read(window + winpos, rem);
        amt = infile.gcount();
      }
      rem = diff - rem;
      if (!infile.eof() && (rem > 0)) {
        infile.read(window, rem);
        amt += infile.gcount();
      }
      hedge_size += amt;
    }
    condition = (hedge_size > 0);
    if (condition) {
      int delta = 0;
      int longest = 0;
      int ehedge = std::min(hedge_size, MAX_HEDGE);
      for (int i = 1; i < std::min(decomp_size, WINDOW_SIZE); ++i) {
        int l = 0;
        for (int j = 0; j < std::min(i, ehedge) && window[(window_ofs - i + j + ARRAY_SIZE) % ARRAY_SIZE] == window[(window_ofs + j) % ARRAY_SIZE]; ++j) {
          ++l;
        }
        if (l > longest) {
          longest = l;
          delta = i;
        }
      }
      if (longest >= 3) {
        buffer[buffer_type_ofs] |= 1 << num_entries++;
        delta = -delta;
        
        buffer[buffer_ofs] = delta & 0xff;
        buffer[buffer_ofs + 1] = (delta >> 4 & 0xf0) | ((longest - 3) & 0x0f);
        buffer_ofs += 2;

        window_ofs += longest;
        hedge_size -= longest;
        decomp_size += longest;
      } else {
        buffer[buffer_ofs++] = window[window_ofs++ % ARRAY_SIZE];
        ++num_entries;
        
        --hedge_size;
        ++decomp_size;
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
          }
        }
        bits >>= 1;
        --bit_count;
      }
    }
  }
  if (window_ofs)
    outfile.write(window, window_ofs);
  outfile.flush();
}