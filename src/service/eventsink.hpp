#ifndef SERVICE_EVENTSINK_HPP
#define SERVICE_EVENTSINK_HPP

#include <Wbemidl.h>
#include <Windows.h>
#include <comdef.h>

// https://docs.microsoft.com/en-us/windows/win32/wmisdk/example--receiving-event-notifications-through-wmi-
class EventSink : public IWbemObjectSink {
  LONG m_lRef;
  bool bDone;

 public:
  EventSink() { m_lRef = 0; }
  ~EventSink() { bDone = true; }

  virtual ULONG STDMETHODCALLTYPE AddRef();
  virtual ULONG STDMETHODCALLTYPE Release();
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

  virtual HRESULT STDMETHODCALLTYPE Indicate(
      LONG lObjectCount, IWbemClassObject __RPC_FAR* __RPC_FAR* apObjArray);

  virtual HRESULT STDMETHODCALLTYPE
  SetStatus(LONG lFlags, HRESULT hResult, BSTR strParam,
            IWbemClassObject __RPC_FAR* pObjParam);
};

#endif /* SERVICE_EVENTSINK_HPP */
