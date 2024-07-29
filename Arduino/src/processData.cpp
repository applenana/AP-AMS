#include "processData.h"

void processData(DataPacket data){
    if (data.sequenceNumber >= data.address){
        //执行指令并回复
        switch (data.commandType){
            case 0x00:

                break;
            case 0x01:

                break;
        }
    }

    if (data.sequenceNumber != data.address){
        //转发数据包

    }
}