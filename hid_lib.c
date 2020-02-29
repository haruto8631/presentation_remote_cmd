// tiny_hid_dllっぽい何か、時々なぜか失敗する

#include <stdlib.h>
#include <windows.h>
#include <setupapi.h>
#include "hidsdi.h"
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")


HANDLE __declspec(dllexport) __stdcall OpenHidHandle(unsigned short vendor_id, unsigned short product_id);
void __declspec(dllexport) __stdcall ReadReport(HANDLE handle, unsigned char* buffer, int *read_bytes_length);
void __declspec(dllexport) __stdcall WriteReport(HANDLE handle, unsigned char* buffer, int *written_bytes_length);
void __declspec(dllexport) __stdcall CloseHidHandle(HANDLE handle);
NTSTATUS GetHidDeviceCaps(HANDLE hid_device_handle);
void PrepareForOverlappedTransfer(void);

HIDP_CAPS capabilities;


HANDLE  __declspec(dllexport) __stdcall OpenHidHandle(unsigned short vendor_id, unsigned short product_id)
{
    GUID hidGuid;
    HANDLE hid_device_handle;
    HIDD_ATTRIBUTES attributes;
    HDEVINFO hid_device_info = 0;
    SP_DEVICE_INTERFACE_DATA device_info_data;
    PSP_DEVICE_INTERFACE_DETAIL_DATA detail_data = NULL;
    LONG r_value; // result? return? recieved?
    DWORD required_size;
    DWORD detail_data_size = 0;

    HidD_GetHidGuid(&hidGuid);
    hid_device_info = SetupDiGetClassDevs((LPGUID)&hidGuid, NULL, NULL, DIGCF_INTERFACEDEVICE | DIGCF_PRESENT);
    
    if (0 == hid_device_info) return INVALID_HANDLE_VALUE; // デバイスがなかったら終わり

    for (DWORD index = 0; ; index++)
    {
        device_info_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        // デバイスの列挙
        r_value = SetupDiEnumDeviceInterfaces(hid_device_info, NULL, &hidGuid, index, &device_info_data);
        if (!r_value)
        {
           // 列挙できるデバイスがないので
           hid_device_handle = INVALID_HANDLE_VALUE;
           break;
        }

        // 領域の確保
        SetupDiGetDeviceInterfaceDetail(hid_device_info, &device_info_data, NULL, 0, &detail_data_size, NULL);
        detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(detail_data_size);
        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        // 列挙したデバイス個々の詳細を取得
        r_value = SetupDiGetDeviceInterfaceDetail(hid_device_info, &device_info_data, detail_data, detail_data_size, &required_size, NULL);
        
        // ハンドルを確保
        hid_device_handle = CreateFile(detail_data->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, (LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        attributes.Size = sizeof(attributes);

        // 列挙したデバイスの属性を取得
        if (HidD_GetAttributes(hid_device_handle, &attributes))
        {
            if (attributes.VendorID == vendor_id && attributes.ProductID == product_id)
            {
                if (HIDP_STATUS_SUCCESS == GetHidDeviceCaps(hid_device_handle))
                {
                    // 目的のデバイスが使用可能になったので後始末をしてbreak
                    free(detail_data);
                    break;
                }
            }

            // 一旦後始末をして、次のデバイスを探す
            CloseHandle(hid_device_handle);
            free(detail_data);
        }
    }

    // 後始末
    SetupDiDestroyDeviceInfoList(hid_device_info);

    return hid_device_handle;

}



NTSTATUS GetHidDeviceCaps(HANDLE hid_device_handle)
{
    PHIDP_PREPARSED_DATA prepersed_data;
    NTSTATUS status = HIDP_STATUS_INVALID_PREPARSED_DATA;
    if (HidD_GetPreparsedData(hid_device_handle, &prepersed_data))
    {
        status = HidP_GetCaps(prepersed_data, &capabilities);
        HidD_FreePreparsedData(prepersed_data);
    }

    return status;
}


void __declspec(dllexport) __stdcall ReadReport(HANDLE handle, unsigned char* buffer, int *read_bytes_length)
{
    ReadFile(handle, buffer, capabilities.InputReportByteLength, read_bytes_length, NULL);
}


void __declspec(dllexport) __stdcall WriteReport(HANDLE handle, unsigned char* buffer, int *written_bytes_length)
{
    WriteFile(handle, buffer, capabilities.OutputReportByteLength, written_bytes_length, NULL);
}


void __declspec(dllexport) __stdcall CloseHidHandle(HANDLE handle)
{
    CloseHandle(handle);
}