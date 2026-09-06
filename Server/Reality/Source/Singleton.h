// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
// Copyright (C) 2006-2010 Rajko Stojadinovic
// http://mxoemu.info
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ---------------------------------------------------------------------------
//
// ***************************************************************************

#ifndef MXOSIM_SINGLETON_H
#define MXOSIM_SINGLETON_H

#include "Errors.h"
#include <typeinfo>
#include <iostream>
#include "StackWalker.h"

/// Should be placed in the appropriate .cpp file somewhere
#define initialiseSingleton( type ) \
template <> type * Singleton < type > :: mSingleton = 0

/// To be used as a replacement for initialiseSingleton( )
///  Creates a file-scoped Singleton object, to be retrieved with getSingleton
#define createFileSingleton( type ) \
initialiseSingleton( type ); \
type the##type

template < class type > class Singleton
{
    public:
        /// Constructor
        Singleton( )
        {
            /// If you hit this assert, this singleton already exists -- you can't create another one!
            WPAssert( mSingleton == 0 );
            mSingleton = static_cast<type *>(this);
        }
        /// Destructor
        ~Singleton( )
        {
            mSingleton = 0;
        }

        static type & getSingleton( ) { 
            if ( !mSingleton ) {
                std::cerr << "Singleton missing for: " << typeid(type).name() << std::endl;
                #ifdef _WIN32
                class MyStackWalker : public StackWalker {
                public:
                    MyStackWalker() : StackWalker() {}
                protected:
                    virtual void OnOutput(LPCSTR szText) { std::cerr << szText; }
                };
                MyStackWalker sw;
                sw.ShowCallstack();
                #endif
                exit(1);
            }
            return *mSingleton;
        }

        /// Retrieve a pointer to the singleton object
        static type * getSingletonPtr( ) { return mSingleton; }

    protected:

        /// Singleton pointer, must be set to 0 prior to creating the object
        static type * mSingleton;
};
#endif