<p align="center"><img src="https://github.com/applenana/AP-AMS/blob/main/icon/icon.png" width="200" height="200" alt="ap-ams"></a>
</p>

<div align="center">
 
# AP-AMS
<!-- prettier-ignore-start -->
<!-- markdownlint-disable-next-line MD036 -->
_✨ 一个简单的适用于拓竹的自动换色系统 ✨_
<!-- prettier-ignore-end -->
</div>

## 特点
- 一个换色单元就可以自己工作！无需上位机！
- 成本低，单个换色单元仅花费**15元**
- 接入homeassistant，数据可视化，方便监视换色以及纠错(待完善)
- 使用esp8266作为MCU,Arduino开发
- 换色单元主体除电机和基础齿轮均为3d打印
- 使用舵机控制齿轮松紧，仅换料时压紧齿轮，正常打印依靠拓竹的近端挤出机
- 正常打印低噪音

## 结构
![image](https://github.com/applenana/AP-AMS/assets/83851967/ca55f6cd-61bb-41c4-b0b0-55ac173ba07e)


## 使用
- 你需要准备:
  - 一颗能沉下心来折腾的心
  - makerworld中提到的部分[材料](https://makerworld.com/zh/models/463829#profileId-372457)
  - 立创开源广场的[材料](https://oshwhub.com/applenana/apams)
  - 这里的release(固件)!
- 控制板配置参考[这里](https://github.com/llleeeqi/AP-AMS/blob/main/doc/%E9%85%8D%E7%BD%AE.md)
- 待续

## 提示
- micropython版本已经弃用,原因是内存无法支撑其在esp8266中运行,你可以尝试在esp32中运行它(不建议)
- 项目仍未完成……请实时关注项目的更新,很有可能解决重大问题
- 模型和gcode已上传至[makerworld](https://makerworld.com/zh/@applenana)
- PCB上传至[立创开源广场](https://oshwhub.com/applenana/apams)

## 鸣谢
- 整个项目的整体思路和部分资料来源于[YBA0312的YBA-AMS项目](https://github.com/YBA0312/YBA-AMS-ESP)!
- 还有YBAAMS的群友提供的各种建议!
