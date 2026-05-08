#include "audio_output_devices.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <Functiondiscoverykeys_devpkey.h>

#include <wrl/client.h>

#pragma comment(lib, "propsys.lib")

using Microsoft::WRL::ComPtr;

std::vector<AudioOutputDeviceInfo> EnumerateAudioRenderDevices()
{
    std::vector<AudioOutputDeviceInfo> list;

    const HRESULT coin = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(coin) && coin != RPC_E_CHANGED_MODE)
        return list;
    const bool coinit_ok = SUCCEEDED(coin);

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        enumerator.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        if (coinit_ok)
            CoUninitialize();
        return list;
    }

    ComPtr<IMMDeviceCollection> collection;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.GetAddressOf());
    if (FAILED(hr)) {
        if (coinit_ok)
            CoUninitialize();
        return list;
    }

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        hr = collection->Item(i, device.GetAddressOf());
        if (FAILED(hr))
            continue;

        LPWSTR idRaw = nullptr;
        if (FAILED(device->GetId(&idRaw)) || !idRaw)
            continue;
        std::wstring id(idRaw);
        CoTaskMemFree(idRaw);

        ComPtr<IPropertyStore> props;
        std::wstring friendly = id;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, props.GetAddressOf()))) {
            PROPVARIANT pv{};
            PropVariantInit(&pv);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal)
                friendly = pv.pwszVal;
            PropVariantClear(&pv);
        }

        list.push_back(AudioOutputDeviceInfo{ std::move(id), std::move(friendly) });
    }

    if (coinit_ok)
        CoUninitialize();
    return list;
}
