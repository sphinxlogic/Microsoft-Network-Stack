/******************************************************************
   Copyright (c) 1999 Microsoft Corporation

   NlbsNic.H -- WMI provider class definition

   Generated by Microsoft WBEM Code Generation Engine
 
   History: Mar to Nov 2001 JosephJ re-implemented from scratch

*******************************************************************/

// Property set identification
//============================

#ifndef _NlbsNic_H_
#define _NlbsNic_H_

#define PROVIDER_NAME_NLBSNIC L"NlbsNic"

using namespace std;

class CNlbsNic : public Provider 
{
    public:
    

        // Constructor/destructor
        //=======================

        CNlbsNic(LPCWSTR lpwszClassName, LPCWSTR lpwszNameSpace);
        virtual ~CNlbsNic();

        BOOL DelayedInitialize(VOID);
        VOID DelayedDeinitialize(VOID);


    protected:
        // Reading Functions
        //============================
        virtual HRESULT EnumerateInstances(MethodContext*  pMethodContext, long lFlags = 0L);
        virtual HRESULT GetObject(CInstance* pInstance, long lFlags = 0L);
        virtual HRESULT ExecQuery(MethodContext *pMethodContext, CFrameworkQuery& Query, long lFlags = 0L);

        // Writing Functions
        //============================
        virtual HRESULT PutInstance(const CInstance& Instance, long lFlags = 0L);
        virtual HRESULT DeleteInstance(const CInstance& Instance, long lFlags = 0L);

        // Other Functions
        virtual HRESULT ExecMethod( const CInstance& Instance,
                        const BSTR bstrMethodName,
                        CInstance *pInParams,
                        CInstance *pOutParams,
                        long lFlags = 0L );

        // TO DO: Declare any additional functions and accessor
        // functions for private data used by this class
        //===========================================================

private:

    CRITICAL_SECTION    m_Lock;

    VOID mfn_Lock(VOID)     {EnterCriticalSection(&m_Lock);}
    VOID mfn_Unlock(VOID)   {LeaveCriticalSection(&m_Lock);}
    BOOL mfn_IsCallerAdmin(VOID); 

    BOOL m_fDelayedInitializationComplete;

};

extern CNlbsNic MyNlbsNicSet;

#endif