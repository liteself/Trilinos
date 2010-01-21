#ifndef __Teko_ImplicitLinearOp_hpp__
#define __Teko_ImplicitLinearOp_hpp__

#include "Teko_Utilities.hpp"

namespace Teko {

/** \brief A virtual class that simplifies the construction
  *        of custom operators. 
  *
  * A virtual class that simplifies the construction
  * of custom operators. 
  */
class ImplicitLinearOp : public Thyra::LinearOpBase<double> {
public:

   /** @brief Range space of this operator */
   virtual VectorSpace range() const = 0;

   /** @brief Domain space of this operator */
   virtual VectorSpace domain() const = 0;

   /** @brief Perform a matrix vector multiply with this implicitly
     * defined blocked operator. 
     *
     * The <code>apply</code> function takes one vector as input 
     * and applies a linear operator. The result
     * is returned in \f$y\f$. If this operator is reprsented as \f$M\f$ then
     * \f$ y = \alpha M x + \beta y \f$
     *
     * @param[in]     x 
     * @param[in,out] y 
     * @param[in]     alpha (default=1)
     * @param[in]     beta  (default=0)
     */
   virtual void implicitApply(const MultiVector & x, MultiVector & y,
              const double alpha = 1.0, const double beta = 0.0) const = 0;

   //! Functions required by Thyra::LinearOpBase 
   //@{ 

   virtual void apply(const Thyra::EConj conj, const Thyra::MultiVectorBase<double> & x, Thyra::MultiVectorBase<double> * y,
                      const double alpha = Teuchos::ScalarTraits<double>::one(),
                      const double beta = Teuchos::ScalarTraits<double>::zero()) const;
 
   //@}
};

} // end namespace Teko

#endif
