/******************************************************************************
 * parallel_framework.cpp
 *
 * Parallel framework for the decompression of a LZ77 parsing
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

#include <sdsl/coder.hpp>
#include <sdsl/enc_vector.hpp>

#ifndef DENS
#define DENS 128
#endif

#ifdef NOPARALLEL
#define cilk_for for
#define cilk_spawn
#define cilk_sync
#define __cilkrts_get_nworkers() 1
#else
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/common.h>
#endif
#define threads  __cilkrts_get_nworkers()

using namespace std;
using namespace sdsl;


// Parallel prefix sum
uint par_prefix_sum(uint* A, uint size) {
  uint chk = size/threads;
  uint aux = A[size-1];
  uint *partial = new uint[threads];
  
  cilk_for(uint i = 0; i < threads; i++) {
    uint  ll = i*chk, ul = ll + chk;
    if(i == threads-1)
      ul = size;

    uint acc = 0, tmp = 0;
    for(uint j = ll; j < ul; j++) {
      tmp = A[j];
      A[j] = acc;
      acc += tmp;
    }

    partial[i] = acc;
  }

  uint tmp = 0, acc = 0;
  for(uint i = 0; i < threads; i++) {
    tmp = partial[i];
    partial[i] = acc;
    acc += tmp;
  }
  
  cilk_for(uint i = 1; i < threads; i++) {
    uint ll = i*chk, ul = ll + chk;
    if(i == threads-1)
      ul = size;

    uint acc = partial[i];
    for(uint j = ll; j < ul; j++)
      A[j] += acc;
  }

  return aux+A[size-1];
}

int main(int argc, char **argv) {
  // Check arguments.
  if(argc < 3) {
    cerr << "usage: " << argv[0] << " in_list_factors decompressed_txt" << endl;
    exit(EXIT_FAILURE);
  }
  
  // Read the list of factors
  uint z; // Number of factors
  uint h; // Number of factors

  std::ifstream new_in(argv[1]);

  new_in.read((char *)&z, sizeof(uint));
  new_in.read((char *)&h, sizeof(uint));
  
  vector<pair<int, int> > F(z);
  enc_vector<coder::elias_delta, DENS> P;
  int_vector<> O(h, 0, 32);
  uint *psum = new uint[z];
  
  for(uint i = 0; i < z; i++)
    new_in.read((char *)&F[i], sizeof(pair<int,int>));  

  P.load(new_in);
  O.load(new_in);
  new_in.close();
  
  cilk_for(uint i = 0; i < z; i++)
    if(!F[i].second)
      psum[i] = 1;
    else
      psum[i] = F[i].second;

  uint n = par_prefix_sum(psum, z);
  uint size_txt = n; // Length of the original text

  char *out_text = new char[size_txt];

  int ll = 0, ul = 0;

  // Only literal factors
  ul = O[0];
  cilk_for(uint j = ll; j <= ul; j++) {
    uint p = P[j];
    out_text[psum[p]] = char(F[p].first);
  }

  ll = ul+1;
  
  // Non-literal factors 
  int dens = P.get_sample_dens();

  for(uint i = 1; i < h; i++) {    
    ul = (int)O[i];

    int init_block = ceil((double)ll/dens);
    int end_block = ul/dens;

    uint8_t offset;
    const uint64_t* d;     

    int val = P.get_value(&d, &offset, ll);
    int len = F[val].second;
    int idx = psum[val];

    int lenlen = len;
    int ii=idx;
    
    for(uint kk=F[val].first; len > 0; kk++, idx++, len--)
      out_text[idx] = out_text[kk];      

    int ub = std::min(ul,init_block*dens);

    for(int l=ll+1; l<ub; l++) {
      val += P.get_diff(&d, &offset);
      
      len = F[val].second;
      idx = psum[val];
      
      for(uint kk=F[val].first; len > 0; kk++, idx++, len--)
	out_text[idx] = out_text[kk];      
    }

    // All the factors are contained in one block
    if(ub > ul) {
      ll = ul+1;
      continue;
    }

    if(end_block > init_block) {      
      cilk_for(uint j = init_block; j < end_block; j++) {
      	uint8_t offset;
      	const uint64_t* d;     
      
      	int val = P.get_value(&d, &offset, j*dens);
      	int len = F[val].second;
      	int idx = psum[val];
      
	for(uint kk=F[val].first; len > 0; kk++, idx++, len--)
	  out_text[idx] = out_text[kk];      
      
      	for(int l=1; l<dens; l++) {
      	  val += P.get_diff(&d, &offset);
	
      	  len = F[val].second;
      	  idx = psum[val];
	
	  for(uint kk=F[val].first; len > 0; kk++, idx++, len--)
	    out_text[idx] = out_text[kk];      
      	}
      }
    }
    val = P.get_value(&d, &offset, end_block*dens);
    len = F[val].second;
    idx = psum[val];

    for(uint kk=F[val].first; len > 0; kk++, idx++, len--)
      out_text[idx] = out_text[kk];      
      
    for(int l=end_block*dens+1; l<=ul; l++) {
      val += P.get_diff(&d, &offset);
	
      len = F[val].second;
      idx = psum[val];
	
      for(uint kk=F[val].first; len > 0; kk++, idx++, len--)
      	out_text[idx] = out_text[kk];      
    }
    
    ll = ul+1;
  }

  return EXIT_SUCCESS;
}
