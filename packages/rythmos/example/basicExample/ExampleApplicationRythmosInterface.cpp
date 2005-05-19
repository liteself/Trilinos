//
// @HEADER
// ***********************************************************************
// 
//                           Rythmos Package
//                 Copyright (2005) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

//#include "ExampleApplication.hpp"
#include "Thyra_EpetraThyraWrappers.hpp"
#include "ExampleApplicationRythmosInterface.hpp"
//#include "Thyra_EpetraLinearOp.hpp"
//#include "Epetra_Map.h"

//-----------------------------------------------------------------------------
// Function      : ExampleApplicationRythmosInterface::ExampleApplicationRythmosInterface
// Purpose       : constructor
// Special Notes :
// Scope         : public
// Creator       : Todd Coffey, SNL
// Creation Date : 05/17/05
//-----------------------------------------------------------------------------
ExampleApplicationRythmosInterface::ExampleApplicationRythmosInterface(Teuchos::RefCountPtr<const Epetra_Map> &epetra_map)
{
  double lambda = -0.5;
  Teuchos::RefCountPtr<ExampleApplication> problem_ = Teuchos::rcp(new ExampleApplication(lambda));
  Teuchos::RefCountPtr<const Epetra_Map> epetra_map_ = epetra_map;
}

//-----------------------------------------------------------------------------
// Function      : ExampleApplication::~ExampleApplication
// Purpose       : destructor
// Special Notes :
// Scope         : public
// Creator       : Todd Coffey, SNL
// Creation Date : 05/05/05
//-----------------------------------------------------------------------------
ExampleApplicationRythmosInterface::~ExampleApplicationRythmosInterface()
{
}

//-----------------------------------------------------------------------------
// Function      : ExampleApplication::evalModel
// Purpose       : Evaluate residual
// Special Notes :
// Scope         : public
// Creator       : Todd Coffey, SNL
// Creation Date : 05/17/05
//-----------------------------------------------------------------------------
int ExampleApplicationRythmosInterface::evalModel(Teuchos::RefCountPtr<Thyra::VectorBase<double> > &y, Teuchos::RefCountPtr<Thyra::VectorBase<double> > &x, double t)
{
  // 05/18/05 tscoffe:  I get a segfault on the following line:
  Teuchos::RefCountPtr<Epetra_Vector> y_epetra = Thyra::get_Epetra_Vector(*epetra_map_,y);
  Teuchos::RefCountPtr<Epetra_Vector> x_epetra = Thyra::get_Epetra_Vector(*epetra_map_,x);
  (*problem_).evalResidual(&*y_epetra,*x_epetra,t);
//  (*problem_).evalResidual(&*(Thyra::get_Epetra_Vector(*epetra_map_,y)),*(Thyra::get_Epetra_Vector(*epetra_map_,x)),t);
  return 0;
}


