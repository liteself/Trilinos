//@HEADER
// ************************************************************************
//
//                 Belos: Block Linear Solvers Package
//                  Copyright 2004 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
//@HEADER
//
// This driver reads a problem from a file, which can be in Harwell-Boeing (*.hb),
// Matrix Market (*.mtx), or triplet format (*.triU, *.triS).  The right-hand side
// from the problem, if it exists, will be used instead of multiple random
// right-hand-sides.  The initial guesses are all set to zero.  An ILU preconditioner
// is constructed using the Ifpack factory.
//
#include "BelosConfigDefs.hpp"
#include "BelosLinearProblem.hpp"
#include "BelosEpetraAdapter.hpp"
#include "BelosLSQRSolMgr.hpp"

#include "EpetraExt_readEpetraLinearSystem.h"
#include "Epetra_Map.h"
#ifdef EPETRA_MPI
  #include "Epetra_MpiComm.h"
#else
  #include "Epetra_SerialComm.h"
#endif
#include "Epetra_CrsMatrix.h"

#include "Ifpack.h"

#include "Teuchos_CommandLineProcessor.hpp"
#include "Teuchos_ParameterList.hpp"

int main(int argc, char *argv[]) {
  //
  int MyPID = 0;
#ifdef EPETRA_MPI
  // Initialize MPI
  MPI_Init(&argc,&argv);
  Epetra_MpiComm Comm(MPI_COMM_WORLD);
  MyPID = Comm.MyPID();
#else
  Epetra_SerialComm Comm;
#endif
  //
  typedef double                            ST;
  typedef Teuchos::ScalarTraits<ST>        SCT;
  typedef SCT::magnitudeType                MT;
  typedef Epetra_MultiVector                MV;
  typedef Epetra_Operator                   OP;
  typedef Belos::MultiVecTraits<ST,MV>     MVT;
  typedef Belos::OperatorTraits<ST,MV,OP>  OPT;

  using Teuchos::ParameterList;
  using Teuchos::RCP;
  using Teuchos::rcp;

  bool verbose = false, proc_verbose = false;
  bool leftprec = true;      // left preconditioning or right.
  // LSQR applies the operator and the transposed operator.
  // A preconditioner must support transpose multiply.
  int frequency = -1;        // frequency of status test output.
  int blocksize = 1;         // blocksize
  // LSQR as currently implemented is a single vector algorithm.
  // However some of the parameters that would be used by a block version
  // have not been removed from this file.
  int numrhs = 1;            // number of right-hand sides to solve for
  int maxiters = -1;         // maximum number of iterations allowed per linear system
  std::string filename("orsirr1_scaled.hb");
  MT relResTol = 1.0e-5;     // relative residual tolerance
  //MT resGrowthFactor = 2.0;  // In this example, warn if |resid| > resGrowthFactor * relResTol
  // un-used variable

  MT relMatTol = 1.e-10;     // relative Matrix error, default value sqrt(eps)
  MT maxCond  = 1.e+5;       // maximum condition number default value 1/eps
  MT damp = 0.;              // regularization (or damping) parameter 

  Teuchos::CommandLineProcessor cmdp(false,true);
  cmdp.setOption("verbose","quiet",&verbose,"Print messages and results.");
  cmdp.setOption("left-prec","right-prec",&leftprec,"Left preconditioning or right.");
  cmdp.setOption("frequency",&frequency,"Solvers frequency for printing residuals (#iters).");
  cmdp.setOption("filename",&filename,"Filename for test matrix.  Acceptable file extensions: *.hb,*.mtx,*.triU,*.triS");
  cmdp.setOption("lambda",&damp,"Regularization parameter");
  cmdp.setOption("tol",&relResTol,"Relative residual tolerance");
  cmdp.setOption("matrixTol",&relMatTol,"Relative error in Matrix");
  cmdp.setOption("max-cond",&maxCond,"Maximum condition number");
  cmdp.setOption("num-rhs",&numrhs,"Number of right-hand sides to be solved for.");
  cmdp.setOption("block-size",&blocksize,"Block size used by LSQR.");
  cmdp.setOption("max-iters",&maxiters,"Maximum number of iterations per linear system (-1 = adapted to problem/block size).");
  if (cmdp.parse(argc,argv) != Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL) {
    return -1;
  }
  if (!verbose)
    frequency = -1;  // reset frequency if test is not verbose

  //
  // *************Get the problem*********************
  //
  RCP<Epetra_Map> Map;
  RCP<Epetra_CrsMatrix> A;
  RCP<Epetra_MultiVector> B, X;
  RCP<Epetra_Vector> vecB, vecX;
  EpetraExt::readEpetraLinearSystem(filename, Comm, &A, &Map, &vecX, &vecB);
  A->OptimizeStorage();
  proc_verbose = verbose && (MyPID==0);  /* Only print on the zero processor */

  // Check to see if the number of right-hand sides is the same as requested.
  if (numrhs>1) {
    X = rcp( new Epetra_MultiVector( *Map, numrhs ) );
    B = rcp( new Epetra_MultiVector( *Map, numrhs ) );
    X->Seed();
    X->Random();
    OPT::Apply( *A, *X, *B );
    X->PutScalar( 0.0 );
  }
  else {
    X = Teuchos::rcp_implicit_cast<Epetra_MultiVector>(vecX);
    B = Teuchos::rcp_implicit_cast<Epetra_MultiVector>(vecB);
  }
  //
  // ************Construct preconditioner*************
  //
  ParameterList ifpackList;

  // allocates an IFPACK factory. No data is associated
  // to this object (only method Create()).
  Ifpack Factory;  // do support transpose multiply

  // create the preconditioner. For valid PrecType values,
  // please check the documentation
  std::string PrecType = "ILU"; // incomplete LU
  int OverlapLevel = 1; // must be >= 0. If Comm.NumProc() == 1,
                        // it is ignored.

  RCP<Ifpack_Preconditioner> Prec = Teuchos::rcp( Factory.Create(PrecType, &*A, OverlapLevel) );
  assert(Prec != Teuchos::null);

  // specify parameters for ILU
  ifpackList.set("fact: drop tolerance", 1e-9);
  ifpackList.set("fact: ilut level-of-fill", 1.0);
  // the combine mode is on the following:
  // "Add", "Zero", "Insert", "InsertAdd", "Average", "AbsMax"
  // Their meaning is as defined in file Epetra_CombineMode.h
  ifpackList.set("schwarz: combine mode", "Add");
  // sets the parameters
  IFPACK_CHK_ERR(Prec->SetParameters(ifpackList));

  // initialize the preconditioner. At this point the matrix must
  // have been FillComplete()'d, but actual values are ignored.
  IFPACK_CHK_ERR(Prec->Initialize());

  // Builds the preconditioners, by looking for the values of
  // the matrix.
  IFPACK_CHK_ERR(Prec->Compute());

  {
    const int errcode = Prec->SetUseTranspose (true);
    if (errcode != 0) {
      throw std::logic_error ("Oh hai! Ifpack_Preconditioner doesn't know how to apply its transpose.");
    } else {
      (void) Prec->SetUseTranspose (false);
    }
  }

  // Create the Belos preconditioned operator from the Ifpack preconditioner.
  // NOTE:  This is necessary because Belos expects an operator to apply the
  //        preconditioner with Apply() NOT ApplyInverse().
  RCP<Belos::EpetraPrecOp> belosPrec = rcp( new Belos::EpetraPrecOp( Prec ) );

  //
  // *****Create parameter list for the LSQR solver manager*****
  //
  const int NumGlobalElements = B->GlobalLength();
  if (maxiters == -1)
    maxiters = NumGlobalElements/blocksize - 1; // maximum number of iterations to run
  //
  ParameterList belosList;
  belosList.set( "Block Size", blocksize );       // Blocksize to be used by iterative solver
  belosList.set( "Lambda", damp );                // Regularization parameter
  belosList.set( "Rel RHS Err", relResTol );      // Relative convergence tolerance requested
  belosList.set( "Rel Mat Err", relMatTol );      // Maximum number of restarts allowed
  belosList.set( "Condition limit", maxCond);     // upper bound for cond(A)
  belosList.set( "Maximum Iterations", maxiters );// Maximum number of iterations allowed
  if (numrhs > 1) {
    belosList.set( "Show Maximum Residual Norm Only", true );  // Show only the maximum residual norm
  }
  if (verbose) {
    belosList.set( "Verbosity", Belos::Errors + Belos::Warnings + 
		   Belos::TimingDetails + Belos::StatusTestDetails );
    if (frequency > 0)
      belosList.set( "Output Frequency", frequency );
  }
  else
    belosList.set( "Verbosity", Belos::Errors + Belos::Warnings );
  //
  // *******Construct a preconditioned linear problem********
  //
  RCP<Belos::LinearProblem<double,MV,OP> > problem
    = rcp( new Belos::LinearProblem<double,MV,OP>( A, X, B ) );
  if (leftprec) {
    problem->setLeftPrec( belosPrec );
  }
  else {
    problem->setRightPrec( belosPrec );
  }    
  bool set = problem->setProblem();
  if (set == false) {
    if (proc_verbose)
      std::cout << std::endl << "ERROR:  Belos::LinearProblem failed to set up correctly!" << std::endl;
    return -1;
  }
  
  // Create an iterative solver manager.
  RCP< Belos::LSQRSolMgr<double,MV,OP> > solver
    = rcp( new Belos::LSQRSolMgr<double,MV,OP>(problem, rcp(&belosList,false)));
  
  //
  // *******************************************************************
  // ******************Start the LSQR iteration*************************
  // *******************************************************************
  //
  if (proc_verbose) {
    std::cout << std::endl << std::endl;
    std::cout << "Dimension of matrix: " << NumGlobalElements << std::endl;
    std::cout << "Number of right-hand sides: " << numrhs << std::endl;
    std::cout << "Block size used by solver: " << blocksize << std::endl;
    std::cout << "Max number of Gmres iterations per restart cycle: " << maxiters << std::endl; 
    std::cout << "Relative residual tolerance: " << relResTol << std::endl;
    std::cout << std::endl;
    std::cout << "Solver's Description: " << std::endl;
    std::cout << solver->description() << std::endl; // visually verify the parameter list
  }
  //
  // Perform solve
  //
  Belos::ReturnType ret = solver->solve();
  //
  // Get the number of iterations for this solve.
  //
  std::vector<double> solNorm( numrhs );      // get solution norm
  MVT::MvNorm( *X, solNorm );
  int numIters = solver->getNumIters();
  MT condNum = solver->getMatCondNum();
  MT matrixNorm= solver->getMatNorm();
  MT resNorm = solver->getResNorm();
  MT lsResNorm = solver->getMatResNorm();
  if (proc_verbose)
    std::cout << "Number of iterations performed for this solve: " << numIters << std::endl
     << "matrix condition number: " << condNum << std::endl
     << "matrix norm: " << matrixNorm << std::endl
     << "residual norm: " << resNorm << std::endl
     << "solution norm: " << solNorm[0] << std::endl
     << "least squares residual Norm: " << lsResNorm << std::endl;
  //
  // Compute actual residuals.
  //
  bool badRes = false;
  std::vector<double> actual_resids( numrhs );
  std::vector<double> rhs_norm( numrhs );
  Epetra_MultiVector resid(*Map, numrhs);
  OPT::Apply( *A, *X, resid );
  MVT::MvAddMv( -1.0, resid, 1.0, *B, resid );
  MVT::MvNorm( resid, actual_resids );
  MVT::MvNorm( *B, rhs_norm );
  if (proc_verbose) {
    std::cout<< "---------- Actual Residuals (normalized) ----------"<<std::endl<<std::endl;
    for ( int i=0; i<numrhs; i++) {
      double actRes = actual_resids[i]/rhs_norm[i];
      std::cout<<"Problem "<<i<<" : \t"<< actRes <<std::endl;
      if (actRes > relResTol) badRes = true;
    }
  }

#ifdef EPETRA_MPI
  MPI_Finalize();
#endif

  if (ret!=Belos::Converged || badRes) {
    if (proc_verbose)
      std::cout << std::endl << "ERROR:  Belos did not converge!" << std::endl;
    return -1;
  }
  //
  // Default return value
  //
  if (proc_verbose)
    std::cout << std::endl << "SUCCESS:  Belos converged!" << std::endl;
  return 0;

  //
} 
