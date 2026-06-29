#include "serial_port.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <iostream>

namespace
{
constexpr speed_t kUartBaudRate = B115200;
}

int uart_init(const char *device)
{
    int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd == -1)
    {
        std::cerr << "Error opening serial port: " << device << std::endl;
        return -1;
    }

    // 配置串口参数
    struct termios options;
    tcgetattr(fd, &options);

    // 设置波特率
    cfsetispeed(&options, kUartBaudRate);
    cfsetospeed(&options, kUartBaudRate);

    // 设置数据位、停止位、校验位等
    options.c_cflag &= ~PARENB; // 无校验位
    options.c_cflag &= ~CSTOPB; // 1位停止位
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;            // 8位数据位
    options.c_cflag &= ~CRTSCTS;       // 禁用硬件流控
    options.c_cflag |= CREAD | CLOCAL; // 启用接收并忽略调制解调器状态线

    // 设置为原始模式
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    // 设置读取超时
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;

    // 应用设置
    if (tcsetattr(fd, TCSANOW, &options) != 0)
    {
        std::cerr << "Error configuring serial port" << std::endl;
        close(fd);
        return -1;
    }

    // 清空缓冲区
    tcflush(fd, TCIOFLUSH);

    return fd;
}
