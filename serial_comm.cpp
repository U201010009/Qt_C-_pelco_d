#include "serial_comm.h"
#include <iostream>
#include <stdio.h>
#include <string.h>

SerialComm::SerialComm()
{
#ifdef WIN32
    m_hCom = 0;
    m_lpszErrorMessage[0] = '\0';
    ZeroMemory(&m_ov, sizeof(m_ov));
#elif __linux__
    m_fd = -1;
#endif
}


SerialComm::~SerialComm() {
    close();
}

int SerialComm::open(char* lpszPortNum,
                     DWORD  dwBaudRate,
                     BYTE   byParity,
                     BYTE   byStopBits,
                     BYTE   byByteSize) {
#ifdef WIN32
    DCB  dcb;   //串口设备控制块 Device Control Block
    BOOL bSuccess;

    memset(&dcb, 0, sizeof(dcb));

    // m_hCom即为函数返回的串口的句柄
    m_hCom = CreateFileA(lpszPortNum,           // pointer to name of the file
                         GENERIC_READ | GENERIC_WRITE, // 允许读写。
                         0,                     // 通讯设备必须以独占方式打开。
                         NULL,                  // 无安全属性，表示该串口不可
                         // 被子程序继承。
                         OPEN_EXISTING,         // 通讯设备已存在。
                         FILE_FLAG_OVERLAPPED,  // 使用异步方式 overlapped I/O。
                         NULL);                 // 通讯设备不能用模板打开。
    if (m_hCom == INVALID_HANDLE_VALUE) {
        std::cout << "CreateFileA "<< lpszPortNum << " error! errno = " << GetLastError() << std::endl;
        return FALSE;
    }

    // 与串口相关的参数非常多，当需要设置串口参数时，通常是先取得串口
    // 的参数结构，修改部分参数后再将参数结构写入
    bSuccess = GetCommState(m_hCom, &dcb);
    if (!bSuccess) {
        SerialComm::close();
        std::cout << "GetCommState error! errno = " << GetLastError() << std::endl;
        return FALSE;
    }
    std::cout << "Get CommStatus : BaudRate = " << dcb.BaudRate << ", Parity = " << dcb.Parity << ", ByteSize = " << dcb.ByteSize << ", StopBits = " << dcb.StopBits << std::endl;

    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = dwBaudRate;   // 串口波特率。
    dcb.Parity = byParity;     // 校验方式，值0~4分别对应无校验、奇
    // 校验、偶校验、校验、置位、校验清零。
    dcb.fParity = 0;             // 为1的话激活奇偶校验检查。
    dcb.fNull = false;
    dcb.ByteSize = byByteSize;   // 一个字节的数据位个数，范围是5~8。
    dcb.StopBits = byStopBits;   // 停止位个数，0~2分别对应1位、1.5位、
    // 2位停止位。

    if (!SetCommState(m_hCom, &dcb))
    {
        SerialComm::close();
        std::cout << "SetCommState error! errno = " << GetLastError() << std::endl;
        return FALSE;
    }

    if (!PurgeComm(m_hCom, PURGE_TXCLEAR | PURGE_RXCLEAR))
    {
        SerialComm::close();
        std::cout << "PurgeComm error! errno = " << GetLastError() << std::endl;
        return FALSE;
    }

    return TRUE;
#elif __linux__
    m_fd = ::open(lpszPortNum, O_RDWR | O_NOCTTY | O_NDELAY);
    if(-1 == m_fd)
    {
        LOG_ERROR << "open " << lpszPortNum << " error!";
        return false;
    }
    return set_opt(dwBaudRate, byParity, byStopBits, byByteSize);
#endif
    return true;
}


DWORD SerialComm::output(LPCVOID pdata, DWORD   len) {
#ifdef WIN32
    BOOL  bSuccess;
    DWORD written = 0;

    if (len < 1)
        return 0;
    //// create event for overlapped I/O
    //m_ov.hEvent = CreateEventA(NULL,   // pointer to security attributes
    //                           FALSE,  // flag for manual-reset event
    //                           FALSE,  // flag for initial state
    //                           "");    // pointer to event-object name
    //if (m_ov.hEvent == INVALID_HANDLE_VALUE) {
    //    SerialComm::ErrorToString("RS232::output()   CreateEvent() failed");
    //    return -1;
    //}

    bSuccess = WriteFile(m_hCom,   // handle to file to write to
                         pdata,    // pointer to data to write to file
                         len,      // number of bytes to write
                         &written, // pointer to number of bytes written
                         &m_ov);  // pointer to structure needed for overlapped I/O

    if (!bSuccess)
    {
        char buff[64] = { 0 };
        std::cout << "WriteFile error! errno = " << GetLastError() << std::endl;
        return -1;
    }

    //// 如果函数执行成功的话检查written的值为写入的字节数，WriteFile函数执行完毕后
    //// 自行填充的，利用此变量的填充值可以用来检查该函数是否将所有的数据成功写入串口
    //if (SerialComm::IsNT()) {
    //    bSuccess = GetOverlappedResult(m_hCom, &m_ov, &written, TRUE);
    //    if (!bSuccess) {
    //        CloseHandle(m_ov.hEvent);
    //        SerialComm::ErrorToString("RS232::output()   GetOverlappedResult() failed");
    //        return -1;
    //    }
    //}
    //else if (len != written) {
    //    CloseHandle(m_ov.hEvent);
    //    SerialComm::ErrorToString("RS232::output()   WriteFile() failed");
    //    return -1;
    //}
    //CloseHandle(m_ov.hEvent);
    return written;
#elif __linux__
    if(-1 == m_fd)
    {
        return -1;
    }

    int ret = write(m_fd, pdata, len);
    return ret;

#endif
}




void SerialComm::close(void) {
#ifdef WIN32
    if (m_hCom > 0)
        CloseHandle(m_hCom);
    m_hCom = 0;
#elif __linux__
    if(m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
#endif
}


bool SerialComm::isAvailable()
{
#ifdef WIN32
    return (m_hCom != INVALID_HANDLE_VALUE) && (m_hCom != NULL);
#elif __linux__
    return (m_fd >= 3);
#endif
}


#ifdef WIN32
DWORD SerialComm::input(LPVOID pdest, DWORD  len, DWORD  dwMaxWait) {
    BOOL  bSuccess;
    DWORD result = 0,
            read = 0, // num read bytes
            mask = 0; // a 32-bit variable that receives a mask
    // indicating the type of event that occurred
    if (len < 1)
        return(0);
    // create event for overlapped I/O
    m_ov.hEvent = CreateEventA(NULL,    // pointer to security attributes
                               FALSE,   // flag for manual-reset event
                               FALSE,   // flag for initial state
                               "");    // pointer to event-object name
    if (m_ov.hEvent == INVALID_HANDLE_VALUE) {
        SerialComm::ErrorToString("RS232::input()   CreateEvent() failed");
        return -1;
    }
    // Specify here the event to be enabled
    bSuccess = SetCommMask(m_hCom, EV_RXCHAR);
    if (!bSuccess) {
        CloseHandle(m_ov.hEvent);
        SerialComm::ErrorToString("RS232::input()   SetCommMask() failed");
        return -1;
    }
    // WaitForSingleObject
    bSuccess = WaitCommEvent(m_hCom, &mask, &m_ov);
    if (!bSuccess) {
        int err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            result = WaitForSingleObject(m_ov.hEvent, dwMaxWait);  //wait dwMaxWait
            // milli seconds before returning
            if (result == WAIT_FAILED) {
                CloseHandle(m_ov.hEvent);
                SerialComm::ErrorToString("RS232::input()   WaitForSingleObject() failed");
                return -1;
            }
        }
    }
    // The specified event occured?
    if (mask & EV_RXCHAR)
    {
        bSuccess = ReadFile(m_hCom, // handle of file to read
                            pdest,  // address of buffer that receives data
                            len,    // number of bytes to read
                            &read,  // address of number of bytes read
                            &m_ov); // address of structure for data
        if (SerialComm::IsNT()) {
            bSuccess = GetOverlappedResult(m_hCom, &m_ov, &read, TRUE);
            if (!bSuccess) {
                CloseHandle(m_ov.hEvent);
                SerialComm::ErrorToString("RS232::input()   GetOverlappedResult() failed");
                return -1;
            }
        }
        else if (!bSuccess) {
            CloseHandle(m_ov.hEvent);
            SerialComm::ErrorToString("RS232::input()   ReadFile() failed");
            return -1;
        }
    }
    else {
        CloseHandle(m_ov.hEvent);
        //        wsprintfA(m_lpszErrorMessage, "RS232::input()   No EV_RXCHAR occured\n");
        //std::cout << "No EV_RXCHAR occured" << std::endl;
        return -1;
    }
    CloseHandle(m_ov.hEvent);
    return read;
}

VOID SerialComm::ErrorToString(char* lpszMessage) {
    //    LPVOID lpMessageBuffer;
    //    DWORD  error = GetLastError();

    //    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
    //        FORMAT_MESSAGE_FROM_SYSTEM,      // source and processing options
    //        NULL,                            // pointer to message source
    //        error,                           // requested message identifie
    //        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // the user default language.
    //        (LPTSTR)&lpMessageBuffer,     // pointer to message buffer
    //        0,                               // maximum size of message buffer
    //        NULL);                           // address of array of message inserts

    //                                         // and copy it in our error string
    //    wsprintfA(m_lpszErrorMessage, "%s: (%d) %s\n", lpszMessage, error, lpMessageBuffer);

    //    LocalFree(lpMessageBuffer);
    //std::cout << lpszMessage << std::endl;
}

BOOL SerialComm::IsNT(void) {
    OSVERSIONINFO osvi;

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);
    if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
        return TRUE;
    else
        return FALSE;
}

char* SerialComm::GetErrorMessage(void) {
    return m_lpszErrorMessage;
}

#elif __linux__
int SerialComm::set_opt(DWORD  dwBaudRate,  // 波特率
                        BYTE   byParity,  // 奇偶校验
                        BYTE   byStopBits,// 停止位个数
                        BYTE   byByteSize/* = 8*/)
{
    if(-1 == m_fd)
    {
        return false;
    }

    struct termios newtio,oldtio;
    if  ( tcgetattr( m_fd,&oldtio)  !=  0)
    {
        perror("SetupSerial 1");
        return -1;
    }
    bzero( &newtio, sizeof( newtio ) );
    newtio.c_cflag  |=  CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;

    switch( byByteSize )
    {
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    }

    switch( byParity )
    {
    case 'O':                     //奇校验
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        newtio.c_iflag |= (INPCK | ISTRIP);
        break;
    case 'E':                     //偶校验
        newtio.c_iflag |= (INPCK | ISTRIP);
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        break;
    case 'N':                    //无校验
        newtio.c_cflag &= ~PARENB;
        break;
    }

    switch( dwBaudRate )
    {
    case 2400:
        cfsetispeed(&newtio, B2400);
        cfsetospeed(&newtio, B2400);
        break;
    case 4800:
        cfsetispeed(&newtio, B4800);
        cfsetospeed(&newtio, B4800);
        break;
    case 9600:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    case 115200:
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    default:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    }
    if( byStopBits == 1 )
    {
        newtio.c_cflag &=  ~CSTOPB;
    }
    else if ( byStopBits == 2 )
    {
        newtio.c_cflag |=  CSTOPB;
    }
    newtio.c_cc[VTIME]  = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(m_fd,TCIFLUSH);
    if((tcsetattr(m_fd,TCSANOW,&newtio))!=0)
    {
        perror("com set error");
        return false;
    }
    printf("set done!\n");
    return true;
}
#endif
