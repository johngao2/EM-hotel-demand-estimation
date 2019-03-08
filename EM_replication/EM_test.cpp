// Actually a logit exercise
// Works with example solver
#include <cppad/ipopt/solve.hpp>
#include <iostream>
#include <math.h>
#include "csv.h"

#define n_times 254	// number of 
#define n_options 8	// 
#define n_types 14 	// number of customer types

namespace
{
using CppAD::AD;

class FG_eval
{
  public:
	typedef CPPAD_TESTVECTOR(AD<double>) ADvector;
	void operator()(ADvector &fg, const ADvector &x)
	{

		assert(fg.size() == 1);
		assert(x.size() == 7);

		// Fortran style indexing
		AD<double> x1 = x[0];
		AD<double> x2 = x[1];
		AD<double> x3 = x[2];
		AD<double> x4 = x[3];
		AD<double> x5 = x[4];
		AD<double> x6 = x[5];
		AD<double> x7 = x[6];

		return;
	}
};
} // namespace

// helper function for printing matrices (debugging)
template <typename T>
void printMatrix(T mat, std::size_t N, std::size_t M, int width)
{
	std::cout << "\n Printing Matrix : \n";
	for (int i = 0; i < N; ++i)
	{
		for (int j = 0; j < M; ++j)
			std::cout << std::setw(width) << *(*(mat + i) + j) << std::setw(width);
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

AD<int>* import_availability(AD<int> avail_matrix[n_times][n_options]){
	// import availability matrix
	// avail_matrix: n_times x n_options matrix
	io::CSVReader<9> in("data/hotel_5/AvailabilityH5.csv");
	in.read_header(io::ignore_no_column, "T", "prod_1", "prod_2", "prod_3",
				   "prod_4", "prod_5", "prod_6", "prod_7", "prod_8");
	int t;
	int prod[n_options];
	int row_counter = 0;
	AD<int> avail_matrix[n_times][n_options];
	while (in.read_row(t, prod[0], prod[1], prod[2], prod[3], prod[4],
					   prod[5], prod[6], prod[7]))
	{
		for (int i = 0; i < n_options+1; i++){
			avail_matrix[row_counter][i] = prod[i];
		}
		row_counter++;
	}
	printMatrix(avail_matrix, n_times, n_options, 3);

	return avail_matrix;
}
int main()
{
	// import preference matrix
	// pref_matrix: n_types x (n_options + 1) matrix
	io::CSVReader<10> in("data/hotel_5/PrefListsBuyUpH5.csv");
	in.read_header(io::ignore_no_column, "cust_type", "rank_1", "rank_2", "rank_3",
				   "rank_4", "rank_5", "rank_6", "rank_7", "rank_8", "rank_9");
	int cust_type;
	int pref_rank[n_options + 1];
	AD<int> pref_matrix[n_types][n_options+1];
	int row_counter = 0;
	while (in.read_row(cust_type, pref_rank[0], pref_rank[1], pref_rank[2],
					   pref_rank[3], pref_rank[4], pref_rank[5], pref_rank[6],
					   pref_rank[7], pref_rank[8]))
	{
		for (int i = 0; i < n_options+1; i++){
			pref_matrix[row_counter][i] = pref_rank[i];
		}
		row_counter++;
	}
	printMatrix(pref_matrix, n_types, n_options+1, 3);

	AD<int>* avail_matrix = import_availability();
}
