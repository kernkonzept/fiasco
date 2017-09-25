#pragma once

namespace Tl_math {

/**
 * Calculate the ld of \a v.
 */
template< unsigned v >
struct Ld { enum { Res = Ld<(v+1)/2>::Res + 1 }; };

// undefined (goes to minus infinity)
template<>
struct Ld<0> { };

template<>
struct Ld<1> { enum { Res = 0 }; };
};
