#include "DS_Filter.h"
#include "driver.h"
#include "com.h"
#include <stdio.h>
#include <string.h>

typedef long STDCALL (*GETCLASS) (const GUID*, const GUID*, void**);

//extern "C" STDCALL void* GetProcAddress(int, const char*); // STDCALL has to be first NetBSD

static void DS_Filter_Start(DS_Filter* This)
{
    HRESULT hr;

    if (This->m_iState != 1)
	return;

    //Debug printf("DS_Filter_Start(%p)\n", This);
    hr = This->m_pFilter->vt->Run(This->m_pFilter, 0);
    if (hr != 0)
    {
	Debug printf("WARNING: m_Filter->Run() failed, error code %x\n", (int)hr);
    }
    hr = This->m_pImp->vt->GetAllocator(This->m_pImp, &This->m_pAll);

    if (hr || !This->m_pAll)
    {
	Debug printf("WARNING: error getting IMemAllocator interface %x\n", (int)hr);
	This->m_pImp->vt->Release((IUnknown*)This->m_pImp);
        return;
    }
    This->m_pImp->vt->NotifyAllocator(This->m_pImp, This->m_pAll, 0);
    This->m_iState = 2;
}

static void DS_Filter_Stop(DS_Filter* This)
{
    if (This->m_iState == 2)
    {
	This->m_iState = 1;
	//Debug	printf("DS_Filter_Stop(%p)\n", This);
	if (This->m_pFilter)
	{
	    //printf("vt: %p\n", m_pFilter->vt);
	    //printf("vtstop %p\n", m_pFilter->vt->Stop);
	    This->m_pFilter->vt->Stop(This->m_pFilter); // causes weird crash ??? FIXME
	}
	else
	    printf("WARNING: DS_Filter::Stop() m_pFilter is NULL!\n");
	This->m_pAll->vt->Release((IUnknown*)This->m_pAll);
	This->m_pAll = 0;
    }
}

void DS_Filter_Destroy(DS_Filter* This)
{
    This->Stop(This);

    This->m_iState = 0;

    if (This->m_pOurInput)
	This->m_pOurInput->vt->Release((IUnknown*)This->m_pOurInput);
    if (This->m_pInputPin)
	This->m_pInputPin->vt->Disconnect(This->m_pInputPin);
    if (This->m_pOutputPin)
	This->m_pOutputPin->vt->Disconnect(This->m_pOutputPin);
    if (This->m_pFilter)
	This->m_pFilter->vt->Release((IUnknown*)This->m_pFilter);
    if (This->m_pOutputPin)
	This->m_pOutputPin->vt->Release((IUnknown*)This->m_pOutputPin);
    if (This->m_pInputPin)
	This->m_pInputPin->vt->Release((IUnknown*)This->m_pInputPin);
    if (This->m_pImp)
	This->m_pImp->vt->Release((IUnknown*)This->m_pImp);

    if (This->m_pOurOutput)
	This->m_pOurOutput->vt->Release((IUnknown*)This->m_pOurOutput);
    if (This->m_pParentFilter)
	This->m_pParentFilter->vt->Release((IUnknown*)This->m_pParentFilter);
    if (This->m_pSrcFilter)
	This->m_pSrcFilter->vt->Release((IUnknown*)This->m_pSrcFilter);

    // FIXME - we are still leaving few things allocated!
    if (This->m_iHandle)
	FreeLibrary(This->m_iHandle);

    free(This);

    CodecRelease();
}

DS_Filter* DS_FilterCreate(const char* dllname, const GUID* id,
			   AM_MEDIA_TYPE* in_fmt,
			   AM_MEDIA_TYPE* out_fmt)
{
    DS_Filter* This = (DS_Filter*) malloc(sizeof(DS_Filter));
    if (!This)
	return NULL;

    CodecAlloc();

    This->m_pFilter = NULL;
    This->m_pInputPin = NULL;
    This->m_pOutputPin = NULL;
    This->m_pSrcFilter = NULL;
    This->m_pParentFilter = NULL;
    This->m_pOurInput = NULL;
    This->m_pOurOutput = NULL;
    This->m_pAll = NULL;
    This->m_pImp = NULL;
    This->m_iState = 0;

    This->Start = DS_Filter_Start;
    This->Stop = DS_Filter_Stop;

    for (;;)
    {
	HRESULT result;
	GETCLASS func;
	struct IClassFactory* factory = NULL;
	struct IUnknown* object = NULL;
	IEnumPins* enum_pins = 0;
	IPin* array[256];
	ULONG fetched;
        unsigned int i;

	This->m_iHandle = LoadLibraryA(dllname);
	if (!This->m_iHandle)
	{
	    printf("Could not open DirectShow DLL: %.200s\n", dllname);
	    break;
	}
	func = (GETCLASS)GetProcAddress(This->m_iHandle, "DllGetClassObject");
	if (!func)
	{
	    printf("Illegal or corrupt DirectShow DLL: %.200s\n", dllname);
	    break;
	}
	result = func(id, &IID_IClassFactory, (void**)&factory);
	if (result || !factory)
	{
	    printf("No such class object\n");
	    break;
	}
	result = factory->vt->CreateInstance(factory, 0, &IID_IUnknown, (void**)&object);
	factory->vt->Release((IUnknown*)factory);
	if (result || !object)
	{
	    printf("Class factory failure\n");
	    break;
	}
	result = object->vt->QueryInterface(object, &IID_IBaseFilter, (void**)&This->m_pFilter);
	object->vt->Release((IUnknown*)object);
	if (result || !This->m_pFilter)
	{
	    printf("Object does not have IBaseFilter interface\n");
            break;
	}
	// enumerate pins
	result = This->m_pFilter->vt->EnumPins(This->m_pFilter, &enum_pins);
	if (result || !enum_pins)
	{
	    printf("Could not enumerate pins\n");
            break;
	}

	enum_pins->vt->Reset(enum_pins);
	result = enum_pins->vt->Next(enum_pins, (ULONG)256, (IPin**)array, &fetched);
	Debug printf("Pins enumeration returned %ld pins, error is %x\n", fetched, (int)result);

	for (i = 0; i < fetched; i++)
	{
	    int direction = -1;
	    array[i]->vt->QueryDirection(array[i], (PIN_DIRECTION*)&direction);
	    if (!This->m_pInputPin && direction == 0)
	    {
		This->m_pInputPin = array[i];
		This->m_pInputPin->vt->AddRef((IUnknown*)This->m_pInputPin);
	    }
	    if (!This->m_pOutputPin && direction == 1)
	    {
		This->m_pOutputPin = array[i];
		This->m_pOutputPin->vt->AddRef((IUnknown*)This->m_pOutputPin);
	    }
	    array[i]->vt->Release((IUnknown*)(array[i]));
	}
	if (!This->m_pInputPin)
	{
	    printf("Input pin not found\n");
            break;
	}
	if (!This->m_pOutputPin)
	{
	    printf("Output pin not found\n");
            break;
	}
	result = This->m_pInputPin->vt->QueryInterface((IUnknown*)This->m_pInputPin,
						       &IID_IMemInputPin,
						       (void**)&This->m_pImp);
	if (result)
	{
	    printf("Error getting IMemInputPin interface\n");
	    break;
	}

	This->m_pOurType = in_fmt;
	This->m_pDestType = out_fmt;
        result = This->m_pInputPin->vt->QueryAccept(This->m_pInputPin, This->m_pOurType);
	if (result)
	{
	    printf("Source format is not accepted\n");
            break;
	}
	This->m_pParentFilter = CBaseFilter2Create();
	This->m_pSrcFilter = CBaseFilterCreate(This->m_pOurType, This->m_pParentFilter);
	This->m_pOurInput = This->m_pSrcFilter->GetPin(This->m_pSrcFilter);
	This->m_pOurInput->vt->AddRef((IUnknown*)This->m_pOurInput);

	result = This->m_pInputPin->vt->ReceiveConnection(This->m_pInputPin,
							  This->m_pOurInput,
							  This->m_pOurType);
	if (result)
	{
	    printf("Error connecting to input pin\n");
            break;
	}

	This->m_pOurOutput = COutputPinCreate(This->m_pDestType);

	result = This->m_pOutputPin->vt->ReceiveConnection(This->m_pOutputPin,
							   (IPin*) This->m_pOurOutput,
							   This->m_pDestType);
	if (result)
	{
	    //printf("Tracking ACELP %d  0%x\n", result);
	    printf("Error connecting to output pin\n");
            break;
	}

	printf("Using DirectShow codec: %s\n", dllname);
	This->m_iState = 1;
        break;
    }

    if (This->m_iState != 1)
    {
	DS_Filter_Destroy(This);
        This = 0;
    }
    return This;
}
