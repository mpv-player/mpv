#include <stdio.h>
#include <string.h>
#include "DS_Filter.h"
#include <except.h>
//#include "../loader/loader.h"
#include <string>
#define __MODULE__ "DirectShow generic filter"

using namespace std;

typedef long STDCALL (*GETCLASS) (const GUID*, const GUID*, void**);
extern "C" char* def_path;

extern "C" int STDCALL LoadLibraryA(const char*);
extern "C" STDCALL void* GetProcAddress(int, const char*);
extern "C" int STDCALL FreeLibrary(int);

DS_Filter::DS_Filter()
    :m_iHandle(0), m_pFilter(0), m_pInputPin(0),
	m_pOutputPin(0), m_pSrcFilter(0), 
	m_pOurInput(0), m_pOurOutput(0),
	m_pImp(0), m_pAll(0), m_pParentFilter(0)
{
}

void DS_Filter::Create(string dllname, const GUID* id, AM_MEDIA_TYPE* in_fmt, AM_MEDIA_TYPE* out_fmt)
{
    try
    {
	string _fullname=def_path;
	_fullname+="/";
	_fullname+=dllname;
	m_iHandle= LoadLibraryA(_fullname.c_str());
	if(!m_iHandle)throw FATAL("Could not open DLL");
        GETCLASS func=(GETCLASS)GetProcAddress(m_iHandle, "DllGetClassObject");
	if(!func)throw FATAL("Illegal or corrupt DLL");

	HRESULT result;
	IClassFactory* factory=0;
	IUnknown* object=0;

	result=func(id, &IID_IClassFactory, (void**)&factory);
	if(result || (!factory)) throw FATAL("No such class object");;
        
            printf("# factory = %X\n",(unsigned int)factory);
            printf("# factory->vt = %X\n",(unsigned int)factory->vt);
            printf("# factory->vt->CreateInstance = %X\n",(unsigned int)factory->vt->CreateInstance);

            printf("Calling factory->vt->CreateInstance()\n");
            result=factory->vt->CreateInstance(factory, 0, &IID_IUnknown, (void**)&object);
            printf("Calling factory->vt->Release()\n");
	
//	result=factory->vt->CreateInstance(factory, 0, &IID_IUnknown, (void**)&object);

        printf("CreateInstance ok %x\n",result);

	factory->vt->Release((IUnknown*)factory);
	if(result || (!object)) throw FATAL("Class factory failure");
	    
	result=object->vt->QueryInterface(object, &IID_IBaseFilter, (void**)&m_pFilter);
	object->vt->Release((IUnknown*)object);
	if(result || (!m_pFilter)) throw FATAL("Object does not have IBaseFilter interface");
	
	IEnumPins* enum_pins=0;
    //	enumerate pins
	result=m_pFilter->vt->EnumPins(m_pFilter, &enum_pins);
	if(result || (!enum_pins)) throw FATAL("Could not enumerate pins");
	IPin* array[256];
	ULONG fetched;
	enum_pins->vt->Reset(enum_pins);
	result=enum_pins->vt->Next(enum_pins, (ULONG)256, (IPin**)array, &fetched);
	printf("Pins enumeration returned %d pins, error is %x\n", fetched, result);
	
	for(int i=0; i<fetched; i++)
	{
	    int direction=-1;
	    array[i]->vt->QueryDirection(array[i], (PIN_DIRECTION*)&direction);
	    if((!m_pInputPin)&&(direction==0))
	    {
		m_pInputPin=array[i];
		m_pInputPin->vt->AddRef((IUnknown*)m_pInputPin);
	    }
	    if((!m_pOutputPin)&&(direction==1))
	    {
		m_pOutputPin=array[i];
		m_pOutputPin->vt->AddRef((IUnknown*)m_pOutputPin);
	    }
	    array[i]->vt->Release((IUnknown*)(array[i]));
	}
	if(!m_pInputPin)throw FATAL("Input pin not found");
	if(!m_pOutputPin)throw FATAL("Output pin not found");

	result=m_pInputPin->vt->QueryInterface((IUnknown*)m_pInputPin, &IID_IMemInputPin, (void**)&m_pImp);
        if(result)
	    throw FATAL("Error getting IMemInputPin interface");
	m_pOurType=in_fmt;
	m_pDestType=out_fmt;
        result=m_pInputPin->vt->QueryAccept(m_pInputPin, m_pOurType);
	if(result) throw FATAL("Source format is not accepted");

	m_pParentFilter=new CBaseFilter2;
        m_pSrcFilter=new CBaseFilter(*m_pOurType, m_pParentFilter);
	m_pOurInput=m_pSrcFilter->GetPin();
	m_pOurInput->vt->AddRef((IUnknown*)m_pOurInput);
	
    	result=m_pInputPin->vt->ReceiveConnection(m_pInputPin, m_pOurInput, m_pOurType);
	if(result) throw FATAL("Error connecting to input pin");
	
	m_pOurOutput=new COutputPin(*m_pDestType);
        result=m_pOutputPin->vt->ReceiveConnection(m_pOutputPin,
	     m_pOurOutput, m_pDestType);
	if(result)throw FATAL("Error connecting to output pin");    
	m_iState=1;
    }
    catch(FatalError e)
    {
	e.PrintAll();
    	if(m_pFilter)m_pFilter->vt->Release((IUnknown*)m_pFilter);
	if(m_pOutputPin)m_pOutputPin->vt->Release((IUnknown*)m_pOutputPin);
	if(m_pInputPin)m_pInputPin->vt->Release((IUnknown*)m_pInputPin);
	if(m_pImp)m_pImp->vt->Release((IUnknown*)m_pImp);
	if(m_pOurInput)m_pOurInput->vt->Release((IUnknown*)m_pOurInput);
	delete m_pSrcFilter;
	delete m_pParentFilter;
	delete m_pOurOutput;
	if(m_iHandle)FreeLibrary(m_iHandle);
	throw;
    }
}
void DS_Filter::Start()
{
    if(m_iState!=1)
	return;
    
    HRESULT hr=m_pFilter->vt->Run(m_pFilter, 0);
    if(hr!=0)
    {
	cerr<<"WARNING: m_Filter->Run() failed, error code "<<hex<<hr<<dec<<endl;
    }
    hr=m_pImp->vt->GetAllocator(m_pImp, &m_pAll);
    if(hr)
    {
        cerr<<"Error getting IMemAllocator interface "<<hex<<hr<<dec<<endl;
        m_pImp->vt->Release((IUnknown*)m_pImp);
        return;
    }
    m_pImp->vt->NotifyAllocator(m_pImp, m_pAll,	0);
    m_iState=2;
    return;
}
void DS_Filter::Stop()
{
    if(m_iState!=2)
	return;
    m_pAll->vt->Release((IUnknown*)m_pAll);
    m_pAll=0;
    m_pFilter->vt->Stop(m_pFilter);
    m_iState=1;
    return;
}
DS_Filter::~DS_Filter()
{
    if(m_iState==0)
	return;
    if(m_iState==2)Stop();
    if(m_pInputPin)m_pInputPin->vt->Disconnect(m_pInputPin);
    if(m_pOutputPin)m_pOutputPin->vt->Disconnect(m_pOutputPin);
    if(m_pFilter)m_pFilter->vt->Release((IUnknown*)m_pFilter);
    if(m_pOutputPin)m_pOutputPin->vt->Release((IUnknown*)m_pOutputPin);
    if(m_pInputPin)m_pInputPin->vt->Release((IUnknown*)m_pInputPin);
    if(m_pOurInput)m_pOurInput->vt->Release((IUnknown*)m_pOurInput);
    if(m_pImp)m_pImp->vt->Release((IUnknown*)m_pImp);
    delete m_pSrcFilter;
    delete m_pParentFilter;
    delete m_pOurOutput;
    if(m_iHandle)FreeLibrary(m_iHandle);
}
