// *****************************************************************************
// Module..: LeddarTech
//
/// \file    LtIntUtilities.h
///
/// \brief   Utilities for integers
///
/// \author  Patrick Boulay
///
/// \since   March 2016
//
// Copyright (c) 2016 LeddarTech Inc. All rights reserved.
// *****************************************************************************

#pragma once

#include "LtDefines.h"

#include <climits>

namespace LeddarUtils
{
    class LtIntUtilities
    {
    public:
        static bool IsBigEndian( void )
        {
            union {
                uint32_t i;
                char c[ 4 ];
            } bint = { 0x01020304 };

            return bint.c[ 0 ] == 1;
        }


        template <typename T>
        static T SwapEndian( T u )
        {
            union
            {
                T u;
                unsigned char u8[ sizeof( T ) ];
            } source, dest;

            source.u = u;

            for( size_t k = 0; k < sizeof( T ); k++ )
                dest.u8[ k ] = source.u8[ sizeof( T ) - k - 1 ];

            return dest.u;
        }

    };
}