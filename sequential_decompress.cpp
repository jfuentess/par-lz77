/******************************************************************************
 * sequential_decompress.cpp
 *
 * Sequential decompression of a LZ77 parsing
 *
 ******************************************************************************
 * Copyright (C) 2019 Diego Arroyuelo, José Fuentes <jfuentess@dcc.uchile.cl>,
 * Simon J. Puglisi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *****************************************************************************/

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <vector>

using namespace std;

int main(int argc, char **argv) {
  // Check arguments.
  if(argc != 3) {
    cerr << "usage: " << argv[0] << " in_list_factors decompressed_txt" << endl;
    exit(EXIT_FAILURE);
  }

  vector<pair<int, int> > F;
  pair<int, int> p;
  vector<int> limits; // Starting position of each factor in the original text

  // Read the list of factors.
  uint z; // Number of factors
  FILE* fp = fopen(argv[1], "rb");
  fseek(fp,0,SEEK_END);
  z = ftell(fp)/sizeof(int)/2;
  fseek(fp,0,SEEK_SET);

  uint size_txt = 0;
  size_t r = 0;
  for(uint i = 0; i < z; i++){
    r = fread(&p, sizeof(pair<int,int>), 1, fp);
    F.push_back(p);

    if(!p.second) size_txt++;
    else size_txt += p.second;
  }
  fclose(fp);

  char *out_txt = new char[size_txt];
  uint idx = 0;
  // Decompress the factors
  for(uint i = 0; i < z; i++)
    if(F[i].second == 0) { // Literal
      out_txt[idx] = char(F[i].first);
      idx++;
    }
    else {
      uint l = F[i].second;
      for(uint j=F[i].first; l > 0; j++, idx++, l--)
	out_txt[idx] = out_txt[j];
    }
    
  return EXIT_SUCCESS;
}
