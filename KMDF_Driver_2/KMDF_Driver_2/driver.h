#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>


#define CMOS_ADDRESS_PORT    0x70
#define CMOS_DATA_PORT       0x71
#define CMOS_TIMER_PERIOD_MS 15000
#define CMOS_REGISTER_NUMBER 11
#define DATA_BUFFER_SIZE     (CMOS_REGISTER_NUMBER * sizeof(unsigned char))

// {FF961F90-4575-4923-A023-F286E844D640}
DEFINE_GUID(GUID_DEVINTERFACE_CMOS_READER, 0xff961f90, 0x4575, 0x4923, 0xa0, 0x23, 0xf2, 0x86, 0xe8, 0x44, 0xd6, 0x40);

// code IOCTL
//                               device category     function number  comunication method    type of acces 
#define IOCTL_CMOS_READ CTL_CODE(FILE_DEVICE_UNKNOWN, 0xAAA,          METHOD_BUFFERED,       FILE_READ_ACCESS)

typedef struct _DEVICE_CONTEXT {
    WDFTIMER Timer;                             // handle for the system timer
    unsigned char CmosData[DATA_BUFFER_SIZE];   // cmos data buffer
    WDFSPINLOCK CmosDataLock;                   // data lock for synchronization
} DEVICE_CONTEXT;

// macro to create function GetDeviceContext with DEVICE_CONTEXT as Context Type
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)


DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD CmdfEvtDeviceAdd;
EVT_WDF_TIMER CmdfEvtTimerFunc;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL CmdfEvtIoDeviceControl;

void ReadRawCMOS(unsigned char array[]);
void ReadCMOSData(unsigned char array[]);