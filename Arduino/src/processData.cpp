#include "processData.h"

void processData(DataPacket data){
    if (data.sequenceNumber >= data.address){
        //执行指令并回复
        switch (data.commandType){
            case 0x00:
                data.length = 0x09;
                data.commandType = 0x80;
                if (FilamentState == "inexist"){data.content = 0x0F;}
                else if (FilamentState == "exist"){data.content = 0xF0;}
                else if (FilamentState == "busy"){data.content = 0xF1;}
                data.sendPacket();

                break;
            case 0x01:
                switch (data.content){
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
                }

                data.length = 0x09;
                data.commandType = 0x81;
                data.content = 0x00;
                data.sendPacket();
                //发送回应数据包

                break;
        }
    }

    if (data.sequenceNumber != data.address){
        //转发数据包
        data.sequenceNumber += 1;
        data.sendPacket();
    }
}