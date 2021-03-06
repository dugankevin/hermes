Nonlinear Parabolic Problem (18)
--------------------------------

**Git reference:** Tutorial example `18-newton-timedep-heat 
<http://git.hpfem.org/hermes.git/tree/HEAD:/hermes2d/tutorial/18-newton-timedep-heat>`_.

We will employ the Newton's method to solve a nonlinear parabolic PDE discretized 
in time by the implicit Euler method. To keep things simple, our model problem is 
a time-dependent version of the nonlinear equation used in the previous three sections,

.. math::

    \frac{\partial u}{\partial t} -\nabla \cdot (\lambda(u)\nabla u) - f(x,y) = 0.

We prescribe nonhomogeneous Dirichlet boundary conditions 

.. math::

    u(x, y) = (x+10)(y+10)/100 \ \ \ \mbox{on } \partial \Omega,

and the same function is used to define the initial condition. The 
problem will be solved in the square $\Omega = (-10,10)^2$ and time interval $(0, T)$.

The weak formulation of the time-discretized problem reads

.. math::

    \int_{\Omega} \frac{u^{n+1} - u^n}{\tau}v + \lambda(u^{n+1})\nabla u^{n+1}\cdot \nabla v - fv\, \mbox{d}x\mbox{d}y = 0,

where the indices $n$ and $n+1$ indicate the previous and new time level, respectively. Hence in each 
time step we need to solve a *time-independent* nonlinear problem, and this is something we learned 
in the previous sections. The weak forms for the Newton's method from the previous sections only 
need to be enhanced with a simple term containing the time step $\tau$ (called TAU):

::

    // Jacobian matrix.
    template<typename Real, typename Scalar>
    Scalar jac(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *u, Func<Real> *v, Geom<Real> *e, ExtData<Scalar> *ext)
    {
      Scalar result = 0;
      Func<Scalar>* u_prev_newton = ext->fn[0];
      for (int i = 0; i < n; i++)
        result += wt[i] * (u->val[i] * v->val[i] / TAU + dlam_du(u_prev_newton->val[i]) * u->val[i] * 
                           (u_prev_newton->dx[i] * v->dx[i] + u_prev_newton->dy[i] * v->dy[i])
                           + lam(u_prev_newton->val[i]) * (u->dx[i] * v->dx[i] + u->dy[i] * v->dy[i]));                    
      return result;
    }

Here the function u_prev_newton plays the role of u_prev from the previous sections - this is the 
previous solution inside the Newton's iteration. Note that the previous time level solution 
$u^n$ that we call u_prev_time is not present in the Jacobian matrix. It is used in the residual only::

    // Fesidual vector.
    template<typename Real, typename Scalar>
    Scalar res(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, Geom<Real> *e, ExtData<Scalar> *ext)
    {
      Scalar result = 0;
      Func<Scalar>* u_prev_newton = ext->fn[0];
      Func<Scalar>* u_prev_time = ext->fn[1];
      for (int i = 0; i < n; i++)
        result += wt[i] * ((u_prev_newton->val[i] - u_prev_time->val[i]) * v->val[i] / TAU +
                          lam(u_prev_newton->val[i]) * (u_prev_newton->dx[i] * v->dx[i] + u_prev_newton->dy[i] * v->dy[i])
		           - heat_src(e->x[i], e->y[i]) * v->val[i]);
      return result;
    }

Note that the function u_prev_newton evolves during the Newton's iteration
but the previous time level solution u_prev_time only is updated after the time step
is finished. The weak forms are registered as usual::

  // Initialize the weak formulation.
  WeakForm wf;
  wf.add_matrix_form(callback(jac), H2D_UNSYM, H2D_ANY, &u_prev_newton);
  wf.add_vector_form(callback(res), H2D_ANY, Tuple<MeshFunction*>(&u_prev_newton, &u_prev_time));

The entire time-stepping loop (minus visualization) looks as follows::

  // Time stepping loop:
  double current_time = 0.0;
  int t_step = 1;
  do {
    info("---- Time step %d, t = %g s.", t_step, current_time); t_step++;

    // Newton's method.
    info("Performing Newton's method.");
    bool verbose = true; // Default is false.
    if (!nls.solve_newton(&u_prev_newton, NEWTON_TOL, NEWTON_MAX_ITER, verbose)) 
      error("Newton's method did not converge.");

    // Update previous time level solution.
    u_prev_time.copy(&u_prev_newton);

    // Update time.
    current_time += TAU;

  } while (current_time < T_FINAL);

The stationary solution is not shown here since we already saw it 
in the previous sections.
