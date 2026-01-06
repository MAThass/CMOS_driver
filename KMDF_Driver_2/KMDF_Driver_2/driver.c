#include "driver.h"


// registers map, same in user app
const unsigned char CMOS_REGISTERS[] = {
    0x00, // seconds 
    0x02, // minutes
    0x04, // hours
    0x06, // day of week
    0x07, // day of month
    0x08, // month
    0x09, // year
    0x32, // century
    0x0A, // status register A
    0x0B, // status register B
    0x10  // type of floppy drivers 
};

void ReadRawCMOS(unsigned char array[]) {
    for (int i = 0; i < CMOS_REGISTER_NUMBER; i++) {
        unsigned char regIndex = CMOS_REGISTERS[i];

        // send register to port 0x70
        WRITE_PORT_UCHAR((unsigned char*)CMOS_ADDRESS_PORT, regIndex);

        // read data from port 0x71
        array[i] = READ_PORT_UCHAR((unsigned char*)CMOS_DATA_PORT);
    }
}

void ReadCMOSData(unsigned char array[]) {
    unsigned char firstRead[DATA_BUFFER_SIZE];
    unsigned char secondRead[DATA_BUFFER_SIZE];
    int attemps = 3;

    while (attemps > 0) {
        ReadRawCMOS(firstRead);
        ReadRawCMOS(secondRead);

        // check readed values
        if (RtlCompareMemory(firstRead, secondRead, DATA_BUFFER_SIZE) == DATA_BUFFER_SIZE) {
            RtlCopyMemory(array, secondRead, DATA_BUFFER_SIZE);
            return;
        }
        attemps--;
    }

    // if after 3 attemps there are still differences, take the last one
    RtlCopyMemory(array, secondRead, DATA_BUFFER_SIZE);
}


// call every CMOS_TIMER_PERIOD_MS
void CmdfEvtTimerFunc(_In_ WDFTIMER Timer) {
    WDFDEVICE hDevice = WdfTimerGetParentObject(Timer);
    DEVICE_CONTEXT* pointerDeviceContext = GetDeviceContext(hDevice);
    unsigned char newCMOSdata[DATA_BUFFER_SIZE];

    ReadCMOSData(newCMOSdata);

    WdfSpinLockAcquire(pointerDeviceContext->CmosDataLock); // lock data
    RtlCopyMemory(pointerDeviceContext->CmosData, newCMOSdata, DATA_BUFFER_SIZE); // quick copy
    WdfSpinLockRelease(pointerDeviceContext->CmosDataLock); // unlock
}

void CmdfEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,            // handle to comunication queue
    _In_ WDFREQUEST Request,        // request object
    _In_ size_t OutputBufferLength, 
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode        // operation code, IOCTL_CMOS_READ
) {
    UNREFERENCED_PARAMETER(InputBufferLength); // we do not have input
    WDFDEVICE hDevice = WdfIoQueueGetDevice(Queue); // get handle to device
    DEVICE_CONTEXT* pointerDeviceContext = GetDeviceContext(hDevice); // 
    NTSTATUS status = STATUS_SUCCESS;
    size_t outBufferSizeInfo = 0;

    if (IoControlCode == IOCTL_CMOS_READ) {
        if (OutputBufferLength < DATA_BUFFER_SIZE) { // buffer have to accommodate cmos data
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else {
            PVOID outputBuffer;
            size_t outputBufferBytes;

            // request memory access by buffer, 
            status = WdfRequestRetrieveOutputBuffer(Request, DATA_BUFFER_SIZE, &outputBuffer, &outputBufferBytes);
            if (NT_SUCCESS(status)) {
                size_t bytesToCopy;// = (DATA_BUFFER_SIZE < outputBufferBytes) ? DATA_BUFFER_SIZE : outputBufferBytes;
                if (DATA_BUFFER_SIZE < outputBufferBytes)
                    bytesToCopy = DATA_BUFFER_SIZE;
                else
                    bytesToCopy = outputBufferBytes;

                WdfSpinLockAcquire(pointerDeviceContext->CmosDataLock); // lock data
                RtlCopyMemory(outputBuffer, pointerDeviceContext->CmosData, bytesToCopy); // quick copy
                WdfSpinLockRelease(pointerDeviceContext->CmosDataLock); // unlock

                outBufferSizeInfo = bytesToCopy;
            }
        }
    }
    else {
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    WdfRequestSetInformation(Request, outBufferSizeInfo); // inform OS about buffor size in request object
    WdfRequestComplete(Request, status); 
}

NTSTATUS CmdfEvtDeviceAdd(
    _In_ WDFDRIVER Driver,              // handle to driver object
    _Inout_ PWDFDEVICE_INIT DeviceInit  // config struct with initial data
) {
    // not use driver object, markt it as unreferneced
    UNREFERENCED_PARAMETER(Driver);
    NTSTATUS status;
    
    // allocate driver object, atributes, pointer to struct device context, timer and comunication queue 
    WDFDEVICE hDevice;
    WDF_OBJECT_ATTRIBUTES attributes;
    DEVICE_CONTEXT* pointerDeviceContext;
    WDF_TIMER_CONFIG timerConfig;
    WDF_IO_QUEUE_CONFIG queueConfig;

    // add struct DEVICE_CONTEXT to all created devices
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &attributes, &hDevice);
    if (!NT_SUCCESS(status)) return status;

    pointerDeviceContext = GetDeviceContext(hDevice);
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    // my device as parent of spinLock
    attributes.ParentObject = hDevice;

    status = WdfSpinLockCreate(&attributes, &pointerDeviceContext->CmosDataLock);
    if (!NT_SUCCESS(status)) return status;

    // show device with GUID in OS
    status = WdfDeviceCreateDeviceInterface(hDevice, &GUID_DEVINTERFACE_CMOS_READER, NULL);
    if (!NT_SUCCESS(status)) return status;

    // create queue in sequentional mode, one request in time
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    // register function to handle IOCTL
    queueConfig.EvtIoDeviceControl = CmdfEvtIoDeviceControl;
    status = WdfIoQueueCreate(hDevice, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, NULL);
    if (!NT_SUCCESS(status)) return status;

    // timer as device's attribute, every CMOS_TIMER_PERIOD_MS call CmdfEvtTimerFunc
    WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig, CmdfEvtTimerFunc, CMOS_TIMER_PERIOD_MS);
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = hDevice;
    status = WdfTimerCreate(&timerConfig, &attributes, &pointerDeviceContext->Timer);

    if (NT_SUCCESS(status)) {
        // countdown start
        WdfTimerStart(pointerDeviceContext->Timer, WDF_REL_TIMEOUT_IN_MS(CMOS_TIMER_PERIOD_MS));
        // first call CmdfEvtTimerFunc withoun wainting CMOS_TIMER_PERIOD_MS
        CmdfEvtTimerFunc(pointerDeviceContext->Timer);
    }

    return status;
}

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,   // pointer to driver struct
    _In_ PUNICODE_STRING RegistryPath   // pointer to driver key in register
) {
    NTSTATUS status = STATUS_SUCCESS;

    // driver config struct declaration
    WDF_DRIVER_CONFIG config;   

    // initialize driver config struct, register callback function CmdfEvtDeviceAdd
    WDF_DRIVER_CONFIG_INIT(&config, CmdfEvtDeviceAdd);
    
    // create driver 
    status = WdfDriverCreate(
        DriverObject,               // pointer to driver struct from OS
        RegistryPath,               // path from OS
        WDF_NO_OBJECT_ATTRIBUTES,   // no aditional sttributes
        &config,                    // pointer to config struct, witch CmdfEvtDeviceAdd adress
        WDF_NO_HANDLE               // optional handle 
    ); 
    
    // retur STATUS_SUCCESS or STATUS_UNSUCCESSFUL
    return status;
}