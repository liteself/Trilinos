/*!
 *  \file ml_epetra_utils.h
 *
 *  \brief Interface to the Trilinos package Anasazi.
 *
 *  \date Last update to Doxygen: 22-Jul-04
 *
 */


#ifndef _ML_EPETRA_UTILS_H_
#define _ML_EPETRA_UTILS_H_

class Epetra_Comm;
class Epetra_BlockMap;
class Epetra_MultiVector;
class Epetra_RowMatrix;
class Epetra_Map;
class Epetra_Vector;
class Epetra_Import;
class Epetra_Object;
class Epetra_CrsMatrix;
class Epetra_RowMatrix;
class Epetra_LinearProblem;

#include "ml_common.h"

#ifdef ML_MPI
#ifndef EPETRA_MPI
#define EPETRA_MPI
#endif
#include "mpi.h"
#endif
#include "ml_include.h"

// ====================================================================== 
//! Matrix-vector function for Epetra matrices.
/*! This is the ML matrix-vector wrap for Epetra matrices.
 */
int Epetra_ML_matvec(ML_Operator *data, int in, double *p, int out,
                 double *ap);

#ifdef WKC
int Epetra_ML_matvec_WKC(ML_Operator *data, int in, double *p, int out,
                 double *ap);
#endif 

//! Getrow function for matrix of type Epetra_RowMatrix.
/*!
 * Supply local matrix (without ghost node columns) for rows given by
 * requested_rows[0 ... N_requested_rows-1].  Return this information in
 * 'row_lengths, columns, values'.  If there is not enough space to complete
 * this operation, return 0. Otherwise, return 1.
 *
 * \param \in  data             Points to user's data containing matrix values.
 * \parrm \in  N_requested_rows Number of rows for which nonzero are to be
 *                  returned.
 * \param \in  requested_rows   Requested_rows[0...N_requested_rows-1] give the
 *                  row indices of the rows for which nonzero values are
 *                  returned.
 * \param \out row_lengths      Row_lengths[i] is the number of nonzeros in the
 *                  row 'requested_rows[i]'
 * \param \out columns,values   Columns[k] and values[k] contains the column
 *                  number and value of a matrix nonzero where all nonzeros for
 *                  requested_rows[i] appear before requested_rows[i+1]'s
 *                  nonzeros.  NOTE: Arrays are of size 'allocated_space'.
 * \param \in allocated_space  Indicates the space available in 'columns' and
 *                  'values' for storing nonzeros. If more space is needed,
 *                  return 0.
 */
int Epetra_ML_getrow(ML_Operator *data, int N_requested_rows,
                 int requested_rows[], int allocated_space, int columns[],
                 double values[], int row_lengths[]);

//! Update vec's ghost node via communication.
/*! Update vec's ghost node via communication. Note: the length of vec is
 * given by N_local + N_ghost where Amat was created via
 *                 \c AZ_matrix_create(N_local);
 * and a 'getrow' function was supplied via
 *                 \c AZ_set_MATFREE_getrow(Amat,,,,N_ghost,);
 *
 * \param \inout vec Vec contains data. On output, ghost values
 *                   are updated.
 *
 * \param \in data  points to user's data containing matrix values.
 *                  and communication information.
 */ 
int Epetra_ML_comm_wrapper(double vec[], void *data);

//! Wraps an Epetra_RowMatrix into an ML_Operators.
/*! This function creates an ML_Operator that is based on the input 
 *  Epetra_RowMatrix. This is a "cheap" wrap in the sense that
 *  only function and pointers are created. Data is still coded as an
 *  Epetra_RowMatrix.
 *
 *  \note ML requires A->RowMatrixRowMap() == A->OperatorRangeMap()
 */
int EpetraMatrix2MLMatrix(ML *ml_handle, int level,
                                Epetra_RowMatrix * Amat);

//! Wraps an Epetra_RowMatrix into an ML_Operators, for the given level.
/*! This function creates an ML_Operator that is based on the input 
 *  Epetra_RowMatrix. This is a "cheap" wrap in the sense that
 *  only function and pointers are created. Data is still coded as an
 *  Epetra_RowMatrix. The ML_Operator is set in the specified level of the
 *  hierarchy.
 *
 *  \note ML requires A->RowMatrixRowMap() == A->OperatorRangeMap()
 */
int Epetra2MLMatrix(Epetra_RowMatrix * A, ML_Operator *Result);

//! Multiplies two Epetra_RowMatrix's, returns the results as an Epetra_CrsMatrix.
Epetra_CrsMatrix *Epetra_MatrixMult(Epetra_RowMatrix *B, Epetra_RowMatrix *Bt);

//! Adds two Epetra_RowMatrix's, returns the result as an Epetra_CrsMatrix
Epetra_CrsMatrix *Epetra_MatrixAdd(Epetra_RowMatrix *B, Epetra_RowMatrix *Bt, double scalar);
int ML_Epetra_CRSinsert(ML_Operator *, int, int *, double *, int);

//! Converts an ML_Operator into an Epetra_CrsMatrix
/*! This function creates a new Epetra_CrsMatrix, and inserts all the nonzero
 * elements of the ML_Operator in it. This is an expensive conversion, in the
 * sense that the Epetra_RowMatrix is a \sl copy of the input ML_Operator.
 *
 * \note This function can be used with rectangular matrices.
 */
int ML_Operator2EpetraCrsMatrix(ML_Operator *Ke, Epetra_CrsMatrix * &
				CrsMatrix, int & MaxNumNonzeros,
				bool CheckNonzeroRow, double &);

#endif /* _ML_EPETRA_UTILS_H_ */
