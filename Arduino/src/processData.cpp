#include "processData.h"
#include "File.h"

void processData(DataPacket data){
    if (data.sequenceNumber >= data.address){
        //执行指令并回复
        uint8_t originLength = data.length;
        uint8_t originCommandType = data.commandType;
        byte originContent[512];
        memcpy(originContent, data.content, 512);// 复制内容

        switch (data.commandType){
            case 0x00:
                data.length = 0x09;
                data.commandType = 0x80;
                if (FilamentState == "inexist"){data.content[0] = 0x0F;}
                else if (FilamentState == "exist"){data.content[0] = 0xF0;}
                else if (FilamentState == "busy"){data.content[0] = 0xF1;}
                data.sendPacket(true);
                break;
            case 0x01:
                switch (data.content[0]){
                    case 0xF0://送出
                        sv.push();
                        mc.forward();
                        FilamentState = "busy";
                        break;
                    case 0x0F://抽回
                        sv.push();
                        mc.backforward();
                        FilamentState = "busy";
                        break;
                    case 0x30://送出结束
                        sv.pull();
                        mc.stop();
                        FilamentState = "exist";
                        break;
                    case 0x03://抽回结束
                        if (Is5sPull){
                            delay(7000);//回抽七秒
                            sv.pull();
                            mc.stop();
                            FilamentState = "exist";
                            break;
                        }
                }

                data.length = 0x09;
                data.commandType = 0x81;
                data.content[0] = 0x00;
                data.sendPacket(true);
                //发送回应数据包
                break;
            case 0x02:
                WriteByteIntoConfig(data.content,40);
                //写入配置
                break;
            case 0x03:
                ReadContentFromConfig(data.content,40);
                //读取配置(更改内容为配置信息)
                data.sendPacket(true);
                //发送回应数据包
                break;
        }

        data.length = originLength;
        data.commandType = originCommandType;
        memcpy(data.content, originContent, 512);
    }
    
    if (data.sequenceNumber != data.address){
        //转发数据包
        data.sequenceNumber += 1;
        data.sendPacket();
    }
}