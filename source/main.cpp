#include "main.h"

int main(int argc, char** argv) {
  
  if (argc == 4) {
    std::string command(argv[1]);
    std::ifstream infile;
    std::ofstream outfile;
    infile.rdbuf()->pubsetbuf(0, 0);
    outfile.rdbuf()->pubsetbuf(0, 0);
    
    if ("pack" == command) {
      infile.open(argv[2], std::ios_base::in | std::ios_base::binary);
      if (infile) {
        outfile.open(argv[3], std::ios_base::out | std::ios_base::binary);
        if (outfile) {
          CLZ::pack(infile, outfile);
          outfile.close();
        } else {
          std::cout << "Could not open " << argv[3] << " for writing" << std::endl;
        }
        infile.close();
      } else {
        std::cout << "Could not open " << argv[2] << " for reading" << std::endl;
      }
    } else if ("pack2" == command) {
      infile.open(argv[2], std::ios_base::in | std::ios_base::binary);
      if (infile) {
        outfile.open(argv[3], std::ios_base::out | std::ios_base::binary);
        if (outfile) {
          CLZ::pack2(infile, outfile);
          outfile.close();
        } else {
          std::cout << "Could not open " << argv[3] << " for writing" << std::endl;
        }
        infile.close();
      } else {
        std::cout << "Could not open " << argv[2] << " for reading" << std::endl;
      }
    } else if ("unpack" == command) {
      infile.open(argv[2], std::ios_base::in | std::ios_base::binary);
      if (infile) {
        if (CLZ::verify(infile)) {
          infile.clear();
          infile.seekg(0, std::ios::beg);
          outfile.open(argv[3], std::ios_base::out | std::ios_base::binary);
          if (outfile) {
            CLZ::unpack(infile, outfile);
            outfile.close();
          } else {
            std::cout << "Could not open " << argv[3] << " for writing" << std::endl;
          }
        } else {
          std::cout << "CLZ file " << argv[2] << " is improperly formatted" << std::endl;
        }
        infile.close();
      } else {
        std::cout << "Could not open " << argv[2] << " for reading" << std::endl;
      }
    } else {
      std::cout << "Unrecognized command" << std::endl;
    }
  } else {
    std::cout << "Wrong number of parameters, program accepts the following commands:" << std::endl
              << "program.exe pack \"uncompressed input file\" \"compressed output file\"" << std::endl
              << "program.exe pack2 \"uncompressed input file\" \"compressed output file\"" << std::endl
              << "program.exe unpack \"compressed input file\" \"uncompressed output file\"" << std::endl << std::endl;
  }
}