#define HERMES_REPORT_WARN
#define HERMES_REPORT_INFO
#define HERMES_REPORT_VERBOSE
#define HERMES_REPORT_FILE "application.log"
#define DEBUG_ORDER
#include "hermes2d.h"

using namespace RefinementSelectors;

//  This example uses the Newton's method to solve a nonlinear complex-valued
//  time-dependent PDE (the Gross-Pitaevski equation describing the behavior
//  of Einstein-Bose quantum gases). For time-discretization one can use either
//  the first-order implicit Euler method or the second-order Crank-Nicolson
//  method.
//
//  PDE: non-stationary complex Gross-Pitaevski equation
//  describing resonances in Bose-Einstein condensates.
//
//  ih \partial \psi/\partial t = -h^2/(2m) \Delta \psi +
//  g \psi |\psi|^2 + 1/2 m \omega^2 (x^2 + y^2) \psi.
//
//  Domain: square (-1, 1)^2.
//
//  BC:  homogeneous Dirichlet everywhere on the boundary

const int INIT_REF_NUM = 2;      // Number of initial uniform refinements.
const int P_INIT = 4;            // Initial polynomial degree.
const double TAU = 0.005;        // Time step.
const double T_FINAL = 2*TAU + 1e-4;        // Time interval length.
const int TIME_DISCR = 2;        // 1 for implicit Euler, 2 for Crank-Nicolson.
const double NEWTON_TOL = 1e-5;  // Stopping criterion for the Newton's method.
const int NEWTON_MAX_ITER = 100; // Maximum allowed number of Newton iterations.
MatrixSolverType matrix_solver = SOLVER_UMFPACK;  // Possibilities: SOLVER_UMFPACK, SOLVER_PETSC,
                                                  // SOLVER_MUMPS, and more are coming.

// Problem constants
const double H = 1;              // Planck constant 6.626068e-34.
const double M = 1;              // Mass of boson.
const double G = 1;              // Coupling constant.
const double OMEGA = 1;          // Frequency.

// Initial conditions.
scalar init_cond(double x, double y, scalar& dx, scalar& dy)
{
  scalar val = exp(-10*(x*x + y*y));
  dx = val * (-20.0 * x);
  dy = val * (-20.0 * y);
  return val;
}

// Boundary condition types.
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

int main(int argc, char* argv[])
{
  // Load the mesh.
  Mesh mesh;
  H2DReader mloader;
  mloader.load("square.mesh", &mesh);

  // Initial mesh refinements.
  for(int i = 0; i < INIT_REF_NUM; i++) mesh.refine_all_elements();

  // Create an H1 space with default shapeset.
  H1Space space(&mesh, bc_types, essential_bc_values, P_INIT);
  int ndof = Space::get_num_dofs(&space);
  info("ndof = %d.", ndof);

  // Previous time level solution.
  Solution psi_prev_time(&mesh, init_cond);

  // Initialize the weak formulation.
  WeakForm wf;
  if(TIME_DISCR == 1) {
    wf.add_matrix_form(callback(J_euler), HERMES_UNSYM, HERMES_ANY);
    wf.add_vector_form(callback(F_euler), HERMES_ANY, &psi_prev_time);
  }
  else {
    wf.add_matrix_form(callback(J_cranic), HERMES_UNSYM, HERMES_ANY);
    wf.add_vector_form(callback(F_cranic), HERMES_ANY, &psi_prev_time);
  }

  // Initialize the FE problem.
  bool is_linear = false;
  DiscreteProblem dp(&wf, &space, is_linear);

  // Set up the solver, matrix, and rhs according to the solver selection.
  SparseMatrix* matrix = create_matrix(matrix_solver);
  Vector* rhs = create_vector(matrix_solver);
  Solver* solver = create_linear_solver(matrix_solver, matrix, rhs);

  // Project the initial condition on the FE space to obtain initial
  // coefficient vector for the Newton's method.
  info("Projecting initial condition to obtain initial vector for the Newton's method.");
  scalar* coeff_vec = new scalar[ndof];
  OGProjection::project_global(&space, &psi_prev_time, coeff_vec, matrix_solver);
  
  // Time stepping loop:
  int nstep = (int)(T_FINAL/TAU + 0.5);
  for(int ts = 1; ts <= nstep; ts++)
  {
    info("Time step %d:", ts);

    // Newton's method.
    info("Performing Newton's method.");
    int it = 1;
    while (1)
    {
      dp.assemble(coeff_vec, matrix, rhs, false);
      
      // Multiply the residual vector with -1 since the matrix 
      // equation reads J(Y^n) \deltaY^{n+1} = -F(Y^n).
      for (int i = 0; i < ndof; i++) rhs->set(i, -rhs->get(i));
      
      // Calculate the l2-norm of residual vector.
      double res_l2_norm = get_l2_norm(rhs);

      // Info for user.
      info("---- Newton iter %d, ndof %d, res. l2 norm %g", it, ndof, res_l2_norm);

      // If l2 norm of the residual vector is within tolerance, or the maximum number 
      // of iteration has been reached, then quit.
      if (res_l2_norm < NEWTON_TOL || it > NEWTON_MAX_ITER) break;

      // Solve the linear system and if successful, obtain the solutions.
      if(!solver->solve())
        error ("Matrix solver failed.\n");

      // Add \deltaY^{n+1} to Y^n.
      for (int i = 0; i < ndof; i++) coeff_vec[i] += solver->get_solution()[i];
      
      if (it >= NEWTON_MAX_ITER)
        error ("Newton method did not converge.");
     
      it++;
    };
    
    // Update previous time level solution.
    Solution::vector_to_solution(coeff_vec, &space, &psi_prev_time);
  }

  delete coeff_vec;
  delete matrix;
  delete rhs;
  delete solver;

  AbsFilter mag2(&psi_prev_time);
#define ERROR_SUCCESS                                0
#define ERROR_FAILURE                               -1
  int success = 1;
  double eps = 1e-5;
  double val = std::abs(mag2.get_pt_value(0.1, 0.1));
  info("Coordinate ( 0.1, 0.1) psi value = %lf", val);
  if (fabs(val - (0.804900)) > eps) {
    printf("Coordinate ( 0.1, 0.1) psi value = %lf\n", val);
    success = 0;
  }

  val = std::abs(mag2.get_pt_value(0.1, -0.1));
  info("Coordinate ( 0.1, -0.1) psi value = %lf", val);
  if (fabs(val - (0.804900)) > eps) {
    printf("Coordinate ( 0.1, -0.1) psi value = %lf\n", val);
    success = 0;
  }

  val = std::abs(mag2.get_pt_value(0.2, 0.1));
  info("Coordinate ( 0.2, 0.1) psi value = %lf", val);
  if (fabs(val - (0.602930)) > eps) {
    printf("Coordinate ( 0.2, 0.1) psi value = %lf\n", val);
    success = 0;
  }

  if (success == 1) {
    printf("Success!\n");
    return ERROR_SUCCESS;
  }
  else {
    printf("Failure!\n");
    return ERROR_FAILURE;
  }
}
