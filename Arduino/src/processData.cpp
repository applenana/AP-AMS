#include "processData.h"
#include "File.h"
#include "modeInfo.h"
void processData(DataPacket data)
{
    if (data.sequenceNumber >= data.address)
    {
        // 执行指令并回复
        uint8_t originLength = data.length;
        uint8_t originCommandType = data.commandType;
        byte originContent[512];
        memcpy(originContent, data.content, 512); // 复制内容

        switch (data.commandType)
        {
        case 0x00:
            data.length = 0x09;
            data.commandType = 0x80;
            if (FilamentState == "inexist")
            {
                data.content[0] = 0x0F;
            }
            else if (FilamentState == "exist")
            {
                data.content[0] = 0xF0;
            }
            else if (FilamentState == "busy")
            {
                data.content[0] = 0xF1;
            }
            data.sendPacket(true);
            break;
        case 0x01:
            switch (data.content[0])
            {
            case 0xF0: // 送出
                sv.push();
                mc.forward(300000); // 电机运行5分钟超时，防止过热(什么进料需要五分钟)
                FilamentState = "busy";
                break;
            case 0x0F:       // 抽回
                delay(1500); // 延迟抽回1.5s
                sv.push();
                mc.backforward(300000); // 电机运行5分钟超时，防止过热(什么退料需要五分钟)
                FilamentState = "busy";
                break;
            case 0x30: // 送出结束
                sv.pull();
                mc.stop();
                mc.setBanMotor(false);
                FilamentState = "exist";
                break;
            case 0x03: // 抽回结束
                int nowModeNum = getModeStatus();
                if (nowModeNum != 4)
                {
                    // delay(5000); // 回抽5秒
                    modeDelay(nowModeNum);
                    sv.pull();
                    mc.stop();
                    mc.setBanMotor(false);
                    FilamentState = "exist";
                    break;
                }
            }

            data.length = 0x09;
            data.commandType = 0x81;
            data.content[0] = 0x00;
            data.sendPacket(true);
            // 发送回应数据包
            break;
        case 0x02:
            WriteByteIntoConfig(data.content, 40);
            // 写入配置
            break;
        case 0x03:
            ReadContentFromConfig(data.content, 40);
            // 读取配置(更改内容为配置信息)
            data.sendPacket(true);
            // 发送回应数据包
            break;
        }

        data.length = originLength;
        data.commandType = originCommandType;
        memcpy(data.content, originContent, 512);
    }

    if (data.sequenceNumber != data.address)
    {
        // 转发数据包
        data.sequenceNumber += 1;
        data.sendPacket();
    }
}