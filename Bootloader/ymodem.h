#ifndef __YMODEM_H__
#define __YMODEM_H__

#include <stdint.h>

// Ymodem 控制字符
#define YM_SOH      0x01  // 128字节数据包头
#define YM_STX      0x02  // 1024字节数据包头
#define YM_EOT      0x04  // 传输结束标志
#define YM_ACK      0x06  // 确认收到
#define YM_NAK      0x15  // 要求重传
#define YM_CAN      0x18  // 取消传输
#define YM_C        0x43  // 字符 'C'，请求数据

// 状态码
#define YM_RETURN_CODE_OK                   0
#define YM_RETURN_CODE_ERROR                -1
#define YM_RETURN_CODE_TIMEOUT              -2
#define YM_RETURN_CODE_ABORT                -3
#define YM_RETURN_CODE_ERROR_DATA           -4
#define YM_RETURN_CODE_EOT                  1


/*
    return：返回读出的数据字节大小。由于是单字节读取，成功则返回1，否则返回0
    para：  读出数据缓存区。
*/
typedef uint8_t (*YM_ReadByteFuc_t)(uint8_t *pData);

/**
    para：  要发送的数据。
 */
typedef void    (*YM_SendByteFuc_t)(uint8_t data);


typedef struct YM_InfoBlock {
    uint8_t  packet_data[1024];         // 最大 1K 包
    uint16_t packet_len;                // 包实际长度 128/1024
    uint32_t file_size;                 // 传入的文件大小
    uint32_t total_receive_byte;        // 截至目前总接收字节数

    YM_ReadByteFuc_t read_byte_cb;
    YM_SendByteFuc_t send_byte_cb;
    uint32_t (*get_tick_cb)(void);

}YM_InfoBlock_t;


int8_t bootYM_EstablishConnection(YM_InfoBlock_t *ym);
int8_t bootYM_AccepctOnePacket(YM_InfoBlock_t *ym);
void bootYM_Abort(YM_InfoBlock_t *ym);

#endif

