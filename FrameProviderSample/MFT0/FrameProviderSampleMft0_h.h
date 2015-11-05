

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.00.0613 */
/* at Mon Jan 18 19:14:07 2038
 */
/* Compiler settings for FrameProviderSampleMft0.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.00.0613 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __FrameProviderSampleMft0_h_h__
#define __FrameProviderSampleMft0_h_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IFrameProviderMft0_FWD_DEFINED__
#define __IFrameProviderMft0_FWD_DEFINED__
typedef interface IFrameProviderMft0 IFrameProviderMft0;

#endif 	/* __IFrameProviderMft0_FWD_DEFINED__ */


#ifndef __FrameProviderMft0_FWD_DEFINED__
#define __FrameProviderMft0_FWD_DEFINED__

#ifdef __cplusplus
typedef class FrameProviderMft0 FrameProviderMft0;
#else
typedef struct FrameProviderMft0 FrameProviderMft0;
#endif /* __cplusplus */

#endif 	/* __FrameProviderMft0_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "Inspectable.h"
#include "mftransform.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IFrameProviderMft0_INTERFACE_DEFINED__
#define __IFrameProviderMft0_INTERFACE_DEFINED__

/* interface IFrameProviderMft0 */
/* [unique][nonextensible][oleautomation][uuid][object] */ 


EXTERN_C const IID IID_IFrameProviderMft0;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("805222A9-54D6-4D89-B581-CB074097FE01")
    IFrameProviderMft0 : public IUnknown
    {
    public:
    };
    
    
#else 	/* C style interface */

    typedef struct IFrameProviderMft0Vtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFrameProviderMft0 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFrameProviderMft0 * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFrameProviderMft0 * This);
        
        END_INTERFACE
    } IFrameProviderMft0Vtbl;

    interface IFrameProviderMft0
    {
        CONST_VTBL struct IFrameProviderMft0Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFrameProviderMft0_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFrameProviderMft0_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFrameProviderMft0_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFrameProviderMft0_INTERFACE_DEFINED__ */



#ifndef __FrameProviderSampleMft0Lib_LIBRARY_DEFINED__
#define __FrameProviderSampleMft0Lib_LIBRARY_DEFINED__

/* library FrameProviderSampleMft0Lib */
/* [version][uuid] */ 


EXTERN_C const IID LIBID_FrameProviderSampleMft0Lib;

EXTERN_C const CLSID CLSID_FrameProviderMft0;

#ifdef __cplusplus

class DECLSPEC_UUID("DDBE4BC1-541F-4D43-A25B-1F23E7AF4505")
FrameProviderMft0;
#endif
#endif /* __FrameProviderSampleMft0Lib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


