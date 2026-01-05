#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <windows.h>
#include <setupapi.h>
#include <strsafe.h>
#include <shlobj.h>
#include <initguid.h>


// {FF961F90-4575-4923-A023-F286E844D640}
DEFINE_GUID(GUID_DEVINTERFACE_CMOS_READER, 0xff961f90, 0x4575, 0x4923, 0xa0, 0x23, 0xf2, 0x86, 0xe8, 0x44, 0xd6, 0x40);

// code IOCTL same in driver
//                               device category     function number  comunication method    type of acces 
#define IOCTL_CMOS_READ CTL_CODE(FILE_DEVICE_UNKNOWN, 0xAAA,         METHOD_BUFFERED,       FILE_READ_ACCESS)

#define DATA_BUFFER_SIZE 11 

// register map, same order in driver
const char* CMOS_REGISTER_NAMES[] = {
    "0x00: Seconds",
    "0x02: Minutes",
    "0x04: Hours",
    "0x06: Weekday",
    "0x07: Day of Month",
    "0x08: Month",
    "0x09: Year",
    "0x32: Century",
    "0x0A: Status Register A",
    "0x0B: Status Register B",
    "0x10: floppy dricers type"
};



/**
 * @brief Znajduje ścieżkę do interfejsu urządzenia sterownika za pomocą GUID.
 * @param DevicePath Bufor do przechowywania ścieżki (max. MAX_PATH).
 * @return TRUE jeśli ścieżka została znaleziona, FALSE w przeciwnym razie.
 */
BOOL FindDevicePath(TCHAR* DevicePath) {

    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_CMOS_READER,         // driver's GUID
        NULL,                                   // search all device type
        NULL,                                   // handle dialoge window
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE   // flags, active device | device interface (search active device)
    );
    // INVALID_HANDLE_VALUE - empty handle hDevInfo
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        std::cout << "SetupDiGetClassDevs failed: " << GetLastError() << std::endl;
        return FALSE;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { 0 }; // create empty form 
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA); // set form size

    
    if (SetupDiEnumDeviceInterfaces(
            hDevInfo,                           // devices info list
            NULL,                               // linker to device struct
            &GUID_DEVINTERFACE_CMOS_READER,     // search devices with that GUID
            0,                                  // index of devices list, I have one device with my GUID
            &deviceInterfaceData                // all info about driver go there
        )
    ) {

        DWORD detailSize = 0;
        // get buffor size required to store struct
        SetupDiGetDeviceInterfaceDetail(
            hDevInfo,                   // devices info list
            &deviceInterfaceData,       // linker to device interface
            NULL,                       // data buffer, in first call null
            0,                          // size of data buffer
            &detailSize,                // size required to store struct with data
            NULL                        // linker to device info, optional
        );// function retur false, but set detailSize

        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(detailSize);
        // alloc detailData with right size

        if (!detailData) {
            SetupDiDestroyDeviceInfoList(hDevInfo);
            std::cout << " malloc failed." << std::endl;
            return FALSE;
        }

        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        // second call SetupDiGetDeviceInterfaceDetail, get info about interface 
        if (SetupDiGetDeviceInterfaceDetail(
                hDevInfo,
                &deviceInterfaceData,
                detailData,             // data buffer
                detailSize,             // size of data buffer
                NULL,
                NULL
            )
        ) {
            // safty copy path to buffer
            StringCchCopy(DevicePath, MAX_PATH, detailData->DevicePath);
            free(detailData);

            SetupDiDestroyDeviceInfoList(hDevInfo);
            return TRUE;
        }
        else {
            std::cout << "SetupDiGetDeviceInterfaceDetail failed: " << GetLastError() << std::endl;
        }

        free(detailData);
    }
    else {
        std::cout << "driver not found Error: " << GetLastError() << std::endl;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return FALSE;
}


int main() {
    TCHAR devicePath[MAX_PATH] = { 0 };
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    unsigned char cmosBuffer[DATA_BUFFER_SIZE] = { 0 };
    DWORD bytesReturned = 0;

    // find path to driver
    if (!FindDevicePath(devicePath)) {
        return 1;
    }

    //std::cout << "found path" << std::endl;

    //open handle to driver
    hDevice = CreateFile(
        devicePath,                          // path to driver
        GENERIC_READ | GENERIC_WRITE,        // read from driver and write (IOCTL request)
        FILE_SHARE_READ | FILE_SHARE_WRITE,  // allow others to use driver
        NULL,                                // deafoult security setting
        OPEN_EXISTING,                       // open driver if exist
        FILE_ATTRIBUTE_NORMAL,               // not use special attribute
        NULL                                 // for drivers always null
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cout << "not open driver failed: " << GetLastError() << std::endl;
        return 1;
    }

    // IOCTL request
    BOOL success = DeviceIoControl(
        hDevice,                          // handle to driver
        IOCTL_CMOS_READ,                  // IOCTL code
        NULL, 0,                          // inBuffer and size
        cmosBuffer, DATA_BUFFER_SIZE,     // outBuffer and size
        &bytesReturned,                   // number of returned bytes
        NULL                              // no OVERLAPPED
    );

    // close handle to driver
    CloseHandle(hDevice);

    if (!success) {
        std::cout << "DeviceIoControl failed: " << GetLastError() << std::endl;
        return 1;
    }

    // check data length
    if (bytesReturned != DATA_BUFFER_SIZE) {
        std::cout << "should be" << DATA_BUFFER_SIZE << " bytes but is" << bytesReturned << " bytes" << std::endl;
    }

    // get desktop path
    char desktopPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, desktopPath))) {
        std::string fullPath = std::string(desktopPath) + "\\cmos_data.txt";

        std::ofstream outFile(fullPath);
        if (outFile.is_open()) {
            outFile << "==== CMOS Data ====\n\n";

            // write data
            for (int i = 0; i < DATA_BUFFER_SIZE; i++) {
                outFile << std::setfill(' ') << std::setw(25) << std::left << CMOS_REGISTER_NAMES[i]
                    // values in BCD, regisret B = 0x02
                    << " " << std::hex << (int)cmosBuffer[i] << "\n";
            }

            outFile.close();
        }
        else {
            std::cout << "not open file: " << fullPath << std::endl;
            return 1;
        }
    }
    else {
        std::cout << "not get path" << std::endl;
        return 1;
    }
    return 0;
}
