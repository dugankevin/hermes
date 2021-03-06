// This file is part of Hermes3D
//
// Copyright (c) 2009 hp-FEM group at the University of Nevada, Reno (UNR).
// Email: hpfem-group@unr.edu, home page: http://hpfem.org/.
//
// Hermes3D is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation; either version 2 of the License,
// or (at your option) any later version.
//
// Hermes3D is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Hermes3D; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "epetra.h"
#include "../error.h"
#include "../callstack.h"

// EpetraMatrix ////////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_EPETRA
  // A communicator for Epetra objects (serial version)
  static Epetra_SerialComm seq_comm;
#endif

EpetraMatrix::EpetraMatrix()
{
  _F_
#ifdef HAVE_EPETRA
  this->mat = NULL;
#if defined(H2D_COMPLEX) || defined(H3D_COMPLEX)
  this->mat_im = NULL;
#endif
  this->grph = NULL;
  this->std_map = NULL;
  this->owner = true;

  this->row_storage = true;
  this->col_storage = false;
#else
  error(EPETRA_NOT_COMPILED);
#endif
}

#ifdef HAVE_EPETRA
EpetraMatrix::EpetraMatrix(Epetra_RowMatrix &op)
{
  _F_
  this->mat = dynamic_cast<Epetra_CrsMatrix *>(&op);
  assert(mat != NULL);
  this->grph = (Epetra_CrsGraph *) &this->mat->Graph();
  this->std_map = (Epetra_BlockMap *) &this->grph->Map();
  this->owner = false;

  this->row_storage = true;
  this->col_storage = false;
}
#endif

EpetraMatrix::~EpetraMatrix()
{
  _F_
#ifdef HAVE_EPETRA
  free();
#endif
}

void EpetraMatrix::prealloc(int n)
{
  _F_
#ifdef HAVE_EPETRA
  this->size = n;
  // alloc trilinos structs
  std_map = new Epetra_Map(n, 0, seq_comm); MEM_CHECK(std_map);
  grph = new Epetra_CrsGraph(Copy, *std_map, 0); MEM_CHECK(grph);
#endif
}

void EpetraMatrix::pre_add_ij(int row, int col)
{
  _F_
#ifdef HAVE_EPETRA
  grph->InsertGlobalIndices(row, 1, &col);
#endif
}

void EpetraMatrix::finish()
{
  _F_
#ifdef HAVE_EPETRA
  mat->FillComplete();
#if defined(H2D_COMPLEX) || defined(H3D_COMPLEX)
  mat_im->FillComplete();
#endif
#endif
}

void EpetraMatrix::alloc()
{
  _F_
#ifdef HAVE_EPETRA
  grph->FillComplete();
  // create the matrix
  mat = new Epetra_CrsMatrix(Copy, *grph); MEM_CHECK(mat);
#if defined(H2D_COMPLEX) || defined(H3D_COMPLEX)
  mat_im = new Epetra_CrsMatrix(Copy, *grph); MEM_CHECK(mat_im);
#endif
#endif
}

void EpetraMatrix::free()
{
  _F_
#ifdef HAVE_EPETRA
  if (owner) {
    delete mat; mat = NULL;
#if defined(H2D_COMPLEX) || defined(H3D_COMPLEX)
    delete mat_im; mat_im = NULL;
#endif
    delete grph; grph = NULL;
    delete std_map; std_map = NULL;
  }
#endif
}

scalar EpetraMatrix::get(int m, int n)
{
  _F_
#ifdef HAVE_EPETRA
    int n_entries = mat->NumGlobalEntries(m);
    std::vector<double> vals(n_entries);
    std::vector<int> idxs(n_entries);
    mat->ExtractGlobalRowCopy(m, n_entries, n_entries, &vals[0], &idxs[0]);
    for (int i = 0; i < n_entries; i++) if (idxs[i] == n) return vals[i];
#endif
    return 0.0;
}

int EpetraMatrix::get_num_row_entries(int row)
{
  _F_
#ifdef HAVE_EPETRA
  return mat->NumGlobalEntries(row);
#else
  return 0;
#endif
}

void EpetraMatrix::extract_row_copy(int row, int len, int &n_entries, double *vals, int *idxs)
{
  _F_
#ifdef HAVE_EPETRA
  mat->ExtractGlobalRowCopy(row, len, n_entries, vals, idxs);
#endif
}

void EpetraMatrix::zero()
{
  _F_
#ifdef HAVE_EPETRA
  mat->PutScalar(0.0);
#if defined(H2D_COMPLEX) || defined(H3D_COMPLEX)
  mat_im->PutScalar(0.0);
#endif
#endif
}

void EpetraMatrix::add(int m, int n, scalar v)
{
  _F_
#ifdef HAVE_EPETRA
  if (v != 0.0 && m >= 0 && n >= 0) {		// ignore dirichlet DOFs
#if !defined(H2D_COMPLEX) && !defined(H3D_COMPLEX)
    int ierr = mat->SumIntoGlobalValues(m, 1, &v, &n);
    if (ierr != 0) error("Failed to insert into Epetra matrix");
#else
    int ierr = mat->SumIntoGlobalValues(m, 1, &std::real(v), &n);
    assert(ierr == 0);
    ierr = mat_im->SumIntoGlobalValues(m, 1, &std::imag(v), &n);
    assert(ierr == 0);
#endif
  }
#endif
}

void EpetraMatrix::add(int m, int n, scalar **mat, int *rows, int *cols)
{
  _F_
#ifdef HAVE_EPETRA
  for (int i = 0; i < m; i++)				// rows
    for (int j = 0; j < n; j++)			// cols
      add(rows[i], cols[j], mat[i][j]);
#endif
}

/// dumping matrix and right-hand side
///
bool EpetraMatrix::dump(FILE *file, const char *var_name, EMatrixDumpFormat fmt)
{
  _F_
  return false;
}

int EpetraMatrix::get_matrix_size() const
{
  _F_
#ifdef HAVE_EPETRA
  return mat->NumGlobalNonzeros();
#else
  return -1;
#endif
}

double EpetraMatrix::get_fill_in() const
{
  _F_
#ifdef HAVE_EPETRA
  return mat->NumGlobalNonzeros() / (double) (size * size);
#else
  return -1;
#endif
}


// EpetraVector ////////////////////////////////////////////////////////////////////////////////////

EpetraVector::EpetraVector()
{
  _F_
#ifdef HAVE_EPETRA
  this->std_map = NULL;
  this->vec = NULL;
#if defined(H2D_COMPLEX) || defined(H3D_COMPLEX)
  this->vec_im = NULL;
#endif
  this->size = 0;
  this->owner = true;
#endif
}

#ifdef HAVE_EPETRA
EpetraVector::EpetraVector(const Epetra_Vector &v)
{
  _F_
  this->vec = (Epetra_Vector *) &v;
  this->std_map = (Epetra_BlockMap *) &v.Map();
  this->size = v.MyLength();
  this->owner = false;
}
#endif

EpetraVector::~EpetraVector()
{
  _F_
#ifdef HAVE_EPETRA
  if (owner) free();
#endif
}

void EpetraVector::alloc(int n)
{
  _F_
#ifdef HAVE_EPETRA
  free();
  size = n;
  std_map = new Epetra_Map(size, 0, seq_comm); MEM_CHECK(std_map);
  vec = new Epetra_Vector(*std_map); MEM_CHECK(vec);
#if defined(H2D_COMPLEX) || defined(H3D_COMPLEX)
  vec_im = new Epetra_Vector(*std_map); MEM_CHECK(vec_im);
#endif
  zero();
#endif
}

void EpetraVector::zero()
{
  _F_
#ifdef HAVE_EPETRA
  for (int i = 0; i < size; i++) (*vec)[i] = 0.0;
#if defined(H2D_COMPLEX) || defined(H3D_COMPLEX)
  for (int i = 0; i < size; i++) (*vec_im)[i] = 0.0;
#endif
#endif
}

void EpetraVector::free()
{
  _F_
#ifdef HAVE_EPETRA
  delete std_map; std_map = NULL;
  delete vec; vec = NULL;
#if defined(H2D_COMPLEX) || defined(H3D_COMPLEX)
  delete vec_im; vec_im = NULL;
#endif
  size = 0;
#endif
}

void EpetraVector::set(int idx, scalar y)
{
  _F_
#ifdef HAVE_EPETRA
#if !defined(H2D_COMPLEX) && !defined(H3D_COMPLEX)
  if (idx >= 0) (*vec)[idx] = y;
#else
  if (idx >= 0) {
    (*vec)[idx] = std::real(y);
    (*vec_im)[idx] = std::imag(y);
  }
#endif
#endif
}

void EpetraVector::add(int idx, scalar y)
{
  _F_
#ifdef HAVE_EPETRA
#if !defined(H2D_COMPLEX) && !defined(H3D_COMPLEX)
  if (idx >= 0) (*vec)[idx] += y;
#else
  if (idx >= 0) {
    (*vec)[idx] += std::real(y);
    (*vec_im)[idx] += std::imag(y);
  }
#endif
#endif
}

void EpetraVector::add(int n, int *idx, scalar *y)
{
  _F_
#ifdef HAVE_EPETRA
  for (int i = 0; i < n; i++)
    add(idx[i], y[i]);
#endif
}

bool EpetraVector::dump(FILE *file, const char *var_name, EMatrixDumpFormat fmt)
{
  _F_
  return false;
}
