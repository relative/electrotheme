#include "eventsink.hpp"
#include "../config.hpp"
#include "../log.hpp"
#include "loader.hpp"
#include "service.hpp"

ULONG EventSink::AddRef() {
  return InterlockedIncrement(&m_lRef);
}

ULONG EventSink::Release() {
  LONG lRef = InterlockedDecrement(&m_lRef);
  if(lRef == 0)
    delete this;
  return lRef;
}

HRESULT EventSink::QueryInterface(REFIID riid, void** ppv) {
  if (riid == IID_IUnknown || riid == IID_IWbemObjectSink) {
    *ppv = (IWbemObjectSink *) this;
    AddRef();
    return WBEM_S_NO_ERROR;
  }
  else return E_NOINTERFACE;
}

HRESULT EventSink::Indicate(long lObjectCount, IWbemClassObject **apObjArray) {
  HRESULT hr;

  _variant_t var;
  _variant_t varT;
  IWbemClassObject* apObj;
  for (int i = 0; i < lObjectCount; i++) {
    apObj = apObjArray[i];
    hr = apObj->Get(_bstr_t(L"TargetInstance"), 0, &var, nullptr, nullptr);
    if (FAILED(hr)) {
      continue;
    }
    IUnknown* proc = var;

    hr = proc->QueryInterface(IID_IWbemClassObject, reinterpret_cast<void**>(&apObj));
    if (FAILED(hr)) {
      VariantClear(&var);
      continue;
    }
    
    uint32_t procId;
    BSTR nm;

    apObj->Get(_bstr_t(L"ProcessId"), 0, &varT, nullptr, nullptr);
    procId = varT.uintVal;
    VariantClear(&varT);

    apObj->Get(_bstr_t(L"Name"), 0, &varT, nullptr, nullptr);
    nm = varT.bstrVal;
    VariantClear(&varT);

    char* chExeName = _com_util::ConvertBSTRToString(nm);

    if (gConfig->watchedExecutables.find(chExeName) == gConfig->watchedExecutables.end()) {
      // we aren't watching this process
      delete[] chExeName;
      proc->Release();
      continue;
    }
    auto app = gConfig->get_application_by_executable(chExeName);

    gService->loader->enqueue({
      .processId = procId,
      .executableName = std::string(chExeName),
      .removeCSP = app->removeCSP
    });

    delete[] chExeName; // ConvertBSTRToString allocs new string

    proc->Release();
    VariantClear(&var);
  }

  return WBEM_S_NO_ERROR;
}

HRESULT EventSink::SetStatus(LONG lFlags, HRESULT hResult, BSTR strParam, IWbemClassObject __RPC_FAR *pObjParam) {
  return WBEM_S_NO_ERROR;
}