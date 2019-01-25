/******************************************************************************
 * adapt_factors.cpp
 *
 * Extending a list of factors with a compact representation of a permutation of
 * factors and an array of offsets 
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
#include <queue>

#include <sdsl/coder.hpp>
#include <sdsl/enc_vector.hpp>

using namespace std;
using namespace sdsl;

#ifndef DENS
#define DENS 128
#endif

// Binary search. It returns the closest lesser/equal value to v in the array a
uint find_index(uint v, uint *a, uint size_f) {
  uint l = 0, u = size_f-1, m = 0;

  while(l <= u) {
    m = l + (u-l)/2;
    if(a[m] == v)
      break;
    else if(a[m] < v)
      l = m+1;
    else
      u = m-1;
  }
  if(a[m] > v)
    m--;
  return m;
}

// Exclusive prefix sum
uint prefix_sum(uint *a, uint n) {
  uint acc = 0, tmp;
  for(uint i = 0; i < n; i++) {
    tmp = a[i];
    a[i] = acc;
    acc += tmp;
  }
  return acc;
}

int main(int argc, char **argv) {
  // Check arguments.
  if(argc < 3) {
    cerr << "usage: " << argv[0] << " in_list_factors out_list_factors" << endl;
    exit(EXIT_FAILURE);
  }
  
  vector<pair<int, int> > F;
  pair<int, int> p;

  // Read the list of factors.
  uint z; // Number of factors
  FILE* fp = fopen(argv[1], "rb");
  fseek(fp,0,SEEK_END);
  z = ftell(fp)/sizeof(int)/2;
  fseek(fp,0,SEEK_SET);

  uint invalid = z; // Represent an invalid back reference

  uint *psum = new uint[z];  
  uint *lb = new uint[z];  // left bound
  uint *rb = new uint[z];  // right bound

  cerr << "Reading the list of " << z << " factors" << endl;
  for(uint i = 0; i < z; i++){
    size_t r = fread(&p, sizeof(pair<int,int>), 1, fp);
    F.push_back(p);

    if(p.second == 0) // Literal
      psum[i] = 1;
    else
      psum[i] = p.second;;
  }
  fclose(fp);

  // Prefix sum
  prefix_sum(psum, z);
  
  // Find the left bound
  cerr << "Finding the left bounds" << endl;
  for(uint i = 0; i < z; i++) {
    if(F[i].second == 0)
      lb[i] = invalid;
    else 
      lb[i] = find_index(F[i].first, psum, z);
  }
  
  // Find the right bound
  cerr << "Finding the right bounds" << endl;
  for(uint i = 0; i < z; i++) {
    if(F[i].second == 0)
      rb[i] = invalid;
    else {
      rb[i] = find_index(F[i].first+F[i].second-1, psum, z);
      if(rb[i] == i) { // Filter for self-defined factors! (This dependency is
	               // already solved) 
	rb[i]--;
      }
    }
  }

  for(uint i = 0; i < z; i++)
    psum[i] = 0;
  
  for(uint i = 0; i < z; i++) {
    if(lb[i] != invalid)
      for(uint j = lb[i]; j <= rb[i]; j++)
	psum[j]++;
  }

  uint m = prefix_sum(psum, z);
  
  cerr << "Computing the array of dependencies" << endl;
  uint *dep = new uint[m];
  for(uint i = 0; i < z; i++) {
    if(lb[i] != invalid)     
      for(uint j = lb[i]; j <= rb[i]; j++) {
	dep[psum[j]] = i;
	psum[j]++;
      }
  }

  cerr << "Computing the array of lengths" << endl;
  int *num = new int[z];
  for(uint i = 0; i < z; i++)
    if(lb[i] == invalid)
      num[i] = 0;
    else
      num[i] = rb[i] - lb[i] + 1;

  for(int i = z-1; i > 0; i--)
    psum[i] = psum[i-1];
  psum[0] = 0;

  delete[] rb;
  delete[] lb;
  
  queue <uint> Q; 
  uint *P = new uint[z];
  vector<uint> O; // offsets

  uint idx = 0, lim=0;
  uint h = 0;
  
  for(uint i = 0; i < z; i++)
    if(!num[i])
      Q.push(i);
  Q.push(invalid);
  h++; // each time that we insert an "invalid" mark, we add a new level
  
  cerr << "(Simulated) graph traversal" << endl;
  while(!Q.empty()) {
    uint e = Q.front();
    Q.pop();

    if(e != invalid) {
      P[idx++] = e;
      int ll = psum[e];
      int ul = (e == z-1)? m-1: psum[e+1]-1;

      for(int i=ll; i <= ul; i++) {
	if(num[dep[i]] <= 0) {
	  cout << "\tis zero before substraction(!)" << endl;
	  exit(1);
	}
	  
	num[dep[i]]--;
	if(num[dep[i]] == 0)
	  Q.push(dep[i]);
      }
    }
    else if(!Q.empty()) {
      Q.push(invalid);
      O.push_back(idx-1);
      h++;

      // Sorting the range of factors to be decompressed concurrently
      sort(P+lim, P+idx);
      lim = idx;      
    }
  }
  O.push_back(idx-1);
  // Sorting the range of factors to be decompressed concurrently
  sort(P+lim, P+idx);

  cerr << "z: " << z << endl;
  cerr << "height: " << h << endl;

  int_vector<> P_sdsl(z, 0, 32);
  int_vector<> O_sdsl(h, 0, 32);

  for(uint i = 0; i < z; i++)
    P_sdsl[i] = P[i];

  delete[] P;

  for(uint i = 0; i < h; i++)
    O_sdsl[i] = O[i];
  
  O.clear();

  enc_vector<coder::elias_delta, DENS> compact_P(P_sdsl);
  
  util::clear(P_sdsl);

  cerr << "Writing the output" << endl;

  std::ofstream new_out(argv[2]);
  new_out.write((char *)&z, sizeof(uint));
  new_out.write((char *)&h, sizeof(uint));
  for(uint i = 0; i < z; i++)
    new_out.write((char *)&F[i], sizeof(pair<int,int>));
  compact_P.serialize(new_out);
  O_sdsl.serialize(new_out);
  new_out.close();

  return EXIT_SUCCESS;
}
