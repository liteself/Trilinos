// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
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
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

/*! \file  example_03.cpp
    \brief Shows how to solve a steady Burgers' optimal control problem using
           full-space methods.
*/

#include "ROL_Algorithm.hpp"
#include "ROL_MoreauYosidaPenaltyStep.hpp"
#include "ROL_StatusTest.hpp"
#include "ROL_BoundConstraint_SimOpt.hpp"
#include "ROL_Vector_SimOpt.hpp"

#include "Teuchos_oblackholestream.hpp"
#include "Teuchos_GlobalMPISession.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"

#include <iostream>
#include <algorithm>

#include "example_07.hpp"

typedef double RealT;

int main(int argc, char *argv[]) {

  Teuchos::GlobalMPISession mpiSession(&argc, &argv);

  // This little trick lets us print to std::cout only if a (dummy) command-line argument is provided.
  int iprint     = argc - 1;
  Teuchos::RCP<std::ostream> outStream;
  Teuchos::oblackholestream bhs; // outputs nothing
  if (iprint > 0)
    outStream = Teuchos::rcp(&std::cout, false);
  else
    outStream = Teuchos::rcp(&bhs, false);

  int errorFlag  = 0;

  // *** Example body.

  try {
    /*************************************************************************/
    /************* INITIALIZE BURGERS FEM CLASS ******************************/
    /*************************************************************************/
    int nx      = 128;   // Set spatial discretization.
    RealT alpha = 1.e-3; // Set penalty parameter.
    RealT nu    = 1e-2;  // Viscosity parameter.
    RealT nl    = 1.0;   // Nonlinearity parameter (1 = Burgers, 0 = linear).
    RealT u0    = 1.0;   // Dirichlet boundary condition at x=0.
    RealT u1    = 0.0;   // Dirichlet boundary condition at x=1.
    RealT f     = 0.0;   // Constant volumetric force.
    RealT cH1   = 1.0;   // Scale for derivative term in H1 norm.
    RealT cL2   = 0.0;   // Scale for mass term in H1 norm.
    Teuchos::RCP<BurgersFEM<RealT> > fem
      = Teuchos::rcp(new BurgersFEM<RealT>(nx,nu,nl,u0,u1,f,cH1,cL2));
    fem->test_inverse_mass(*outStream);
    fem->test_inverse_H1(*outStream);
    /*************************************************************************/
    /************* INITIALIZE SIMOPT OBJECTIVE FUNCTION **********************/
    /*************************************************************************/
    Teuchos::RCP<std::vector<RealT> > ud_rcp
      = Teuchos::rcp( new std::vector<RealT> (nx, 1.0) );
    Teuchos::RCP<ROL::Vector<RealT> > ud
      = Teuchos::rcp(new L2VectorPrimal<RealT>(ud_rcp,fem));
    Objective_BurgersControl<RealT> obj(fem,ud,alpha);
    /*************************************************************************/
    /************* INITIALIZE SIMOPT EQUALITY CONSTRAINT *********************/
    /*************************************************************************/
    bool useEChessian = false;
    EqualityConstraint_BurgersControl<RealT> con(fem, useEChessian);
    /*************************************************************************/
    /************* INITIALIZE BOUND CONSTRAINTS ******************************/
    /*************************************************************************/
    // INITIALIZE STATE CONSTRAINTS
    std::vector<RealT> Ulo(nx, 0.0), Uhi(nx, 1.0);
    //std::vector<RealT> Ulo(nx, -1.e8), Uhi(nx, 1.e8);
    Teuchos::RCP<ROL::BoundConstraint<RealT> > Ubnd
       = Teuchos::rcp(new H1BoundConstraint<RealT>(Ulo,Uhi,fem));
    //Ubnd.deactivate();
    // INITIALIZE CONTROL CONSTRAINTS
    //std::vector<RealT> Zlo(nx+2, -1.e8), Zhi(nx+2, 1.e8);
    std::vector<RealT> Zlo(nx+2,0.0), Zhi(nx+2,2.0);
    Teuchos::RCP<ROL::BoundConstraint<RealT> > Zbnd
      = Teuchos::rcp(new L2BoundConstraint<RealT>(Zlo,Zhi,fem));
    //bnd2.deactivate();
    // INITIALIZE SIMOPT BOUND CONSTRAINTS
    ROL::BoundConstraint_SimOpt<RealT> bnd(Ubnd,Zbnd);
    /*************************************************************************/
    /************* INITIALIZE VECTOR STORAGE *********************************/
    /*************************************************************************/
    // INITIALIZE CONTROL VECTORS
    Teuchos::RCP<std::vector<RealT> > z_rcp
      = Teuchos::rcp( new std::vector<RealT> (nx+2, 1.0) );
    Teuchos::RCP<std::vector<RealT> > gz_rcp
      = Teuchos::rcp( new std::vector<RealT> (nx+2, 1.0) );
    Teuchos::RCP<std::vector<RealT> > yz_rcp
      = Teuchos::rcp( new std::vector<RealT> (nx+2, 1.0) );
    for (int i=0; i<nx+2; i++) {
      (*z_rcp)[i]  = 10.0*(RealT)rand()/(RealT)RAND_MAX-5.0;
      (*yz_rcp)[i] = 10.0*(RealT)rand()/(RealT)RAND_MAX-5.0;
    }
    Teuchos::RCP<ROL::Vector<RealT> > zp
      = Teuchos::rcp(new L2VectorPrimal<RealT>(z_rcp,fem));
    Teuchos::RCP<ROL::Vector<RealT> > gzp
      = Teuchos::rcp(new L2VectorDual<RealT>(gz_rcp,fem));
    Teuchos::RCP<ROL::Vector<RealT> > yzp
      = Teuchos::rcp(new L2VectorPrimal<RealT>(yz_rcp,fem));
    // INITIALIZE STATE VECTORS
    Teuchos::RCP<std::vector<RealT> > u_rcp
      = Teuchos::rcp( new std::vector<RealT> (nx, 1.0) );
    Teuchos::RCP<std::vector<RealT> > gu_rcp
      = Teuchos::rcp( new std::vector<RealT> (nx, 1.0) );
    Teuchos::RCP<std::vector<RealT> > yu_rcp
      = Teuchos::rcp( new std::vector<RealT> (nx, 1.0) );
    for (int i=0; i<nx; i++) {
      (*u_rcp)[i]  = 10.0*(RealT)rand()/(RealT)RAND_MAX-5.0;
      (*yu_rcp)[i] = 10.0*(RealT)rand()/(RealT)RAND_MAX-5.0;
    }
    Teuchos::RCP<ROL::Vector<RealT> > up
      = Teuchos::rcp(new H1VectorPrimal<RealT>(u_rcp,fem));
    Teuchos::RCP<ROL::Vector<RealT> > gup
      = Teuchos::rcp(new H1VectorDual<RealT>(gu_rcp,fem));
    Teuchos::RCP<ROL::Vector<RealT> > yup
      = Teuchos::rcp(new H1VectorPrimal<RealT>(yu_rcp,fem));
    // INITIALIZE CONSTRAINT VECTORS
    Teuchos::RCP<std::vector<RealT> > c_rcp
      = Teuchos::rcp( new std::vector<RealT> (nx, 1.0) );
    Teuchos::RCP<std::vector<RealT> > l_rcp
      = Teuchos::rcp( new std::vector<RealT> (nx, 1.0) );
    for (int i=0; i<nx; i++) {
      (*l_rcp)[i] = (RealT)rand()/(RealT)RAND_MAX;
    }
    H1VectorDual<RealT> c(c_rcp,fem);
    H1VectorPrimal<RealT> l(l_rcp,fem);
    // INITIALIZE SIMOPT VECTORS
    ROL::Vector_SimOpt<RealT> x(up,zp);
    ROL::Vector_SimOpt<RealT> g(gup,gzp);
    ROL::Vector_SimOpt<RealT> y(yup,yzp);
    /*************************************************************************/
    /************* CHECK DERIVATIVES AND CONSISTENCY *************************/
    /*************************************************************************/
    // CHECK OBJECTIVE DERIVATIVES
    obj.checkGradient(x,g,y,true,*outStream);
    obj.checkHessVec(x,g,y,true,*outStream);
    // CHECK EQUALITY CONSTRAINT DERIVATIVES
    con.checkApplyJacobian(x,y,c,true,*outStream);
    con.checkApplyAdjointHessian(x,*yup,y,g,true,*outStream);
    // CHECK EQUALITY CONSTRAINT CONSISTENCY
    con.checkSolve(*up,*zp,c,true,*outStream);
    con.checkAdjointConsistencyJacobian_1(l,*yup,*up,*zp,true,*outStream);
    con.checkAdjointConsistencyJacobian_2(l,*yzp,*up,*zp,true,*outStream);
    con.checkInverseJacobian_1(c,*yup,*up,*zp,true,*outStream);
    con.checkInverseAdjointJacobian_1(c,*yup,*up,*zp,true,*outStream);
    *outStream << "\n";
    // CHECK PENALTY OBJECTIVE DERIVATIVES
    ROL::MoreauYosidaPenalty<RealT> myPen(obj,bnd,x,10.0);
    myPen.checkGradient(x, y, true, *outStream);
    myPen.checkHessVec(x, g, y, true, *outStream);
    /*************************************************************************/
    /************* RUN OPTIMIZATION ******************************************/
    /*************************************************************************/
    // READ IN XML INPUT
    std::string filename = "input.xml";
    Teuchos::RCP<Teuchos::ParameterList> parlist
      = Teuchos::rcp( new Teuchos::ParameterList() );
    Teuchos::updateParametersFromXmlFile( filename,
      Teuchos::Ptr<Teuchos::ParameterList>(&*parlist) );
    // DEFINE STATUS TEST
    RealT gtol  = parlist->get("Gradient Tolerance",1e-10);
    RealT ctol  = parlist->get("Constraint Tolerance",1e-10);
    RealT stol  = parlist->get("Step Tolerance",1.e-4*gtol);
    int   maxit = parlist->get("Maximum Number of Iterations",1000);
    ROL::StatusTestSQP<RealT> status(gtol, ctol, stol, maxit);    
    // DEFINE STEP
    ROL::MoreauYosidaPenaltyStep<RealT> step(*parlist);
    // DEFINE ALGORITHM
    ROL::DefaultAlgorithm<RealT> algo(step,status,false);
    // INITIALIZE STATE AND LAGRANGE MULTIPLIERS
    RealT zerotol = 0.0;
    con.solve(*up,*zp,zerotol);
    obj.gradient_1(*gup,*up,*zp,zerotol);
    gup->scale(-1.0);
    con.applyInverseAdjointJacobian_1(l,*gup,*up,*zp,zerotol);
    gup->zero();
    c.zero();
    // RUN OPTIMIZATION
    algo.run(x, g, l, c, obj, con, bnd, true, *outStream);
    /*************************************************************************/
    /************* PRINT CONTROL AND STATE TO SCREEN *************************/
    /*************************************************************************/
    for ( int i = 0; i < nx+2; i++ ) {
      *outStream << "\n";
      *outStream << std::scientific << std::setprecision(10);
      *outStream << std::setw(20) << std::left << (*z_rcp)[i];
      if ( i == 0 ) {
        *outStream << std::setw(20) << std::left << 1.0;
      }
      if ( i != 0 && i != nx+1 ) {
        *outStream << std::setw(20) << std::left << (*u_rcp)[i-1];
      }
      if ( i == nx+1 ) {
        *outStream << std::setw(20) << std::left << 0.0;
      }
      *outStream << "\n";
    }
  }
  catch (std::logic_error err) {
    *outStream << err.what() << "\n";
    errorFlag = -1000;
  }; // end try

  if (errorFlag != 0)
    std::cout << "End Result: TEST FAILED\n";
  else
    std::cout << "End Result: TEST PASSED\n";

  return 0;
}
