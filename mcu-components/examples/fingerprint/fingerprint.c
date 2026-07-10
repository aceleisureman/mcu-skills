#include "fingerprint.h"
#include "usart.h"
#include "delay.h"

u8 FPM10A_RECEIVE_BUFFER[32];

static const u8 PACK_HEAD[6] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF};
static const u8 CMD_GET_IMG[6]       = {0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
static const u8 CMD_TO_BUF1[7]       = {0x01, 0x00, 0x04, 0x02, 0x01, 0x00, 0x08};
static const u8 CMD_TO_BUF2[7]       = {0x01, 0x00, 0x04, 0x02, 0x02, 0x00, 0x09};
static const u8 CMD_REG_MODEL[6]     = {0x01, 0x00, 0x03, 0x05, 0x00, 0x09};
static const u8 CMD_DELETE_ALL[6]    = {0x01, 0x00, 0x03, 0x0D, 0x00, 0x11};
static const u8 CMD_READ_SYS_PARA[6] = {0x01, 0x00, 0x03, 0x0F, 0x00, 0x13};
static const u8 CMD_TEMPLATE_NUM[6]  = {0x01, 0x00, 0x03, 0x1D, 0x00, 0x21};

static void _send_head(void)
{
    u8 i;

    for(i = 0; i < 6; i++)
    {
        USART1_SendByte(PACK_HEAD[i]);
    }
}

static void _send_cmd_recv(const u8 *cmd, u8 cmd_len, u16 resp_len, u16 timeout)
{
    u8 i;

    USART1_ClearBuf();
    _send_head();
    for(i = 0; i < cmd_len; i++)
    {
        USART1_SendByte(cmd[i]);
    }
    USART1_ReceiveData(FPM10A_RECEIVE_BUFFER, resp_len, timeout);
}

static u8 _check_head(void)
{
    return (FPM10A_RECEIVE_BUFFER[0] == 0xEF &&
            FPM10A_RECEIVE_BUFFER[1] == 0x01) ? 1 : 0;
}

void Fingerprint_Init(void)
{
    USART1_Init(57600);
    USART1_ClearBuf();
    delay_ms(800);

    if(Fingerprint_HandShake() != ACK_SUCCESS)
    {
        Fingerprint_AutoBaud();
        USART1_ClearBuf();
        delay_ms(100);
    }
}

u8 Fingerprint_HandShake(void)
{
    _send_cmd_recv(CMD_READ_SYS_PARA, 6, 28, 800);
    if(!_check_head())
    {
        return ACK_RECV_ERR;
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 Fingerprint_AutoBaud(void)
{
    static const u32 baud_table[] = {57600, 115200, 38400, 19200, 9600};
    u8 i;
    u8 res;

    for(i = 0; i < sizeof(baud_table) / sizeof(baud_table[0]); i++)
    {
        USART1_Init(baud_table[i]);
        USART1_ClearBuf();
        delay_ms(120);
        res = Fingerprint_HandShake();
        if(res == ACK_SUCCESS)
        {
            return ACK_SUCCESS;
        }
    }

    USART1_Init(57600);
    USART1_ClearBuf();
    return ACK_RECV_ERR;
}

u8 Fingerprint_GetTemplateCount(u16 *count)
{
    _send_cmd_recv(CMD_TEMPLATE_NUM, 6, 14, 800);
    if(!_check_head())
    {
        return ACK_RECV_ERR;
    }
    if(FPM10A_RECEIVE_BUFFER[9] == ACK_SUCCESS && count)
    {
        *count = ((u16)FPM10A_RECEIVE_BUFFER[10] << 8) |
                  FPM10A_RECEIVE_BUFFER[11];
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 FPM10A_Get_Image(void)
{
    _send_cmd_recv(CMD_GET_IMG, 6, 12, 500);
    if(!_check_head())
    {
        return ACK_NO_FINGER;
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 FPM10A_Img_To_Buffer1(void)
{
    _send_cmd_recv(CMD_TO_BUF1, 7, 12, 500);
    if(!_check_head())
    {
        return ACK_RECV_ERR;
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 FPM10A_Img_To_Buffer2(void)
{
    _send_cmd_recv(CMD_TO_BUF2, 7, 12, 500);
    if(!_check_head())
    {
        return ACK_RECV_ERR;
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 FPM10A_Reg_Model(void)
{
    _send_cmd_recv(CMD_REG_MODEL, 6, 12, 800);
    if(!_check_head())
    {
        return ACK_RECV_ERR;
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 FPM10A_Save_Finger(u16 finger_id)
{
    u8 save_cmd[9];
    u16 checksum;

    save_cmd[0] = 0x01;
    save_cmd[1] = 0x00;
    save_cmd[2] = 0x06;
    save_cmd[3] = 0x06;
    save_cmd[4] = 0x01;
    save_cmd[5] = (finger_id >> 8) & 0xFF;
    save_cmd[6] = finger_id & 0xFF;

    checksum = save_cmd[0] + save_cmd[1] + save_cmd[2] +
               save_cmd[3] + save_cmd[4] + save_cmd[5] + save_cmd[6];
    save_cmd[7] = (checksum >> 8) & 0xFF;
    save_cmd[8] = checksum & 0xFF;

    _send_cmd_recv(save_cmd, 9, 12, 500);
    if(!_check_head())
    {
        return ACK_RECV_ERR;
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 FPM10A_Search_Finger(void)
{
    u8 search_cmd[11] = {0x01, 0x00, 0x08, 0x04, 0x01,
                         0x00, 0x00, 0x03, 0xE7, 0x00, 0xF8};

    _send_cmd_recv(search_cmd, 11, 16, 800);
    if(!_check_head())
    {
        return ACK_NO_FOUND;
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 FPM10A_Delete_All(void)
{
    _send_cmd_recv(CMD_DELETE_ALL, 6, 12, 1000);
    if(!_check_head())
    {
        return ACK_RECV_ERR;
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 Fingerprint_GetImage(void)
{
    return FPM10A_Get_Image();
}

u8 Fingerprint_GenChar(u8 BufferID)
{
    return (BufferID == 1) ? FPM10A_Img_To_Buffer1()
                           : FPM10A_Img_To_Buffer2();
}

u8 Fingerprint_Match(void)
{
    return ACK_SUCCESS;
}

u8 Fingerprint_RegModel(void)
{
    return FPM10A_Reg_Model();
}

u8 Fingerprint_StoreChar(u8 BufferID, u16 PageID)
{
    (void)BufferID;
    return FPM10A_Save_Finger(PageID);
}

u8 Fingerprint_Empty(void)
{
    return FPM10A_Delete_All();
}

u8 Fingerprint_DeleteChar(u16 PageID, u16 Count)
{
    u8 del_cmd[10];
    u16 checksum;

    del_cmd[0] = 0x01;
    del_cmd[1] = 0x00;
    del_cmd[2] = 0x07;
    del_cmd[3] = 0x0C;
    del_cmd[4] = (PageID >> 8) & 0xFF;
    del_cmd[5] = PageID & 0xFF;
    del_cmd[6] = (Count >> 8) & 0xFF;
    del_cmd[7] = Count & 0xFF;

    checksum = del_cmd[0] + del_cmd[1] + del_cmd[2] + del_cmd[3] +
               del_cmd[4] + del_cmd[5] + del_cmd[6] + del_cmd[7];
    del_cmd[8] = (checksum >> 8) & 0xFF;
    del_cmd[9] = checksum & 0xFF;

    _send_cmd_recv(del_cmd, 10, 12, 500);
    if(!_check_head())
    {
        return ACK_RECV_ERR;
    }
    return FPM10A_RECEIVE_BUFFER[9];
}

u8 Fingerprint_Search(u8 BufferID, u16 StartPage, u16 PageNum,
                      u16 *PageID, u16 *Score)
{
    u8 result;
    u8 search_cmd[11];
    u16 checksum;

    search_cmd[0] = 0x01;
    search_cmd[1] = 0x00;
    search_cmd[2] = 0x08;
    search_cmd[3] = 0x04;
    search_cmd[4] = BufferID;
    search_cmd[5] = (StartPage >> 8) & 0xFF;
    search_cmd[6] = StartPage & 0xFF;
    search_cmd[7] = (PageNum >> 8) & 0xFF;
    search_cmd[8] = PageNum & 0xFF;

    checksum = search_cmd[0] + search_cmd[1] + search_cmd[2] +
               search_cmd[3] + search_cmd[4] + search_cmd[5] +
               search_cmd[6] + search_cmd[7] + search_cmd[8];
    search_cmd[9] = (checksum >> 8) & 0xFF;
    search_cmd[10] = checksum & 0xFF;

    _send_cmd_recv(search_cmd, 11, 16, 800);
    if(!_check_head())
    {
        return ACK_NO_FOUND;
    }

    result = FPM10A_RECEIVE_BUFFER[9];
    if(result == ACK_SUCCESS)
    {
        if(PageID)
        {
            *PageID = ((u16)FPM10A_RECEIVE_BUFFER[10] << 8) |
                      FPM10A_RECEIVE_BUFFER[11];
        }
        if(Score)
        {
            *Score = ((u16)FPM10A_RECEIVE_BUFFER[12] << 8) |
                     FPM10A_RECEIVE_BUFFER[13];
        }
    }

    return result;
}
