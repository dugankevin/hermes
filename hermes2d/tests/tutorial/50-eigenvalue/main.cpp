#define HERMES_REPORT_INFO
#include "hermes2d.h"
#include <stdio.h>

// This test makes sure that example 50-eigenvalue works correctly.

int NUMBER_OF_EIGENVALUES = 6;                    // Desired number of eigenvalues.
int P_INIT = 4;                                   // Uniform polynomial degree of mesh elements.
const int INIT_REF_NUM = 0;                       // Number of initial mesh refinements.
double TARGET_VALUE = 2.0;                        // PySparse parameter: Eigenvalues in the vicinity of this number will be computed. 
double TOL = 1e-10;                               // Pysparse parameter: Error tolerance.
int MAX_ITER = 1000;                              // PySparse parameter: Maximum number of iterations.
MatrixSolverType matrix_solver = SOLVER_UMFPACK;  // Possibilities: SOLVER_AMESOS, SOLVER_MUMPS, 
                                                  // SOLVER_PARDISO, SOLVER_PETSC, SOLVER_UMFPACK

// Boundary condition types.
// Note: "essential" means that solution value is prescribed.
BCType bc_types(int marker)
{
  return BC_ESSENTIAL;
}

// Essential (Dirichlet) boundary condition values.
scalar essential_bc_values(int ess_bdy_marker, double x, double y)
{
  return 0;
}

// Weak forms.
#include "forms.cpp"

// Write the matrix in Matrix Market format.
void write_matrix_mm(const char* filename, Matrix* mat) 
{
  int ndof = mat->get_size();
  FILE *out = fopen(filename, "w" );
  int nz=0;
  for (int i=0; i < ndof; i++) {
    for (int j=0; j <=i; j++) { 
      double tmp = mat->get(i,j);
      if (fabs(tmp) > 1e-15) nz++;
    }
  } 

  fprintf(out,"%%%%MatrixMarket matrix coordinate real symmetric\n");
  fprintf(out,"%d %d %d\n", ndof, ndof, nz);
  for (int i=0; i < ndof; i++) {
    for (int j=0; j <=i; j++) { 
      double tmp = mat->get(i,j);
      if (fabs(tmp) > 1e-15) fprintf(out, "%d %d %24.15e\n", i+1, j+1, tmp);
    }
  } 
  fclose(out);
}

int main(int argc, char* argv[])
{
  info("Desired number of eigenvalues: %d.", NUMBER_OF_EIGENVALUES);

  // Load the mesh.
  Mesh mesh;
  H2DReader mloader;
  mloader.load("domain.mesh", &mesh);

  // Perform initial mesh refinements (optional).
  for (int i = 0; i < INIT_REF_NUM; i++) mesh.refine_all_elements();

  // Create an H1 space with default shapeset.
  H1Space space(&mesh, bc_types, essential_bc_values, P_INIT);
  int ndof = Space::get_num_dofs(&space);
  info("ndof: %d.", ndof);

  // Initialize the weak formulation for the left hand side i.e. H 
  WeakForm wf_left, wf_right;
  wf_left.add_matrix_form(callback(bilinear_form_left));
  wf_right.add_matrix_form(callback(bilinear_form_right));

  // Initialize matrices and matrix solver.
  SparseMatrix* matrix_left = create_matrix(matrix_solver);
  SparseMatrix* matrix_right = create_matrix(matrix_solver);
  Vector* eivec = create_vector(matrix_solver);
  Solver* solver = create_linear_solver(matrix_solver, matrix_left, eivec);

  // Assemble the matrices.
  bool is_linear = true;
  DiscreteProblem dp_left(&wf_left, &space, is_linear);
  dp_left.assemble(matrix_left, eivec);
  DiscreteProblem dp_right(&wf_right, &space, is_linear);
  dp_right.assemble(matrix_right, eivec);

  // Write matrix_left in MatrixMarket format.
  write_matrix_mm("mat_left.mtx", matrix_left);

  // Write matrix_left in MatrixMarket format.
  write_matrix_mm("mat_right.mtx", matrix_right);

  // Calling Python eigensolver. Solution will be written to "eivecs.dat".
  char call_cmd[255];
  sprintf(call_cmd, "python solveGenEigenFromMtx.py mat_left.mtx mat_right.mtx %g %d %g %d", 
	  TARGET_VALUE, NUMBER_OF_EIGENVALUES, TOL, MAX_ITER);
  system(call_cmd);

  // Initializing solution vector, solution and ScalarView.
  double* coeff_vec = new double(ndof);
  Solution sln;

  // Reading solution vectors from file and visualizing.
  FILE *file = fopen("eivecs.dat", "r");
  char line [64];                  // Maximum line size.
  fgets(line, sizeof line, file);  // ndof
  int n = atoi(line);            
  if (n != ndof) error("Mismatched ndof in the eigensolver output file.");  
  fgets(line, sizeof line, file);  // Number of eigenvectors in the file.
  int neig = atoi(line);
  if (neig != NUMBER_OF_EIGENVALUES) error("Mismatched number of eigenvectors in the eigensolver output file.");  
  for (int ieig = 0; ieig < neig; ieig++) {
    // Get next eigenvector from the file.
    for (int i = 0; i < ndof; i++) {  
      fgets(line, sizeof line, file);
      coeff_vec[i] = atof(line);
    }

    // Convert coefficient vector into a Solution.
    Solution::vector_to_solution(coeff_vec, &space, &sln);
  }  
  fclose(file);

  delete [] coeff_vec;
  // wait for test functions. 
};

