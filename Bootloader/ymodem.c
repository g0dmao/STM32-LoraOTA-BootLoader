#include "ymodem.h"
#include "configBootloader.h"


/**
 * @brief 执行一次CRC算法
 *
 * @param crcIn 上一次执行CRC算法的结果
 * @param byte  要追加的数据（1字节）
 * @return uint16_t 此次执行后的crc结果
 */
static uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
    uint32_t crc = crcIn;
    /*
        第9位是“标志位”，起计数作用：
            当第9位的1被移到第16位时，说明做了8次左移操作、
    */
    uint32_t in = byte | 0x100;

    /*
        在crc算法中，需要把除数的（清除前导0后的）最高位1，
        与被除数的（清除前导0后的）最高位1对齐，再做异或运算。

        注意，以下注释的位数是从第0位开始算的，也就是说，从第0-第16位，位的数量是17。

        由于1 XOR 1 结果是确定的0，所以我们可以只异或最高位后的n位，
        n 取决于除数的位数，比如除数（清除前导0后）有16位（从第0位开始算），
        则只需异或0~15位。

        结合上面所说的，在下面这个crc实现算法中，除数本质上应该是0x11021，但是我们
        简写为了0x1021，因为我们不想让第16位做运算，其结果是确定的——0。
        因此我们人为地把最终运算结果的第16位置0，以避免运算。
        这一点我们通过将最后的结果 & 0xFFFF 做到。

    */

    // 追加8位新的数据，清除前导0，对齐最高位，异或0-15位。
    do {
        crc <<= 1;
        in <<= 1;
        if (in & 0x100) {
            ++crc;
        }
        if (crc & 0x10000) { // 移位的过程中，只要第16位为1，则说明已经对齐，就进行异或操作
            crc ^= 0x1021;
        }
    } while (!(in & 0x10000));// 判断第16位是不是1来判断是否进行了8次左移操作

    return crc & 0xFFFFu;
}

uint16_t CalcCRC16(const uint8_t *data, uint32_t size)
{
    uint32_t crc = 0;
    for (uint32_t i = 0; i < size; ++i) {
        crc = UpdateCRC16(crc, data[i]);
    }
    // 把最后两字节推过去对齐做运算。
    crc = UpdateCRC16(crc, 0);
    crc = UpdateCRC16(crc, 0);
    return crc & 0xFFFFu;
}

/**
 * @brief  接收并解析单个 Ymodem 数据包
 * @param  ym: Ymodem 上下文控制块
 * @param  timeout: 超时时间 (ms)
 * @retval 状态码 (YM_RETURN_CODE_XXX)
 */
static int8_t ReceivePacket(YM_InfoBlock_t *ym, uint32_t timeout)
{
    uint8_t ch;
    uint32_t start_time = ym->get_tick_cb();
    uint16_t packet_size = 0;
    uint8_t seq[2]; // 包号与反码

    // 1. 等待并解析包头
    while (1)
    {
        if (ym->read_byte_cb(&ch))
        {
            if (ch == YM_SOH) {
                packet_size = 128;
                break;
            } else if (ch == YM_STX) {
                packet_size = 1024;
                break;
            } else if (ch == YM_EOT) {
                return YM_RETURN_CODE_EOT;   // 收到结束符
            } else if (ch == YM_CAN) {
                return YM_RETURN_CODE_ABORT; // 收到取消指令
            }
        }
        // 检测超时
        if (ym->get_tick_cb() - start_time > timeout) return YM_RETURN_CODE_TIMEOUT;
    }

    // 记录期望接收的物理包长
    ym->packet_len = packet_size;

    // 2. 读取包号和反码 (2字节)
    for (int i = 0; i < 2; i++)
    {
        start_time = ym->get_tick_cb();
        while (!ym->read_byte_cb(&seq[i]))
        {
            if (ym->get_tick_cb() - start_time > timeout) return YM_RETURN_CODE_TIMEOUT;
        }
    }

    // 3. 读取核心数据区 (128 或 1024 字节)
    // 直接写进 ym->packet_data 缓冲区
    for (int i = 0; i < packet_size; i++)
    {
        start_time = ym->get_tick_cb();
        while (!ym->read_byte_cb(&ym->packet_data[i]))
        {
            if (ym->get_tick_cb() - start_time > timeout) return YM_RETURN_CODE_TIMEOUT;
        }
    }

    // 4. 读取 CRC 校验码 (2字节)
    uint8_t crc_bytes[2];
    for (int i = 0; i < 2; i++)
    {
        start_time = ym->get_tick_cb();
        while (!ym->read_byte_cb(&crc_bytes[i]))
        {
            if (ym->get_tick_cb() - start_time > timeout) return YM_RETURN_CODE_TIMEOUT;
        }
    }

    // ==========================================
    // 5. 数据抽取完毕，开始集中校验
    // ==========================================

    // 校验包号与反码
    if (seq[0] != (uint8_t)(~seq[1]))
    {
        return YM_RETURN_CODE_ERROR_DATA;
    }

    // 校验 CRC
    uint16_t crc_recv = (crc_bytes[0] << 8) | crc_bytes[1];
    uint16_t crc_calc = CalcCRC16(ym->packet_data, packet_size);

    if (crc_recv != crc_calc)
    {
        return YM_RETURN_CODE_ERROR_DATA;
    }

    return YM_RETURN_CODE_OK;
}

// static void ClearInfoBlock(YM_InfoBlock_t *ym)
// {
//     ym->file_size = 0;
//     ym->packet_len = 0;
//     ym->total_receive_byte = 0;
// }


int8_t bootYM_EstablishConnection(YM_InfoBlock_t *ym)
{

    int8_t status;

    ym->send_byte_cb(YM_C);
    status = ReceivePacket(ym, 1000); // 1秒超时

    if(status == YM_RETURN_CODE_OK)
    {
        // 清除上一次传输的脏数据
        ym->total_receive_byte = 0;
        ym->file_size = 0;

        // 解析第 0 包（包含文件名和大小）
        // 格式： "filename.bin" + \0 + "12345" + \0
        if(ym->packet_data[0] != 0)
        {
            uint16_t name_len = 0;

            // 寻找 '\0' 且带有边界保护
            while (name_len < ym->packet_len && ym->packet_data[name_len] != '\0')
            {
                name_len++;
            }

            // 提取文件名（截断到缓冲区大小）
            uint16_t copy_len = (name_len < YM_FILE_NAME_MAX_LEN - 1)
                                ? name_len : (YM_FILE_NAME_MAX_LEN - 1);
            for (uint16_t j = 0; j < copy_len; j++)
            {
                ym->file_name[j] = (char)ym->packet_data[j];
            }
            ym->file_name[copy_len] = '\0';

            // 确保找到了 '\0' 且后面还有空间存放文件大小字符
            if (name_len < (ym->packet_len - 1))
            {
                // 指针指向文件大小的首地址
                uint8_t *size_str = ym->packet_data + name_len + 1;
                uint32_t file_size = 0;
                uint16_t i = 0;

                // 避免使用标准库，自己实现atoi函数。安全解析 ASCII 数字，遇到非数字字符或越界则停止
                while((name_len + 1 + i) < ym->packet_len &&
                       size_str[i] >= '0' && size_str[i] <= '9')
                {
                    file_size = file_size * 10 + (size_str[i] - '0');
                    i++;
                }

                ym->file_size = file_size;
                ym->send_byte_cb(YM_ACK); // 确认收到第 0 包
                ym->send_byte_cb(YM_C);   // 再次发 'C'，请求开始传真正的数据
            }else
            {
                // 包格式严重错误
                return YM_RETURN_CODE_ERROR_DATA;
            }

        }
    }

    return status;

}

int8_t bootYM_AccepctOnePacket(YM_InfoBlock_t *ym)
{

    uint8_t eot_count = 0;
    uint8_t retry_count = 10;
    int8_t status;

    while(retry_count > 0)
    {
        status = ReceivePacket(ym, 3000); // 放宽到3秒超时

        switch(status)
        {
            case YM_RETURN_CODE_OK:

                uint16_t actual_write_len = ym->packet_len;
                // 如果【当前已写偏移量 + 这包的长度】超过了【实际文件大小】
                // 说明这肯定是最后一包，我们需要把它截断，只写有效数据
                if ((ym->total_receive_byte + ym->packet_len) > ym->file_size)
                {
                    actual_write_len = ym->file_size - ym->total_receive_byte;
                    ym->packet_len = actual_write_len;

                }

                ym->total_receive_byte += actual_write_len;

                ym->send_byte_cb(YM_ACK);

                return YM_RETURN_CODE_OK;

            case YM_RETURN_CODE_EOT:
                eot_count++;
                if(eot_count == 1)
                {
                    ym->send_byte_cb(YM_NAK); // Ymodem 标准：第一个 EOT 回 NAK
                    continue;
                }else
                {
                    // 校验包完整性
                    if(ym->total_receive_byte == ym->file_size)
                    {
                        ym->send_byte_cb(YM_ACK); // 第二个 EOT 回 ACK，传输正式结束
                        ym->send_byte_cb(YM_C);   // 请求发送空包以彻底终止
                        return YM_RETURN_CODE_EOT;
                    }else
                    {
                        // 发现文件长度对不上，直接报错，防止启动残缺的固件！
                        ym->send_byte_cb(YM_CAN);
                        ym->send_byte_cb(YM_CAN);
                        return YM_RETURN_CODE_ERROR;
                    }

                }

            case YM_RETURN_CODE_ERROR_DATA:
            case YM_RETURN_CODE_TIMEOUT:
                ym->send_byte_cb(YM_NAK); // CRC或包号错误，要求重传当前包
                retry_count--;
                continue;

            case YM_RETURN_CODE_ABORT:
                ym->send_byte_cb(YM_ACK);
                return YM_RETURN_CODE_ABORT;

            default:
                break;
        }
    }
    ym->send_byte_cb(YM_CAN);
    ym->send_byte_cb(YM_CAN);
    return YM_RETURN_CODE_ERROR;
}

void bootYM_Abort(YM_InfoBlock_t *ym)
{
    ym->send_byte_cb(YM_CAN);
    ym->send_byte_cb(YM_CAN);
}

