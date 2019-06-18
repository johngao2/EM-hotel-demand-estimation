#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <math.h>
#include <boost/tokenizer.hpp>
#include <cppad/ipopt/solve.hpp>

// sprint 3 dimensions
#define n_times 24219
#define n_options 4900
#define n_types 4900

// // sprint 1 dimensions
// #define n_times 3558100
// #define n_options 7
// #define n_types 8

// GLOBAL VARS
double m_vec[n_types];			   // m_vector, counts number of occurences of a type n arrival
double current_x_vec[n_types + 1]; // current solution vector
double x_diff_vec[n_types];		   // tracks changes in solution
double a_vec[n_times];			   // a_vector, tracks if there was an arrival in a period
double lambda;					   // arrival parameter
double alpha = 0.1;				   // regularization hyperparameter
int n_purch;					   // tracks number total number of purchases

namespace
{
using CppAD::AD;

// helper function to check if a file exists
bool is_file_exist(const char *fileName)
{
	std::ifstream infile(fileName);
	return infile.good();
}

// helper function for printing matrices (debugging)
template <typename T>
void printMatrix(const char *text, T mat, std::size_t N, std::size_t M, int width)
{
	std::cout << text << std::endl;

	for (int i = 0; i < N; i++)
	{
		for (int j = 0; j < M; j++)
			std::cout << std::setw(width) << std::fixed << std::setprecision(2)
					  << mat[i][j] << std::setw(width);
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

// helper function for printing vectors (debugging)
template <typename T>
void printVector(const char *text, T mat, std::size_t N, int width, int precision = 2)
{
	std::cout << text << std::endl;

	for (int i = 0; i < N; i++)
	{
		std::cout << std::setw(width) << std::fixed << std::setprecision(precision)
				  << mat[i] << std::setw(width);
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

// import preference matrix (sigma_matrix)
// this is equivalent to the sigma table where sigma_i(j_t)
// = rank of j_t in customer type i
// each row is a cust type, each col is a product, each value is a rank
// first column represents non-purchase option
// 0 is most preferred, n_options+1 is least preferred
// returns sigma_matrix: a n_types x (n_options + 1) matrix
int **import_prefs(const char *pref_filename, int verbose = 0)
{
	// importing csv
	using namespace boost;
	std::string data(pref_filename);
	std::ifstream in(data.c_str());

	// error check
	if (!in.is_open())
	{
		std::cout << "ERROR: CSV NOT FOUND" << std::endl;
		std::exit(1);
	}

	// declare parsing vars
	typedef tokenizer<escaped_list_separator<char>> Tokenizer;
	std::vector<std::string> vec;
	std::string line;
	int header_tf = 1; // helper vars to ignore header and index values
	int index_tf = 1;
	int col_counter = 0; // col counter for to select which col to insert value
	int row_counter = 0;

	// vars to store data
	int **pref_matrix = 0; // initialize
	pref_matrix = new int *[n_types];

	if (verbose == 1)
	{
		std::cout << "Loading preferences" << std::endl;
	}

	// read line by line
	while (getline(in, line))
	{
		// declare stuff
		index_tf = 1;
		col_counter = 0;
		Tokenizer tok(line);
		pref_matrix[row_counter] = new int[n_options + 1]; // new row

		// ignore first line
		if (header_tf == 1)
		{
			header_tf = 0;
			continue;
		}

		// iterate over row tokens
		for (Tokenizer::iterator it(tok.begin()),
			 end(tok.end());
			 it != end; ++it)
		{
			// skip first value since it's index
			if (index_tf == 1)
			{
				index_tf = 0;
				continue;
			}
			pref_matrix[row_counter][col_counter] = std::stoi(*it);
			col_counter++;
		}
		row_counter++;

		if (verbose == 1)
		{
			// printing progress
			if (row_counter % 1000 == 0)
			{
				std::cout << row_counter << std::endl;
			}
		}
	}

	// convert to sigma matrix
	int **sigma_matrix = 0;
	sigma_matrix = new int *[n_types];
	bool viable; // tracks if an option is viable (i.e. ranked lower than nonpurchase)
	// constructs sigma matrix for viable options
	for (int i = 0; i < n_types; i++)
	{
		sigma_matrix[i] = new int[n_options + 1];
		viable = 1;
		for (int rank = 0; rank < n_options + 1; rank++)
		{
			int k = pref_matrix[i][rank];
			if (viable == 1)
			{
				if (k == 0)
				{
					viable = 0;
					sigma_matrix[i][k] = rank + 1;
				}
				else
				{
					sigma_matrix[i][k] = rank + 1;
				}
			}
		}
	}

	// zero index rankings, and fill nonviable options with rank (n_options+1)
	for (int i = 0; i < n_types; i++)
	{
		for (int k = 0; k < n_options + 1; k++)
		{
			if (sigma_matrix[i][k] == 0)
			{
				sigma_matrix[i][k] = n_options + 1;
			}
			else
			{
				sigma_matrix[i][k]--;
			}
		}
	}
	if (verbose == 1)
	{
		std::cout << "Done preferences" << std::endl;
	}
	return sigma_matrix;
}

// import availability matrix
// returns avail_matrix: a n_times x n_options matrix
int **import_availability(const char *avail_filename, int verbose = 0)
{
	if (verbose == 1)
	{
		std::cout << "Loading avail" << std::endl;
	}
	// importing csv
	using namespace boost;
	std::string data(avail_filename);
	std::ifstream in(data.c_str());

	// error check
	if (!in.is_open())
	{
		std::cout << "ERROR: CSV NOT FOUND" << std::endl;
		std::exit(1);
	}

	// declare parsing vars
	typedef tokenizer<escaped_list_separator<char>> Tokenizer;
	std::vector<std::string> vec;
	std::string line;
	int header_tf = 1; // helper vars to ignore header and index values
	int index_tf = 1;
	int col_counter = 0; // col counter for to select which col to insert value
	int row_counter = 0;

	// vars to store data
	int **avail_matrix = 0; // initialize
	avail_matrix = new int *[n_times];

	// read line by line
	while (getline(in, line))
	{
		// declare stuff
		index_tf = 1;
		col_counter = 0;
		Tokenizer tok(line);
		avail_matrix[row_counter] = new int[n_options]; // new row

		// ignore first line
		if (header_tf == 1)
		{
			header_tf = 0;
			continue;
		}

		// iterate over row tokens
		for (Tokenizer::iterator it(tok.begin()),
			 end(tok.end());
			 it != end; ++it)
		{
			// skip first value since it's index
			if (index_tf == 1)
			{
				index_tf = 0;
				continue;
			}
			avail_matrix[row_counter][col_counter] = std::stoi(*it);
			col_counter++;
		}
		row_counter++;

		if (verbose == 1)
		{
			// printing progress
			if (row_counter % 1000 == 0)
			{
				std::cout << row_counter << std::endl;
			}
		}
	}
	if (verbose == 1)
	{
		std::cout << "Done avail" << std::endl;
	}
	return avail_matrix;
}

// import transaction vector
// returns trans_vector: a length T vector
int *import_transactions(const char *trans_filename, int verbose = 0)
{
	if (verbose == 1)
	{
		std::cout << "Loading trans" << std::endl;
	}
	// importing csv
	using namespace boost;
	std::string data(trans_filename);
	std::ifstream in(data.c_str());

	// error check
	if (!in.is_open())
	{
		std::cout << "ERROR: CSV NOT FOUND" << std::endl;
		std::exit(1);
	}

	// declare parsing vars
	typedef tokenizer<escaped_list_separator<char>> Tokenizer;
	std::vector<std::string> vec;
	std::string line;
	int header_tf = 1; // helper vars to ignore header and index values
	int index_tf = 1;
	int row_counter = 0;

	// vars to store data
	int *trans_vec = new int[n_times];

	// read line by line
	while (getline(in, line))
	{
		// declare stuff
		index_tf = 1;
		Tokenizer tok(line);

		// ignore first line
		if (header_tf == 1)
		{
			header_tf = 0;
			continue;
		}

		// iterate over row tokens
		for (Tokenizer::iterator it(tok.begin()),
			 end(tok.end());
			 it != end; ++it)
		{
			// skip first value since it's index
			if (index_tf == 1)
			{
				index_tf = 0;
				continue;
			}
			trans_vec[row_counter] = std::stoi(*it);
		}
		row_counter++;

		// printing progress
		if (verbose == 1)
		{
			if (row_counter % 1000 == 0)
			{
				std::cout << row_counter << std::endl;
			}
		}
	}
	if (verbose == 1)
	{
		std::cout << "Done trans" << std::endl;
	}
	return trans_vec;
}

// build compatible type (mu_t) sets
// returns mu_matrix: a n_times x n_types matrix
// each col is a type, each row is a time period
// if value is 1, then that type is compatible in that time period
int **build_mu_mat(int **sigma_matrix, int **avail_matrix, int *trans_vec, int verbose = 0)
{
	// initialize blank mu matrix
	int **mu_matrix = 0; // initialize
	mu_matrix = new int *[n_times];

	// build mu matrix if it doesn't exist
	if (!is_file_exist("mu_matrix.csv"))
	{
		std::cout << "MU matrix not found, building it now" << std::endl;
		// build mu matrix
		int *ranking;
		bool compatible; // boolean var to keep track of if a type is compatible
		for (int t = 0; t < n_times; t++)
		{
			mu_matrix[t] = new int[n_types];
			int j_t = trans_vec[t];			  // subtracted for zero indexing
			for (int i = 0; i < n_types; i++) // iterate over each cust type
			{
				compatible = 1;
				ranking = sigma_matrix[i];
				for (int k = 1; k < n_options + 1; k++) // iterate over each option
				{
					bool available = avail_matrix[t][k - 1]; // need to subtract 1 from k due to 1-indexed sigma matrix
					if (available == 1 && k != j_t)			 // if item is available and not selected
					{
						if (sigma_matrix[i][k] < sigma_matrix[i][j_t])
						// if any item is ranked better than chosen item
						{
							compatible = 0;
						}
					}
				}
				// check if nonpurchase is preferred to chosen item
				if (sigma_matrix[i][0] < sigma_matrix[i][j_t])
				{
					compatible = 0;
				}
				mu_matrix[t][i] = compatible;
			}
			if (verbose == 1)
			{
				// printing progress
				if (t % 1 == 0)
				{
					std::cout << t << std::endl;
				}
			}
		}

		// saving mu matrix
		std::ofstream output;
		output.open("mu_matrix.csv");
		// output header
		output << "idx";
		for (int i = 0; i < n_types; i++)
		{
			output << ',' << std::to_string(i + 1);
		}
		output << '\n';
		// output body
		for (int t = 0; t < n_times; t++)
		{
			output << t;
			for (int i = 0; i < n_types; i++)
			{
				output << ',' << mu_matrix[t][i];
			}
			output << '\n';
		}
	}
	else
	{
		// import mu matrix if it exists
		std::cout << "MU matrix found, importing" << std::endl;

		// importing csv
		using namespace boost;
		std::string data("mu_matrix.csv");
		std::ifstream in(data.c_str());

		// declare parsing vars
		typedef tokenizer<escaped_list_separator<char>> Tokenizer;
		std::vector<std::string> vec;
		std::string line;
		int header_tf = 1; // helper vars to ignore header and index values
		int index_tf = 1;
		int col_counter = 0; // col counter for to select which col to insert value
		int row_counter = 0;

		// read line by line
		while (getline(in, line))
		{
			// declare stuff
			index_tf = 1;
			col_counter = 0;
			Tokenizer tok(line);
			mu_matrix[row_counter] = new int[n_types];

			// ignore first line
			if (header_tf == 1)
			{
				header_tf = 0;
				continue;
			}

			// iterate over row tokens
			for (Tokenizer::iterator it(tok.begin()),
				 end(tok.end());
				 it != end; ++it)
			{
				// skip first value since it's index
				if (index_tf == 1)
				{
					index_tf = 0;
					continue;
				}
				mu_matrix[row_counter][col_counter] = std::stoi(*it);
				col_counter++;
			}
			row_counter++;
			if (verbose == 1)
			{
				// printing progress
				if (row_counter % 1000 == 0)
				{
					std::cout << row_counter << std::endl;
				}
			}
		}
		if (verbose == 1)
		{
			std::cout << "Done importing mu matrix" << std::endl;
		}
	}
	return mu_matrix;
}

// literally just counts number of purchases
void count_purchases(int *trans_vec)
{
	for (int t = 0; t < n_times; t++)
	{
		if (trans_vec[t] != 0)
		{
			n_purch++;
		}
	}
}

// build p_sigma matrix (calculates customer type probabilities
// based on the data and previous guess for xi)
// p_sigma is confusingly denoted as x_it in the pseudocode
// resurns p_sigma matrix: n_times x n_types matrix
double **build_cust_type_probs(int **mu_matrix, int verbose = 0)
{
	if (verbose == 1)
	{
		std::cout << "building sigma matrix" << std::endl;
	}

	double **p_sigma_matrix = 0;
	p_sigma_matrix = new double *[n_times];
	for (int t = 0; t < n_times; t++)
	{
		if (verbose == 1)
		{
			// printing progress
			if (t % 1000 == 0)
			{
				std::cout << t << std::endl;
			}
		}
		p_sigma_matrix[t] = new double[n_types];

		// first calculate sum probabilities in each time
		// this is the denominator sum(x_h) for all compatible types h
		double sumprob = 0;

		for (int i = 0; i < n_types; i++)
		{
			sumprob += mu_matrix[t][i] * current_x_vec[i];
		}

		// update type probability if that type is compatible
		for (int i = 0; i < n_types; i++)
		{
			if (mu_matrix[t][i] == 1) // set was compatible, so update this value
			{
				p_sigma_matrix[t][i] = current_x_vec[i] / sumprob;
			}
			else
			{
				p_sigma_matrix[t][i] = 0;
			}
		}
	}
	if (verbose == 1)
	{
		std::cout << "sigma matrix done" << std::endl;
	}
	return p_sigma_matrix;
}

// updates a_t estimates based on compatible types and transaction data
void update_arrival_estimates(int *trans_vec, int **mu_matrix, int verbose = 0)
{
	if (verbose == 1)
	{
		std::cout << "estimating a_t" << std::endl;
	}
	for (int t = 0; t < n_times; t++)
	{
		if (verbose == 1)
		{
			if (t % 1000 == 0)
			{
				std::cout << t << std::endl;
			}
		}

		if (trans_vec[t] != 0) // case 1: definitely arrival, item was purchased
		{
			a_vec[t] = 1;
		}
		else
		{
			int num_compat_types = 0;
			for (int i = 0; i < n_types; i++)
			{
				num_compat_types += mu_matrix[t][i];
			}
			if (num_compat_types == 0) // case 2: definitely non-arrival, no compatible types
			{
				a_vec[t] = 0;
			}
			else // case 3: might have been an arrival, update a_t with probability
			{
				double sum_compat_probs = 0; // sum of probabilities of compatible types
				for (int i = 0; i < n_types; i++)
				{
					sum_compat_probs += current_x_vec[i] * mu_matrix[t][i];
				}
				a_vec[t] = (lambda * sum_compat_probs) / (lambda * sum_compat_probs + (1 - lambda));
			}
		}
	}
	if (verbose == 1)
	{
		std::cout << "a_t estimation done" << std::endl;
	}
}

// estimate m vector, last part of e-step
// returns m_vec: length N
void estimate_m_vec(double **p_sigma_matrix, int verbose = 0)
{
	if (verbose == 1)
	{
		std::cout << "estimating m_vec" << std::endl;
	}
	for (int i = 0; i < n_types; i++)
	{
		if (verbose == 1)
		{
			if (i % 1000 == 0)
			{
				std::cout << i << std::endl;
			}
		}
		m_vec[i] = 0;
		for (int t = 0; t < n_times; t++)
		{
			m_vec[i] += a_vec[t] * p_sigma_matrix[t][i];
		}
	}
	if (verbose == 1)
	{
		std::cout << "m_vec estimation done" << std::endl;
	}
}

// Problem formulated here
class FG_eval
{
public:
	typedef CPPAD_TESTVECTOR(AD<double>) ADvector;
	void operator()(ADvector &fg, const ADvector &x)
	{
		assert(fg.size() == 2); // 1 LL formula, 1 sum of x constraint, 2*n_types regularization constraints
		assert(x.size() == n_types + 1);

		// printVector("m_vec:", m_vec, n_types, 4, 5);

		//x[0:n_types) represents type probs, x[n_types] is lambda

		// add type probs to objective
		for (int i = 0; i < n_types; i++)
		{
			fg[0] += m_vec[i] * log(x[i]);
		}
		// add lambda terms to objective
		double sum_ambiguous_a = 0;
		for (int t = 0; t < n_times; t++)
		{
			if (a_vec[t] != 1)
			{
				sum_ambiguous_a += a_vec[t];
			}
		}

		AD<double> reg_sum = 0;
		// sum regularization terms
		for (int i = 0; i < n_types; i++)
		{
			reg_sum += pow(x[i], 2);
		}
		reg_sum = reg_sum * alpha;

		// main function to optimize
		fg[0] += (n_purch + sum_ambiguous_a) * log(x[n_types]) + ((n_times - n_purch) - sum_ambiguous_a) * log(1 - x[n_types]) - reg_sum;

		// constraint that sum of x's = 1
		for (int i = 0; i < n_types; i++)
		{
			fg[1] += x[i];
		}

		return;
	}
};
} // namespace

// m-step optimization
void m_step()
{
	bool ok;

	size_t i;
	typedef CPPAD_TESTVECTOR(double) Dvector;

	// number of independent variables n_types + 1 extra for lambda + regularizers vars
	size_t nx = n_types + 1;
	// number of constraints (range dimension for g)
	size_t ng = 1;
	// initial value of the independent variables

	Dvector xi(nx);
	for (i = 0; i < n_types + 1; i++)
	{
		xi[i] = 0.1;
	}

	// lower and upper limits for x
	Dvector xl(nx), xu(nx);
	for (i = 0; i < nx; i++)
	{
		xl[i] = 0;
		xu[i] = 1;
	}

	// lower and upper limits for g
	Dvector gl(ng), gu(ng);
	for (i = 0; i < ng; i++)
	{
		gl[i] = 1;
		gu[i] = 1;
	}

	// object that computes objective and constraints
	FG_eval fg_eval;

	// options
	std::string options;
	// printing
	options += "Integer print_level  5\n";
	options += "String print_timing_statistics  yes\n";
	// scaling to maximize instead of minimize
	options += "Numeric obj_scaling_factor   -1\n";
	// maximum number of iterations
	options += "Integer max_iter     200\n";
	// approximate accuracy in first order necessary conditions;
	// see Mathematical Programming, Volume 106, Number 1,
	// Pages 25-57, Equation (6)
	options += "Numeric tol          1e-8\n";
	// derivative testing
	options += "String  derivative_test            second-order\n";
	// maximum amount of random pertubation; e.g.,
	// when evaluation finite diff
	options += "Numeric point_perturbation_radius  4.\n";

	// place to return solution
	CppAD::ipopt::solve_result<Dvector> solution;

	// solve the problem
	CppAD::ipopt::solve<Dvector, FG_eval>(
		options, xi, xl, xu, gl, gu, fg_eval, solution);
	//
	// Check some of the solution values
	//
	ok &= solution.status == CppAD::ipopt::solve_result<Dvector>::success;
	//

	// printVector("PAST SOLUTION", current_x_vec, n_types, 3, 5);
	// printVector("NEW SOLUTION", solution.x, n_types, 3, 5);

	// update current optimal x vector and difference vector, as well as lambda
	for (int i = 0; i < n_types; i++)
	{
		x_diff_vec[i] = std::abs(current_x_vec[i] - solution.x[i]);
		current_x_vec[i] = solution.x[i];
	}
	lambda = solution.x[n_types];

	std::cout << "CURRENT OBJECTIVE VALUE: " << solution.obj_value << std::endl;
	// std::cout << "CURR LAMBDA: " << solution.x[n_types] << std::endl;

	// printVector("DIFFERENCE", x_diff_vec, n_types, 3, 5);
}

// alternate m_step function for testing
void closed_form_m_step(double search_bound, int iter_limit, int verbose = 0)
{
	if (verbose == 1)
	{
		std::cout << "Searching for lagrange multiplier" << std::endl;
	}

	// binary search for lagrange multiplier
	double sum_x = 0;
	double lagm = 0; // lagrange multiplier
	double upper_bound = search_bound;
	double lower_bound = -search_bound;
	int iter_count = 0;
	double temp_x_vec[n_types];

	while (sum_x <= 1 - 1e-8 || sum_x >= 1 + 1e-8)
	{
		sum_x = 0;
		// calc sum of xi with current lambda
		for (int i = 0; i < n_types; i++)
		{
			temp_x_vec[i] = (-lagm + sqrt(pow(lagm, 2) + 8 * alpha + m_vec[i])) / (4 * alpha);
			sum_x += temp_x_vec[i];
		}

		// alter lagrange multiplier accordingly
		if (sum_x > 1) // sum x_i too high, increase lagrange multiplier
		{
			lower_bound = lagm;
			lagm = (lagm + upper_bound) / 2;
		}
		else // sum x_i too low, increase lagrange multiplier
		{
			upper_bound = lagm;
			lagm = (lagm - lower_bound) / 2;
		}

		iter_count += 1;
		if (verbose == 1)
		{
			if (iter_count % 10 == 0)
			{
				std::cout << "Current lagrange iteration: " << iter_count << std::endl;
				std::cout << "LAGM: " << lagm << std::endl;
			}
		}

		// stopping if nothing found
		if (iter_count > iter_limit)
		{
			std::cout << "Could not find lagrange multiplier within the given bounds" << std::endl;
			exit(1);
		}
	}

	// update x vec using closed form solution
	for (int i = 0; i < n_types; i++)
	{
		x_diff_vec[i] = std::abs(temp_x_vec[i] - current_x_vec[i]);
		current_x_vec[i] = temp_x_vec[i];
	}

	// calculating objective value
	// add log type probs to objective
	double LL = 0;
	for (int i = 0; i < n_types; i++)
	{
		LL += m_vec[i] * log(current_x_vec[i]);
	}

	// calculating sum ambiguous a_t
	double sum_ambiguous_a = 0;
	for (int t = 0; t < n_times; t++)
	{
		if (a_vec[t] != 1)
		{
			sum_ambiguous_a += a_vec[t];
		}
	}

	// get lambda using closed form solution
	lambda = (n_purch + sum_ambiguous_a) / n_times;
	if (lambda == 1)
	{ // adjusting lambda from 1 in case it happens so that log still works
		lambda -= 1e-9;
	}

	// calculating sum of regularization terms
	double reg_sum = 0;
	for (int i = 0; i < n_types; i++)
	{
		reg_sum += pow(current_x_vec[i], 2);
	}
	reg_sum = reg_sum * alpha;

	if (verbose == 1)
	{
		std::cout << "Sum ambig a " << sum_ambiguous_a << std::endl;
		std::cout << "lambda " << lambda << std::endl;
		std::cout << "reg_sum " << reg_sum << std::endl;
	}

	// combining in main function
	LL += (n_purch + sum_ambiguous_a) * lambda + ((n_times - n_purch) - sum_ambiguous_a) * log(1 - lambda) - reg_sum;

	std::cout << "CURRENT OBJECTIVE VALUE: " << LL << std::endl;
}

// calculates different LL function using equation (2)
double real_LL(int **mu_matrix)
{
	double LL = 0;
	for (int t = 0; t < n_times; t++)
	{
		double temp = 0; // temp variable to store xi's for one time period
		for (int i = 0; i < n_types; i++)
		{
			temp += current_x_vec[i] * mu_matrix[t][i];
		}
		temp = log(temp);
		LL += temp;
	}

	return LL;
}

int main()
{
	// load data and preprocessing
	int **sigma_matrix = import_prefs("../../../data/cabot_data/sprint_3/types_s3.csv", 1);
	int **avail_matrix = import_availability("../../../data/cabot_data/sprint_3/avail_s3.csv", 1);
	int *trans_vec = import_transactions("../../../data/cabot_data/sprint_3/trans_s3.csv", 1);
	int **mu_matrix = build_mu_mat(sigma_matrix, avail_matrix, trans_vec, 1);

	// Data import debugging prints ##################################################
	{
		printMatrix("PREFERENCE MATRIX:", sigma_matrix, 8, 9, 3);
		printMatrix("AVAILABILITY MATRIX:", avail_matrix, 10, 10, 3);
		printVector("TRANSACTION VECTOR:", trans_vec, 10, 3);
		printMatrix("MU MATRIX:", mu_matrix, 10, n_types, 3);
	}

	// init: set a_vec to 0 x_vec to 1/N, lambda to 0.5, count purchases
	std::fill_n(a_vec, n_times, 0);
	std::fill_n(current_x_vec, n_types, 1.0 / n_types);
	lambda = 0.3;
	count_purchases(trans_vec);

	// std::cout << "NUM_PURCHASES: " << n_purch << std::endl;

	// initialization debugging prints:
	{
		// printVector("A_VEC", a_vec, 10, 5);
		// printVector("current_x_vec", current_x_vec, 10, 5);
	}

	// EM loop starts here
	double maxdiff;
	bool done = 0;
	while (!done)
	{
		// E step:
		// update cust type probs
		double **p_sigma_matrix = build_cust_type_probs(mu_matrix, 1);
		// update a_t predictions
		update_arrival_estimates(trans_vec, mu_matrix, 1);
		estimate_m_vec(p_sigma_matrix, 1);

		// // check that a_vec prediciton works
		// printVector("A_VEC", a_vec, n_times, 5);

		std::cout << "E-step finished" << std::endl;

		// M step:
		closed_form_m_step(1e6, 1e6, 1);

		// find max difference of solution, exit loop if small enough
		maxdiff = *std::max_element(x_diff_vec, x_diff_vec + n_types);
		if (maxdiff < 1e-9)
		{
			done = 1;
		}

		for (int t = 0; t < n_times; t++)
		{
			delete[] p_sigma_matrix[t];
		}

		delete[] p_sigma_matrix;
		// printVector("CURRENT SOLUTION", current_x_vec, n_types, 3, 5);
	}
	// print final fitted params
	// printVector("FINAL X_VEC", current_x_vec, n_types, 5, 5);
	std::cout << "FINAL LAMBDA: " << lambda << std::endl;

	// save to csv
	std::ofstream output;
	output.open("sprint3_results.csv");
	output << "var, value\n";
	for (int i = 0; i < n_types; i++)
	{
		output << 'x';
		output << i + 1;
		output << ',';
		output << current_x_vec[i];
		output << '\n';
	}
	output << "lambda," << lambda << '\n';

	std::cout << "TEST DONE" << std::endl;
	return 1;
}