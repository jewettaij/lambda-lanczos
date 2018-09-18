#ifndef LAMBDA_LANCZOS_H_
#define LAMBDA_LANCZOS_H_

#include <vector>
#include <functional>

namespace lambda_lanczos {

class LambdaLanczos {
public:
  LambdaLanczos(std::function<void(const std::vector<double>&, std::vector<double>&)> matmul, int matsize);
  
  int matsize;
  int max_iteration;
  double eps = 1e-12;
  double tridiag_eps_ratio = 1e-1;
  size_t initial_vector_size = 200;

  std::function<void(const std::vector<double>&, std::vector<double>&)> matmul;
  std::function<void(std::vector<double>&)> init_vector = init_random;

  int run(double&, std::vector<double>&);

private:  
  static void schmidt_orth(std::vector<double>&, const std::vector<std::vector<double>>&);
  double compute_eigenvalue(const std::vector<double>&,
			    const std::vector<double>&);
  static double tridiagonal_eigen_limit(const std::vector<double>&,
				 const std::vector<double>&);
  static int num_of_eigs_smaller_than(double,
			     const std::vector<double>&,
			     const std::vector<double>&);

  static void init_random(std::vector<double>&);
};

void mul_compressed_mat(double*, int*, int*, int, int, double*, double*);
  
} /* namespace lambda_lanczos */

#endif /* LAMBDA_LANCZOS_H_ */
