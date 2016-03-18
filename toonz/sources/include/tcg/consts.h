

#ifndef TCG_CONSTS_H
#define TCG_CONSTS_H

/*!
  \file   consts.h

  \brief  This file contains several useful constants to be used with tcg.
*/

//*************************************************************************************
//    TCG  Constants
//*************************************************************************************

namespace tcg
{

//! Contains several useful constants to be used with tcg.
namespace consts
{

const double pi = 3.1415926535897932384626433832795;	  //!< The Pi constant.
const double pi_half = 1.5707963267948966192313216916398; //!< Half of Pi.
const double pi_3half = 3.0 * pi_half;					  //!< Three halves of Pi.
const double pi_twice = 2.0 * pi;						  //!< Twice Pi.

const double rad_to_deg = 180.0 / pi; //!< Radians to degrees factor.
const double deg_to_rad = pi / 180.0; //!< Degrees to radians factor.

const double sqrt2 = 1.4142135623730950488016887242097;		 //!< Square root of 2.
const double sqrt2_half = 0.7071067811865475244008443621048; //!< Half of the square root of 2.
}
} // namespace tcg::consts

#endif // TCG_CONSTS_H
