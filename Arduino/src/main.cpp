#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <map>
#include <LittleFS.h>
#include <Servo.h>
#include <Adafruit_NeoPixel.h>

#define MSG_BUFFER_SIZE	(4096)//mqtt服务器的配置

//-=-=-=-=-=-=-=-=↓用户配置↓-=-=-=-=-=-=-=-=-=-=
String wifiName;//
String wifiKey;//
String bambu_mqtt_broker;//
String bambu_mqtt_password;//
String bambu_device_serial;//
String filamentID;//
String ha_mqtt_broker;
String ha_mqtt_user;
String ha_mqtt_password;
//-=-=-=-=-=-=-=-=↑用户配置↑-=-=-=-=-=-=-=-=-=-=

//-=-=-=-=-=-↓系统配置↓-=-=-=-=-=-=-=-=-=
bool debug = false;
String sw_version = "v2.5-beta";
String bambu_mqtt_user = "bblp";
String bambu_mqtt_id = "ams";
String ha_mqtt_id = "ams";
String ha_topic_subscribe;
String bambu_topic_subscribe;// = "device/" + String(bambu_device_serial) + "/report";
String bambu_topic_publish;// = "device/" + String(bambu_device_serial) + "/request";
String bambu_resume = "{\"print\":{\"command\":\"resume\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}"; // 重试|继续打印
String bambu_unload = "{\"print\":{\"command\":\"ams_change_filament\",\"curr_temp\":220,\"sequence_id\":\"1\",\"tar_temp\":220,\"target\":255},\"user_id\":\"1\"}";
String bambu_load = "{\"print\":{\"command\":\"ams_change_filament\",\"curr_temp\":220,\"sequence_id\":\"1\",\"tar_temp\":220,\"target\":254},\"user_id\":\"1\"}";
String bambu_done = "{\"print\":{\"command\":\"ams_control\",\"param\":\"done\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}"; // 完成
String bambu_clear = "{\"print\":{\"command\": \"clean_print_error\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}";
String bambu_status = "{\"pushing\": {\"sequence_id\": \"0\", \"command\": \"pushall\"}}";
//String bambu_pause = "{\"print\": {\"command\": \"gcode_line\",\"sequence_id\": \"1\",\"param\": \"M400U1\"},\"user_id\": \"1\"}";
//String bambu_pause = "{\"print\":{\"command\":\"pause\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}";
int servoPin = 13;//舵机引脚
int motorPin1 = 4;//电机一号引脚
int motorPin2 = 5;//电机二号引脚
int bufferPin1 = 14;//缓冲器1
int bufferPin2 = 16;//缓冲器2
unsigned int bambuRenewTime = 1250;//拓竹更新时间
unsigned int haRenewTime = 3000;//ha推送时间
int backTime = 1000;
unsigned int ledBrightness;//led默认亮度
String filamentType;
int filamentTemp;
int ledR;
int ledG;
int ledB;
#define ledPin 12//led引脚
#define ledPixels 3//led数量
//-=-=-=-=-=-↑系统配置↑-=-=-=-=-=-=-=-=-=

//-=-=-=-=-=-mqtt回调逻辑需要的变量-=-=-=-=-=-=
bool unloadMsg;
bool completeMSG;
bool reSendUnload;
bool isSendFilament;
long sendOutTimeOut;
int sendOutTimes;
long pullBackTimeOut;
int pullBackTimes;
String commandStr = "";//命令传输
//-=-=-=-=-=-=end

unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
WiFiClientSecure bambuWifiClient;
PubSubClient bambuClient(bambuWifiClient);
WiFiClient haWifiClient;
PubSubClient haClient(haWifiClient);

Adafruit_NeoPixel leds(ledPixels, ledPin, NEO_GRB + NEO_KHZ800);

unsigned long bambuLastTime = millis();
unsigned long haLastTime = millis();
unsigned long bambuCheckTime = millis();//mqtt定时任务
unsigned long haCheckTime = millis();
int inLed = 2;//跑马灯led变量
int waitLed = 2;
int completeLed = 2;

Servo servo;//初始化舵机

void ledAll(unsigned int r, unsigned int g, unsigned int b) {//led填充
    leds.fill(leds.Color(r,g,b));
    leds.show();
}

//连接wifi
void connectWF(String wn,String wk) {
    ledAll(0,0,0);
    int led = 2;
    int count = 1;
    WiFi.begin(wn, wk);
    Serial.print("连接到wifi [" + String(wifiName) + "]");
    while (WiFi.status() != WL_CONNECTED) {
        count ++;
        if (led == -1){
            led = 2;
            ledAll(0,0,0);
        }else{
            leds.setPixelColor(led,leds.Color(0,255,0));
            leds.show();
            led--;
        }
        Serial.print(".");
        delay(250);
        if (count > 100){
            ledAll(255,0,0);
            Serial.println("WIFI连接超时!请检查你的wifi配置");
            Serial.println("WIFI名["+String(wifiName)+"]");
            Serial.println("WIFI密码["+String(wifiKey)+"]");
            Serial.println("本次输出[]内没有内置空格!");
            Serial.println("你将有两种选择:");
            Serial.println("1:已经确认我的wifi配置没有问题!继续重试!");
            Serial.println("2:我的配置有误,删除配置重新书写");
            Serial.println("请输入你的选择:");
            while (Serial.available() == 0){
                Serial.print(".");
                delay(100);
            }
            String content = Serial.readString();
            if (content == "2"){if(LittleFS.remove("/config.json")){Serial.println("SUCCESS!");ESP.restart();}}
            ESP.restart();
        }
    }
    Serial.println("");
    Serial.println("WIFI已连接");
    Serial.println("IP: ");
    Serial.println(WiFi.localIP());
    ledAll(50,255,50);
}

//获取持久数据
JsonDocument getPData(){
    File file = LittleFS.open("/data.json", "r");
    JsonDocument Pdata;
    deserializeJson(Pdata, file);
    file.close();
    return Pdata;
}
//写入持久数据
void writePData(JsonDocument Pdata){
    // 检查Pdata是否包含所需的参数
    if (Pdata.containsKey("lastFilament") && Pdata.containsKey("step") && Pdata.containsKey("subStep") && Pdata.containsKey("filamentID")) {
        File file = LittleFS.open("/data.json", "w");
        serializeJson(Pdata, file);
        file.close();
    } else {
        Serial.println("错误：数据缺少必要的参数，无法存储。");
        if(LittleFS.remove("/data.json")){Serial.println("SUCCESS!");ESP.restart();}
    }
}

//获取配置数据
JsonDocument getCData(){
    File file = LittleFS.open("/config.json", "r");
    JsonDocument Cdata;
    deserializeJson(Cdata, file);
    file.close();
    return Cdata;
}
//写入配置数据
void writeCData(JsonDocument Cdata){
    File file = LittleFS.open("/config.json", "w");
    serializeJson(Cdata, file);
    file.close();
}

//定义电机驱动类和舵机控制类
class Machinery {
  private:
    int pin1;
    int pin2;
    bool isStop = true;
    String state = "停止";
  public:
    Machinery(int pin1, int pin2) {
      this->pin1 = pin1;
      this->pin2 = pin2;
      pinMode(pin1, OUTPUT);
      pinMode(pin2, OUTPUT);
    }

    void forward() {
      digitalWrite(pin1, HIGH);
      digitalWrite(pin2, LOW);
      isStop = false;
      state = "前进";
    }

    void backforward() {
      digitalWrite(pin1, LOW);
      digitalWrite(pin2, HIGH);
      isStop = false;
      state = "后退";
    }

    void stop() {
      digitalWrite(pin1, HIGH);
      digitalWrite(pin2, HIGH);
      isStop = true;
      state = "停止";
    }

    bool getStopState() {
        return isStop;
    }
    String getState(){
        return state;
    }
};
class ServoControl {
    private:
        int angle = -1;
        String state = "自定义角度";
    public:
        ServoControl(){
        }
        void push() {
            servo.write(0);
            angle = 0;
            state = "推";
        }
        void pull() {
            servo.write(180);
            angle = 180;
            state = "拉";
        }
        void writeAngle(int angle) {
            servo.write(angle);
            angle = angle;
            state = "自定义角度";
        }
        int getAngle(){
            return angle;
        }
        String getState(){
            return state;
        }
    };
//定义电机舵机变量
ServoControl sv;
Machinery mc(motorPin1, motorPin2);

void statePublish(String content){
    Serial.println(content);
    haClient.publish(("AMS/"+filamentID+"/state").c_str(),content.c_str());
}

//连接拓竹mqtt
void connectBambuMQTT() {
    int count = 1;
    while (!bambuClient.connected()) {
        count ++;
        Serial.println("尝试连接拓竹mqtt|"+bambu_mqtt_id+"|"+bambu_mqtt_user+"|"+bambu_mqtt_password);
        if (bambuClient.connect(bambu_mqtt_id.c_str(), bambu_mqtt_user.c_str(), bambu_mqtt_password.c_str())) {
            Serial.println("连接成功!");
            //Serial.println(bambu_topic_subscribe);
            bambuClient.subscribe(bambu_topic_subscribe.c_str());
            ledAll(ledR,ledG,ledB);
        } else {
            Serial.print("连接失败，失败原因:");
            Serial.print(bambuClient.state());
            Serial.println("在一秒后重新连接");
            delay(1000);
            ledAll(255,0,0);
        }

        if (count > 30){
            ledAll(255,0,0);
            Serial.println("拓竹连接超时!请检查你的配置");
            Serial.println("拓竹ip地址["+String(bambu_mqtt_broker)+"]");
            Serial.println("拓竹序列号["+String(bambu_device_serial)+"]");
            Serial.println("拓竹访问码["+String(bambu_mqtt_password)+"]");
            Serial.println("本次输出[]内没有内置空格!");
            Serial.println("你将有两种选择:");
            Serial.println("1:已经确认我的配置没有问题!继续重试!");
            Serial.println("2:我的配置有误,删除配置重新书写");
            Serial.println("请输入你的选择:");
            while (Serial.available() == 0){
                Serial.print(".");
                delay(100);
            }
            String content = Serial.readString();
            if (content == "2"){if(LittleFS.remove("/config.json")){Serial.println("SUCCESS!");ESP.restart();}}
            ESP.restart();
        }
    }
}

//拓竹mqtt回调
void bambuCallback(char* topic, byte* payload, unsigned int length) {
    JsonDocument* data = new JsonDocument();
    deserializeJson(*data, payload, length);
    String sequenceId = (*data)["print"]["sequence_id"].as<String>();
    String amsStatus = (*data)["print"]["ams_status"].as<String>();
    String printError = (*data)["print"]["print_error"].as<String>();
    String hwSwitchState = (*data)["print"]["hw_switch_state"].as<String>();
    String gcodeState = (*data)["print"]["gcode_state"].as<String>();
    String mcPercent = (*data)["print"]["mc_percent"].as<String>();
    String mcRemainingTime = (*data)["print"]["mc_remaining_time"].as<String>();
    // 手动释放内存
    delete data;

    if (!(amsStatus == printError && printError == hwSwitchState && hwSwitchState == gcodeState && gcodeState == mcPercent && mcPercent == mcRemainingTime && mcRemainingTime == "null")) {
        if (debug){
            statePublish(sequenceId+"|ams["+amsStatus+"]"+"|err["+printError+"]"+"|hw["+hwSwitchState+"]"+"|gcode["+gcodeState+"]"+"|mcper["+mcPercent+"]"+"|mcrtime["+mcRemainingTime+"]");
            //Serial.print("Free memory: ");
            //Serial.print(ESP.getFreeHeap());
            //Serial.println(" bytes");
            statePublish("Free memory: "+String(ESP.getFreeHeap())+" bytes");
            statePublish("-=-=-=-=-");}
        bambuLastTime = millis();
    }
    
    /*
    step代表了换色进行的五大主要步骤
    1——收到换色指令，进行规划和分配  红绿蓝
    2——退料 白色
    3——进料 黄色
    4——待命 蓝绿色
    5——继续 绿色
    subStep代表子步骤，用于细分主步骤
    */
    JsonDocument Pdata = getPData();
    if (gcodeState == "PAUSE" and mcPercent.toInt() > 100){
        String nextFilament = String(mcPercent.toInt() - 110 + 1);
        Pdata["nextFilament"] = nextFilament;            
        if (Pdata["step"] == "1"){
            statePublish("收到换色指令，进入换色准备状态");
            leds.setPixelColor(2,leds.Color(255,0,0));
            leds.setPixelColor(1,leds.Color(0,255,0));
            leds.setPixelColor(0,leds.Color(0,0,255));
            leds.show();
            unloadMsg = false;
            completeMSG = false;
            reSendUnload = false;
            isSendFilament = false;
            sv.pull();
            mc.stop();
            statePublish("本机通道"+String(Pdata["filamentID"])+"|上料通道"+String(Pdata["lastFilament"])+"|下一耗材通道"+nextFilament);

            if (Pdata["filamentID"] == Pdata["lastFilament"]){
                statePublish("本机通道["+String(Pdata["filamentID"])+"]在上料");//如果处于上料状态，那么对于这个换色单元来说，接下来只能退料或者继续打印（不退料）
                if (nextFilament == Pdata["filamentID"]){
                    statePublish("本机通道,上料通道,下一耗材通道全部相同!无需换色!");
                    Pdata["step"] = "5";
                    Pdata["subStep"] = "1";
                }else{
                    //bambuClient.publish(bambu_topic_publish.c_str(),bambu_pause.c_str());
                    statePublish("下一耗材通道与本机通道不同，需要换料，准备退料");
                    Pdata["step"] = "2";
                    Pdata["subStep"] = "1";
                }
            }else{
                statePublish("本机通道["+String(Pdata["filamentID"])+"]不在上料");//如果本换色单元不在上料，那么又两个可能，要么本次换色与自己无关，要么就是要准备进料
                if (nextFilament == Pdata["filamentID"]){
                    statePublish("本机通道将要换色，准备送料");
                    Pdata["step"] = String("3");
                    Pdata["subStep"] = String("1");
                }else{
                    statePublish("本机通道与本次换色无关，无需换色");
                    Pdata["step"] = String("4");
                    Pdata["subStep"] = String("1");
                }
            }
        }else{
            ledAll(ledR,ledG,ledB);
            }
    }if (Pdata["step"] == "2"){
        if (Pdata["subStep"] == "1"){
            statePublish("进入退料状态");
            leds.clear();
            leds.setPixelColor(2,leds.Color(255,255,255));
            leds.show();
            bambuClient.publish(bambu_topic_publish.c_str(),bambu_unload.c_str());
            reSendUnload = true;
            Pdata["subStep"] = "2";
        }else if (Pdata["subStep"] == "2"){
            leds.setPixelColor(1,leds.Color(255,255,255));
            leds.show();
            if (not reSendUnload){
                reSendUnload = true;
                delay(3000);
                statePublish("仍未退料,发出退料请求");
                bambuClient.publish(bambu_topic_publish.c_str(),bambu_unload.c_str());
            }else if (printError == "318750723" or printError == "134184963") {
                statePublish("拔出耗材");
                sv.push();
                mc.backforward();
                pullBackTimeOut = millis();
                pullBackTimes = 1;
                Pdata["subStep"] = "3";
            } else if (printError == "318734339" or printError == "134201347") {
                statePublish("拔出耗材");
                sv.push();
                mc.backforward();
                pullBackTimeOut = millis();
                pullBackTimes = 1;
                bambuClient.publish(bambu_topic_publish.c_str(), bambu_resume.c_str());
                Pdata["subStep"] = "3";
            }
        }else if (Pdata["subStep"] == "3"){
            if (amsStatus == "0"){
                statePublish("退料完成，本次换色完成");
                leds.setPixelColor(2,leds.Color(255,255,255));
                leds.show();
                delay(backTime);
                sv.pull();
                mc.stop();
                Pdata["step"] = "4";
                Pdata["subStep"] = "1";
            }else{
                if ((millis() - pullBackTimeOut)>30*1000){
                    sv.pull();
                    mc.stop();
                    statePublish("退料超时，请手动拔出耗材并检查机器状态！");
                }else{
                    sv.push();
                    mc.backforward();
                }
            }
        }
    }else if (Pdata["step"] == "3"){
        isSendFilament = true;
        if (Pdata["subStep"] == "1"){
            if (amsStatus == "0") {
                statePublish("进入送料状态");
                leds.clear();
                leds.setPixelColor(2,leds.Color(255,255,0));
                leds.show();
                bambuClient.publish(bambu_topic_publish.c_str(), bambu_load.c_str());
                Pdata["subStep"] = "2"; // 更新 subStep
            } else {
                if (!unloadMsg){
                    statePublish("等待耗材退料完成……");
                    unloadMsg = true;
                }else{
                    Serial.print(".");
                }

                //跑马灯
                if (inLed == -1){
                    inLed = 2;
                    ledAll(0,0,0);
                }else{
                    leds.setPixelColor(inLed,leds.Color(255,255,0));
                    leds.show();
                    inLed--;
                }
            }
        }else if (Pdata["subStep"] == "2" && (printError == "318750726" or printError == "134201350")){
            statePublish("送入耗材");
            sv.push();
            mc.forward();
            Pdata["subStep"] = "3";
            sendOutTimeOut = millis();
            sendOutTimes = 1;

            leds.clear();
            leds.setPixelColor(2,leds.Color(255,255,0));
            leds.setPixelColor(1,leds.Color(255,255,0));
            leds.show();
        }else if (Pdata["subStep"] == "3"){
            if ((amsStatus == "262" && hwSwitchState == "1") or digitalRead(bufferPin1) == 1){
                statePublish("停止送料");
                mc.stop();
                bambuClient.publish(bambu_topic_publish.c_str(),bambu_done.c_str());
                Pdata["subStep"] = "4";

                leds.setPixelColor(0,leds.Color(255,255,0));
                leds.show();
            }else{
                if (sendOutTimes <=3){
                    if ((millis() - sendOutTimeOut)>30*1000){
                        statePublish("送料超时!尝试重新送料中");
                        sendOutTimeOut = millis();
                        sendOutTimes += 1;
                        sv.push();
                        mc.backforward();
                        delay(3*1000);
                        sv.push();
                        mc.forward();
                    }else{
                        sv.push();
                        mc.forward();
                    }
                }else{
                    statePublish("送料错误！请手动送料并检查挤出机状态!");
                }
            }
        }else if (Pdata["subStep"] == "4"){
            if (hwSwitchState == "0") {
                statePublish("检测到送料失败，重新送料!");
                mc.backforward();
                delay(2000);
                mc.stop();
                Pdata["subStep"] = "2";
                
                leds.setPixelColor(2,leds.Color(255,255,0));
                leds.setPixelColor(1,leds.Color(255,0,0));
                leds.show();
            } else if (hwSwitchState == "1") {
                statePublish("送料成功，等待挤出换料");
                sv.pull(); 
                Pdata["subStep"] = "5"; // 更新 subStep

                leds.setPixelColor(2,leds.Color(255,255,0));
                leds.setPixelColor(1,leds.Color(0,255,0));
                leds.show();
            }
        }else if (Pdata["subStep"] == "5"){
            if (printError == "318734343") {
                if (hwSwitchState == "1"){
                    statePublish("被虚晃一枪！重新点击确认");
                    bambuClient.publish(bambu_topic_publish.c_str(), bambu_done.c_str());
                    leds.setPixelColor(2,leds.Color(255,255,0));
                    leds.setPixelColor(1,leds.Color(0,255,0));
                    leds.setPixelColor(0,leds.Color(0,255,0));
                    leds.show();
                }else if (hwSwitchState == "0"){
                    statePublish("检测到送料失败……进入步骤AGAIN重新送料");
                    leds.setPixelColor(2,leds.Color(255,255,0));
                    leds.setPixelColor(1,leds.Color(255,0,0));
                    leds.setPixelColor(0,leds.Color(255,0,0));
                    leds.show();
                    sv.push();
                    mc.backforward();
                    delay(2000);
                    mc.stop();
                    Pdata["subStep"] = "AGAIN";
                }
            } else if (amsStatus == "768") {
                statePublish("芜湖~换料完成！");
                ledAll(0,255,0);
                delay(1000);
                bambuClient.publish(bambu_topic_publish.c_str(), bambu_resume.c_str());
                Pdata["step"] = "4";
                Pdata["subStep"] = "1";
            }
        }else if (Pdata["subStep"] == "AGAIN"){
            if (hwSwitchState == "0"){
                statePublish("尝试重新送料");
                mc.forward();
            }else if (hwSwitchState == "1"){
                statePublish("送料成功!");
                mc.stop();
                sv.pull();
                Pdata["subStep"] = "5";
            }
        }
    }else if (Pdata["step"] == "4"){
        if (!completeMSG){
            statePublish("进入看戏状态,等待换色完成");
            completeMSG = true;
        }else{
            Serial.print(".");
        }

        //跑马灯
        if (waitLed == -1){
            waitLed = 2;
            ledAll(0,0,0);
        }else{
            leds.setPixelColor(waitLed,leds.Color(0,255,255));
            leds.show();
            waitLed--;
        }

        if (amsStatus == "1280" and gcodeState != "PAUSE") {
            String nextFilament = Pdata["nextFilament"];
            statePublish("换色完成！切换上料通道为["+nextFilament+"]");
            Pdata["step"] = "1";
            Pdata["subStep"] = "1";
            Pdata["lastFilament"] = nextFilament;
            ledAll(0,255,0);
            if(isSendFilament){
                delay(500);
                statePublish("发送新的温度!");
                statePublish("{\"print\": {\"command\": \"gcode_line\",\"sequence_id\": \"1\",\"param\": \"M109 S"+String(filamentTemp)+"\\n\"},\"user_id\": \"1\"}");
                bambuClient.publish(bambu_topic_publish.c_str(),
                ("{\"print\": {\"command\": \"gcode_line\",\"sequence_id\": \"1\",\"param\": \"M109 S"+String(filamentTemp)+"\\n\"},\"user_id\": \"1\"}")
                .c_str());
            }
        }
    }else if (Pdata["step"] == "5"){
        bambuClient.publish(bambu_topic_publish.c_str(),bambu_resume.c_str());
        delay(500);
        if (!completeMSG){
            statePublish("发送继续指令");
            completeMSG = true;
        }else{
            Serial.print(".");
        }
        
        //跑马灯
        if (completeLed == -1){
            completeLed = 2;
            ledAll(0,0,0);
        }else{
            leds.setPixelColor(completeLed,leds.Color(0,255,255));
            leds.show();
            completeLed--;
        }

        if (amsStatus == "1280") {
            statePublish("完成!");
            Pdata["step"] = "1";
            Pdata["subStep"] = "1";
            ledAll(0,255,0);
        }
    }
    
    writePData(Pdata);
}

//连接hamqtt
void connectHaMQTT() {
    int count = 1;
    while (!haClient.connected()) {
        count ++;
        Serial.println("尝试连接ha mqtt|"+ha_mqtt_broker+"|"+ha_mqtt_user+"|"+ha_mqtt_password+"|"+String(ESP.getChipId(), HEX));
        if (haClient.connect(String(ESP.getChipId(), HEX).c_str(), ha_mqtt_user.c_str(), ha_mqtt_password.c_str())) {
            Serial.println("连接成功!");
            //Serial.println(ha_topic_subscribe);
            haClient.subscribe(ha_topic_subscribe.c_str());
            ledAll(ledR,ledG,ledB);
        } else {
            Serial.print("连接失败，失败原因:");
            Serial.print(haClient.state());
            Serial.println("在一秒后重新连接");
            delay(1000);
            ledAll(255,0,0);
        }

        if (count > 30){
            ledAll(255,0,0);
            Serial.println("HA连接超时!请检查你的配置");
            Serial.println("HAip地址["+String(ha_mqtt_broker)+"]");
            Serial.println("HA账号["+String(ha_mqtt_user)+"]");
            Serial.println("HA密码["+String(ha_mqtt_password)+"]");
            Serial.println("本次输出[]内没有内置空格!");
            Serial.println("你将有两种选择:");
            Serial.println("1:已经确认我的配置没有问题!继续重试!");
            Serial.println("2:我的配置有误,删除配置重新书写");
            Serial.println("请输入你的选择:");
            while (Serial.available() == 0){
                Serial.print(".");
                delay(100);
            }
            String content = Serial.readString();
            if (content == "2"){if(LittleFS.remove("/config.json")){Serial.println("SUCCESS!");ESP.restart();}}
            ESP.restart();
        }
    }
}

void haCallback(char* topic, byte* payload, unsigned int length) {
    JsonDocument data;
    deserializeJson(data, payload, length);
    serializeJsonPretty(data,Serial);
    // 手动释放内存
    JsonDocument PData = getPData();
    JsonDocument CData = getCData();

    if (data["command"] == "onTun"){
        PData["lastFilament"] = data["value"].as<String>();
    }else if (data["command"] == "svAng"){
        sv.writeAngle(data["value"].as<String>().toInt());
    }else if (data["command"] == "step"){
        PData["step"] = data["value"].as<String>();
    }else if (data["command"] == "subStep"){
        PData["subStep"] = data["value"].as<String>();
    }else if (data["command"] == "wifiName"){
        CData["wifiName"] = data["value"].as<String>();
        wifiName = data["value"].as<String>();
    }else if (data["command"] == "wifiKey"){
        CData["wifiKey"] = data["value"].as<String>();
        wifiKey = data["value"].as<String>();
    }else if (data["command"] == "bambuIPAD"){
        CData["bambu_mqtt_broker"] = data["value"].as<String>();
        bambu_mqtt_broker = data["value"].as<String>();
    }else if (data["command"] == "bambuSID"){
        CData["bambu_device_serial"] = data["value"].as<String>();
        bambu_device_serial = data["value"].as<String>();
    }else if (data["command"] == "bambuKey"){
        CData["bambu_mqtt_password"] = data["value"].as<String>();
        bambu_mqtt_password = data["value"].as<String>();
    }else if (data["command"] == "LedBri"){
        ledBrightness = data["value"].as<String>().toInt();
        leds.setBrightness(ledBrightness);
        CData["ledBrightness"] = ledBrightness;
    }else if (data["command"] == "command"){
        commandStr = data["value"].as<String>();
        haClient.publish(("AMS/"+filamentID+"/command").c_str(),data["value"].as<String>().c_str());
        haClient.publish(("AMS/"+filamentID+"/command").c_str(),"");
    }else if (data["command"] == "mcState"){
        if (data["value"] == "前进"){
            mc.forward();
        }else if (data["value"] == "后退"){
            mc.backforward();
        }else if (data["value"] == "停止"){
            mc.stop();
        }
    }else if (data["command"] == "svState"){
        if (data["value"] == "推"){
            sv.push();
        }else if (data["value"] == "拉"){
            sv.pull();
        }
    }else if (String(data["command"]).indexOf("filaLig") != -1){
        if (String(data["command"]).indexOf("swi") != -1){
            if (data["value"] == "ON"){
                leds.setBrightness(ledBrightness);
                haClient.publish(("AMS/"+filamentID+"/filaLig/swi").c_str(),"{\"command\":\"filaLigswi\",\"value\":\"ON\"}");
            }else if (data["value"] == "OFF"){
                leds.setBrightness(0);
                haClient.publish(("AMS/"+filamentID+"/filaLig/swi").c_str(),"{\"command\":\"filaLigswi\",\"value\":\"OFF\"}");
            }
        }else if (String(data["command"]).indexOf("bri") != -1){
            ledBrightness = data["value"].as<String>().toInt();
            leds.setBrightness(ledBrightness);
            CData["ledBrightness"] = ledBrightness;
        }else if (String(data["command"]).indexOf("rgb") != -1){
            String input = String(data["value"]);
            int comma1 = input.indexOf(',');
            int comma2 = input.indexOf(',', comma1 + 1);

            int r = input.substring(0, comma1).toInt();
            int g = input.substring(comma1 + 1, comma2).toInt();
            int b = input.substring(comma2 + 1).toInt();

            ledR = r;
            ledG = g;
            ledB = b;

            CData["ledR"] = ledR;
            CData["ledG"] = ledG;
            CData["ledB"] = ledB;
        }
    }else if (data["command"] == "filamentTemp"){
        filamentTemp = data["value"].as<int>();
        CData["filamentTemp"] = filamentTemp;
    }else if (data["command"] == "filamentType"){
        filamentType = data["value"].as<String>();
        CData["filamentType"] = filamentType;
    }else if (data["command"] == "backTime"){
        backTime = (data["value"].as<String>()).toInt();
        CData["backTime"] = backTime;
    }
    
    writePData(PData);
    writeCData(CData);
    haClient.publish(("AMS/"+filamentID+"/nowTun").c_str(),filamentID.c_str());
    haClient.publish(("AMS/"+filamentID+"/nextTun").c_str(),PData["nextFilament"].as<String>().c_str());
    haClient.publish(("AMS/"+filamentID+"/onTun").c_str(),PData["lastFilament"].as<String>().c_str());
    haClient.publish(("AMS/"+filamentID+"/svAng").c_str(),String(sv.getAngle()).c_str());
    haClient.publish(("AMS/"+filamentID+"/step").c_str(),PData["step"].as<String>().c_str());
    haClient.publish(("AMS/"+filamentID+"/subStep").c_str(),PData["subStep"].as<String>().c_str());
    haClient.publish(("AMS/"+filamentID+"/wifiName").c_str(),wifiName.c_str());
    haClient.publish(("AMS/"+filamentID+"/wifiKey").c_str(),wifiKey.c_str());
    haClient.publish(("AMS/"+filamentID+"/bambuIPAD").c_str(),bambu_mqtt_broker.c_str());
    haClient.publish(("AMS/"+filamentID+"/bambuSID").c_str(),bambu_device_serial.c_str());
    haClient.publish(("AMS/"+filamentID+"/bambuKey").c_str(),bambu_mqtt_password.c_str());
    haClient.publish(("AMS/"+filamentID+"/LedBri").c_str(),String(ledBrightness).c_str());
    haClient.publish(("AMS/"+filamentID+"/mcState").c_str(),mc.getState().c_str());
    haClient.publish(("AMS/"+filamentID+"/svState").c_str(),sv.getState().c_str());
    haClient.publish(("AMS/"+filamentID+"/filaLig/bri").c_str(),String(ledBrightness).c_str());
    haClient.publish(("AMS/"+filamentID+"/filaLig/rgb").c_str(),((String(ledR)+","+String(ledG)+","+String(ledB))).c_str());
    haClient.publish(("AMS/"+filamentID+"/filamentTemp").c_str(),String(filamentTemp).c_str());
    haClient.publish(("AMS/"+filamentID+"/filamentType").c_str(),String(filamentType).c_str());
}

//定时任务
void bambuTimerCallback() {
    if (debug){Serial.println("bambu定时任务执行！");}
    bambuClient.publish(bambu_topic_publish.c_str(), bambu_status.c_str());
}
//定时任务
void haTimerCallback() {
    if (debug){Serial.println("ha定时任务执行！");}
    JsonDocument PData = getPData();
    haClient.publish(("AMS/"+filamentID+"/nowTun").c_str(),filamentID.c_str());
    haClient.publish(("AMS/"+filamentID+"/nextTun").c_str(),PData["nextFilament"].as<String>().c_str());
    haClient.publish(("AMS/"+filamentID+"/onTun").c_str(),PData["lastFilament"].as<String>().c_str());
    haClient.publish(("AMS/"+filamentID+"/svAng").c_str(),String(sv.getAngle()).c_str());
    haClient.publish(("AMS/"+filamentID+"/step").c_str(),PData["step"].as<String>().c_str());
    haClient.publish(("AMS/"+filamentID+"/subStep").c_str(),PData["subStep"].as<String>().c_str());
    haClient.publish(("AMS/"+filamentID+"/wifiName").c_str(),wifiName.c_str());
    haClient.publish(("AMS/"+filamentID+"/wifiKey").c_str(),wifiKey.c_str());
    haClient.publish(("AMS/"+filamentID+"/bambuIPAD").c_str(),bambu_mqtt_broker.c_str());
    haClient.publish(("AMS/"+filamentID+"/bambuSID").c_str(),bambu_device_serial.c_str());
    haClient.publish(("AMS/"+filamentID+"/bambuKey").c_str(),bambu_mqtt_password.c_str());
    haClient.publish(("AMS/"+filamentID+"/LedBri").c_str(),String(ledBrightness).c_str());
    haClient.publish(("AMS/"+filamentID+"/mcState").c_str(),mc.getState().c_str());
    haClient.publish(("AMS/"+filamentID+"/svState").c_str(),sv.getState().c_str());
    haClient.publish(("AMS/"+filamentID+"/backTime").c_str(),String(backTime).c_str());
    haClient.publish(("AMS/"+filamentID+"/filaLig/rgb").c_str(),((String(ledR)+","+String(ledG)+","+String(ledB))).c_str());
    haClient.publish(("AMS/"+filamentID+"/filamentTemp").c_str(),String(filamentTemp).c_str());
    haClient.publish(("AMS/"+filamentID+"/filamentType").c_str(),String(filamentType).c_str());

    haLastTime = millis();
}

JsonArray initText(String name,String id,String detail,JsonArray array){
    String topic = "homeassistant/text/ams"+id+detail+"/config";
    String json = ("{\"name\":\"" +name +"\",\"command_topic\":\"AMS/" +filamentID +"\",\"state_topic\":\"AMS/" +
    id +"/" +detail +"\",\"command_template\":\"{\\\"command\\\":\\\"" +detail +
    "\\\",\\\"value\\\":\\\"{{  value  }}\\\"}\",\"unique_id\": \"ams"+"text"+id+name+"\", \"device\":{\"identifiers\":\"APAMS"
    +id+"\",\"name\":\"AP-AMS-"+id+"通道\",\"manufacturer\":\"AP-AMS\",\"hw_version\":\""+sw_version+"\"}}");
    //Serial.println(json);
    array.add(topic);
    haClient.publish(topic.c_str(),json.c_str());
    return array;
}

JsonArray initSensor(String name,String id,String detail,JsonArray array){
    String topic = "homeassistant/sensor/ams"+id+detail+"/config";
    String json = ("{\"name\":\""+name+"\",\"state_topic\":\"AMS/"+id+"/"+detail+"\",\"unique_id\": \"ams"
    +"sensor"+id+name+"\", \"device\":{\"identifiers\":\"APAMS"+id+"\",\"name\":\"AP-AMS-"+id
    +"通道\",\"manufacturer\":\"AP-AMS\",\"hw_version\":\""+sw_version+"\"}}");
    array.add(topic);
    haClient.publish(topic.c_str(),json.c_str());
    return array;
}

JsonArray initSelect(String name,String id,String detail,String options,JsonArray array){
    String topic = "homeassistant/select/ams"+id+detail+"/config";
    String json = ("{\"name\":\"" +name +"\",\"command_topic\":\"AMS/" +filamentID +
    "\",\"state_topic\":\"AMS/" +id +"/" +detail +"\",\"command_template\":\"{\\\"command\\\":\\\"" +
    detail +"\\\",\\\"value\\\":\\\"{{  value  }}\\\"}\",\"options\":["+options+"],\"unique_id\": \"ams"+"select"
    +id+name+"\", \"device\":{\"identifiers\":\"APAMS"+id+"\",\"name\":\"AP-AMS-"+id
    +"通道\",\"manufacturer\":\"AP-AMS\",\"hw_version\":\""+sw_version+"\"}}");
    array.add(topic);
    haClient.publish(topic.c_str(),json.c_str());
    return array;
}

JsonArray initLight(String name,String id,String detail,JsonArray array){
    String topic = "homeassistant/light/ams"+id+detail+"/config";
    String json = "{\"name\":\"" + name + "\""
    + ",\"state_topic\":\"AMS/" + id + "/" + detail + "/swi\",\"command_topic\":\"AMS/" + id + "\","
    + "\"brightness_state_topic\":\"AMS/" + id + "/" + detail + "/bri\",\"brightness_command_topic\":\"AMS/" + id + "\","
    + "\"brightness_command_template\":\"{\\\"command\\\":\\\"" + detail + "bri\\\",\\\"value\\\":\\\"{{ value }}\\\"}\","
    + "\"rgb_state_topic\":\"AMS/" + id + "/" + detail + "/rgb\",\"rgb_command_topic\":\"AMS/" + id + "\","
    + "\"rgb_command_template\":\"{\\\"command\\\":\\\"" + detail + "rgb\\\",\\\"value\\\":\\\"{{ value }}\\\"}\","
    + "\"unique_id\":\"ams" + "light" + id + detail + "\","
    + "\"payload_on\":\"{\\\"command\\\":\\\"" + detail + "swi\\\",\\\"value\\\":\\\"ON\\\"}\","
    + "\"payload_off\":\"{\\\"command\\\":\\\"" + detail + "swi\\\",\\\"value\\\":\\\"OFF\\\"}\""
    + ",\"device\":{\"identifiers\":\"APAMS" + id + "\",\"name\":\"AP-AMS-" + id + "通道\",\"manufacturer\":\"AP-AMS\",\"hw_version\":\"" + sw_version + "\"}}";
    array.add(topic);
    haClient.publish(topic.c_str(),json.c_str());
    return array;
}

void setup() {
    leds.begin();
    Serial.begin(115200);
    LittleFS.begin();
    delay(1);
    leds.clear();
    leds.show();

    if (!LittleFS.exists("/config.json")) {
        ledAll(255,0,0);
        Serial.println("");
        Serial.println("不存在配置文件!创建配置文件!");
        Serial.println("1.请输入wifi名:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        wifiName = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+wifiName);

        delay(500);
        ledAll(255,0,0);
        
        Serial.println("2.请输入wifi密码:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        wifiKey = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+wifiKey);
        
        delay(500);
        ledAll(255,0,0);

        Serial.println("3.请输入拓竹打印机的ip地址:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        bambu_mqtt_broker = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+bambu_mqtt_broker);
        
        delay(500);
        ledAll(255,0,0);

        Serial.println("4.请输入拓竹打印机的访问码:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        bambu_mqtt_password = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+bambu_mqtt_password);
        
        delay(500);
        ledAll(255,0,0);
        
        Serial.println("5.请输入拓竹打印机的序列号:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        bambu_device_serial = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+bambu_device_serial);
        
        delay(500);
        ledAll(255,0,0);
    
        Serial.println("6.请输入本机通道编号:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        filamentID = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+filamentID);
        
        delay(500);
        ledAll(255,0,0);
    
        Serial.println("7.请输入ha服务器地址:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        ha_mqtt_broker = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+ha_mqtt_broker);
        
        delay(500);
        ledAll(255,0,0);
    
        Serial.println("8.请输入ha账号(无则输入“NONE”):");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        String message = Serial.readString();
        if (message != "NONE"){
            ha_mqtt_user = message;
        }
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+message);
        
        delay(500);
        ledAll(255,0,0);
    
        Serial.println("9.请输入ha密码(无则输入“NONE”):");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        String tmpmessage = Serial.readString();
        if (tmpmessage != "NONE"){
            ha_mqtt_password = tmpmessage;
        }
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+tmpmessage);

        delay(500);
        ledAll(255,0,0);

        Serial.println("10.请输入回抽延时[单位毫秒]:");
        while (!(Serial.available() > 0)){
            delay(100);
        }

        backTime = Serial.readString().toInt();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+String(backTime));
        
        Serial.println("11.请输入本通道耗材温度(后续可更改):");
        while (!(Serial.available() > 0)){
            delay(100);
        }

        filamentTemp = Serial.readString().toInt();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+String(filamentTemp));
        
        JsonDocument Cdata;
        Cdata["wifiName"] = wifiName;
        Cdata["wifiKey"] = wifiKey;
        Cdata["bambu_mqtt_broker"] = bambu_mqtt_broker;
        Cdata["bambu_mqtt_password"] = bambu_mqtt_password;
        Cdata["bambu_device_serial"] = bambu_device_serial;
        Cdata["filamentID"] = filamentID;
        Cdata["ledBrightness"] = 100;
        Cdata["ha_mqtt_broker"] = ha_mqtt_broker;
        Cdata["ha_mqtt_user"] = ha_mqtt_user;
        Cdata["ha_mqtt_password"] = ha_mqtt_password;
        Cdata["backTime"] = backTime;
        Cdata["filamentTemp"]  = filamentTemp;
        Cdata["filamentType"] = filamentType;
        ledR = 0;
        ledG = 0;
        ledB = 255;
        Cdata["ledR"] = ledR;
        Cdata["ledG"] = ledG;
        Cdata["ledB"] = ledB;
        ledBrightness = 100;
        writeCData(Cdata);
    }else{
        JsonDocument Cdata = getCData();
        serializeJsonPretty(Cdata,Serial);
        wifiName = Cdata["wifiName"].as<String>();
        wifiKey = Cdata["wifiKey"].as<String>();
        bambu_mqtt_broker = Cdata["bambu_mqtt_broker"].as<String>();
        bambu_mqtt_password = Cdata["bambu_mqtt_password"].as<String>();
        bambu_device_serial = Cdata["bambu_device_serial"].as<String>();
        filamentID = Cdata["filamentID"].as<String>();
        ledBrightness = Cdata["ledBrightness"].as<unsigned int>();
        ha_mqtt_broker = Cdata["ha_mqtt_broker"].as<String>();
        ha_mqtt_user = Cdata["ha_mqtt_user"].as<String>();
        ha_mqtt_password = Cdata["ha_mqtt_password"].as<String>();
        backTime = Cdata["backTime"].as<int>();
        filamentTemp = Cdata["filamentTemp"].as<int>();
        filamentType = Cdata["filamentType"].as<String>();
        ledR = Cdata["ledR"];
        ledG = Cdata["ledG"];
        ledB = Cdata["ledB"];
        ledAll(0,255,0);
    }
    bambu_topic_subscribe = "device/" + String(bambu_device_serial) + "/report";
    bambu_topic_publish = "device/" + String(bambu_device_serial) + "/request";
    ha_topic_subscribe = "AMS/"+filamentID;
    leds.setBrightness(ledBrightness);

    connectWF(wifiName,wifiKey);

    servo.attach(servoPin,500,2500);
    //servo.write(20);//初始20°方便后续调试

    pinMode(bufferPin1, INPUT_PULLDOWN_16);
    pinMode(bufferPin2, INPUT_PULLDOWN_16);

    bambuWifiClient.setInsecure();
    bambuClient.setServer(bambu_mqtt_broker.c_str(), 8883);
    bambuClient.setCallback(bambuCallback);
    bambuClient.setBufferSize(4096);
    haClient.setServer(ha_mqtt_broker.c_str(),1883);
    haClient.setCallback(haCallback);
    haClient.setBufferSize(4096);
    
    if (!LittleFS.exists("/data.json")) {
        JsonDocument Pdata;
        Pdata["lastFilament"] = "1";
        Pdata["step"] = "1";
        Pdata["subStep"] = "1";
        Pdata["filamentID"] = filamentID;
        writePData(Pdata);
        Serial.println("初始化数据成功！");
    } else {
        JsonDocument Pdata = getPData();
        Pdata["filamentID"] = filamentID;
        //Pdata["lastFilament"] = "1";//每次都将上一次的耗材定义为1(不建议使用)
        writePData(Pdata);
        serializeJsonPretty(Pdata, Serial);
        Serial.println("成功读取配置文件!");
    }

    connectBambuMQTT();
    connectHaMQTT();

    JsonDocument haData;
    JsonArray discoverList = haData["discovery_topic"].to<JsonArray>();

    discoverList = initText("上料通道",filamentID,"onTun",discoverList);
    discoverList = initText("舵机角度",filamentID,"svAng",discoverList);
    discoverList = initText("主要步骤",filamentID,"step",discoverList);
    discoverList = initText("次要步骤",filamentID,"subStep",discoverList);
    discoverList = initText("WIFI名",filamentID,"wifiName",discoverList);
    discoverList = initText("WIFI密码",filamentID,"wifiKey",discoverList);
    discoverList = initText("拓竹IP地址",filamentID,"bambuIPAD",discoverList);
    discoverList = initText("拓竹序列号",filamentID,"bambuSID",discoverList);
    discoverList = initText("拓竹访问码",filamentID,"bambuKey",discoverList);
    discoverList = initText("LED亮度",filamentID,"LedBri",discoverList);
    discoverList = initText("执行指令",filamentID,"command",discoverList);
    discoverList = initText("回抽延时",filamentID,"backTime",discoverList);
    discoverList = initText("耗材温度",filamentID,"filamentTemp",discoverList);
    discoverList = initText("耗材类型",filamentID,"filamentType",discoverList);
    discoverList = initSelect("电机状态",filamentID,"mcState","\"前进\",\"后退\",\"停止\"",discoverList);
    discoverList = initSelect("舵机状态",filamentID,"svState","\"推\",\"拉\",\"自定义角度\"",discoverList);
    discoverList = initSensor("状态",filamentID,"state",discoverList);
    discoverList = initSensor("本机通道",filamentID,"nowTun",discoverList);
    discoverList = initSensor("下一通道",filamentID,"nextTun",discoverList);
    discoverList = initLight("耗材指示灯",filamentID,"filaLig",discoverList);

    File file = LittleFS.open("/ha.json", "w");
    serializeJson(haData, file);
    file.close();
    Serial.println("初始化ha成功!");
    Serial.println("");
    serializeJsonPretty(haData,Serial);
    Serial.println("");

    haClient.publish(("AMS/"+filamentID+"/filaLig/swi").c_str(),"{\"command\":\"filaLigswi\",\"value\":\"ON\"}");
    haClient.publish(("AMS/"+filamentID+"/filaLig/bri").c_str(),String(ledBrightness).c_str());
    haClient.publish(("AMS/"+filamentID+"/filaLig/rgb").c_str(),((String(ledR)+","+String(ledG)+","+String(ledB))).c_str());
    Serial.println("-=-=-=setup执行完成!=-=-=-");
}

void loop() {
    if (!bambuClient.connected()) {
        connectBambuMQTT();
    }
    bambuClient.loop();
    
    if (!haClient.connected()) {
        connectHaMQTT();
    }
    haClient.loop();

    unsigned long nowTime =  millis();
    if (nowTime-bambuLastTime > bambuRenewTime and nowTime-bambuCheckTime > bambuRenewTime*0.8){
        bambuTimerCallback();
        bambuCheckTime = millis();
        leds.setPixelColor(0,leds.Color(10,255,10));
        leds.show();
        delay(10);
        leds.setPixelColor(0,leds.Color(0,0,0));
        leds.show();
    }
    if (nowTime-haLastTime > haRenewTime and nowTime-haCheckTime > haRenewTime*0.8){
        haTimerCallback();
        haCheckTime = millis();
        leds.setPixelColor(0,leds.Color(10,10,255));
        leds.show();
        delay(10);
        leds.setPixelColor(0,leds.Color(0,0,0));
        leds.show();
    }

    if (not mc.getStopState()){
        if (digitalRead(bufferPin1) == 1 or digitalRead(bufferPin2) == 1){
        mc.stop();}
        delay(100);
    }

    if (Serial.available()>0 or commandStr != ""){

        String content;
        if (Serial.available()>0){
            content = Serial.readString();
        }else if (commandStr != ""){
            content = commandStr;
            commandStr = "";
        }

        if (content=="delet config"){
            if(LittleFS.remove("/config.json")){Serial.println("SUCCESS!");ESP.restart();}
        }else if (content == "delet data")
        {
            if(LittleFS.remove("/data.json")){Serial.println("SUCCESS!");ESP.restart();}
        }else if (content == "delet ha")
        {
            if(LittleFS.remove("/ha.json")){Serial.println("SUCCESS!");ESP.restart();}
        }else if (content == "confirm")
        {
            bambuClient.publish(bambu_topic_publish.c_str(),bambu_done.c_str());
            Serial.println("confirm SEND!");
        }else if (content == "resume")
        {
            bambuClient.publish(bambu_topic_publish.c_str(),bambu_resume.c_str());
            Serial.println("resume SEND!");
        }else if (content == "debug")
        {
            debug = not debug;
            Serial.println("debug change");
        }else if (content == "push")
        {
            sv.push();
            Serial.println("push COMPLETE");
        }else if (content == "pull")
        {
            sv.pull();
            Serial.println("pull COMPLETE");
        }else if (content.indexOf("sv") != -1)
        {   
            String numberString = "";
            for (unsigned int i = 0; i < content.length(); i++) {
            if (isdigit(content[i])) {
                numberString += content[i];
            }
            }
            int number = numberString.toInt(); 
            sv.writeAngle(number);
            Serial.println("["+numberString+"]COMPLETE");
        }else if (content == "forward" or content == "fw")
        {
            mc.forward();
            Serial.println("forwarding!");
        }else if (content == "backforward" or content == "bfw")
        {
            mc.backforward();
            Serial.println("backforwarding!");
        }else if (content == "stop"){
            mc.stop();
            Serial.println("Stop!");
        }else if (content.indexOf("renewTime") != -1 or content.indexOf("rt") != -1)        {
            String numberString = "";
            for (unsigned int i = 0; i < content.length(); i++) {
            if (isdigit(content[i])) {
                numberString += content[i];
            }}
            unsigned int number = numberString.toInt();
            bambuRenewTime = number;
            Serial.println("["+numberString+"]COMPLETE");
        }else if (content.indexOf("ledbright") != -1 or content.indexOf("lb") != -1)        {
            String numberString = "";
            for (unsigned int i = 0; i < content.length(); i++) {
            if (isdigit(content[i])) {
                numberString += content[i];
            }}
            unsigned int number = numberString.toInt();
            ledBrightness = number;
            JsonDocument Cdata = getCData();
            Cdata["ledBrightness"] = ledBrightness;
            writeCData(Cdata);
            Serial.println("["+numberString+"]修改成功！亮度重启后生效");
        }else if (content == "rgb"){
            Serial.println("RGB Testing......");
            ledAll(255,0,0);
            delay(1000);
            ledAll(0,255,0);
            delay(1000);
            ledAll(0,0,255);
            delay(1000);
        }else if (content == "delet all ha device")
        {
            File file = LittleFS.open("/ha.json", "r");
            JsonDocument haData;
            deserializeJson(haData, file);
            file.close();
            JsonArray list = haData["discovery_topic"].as<JsonArray>();
            for (JsonVariant value : list) {
                String topic = value.as<String>();
                haClient.publish(topic.c_str(),"");
                Serial.println("已删除["+topic+"]");
            }
        }
    }
}
