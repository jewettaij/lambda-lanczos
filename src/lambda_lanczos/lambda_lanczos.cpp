#include <vector>
#include <memory>
#include <limits>
#include <iostream>
#include <random>
#include <functional>
#include <cassert>
#include "lambda_lanczos.hpp"
#include "lambda_lanczos_util.hpp"

namespace lambda_lanczos {

template<typename T>
using vector = std::vector<T>;

using std::fill;
using std::abs;
using std::begin;
using std::end;
using namespace lambda_lanczos_util;

LambdaLanczos::LambdaLanczos(std::function<void(const std::vector<double>&,
						std::vector<double>&)> matmul, int matsize) {
  this->matmul = matmul;
  this->matsize = matsize;
  this->max_iteration = matsize;
}

int LambdaLanczos::run(double& eigvalue, vector<double>& eigvec) {
  assert(0 < this->tridiag_eps_ratio && this->tridiag_eps_ratio < 1);
  
  vector<vector<double>> u; // Lanczos vectors
  vector<double> alpha; // Diagonal elements of an approximated tridiagonal matrix
  vector<double> beta;  // Subdiagonal elements of an approximated tridiagonal matrix

  const int n = this->matsize;

  u.reserve(this->initial_vector_size);
  alpha.reserve(this->initial_vector_size);
  beta.reserve(this->initial_vector_size);

  u.emplace_back(n, 0.0); // Same as u.push_back(vector<double>(n, 0.0))
  
  vector<double> vk(n, 0.0);
  
  double alphak = 0.0;
  alpha.push_back(alphak);
  double betak = 0.0;
  beta.push_back(betak);
  
  vector<double> uk(n);
  init_random(uk);
  u.push_back(uk);

  double ev, pev; // Calculated eigen value and previous one
  pev = std::numeric_limits<double>::max();

  int itern = this->max_iteration;
  for(int k = 1;k <= this->max_iteration;k++) {
    fill(vk.begin(), vk.end(), 0.0);
    this->matmul(u.back(), vk);
    alphak = std::inner_product(begin(u.back()), end(u.back()),
				begin(vk), 0.0);
    alpha.push_back(alphak);
    
    for(int i = 0;i < n; i++) {
      uk[i] = vk[i] - betak*u[k-1][i] - alphak*u[k][i];
    }
    
    schmidt_orth(uk, u);
    
    betak = norm(uk);
    beta.push_back(betak);

    ev = compute_eigenvalue(alpha, beta);

    const double zero_threshold = 1e-16;
    if(betak < zero_threshold) {
      u.push_back(uk);
      /* This element will never be accessed,
	 but this "push" guarantees u to always have one more element than 
	 alpha and beta do.*/
      itern = k;
      break;
    }

    normalize(uk);
    u.push_back(uk);

    if(abs(ev-pev) < std::min(abs(ev), abs(pev))*this->eps) {
      itern = k;
      break;
    } else {
      pev = ev;
    }
  }

  eigvalue = ev;

  int m = alpha.size();
  vector<double> cv(m+1);
  cv[0] = 0.0;
  cv[m] = 0.0;
  cv[m-1] = 1.0;
  
  beta[m-1] = 0.0;

  if(eigvec.size() < n) {
    eigvec.resize(n);
  }
  
  for(int i = 0;i < n;i++) {
    eigvec[i] = cv[m-1]*u[m-1][i];
  }

  for(int k = m-2;k >= 1;k--) {
    cv[k] = ((ev - alpha[k+1])*cv[k+1] - beta[k+1]*cv[k+2])/beta[k];

    for(int i = 0;i < n;i++) {
      eigvec[i] += cv[k]*u[k][i];
    }
  }

  // Normalize the eigenvector
  double nrm2 = norm(eigvec);
  for(int i = 0;i < n;i++) {
    eigvec[i] = eigvec[i]/nrm2;
  }

  return itern;
}

void LambdaLanczos::schmidt_orth(vector<double>& uorth, const vector<vector<double>>& u) {
  /* Vectors in u must be normalized, but uorth doesn't have to be. */
  
  int n = uorth.size();
  
  for(int k = 0;k < u.size();k++) {
    double innprod = inner_product(begin(uorth), end(uorth),
				   begin(u[k]), 0.0);
    
    for(int i = 0;i < n;i++) {
      uorth[i] -= innprod * u[k][i];
    }
  }
}

double LambdaLanczos::compute_eigenvalue(const vector<double>& alpha,
			  const vector<double>& beta) {
  double r = tridiagonal_eigen_limit(alpha, beta);
  double eps = this->eps * this->tridiag_eps_ratio;

  double a,b,mid;
  double bmid = std::numeric_limits<double>::max();
  a = -r;
  b = r;

  int nmid;
  while(b-a > std::min(abs(a), abs(b))*eps) {
    mid = (a+b)/2.0;
    nmid = num_of_eigs_smaller_than(mid, alpha, beta);
    if(nmid >= 1){
      b = mid;
    }else{
      a = mid;
    }
    if(mid == bmid) {
      /* This avoids an infinite loop due to zero matrix */
      break;
    }
    bmid = mid;
  }

  return a;
}


/* Compute the eigenvalue limit by Gerschgorin theorem */
double LambdaLanczos::tridiagonal_eigen_limit(const vector<double>& alpha,
			       const vector<double>& beta) {
  double r2 = std::inner_product(begin(alpha), end(alpha), begin(alpha), 0.0);
  r2 += 2*std::inner_product(begin(beta), end(beta), begin(beta), 0.0);
  
  return r2;
}


/*
 Algorithm from
 Peter Arbenz et al., "High Performance Algorithms for Structured Matrix Problems"
 */
int LambdaLanczos::num_of_eigs_smaller_than(double c,
			     const vector<double>& alpha,
			     const vector<double>& beta) {
  double q_i = 1.0;
  int count = 0;
  int m = alpha.size();
  
  for(int i = 1;i < m;i++){
    q_i = alpha[i] - c - beta[i-1]*beta[i-1]/q_i;
    if(q_i < 0){
      count++;
    }
    if(q_i == 0){
      q_i = 1e-12;
    }
  }

  return count;
}

void mul_compressed_mat(double* ca, int* ci, int* cj,
			int ca_size,
			int n, double* uk, double* vk) {

  for(int i = 0;i < n;i++) {
    vk[i] = 0.0;
  }
  for(int index = 0;index < ca_size;index++) {
    vk[ci[index]] += ca[index]*uk[cj[index]];
    if(ci[index] != cj[index]) {
      // Multiply corresponding upper triangular element
      vk[cj[index]] += ca[index]*uk[ci[index]];
    }
  }
}

void LambdaLanczos::init_random(vector<double>& v) {
  std::random_device dev;
  std::mt19937 mt(dev());
  std::uniform_real_distribution<> rand(-1.0, 1.0);

  int n = v.size();
  for(int i = 0;i < n;i++) {
    v[i] = rand(mt);
  }

  normalize(v);
}

} /* namespace lambda_lanczos */
