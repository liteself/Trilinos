/*
//@HEADER
// ************************************************************************
//
//   Kokkos: Manycore Performance-Portable Multidimensional Arrays
//              Copyright (2012) Sandia Corporation
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
// Questions? Contact  H. Carter Edwards (hcedwar@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#ifndef KOKKOS_EXAMPLE_CG_SOLVE
#define KOKKOS_EXAMPLE_CG_SOLVE

#include <cmath>
#include <limits>
#include <Kokkos_View.hpp>
#include <Kokkos_CrsMatrix.hpp>
#include <Kokkos_MV.hpp>
#include <impl/Kokkos_Timer.hpp>
#include <Kokkos_ArithTraits.hpp>

#include <Teuchos_CommHelpers.hpp>
#include <Tpetra_CrsMatrix.hpp>
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

namespace Kokkos {
namespace Example {

template <typename Scalar>
inline
Scalar all_reduce( Scalar local , const Teuchos::RCP<const Teuchos::Comm<int> >& comm )
{
  Scalar global = 0 ;
  Teuchos::reduceAll( *comm , Teuchos::REDUCE_SUM , 1 , & local , & global );
  return global ;
}

template <typename Scalar>
inline
Scalar all_reduce_max( Scalar local , const Teuchos::RCP<const Teuchos::Comm<int> >& comm )
{
  Scalar global = 0 ;
  Teuchos::reduceAll( *comm , Teuchos::REDUCE_MAX , 1 , & local , & global );
  return global ;
}

struct result_struct {
  double addtime,dottime,matvectime,norm_res,iter_time;
  int iteration;
  result_struct() :
    addtime(0), dottime(0), matvectime(0), norm_res(0), iter_time(0),
    iteration(0) {}
  result_struct(double add, double dot, double matvec,int niter,double res) :
    addtime(add),dottime(dot),matvectime(matvec),
    norm_res(res),iter_time(add+dot+matvec),iteration(niter) {};
};

template<class CrsMatrix, class Vector>
result_struct cg_solve(
  Teuchos::RCP<CrsMatrix> A, Teuchos::RCP<Vector> b, Teuchos::RCP<Vector> x,
  int max_iter = 200,
  typename CrsMatrix::scalar_type tolerance =
    Kokkos::Details::ArithTraits<typename CrsMatrix::scalar_type>::epsilon(),
  int print = 0)
{
  typedef typename CrsMatrix::scalar_type ScalarType;
  typedef Kokkos::Details::ArithTraits<ScalarType> KAT;
  typedef typename KAT::mag_type MagnitudeType;
  typedef typename CrsMatrix::local_ordinal_type LocalOrdinalType;
  Teuchos::RCP<Vector> r,p,Ap;

  // create temporary Vectors
  r = Tpetra::createVector<ScalarType>(A->getRangeMap());
  p = Tpetra::createVector<ScalarType>(A->getRangeMap());
  Ap = Tpetra::createVector<ScalarType>(A->getRangeMap());

  // fill with initial Values (make this a functor call or something)
  int length = r->getLocalLength();
  for(int i = 0;i<length;i++) {
    x->replaceLocalValue(i,0,0);
    r->replaceLocalValue(i,0,1);
    Ap->replaceLocalValue(i,0,1);
  }

  MagnitudeType normr = 0;
  MagnitudeType rtrans = 0;
  MagnitudeType oldrtrans = 0;

  LocalOrdinalType print_freq = max_iter/10;
  if (print_freq>50) print_freq = 50;
  if (print_freq<1)  print_freq = 1;

  double dottime = 0;
  double addtime = 0;
  double matvectime = 0;

  Kokkos::Impl::Timer timer;
  p->update(1.0,*x,0.0,*x,0.0);
  addtime += timer.seconds(); timer.reset();


  A->apply(*p, *Ap);
  matvectime += timer.seconds(); timer.reset();

  r->update(1.0,*b,-1.0,*Ap,0.0);
  addtime += timer.seconds(); timer.reset();

  //rtrans = r->dot(*r);
  Teuchos::Array<double> dots(1);
  r->dot(*r,dots());
  rtrans = dots[0];
  dottime += timer.seconds(); timer.reset();

  normr = std::sqrt(rtrans);

  MagnitudeType normr_orig = normr;

  if (print) {
    std::cout << "Initial Residual = "<< normr << std::endl;
  }

  MagnitudeType brkdown_tol = KAT::epsilon();

  // Count external so that we keep iteration count in the end
  LocalOrdinalType k;
  for(k=1; k <= max_iter && normr / normr_orig > tolerance; ++k) {
    if (k == 1) {
      p->update(1.0,*r,0.0,*r,0.0);
      addtime += timer.seconds(); timer.reset();
    }
    else {
      oldrtrans = rtrans;
      //rtrans = r->dot(*r);
      r->dot(*r,dots());
      rtrans = dots[0];
      dottime += timer.seconds(); timer.reset();
      MagnitudeType beta = rtrans/oldrtrans;
      p->update(beta,*p,1.0,*r,0.0);
      addtime += timer.seconds(); timer.reset();
    }
    normr = std::sqrt(rtrans);
    if (print && (k%print_freq==0 || k==max_iter)) {
      std::cout << "Iteration = "<<k<<"   Residual = "<<normr<<std::endl;
    }

    MagnitudeType alpha = 0;
    MagnitudeType p_ap_dot = 0;
    A->apply(*p, *Ap);
    matvectime += timer.seconds(); timer.reset();
    //p_ap_dot = Ap->dot(*p);
    Ap->dot(*p,dots());
    p_ap_dot = dots[0];

    dottime += timer.seconds(); timer.reset();

   if (p_ap_dot < brkdown_tol) {
      if (p_ap_dot < 0 ) {
        std::cerr << "cg_solve ERROR, numerical breakdown!"<<std::endl;
        return result_struct(0,0,0,0,0);
      }
      else brkdown_tol = 0.1 * p_ap_dot;
    }
    alpha = rtrans/p_ap_dot;


    x->update(1.0,*x,alpha,*p,0.0);
    r->update(1.0,*r,-alpha,*Ap,0.0);
    addtime += timer.seconds(); timer.reset();

  }
  //rtrans = r->dot(*r);
  r->dot(*r,dots());
  rtrans = dots[0];

  normr = std::sqrt(rtrans);


  return result_struct(addtime,dottime,matvectime,k-1,normr);
}




} // namespace Example
} // namespace Kokkos

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

#endif /* #ifndef KOKKOS_EXAMPLE_CG_SOLVE */
