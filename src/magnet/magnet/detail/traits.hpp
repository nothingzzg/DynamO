/*    DYNAMO:- Event driven molecular dynamics simulator 
 *    http://www.marcusbannerman.co.uk/dynamo
 *    Copyright (C) 2009  Marcus N Campbell Bannerman <m.bannerman@gmail.com>
 *
 *    This program is free software: you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    version 3 as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

//Simple macro to convert a token to a string
#define STRINGIFY(A) #A

//This is for defining scalar types in CL_TYPE_FACTORY
#define SCALAR_TYPE(F,kernel_type)		\
  F(kernel_type,cl_##kernel_type,1,cl_##kernel_type)

//This is for defining vector types in CL_TYPE_FACTORY
#define VEC_TYPE(F,kernel_type)						\
  F(kernel_type, cl_##kernel_type, 1, cl_##kernel_type)			\
  F(kernel_type##2, cl_##kernel_type##2, 2, cl_##kernel_type)		\
  F(kernel_type##4, cl_##kernel_type##4, 4, cl_##kernel_type)		\
  F(kernel_type##8, cl_##kernel_type##8, 8, cl_##kernel_type)		\
  F(kernel_type##16, cl_##kernel_type##16, 16, cl_##kernel_type)
  

//This macro factory lets us generate type traits in an easy manner
//The format for the passed macro function F is 
//F(kernel_type, host_type, tensor_order, basetype)
#define CL_TYPE_FACTORY(F)			\
  VEC_TYPE(F,char)				\
  VEC_TYPE(F,uchar)				\
  VEC_TYPE(F,short)				\
  VEC_TYPE(F,ushort)				\
  VEC_TYPE(F,int)				\
  VEC_TYPE(F,uint)				\
  VEC_TYPE(F,long)				\
  VEC_TYPE(F,ulong)				\
  VEC_TYPE(F,float)				\
  VEC_TYPE(F,double)				\
//  VEC_TYPE(F,half)

namespace magnet {
  namespace detail {
    template<class T>
    struct traits
    {
      static const bool isCLType = false;
    };
    
    //We use the CL_TYPE_FACTORY to generate our type traits
#define TRAIT_FACTORY(cl_type,hosttype,tensororder,basetype)	\
    template<> struct traits<hosttype>				\
    {								\
      static const bool is_CL_type = true;			\
      static const int  tensor_order = tensororder;		\
      static inline std::string kernel_type()			\
      { return STRINGIFY(cl_type); }				\
    };
    
    //Call the factory
    CL_TYPE_FACTORY(TRAIT_FACTORY)
    
#undef TRAIT_FACTORY
  }
}

#undef CL_TYPE_FACTORY
#undef STRINGIFY
