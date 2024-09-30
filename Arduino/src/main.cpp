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
#define AGAIN 100//定义步骤特殊变量

//-=-=-=-=-=-=-=-=↓用户配置↓-=-=-=-=-=-=-=-=-=-=
String wifiName;//
String wifiKey;//
String bambu_mqtt_broker;//
String bambu_mqtt_password;//
String bambu_device_serial;//
int filamentID;//
String ha_mqtt_broker;
String ha_mqtt_user;
String ha_mqtt_password;
//-=-=-=-=-=-=-=-=↑用户配置↑-=-=-=-=-=-=-=-=-=-=

//-=-=-=-=-=-↓系统配置↓-=-=-=-=-=-=-=-=-=
bool debug = false;
String sw_version = "v2.5";
String bambu_mqtt_user = "bblp";
String bambu_mqtt_id = "ams";
String bambu_topic_subscribe;// = "device/" + String(bambu_device_serial) + "/report";
String bambu_topic_publish;// = "device/" + String(bambu_device_serial) + "/request";
String bambu_resume = "{\"print\":{\"command\":\"resume\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}"; // 重试|继续打印
String bambu_unload = "{\"print\":{\"command\":\"ams_change_filament\",\"curr_temp\":220,\"sequence_id\":\"1\",\"tar_temp\":220,\"target\":255},\"user_id\":\"1\"}";
String bambu_load = "{\"print\":{\"command\":\"ams_change_filament\",\"curr_temp\":220,\"sequence_id\":\"1\",\"tar_temp\":220,\"target\":254},\"user_id\":\"1\"}";
String bambu_done = "{\"print\":{\"command\":\"ams_control\",\"param\":\"done\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}"; // 完成
String bambu_clear = "{\"print\":{\"command\": \"clean_print_error\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}";
String bambu_status = "{\"pushing\": {\"sequence_id\": \"0\", \"command\": \"pushall\"}}";
int servoPin = 13;//舵机引脚
int motorPin1 = 4;//电机一号引脚
int motorPin2 = 5;//电机二号引脚
int bufferPin1 = 14;//缓冲器1
int bufferPin2 = 16;//缓冲器2
unsigned int bambuRenewTime = 1250;//拓竹更新时间
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
int lastFilament;
int nextFilament;
bool isFirstTime;//是否第一次换色
int step = 1;
int subStep = 1;
//-=-=-=-=-=-=end

unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
WiFiClientSecure bambuWifiClient;
PubSubClient bambuClient(bambuWifiClient);

Adafruit_NeoPixel leds(ledPixels, ledPin, NEO_GRB + NEO_KHZ800);

unsigned long bambuLastTime = millis();
unsigned long bambuCheckTime = millis();//mqtt定时任务
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
    pinMode(2,SPECIAL);
    Serial1.println("[通道"+String(filamentID)+"]"+content);
    Serial1.flush();
    pinMode(2,INPUT);
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
    String layerNum = (*data)["print"]["layer_num"].as<String>();
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

    if (gcodeState == "FAILED"){
        step = 1;
        subStep = 1;
    }
    if (gcodeState == "PAUSE" and mcPercent.toInt() > 100){
        nextFilament = mcPercent.toInt() - 110 + 1;
        lastFilament = layerNum.toInt() -10 + 1; 
        if (step == 1){
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
            statePublish("本机通道"+String(filamentID)+"|上料通道"+String(lastFilament)+"|下一耗材通道"+String(nextFilament));
            
            if (amsStatus == "768"){
                //如果已经换色结束,不再进行进退了操作
                statePublish("换色已结束!");
                step = 4;
                subStep = 1;
            }else if (filamentID == lastFilament){
                statePublish("本机通道["+String(filamentID)+"]在上料");//如果处于上料状态，那么对于这个换色单元来说，接下来只能退料或者继续打印（不退料）
                if (nextFilament == filamentID){
                    statePublish("本机通道,上料通道,下一耗材通道全部相同!无需换色!");
                    isFirstTime = false;
                    step = 5;
                    subStep = 1;
                }else{
                    if (amsStatus != "0"){
                        statePublish("下一耗材通道与本机通道不同，需要换料，准备退料");
                        step = 2;
                        subStep = 1;
                    }else{
                        statePublish("退料已结束！");
                        step = 4;
                        subStep = 1;
                    }
                }
            }else{
                statePublish("本机通道["+String(filamentID)+"]不在上料");//如果本换色单元不在上料，那么又两个可能，要么本次换色与自己无关，要么就是要准备进料
                if (nextFilament == filamentID){
                    if (lastFilament == -4){
                        statePublish("首次换色！先退料再送料！");
                        isFirstTime = true;
                        bambuClient.publish(bambu_topic_publish.c_str(),bambu_unload.c_str());
                    }else{
                        statePublish("本机通道将要换色，准备送料");
                        isFirstTime = false;
                    }
                    if (amsStatus == "258" or amsStatus == "261"){
                        subStep = 2;
                    }else if(amsStatus == "262"){
                        subStep = 3;
                    }else if(amsStatus == "263"){
                        subStep = 4;
                    }else{
                        subStep = 1;
                    }
                    subStep = 1;
                    step = 3;
                }else{
                    if (lastFilament == -4){
                        isFirstTime = false;
                        statePublish("首次换色……等待其它通道操作");
                    }
                    statePublish("本机通道与本次换色无关，无需换色");
                    step = 4;
                    subStep = 1;
                }
            }
        }else{
            ledAll(ledR,ledG,ledB);
            }
    }if (step == 2){
        if (subStep == 1){
            statePublish("进入退料状态");
            leds.clear();
            leds.setPixelColor(2,leds.Color(255,255,255));
            leds.show();
            bambuClient.publish(bambu_topic_publish.c_str(),bambu_unload.c_str());
            reSendUnload = true;
            subStep = 2;
        }else if (subStep == 2){
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
                subStep = 3;
            } else if (printError == "318734339" or printError == "134201347") {
                statePublish("拔出耗材");
                sv.push();
                mc.backforward();
                pullBackTimeOut = millis();
                pullBackTimes = 1;
                bambuClient.publish(bambu_topic_publish.c_str(), bambu_resume.c_str());
                subStep = 3;
            }
        }else if (subStep == 3){
            if (amsStatus == "0"){
                statePublish("退料完成，本次换色完成");
                leds.setPixelColor(2,leds.Color(255,255,255));
                leds.show();
                delay(backTime);
                sv.pull();
                mc.stop();
                step = 4;
                subStep = 1;
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
    }else if (step == 3){
        isSendFilament = true;
        if (subStep == 1){
            if (amsStatus == "0") {
                statePublish("进入送料状态");
                leds.clear();
                leds.setPixelColor(2,leds.Color(255,255,0));
                leds.show();
                bambuClient.publish(bambu_topic_publish.c_str(), bambu_load.c_str());
                subStep = 2; // 更新 subStep
            } else {
                if (!unloadMsg){
                    if (isFirstTime){statePublish("请手动拔出上料料线");}
                    else{statePublish("等待耗材退料完成……");}
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
        }else if (subStep == 2 && (printError == "318750726" or printError == "134201350")){
            statePublish("送入耗材");
            sv.push();
            mc.forward();
            subStep = 3;
            sendOutTimeOut = millis();
            sendOutTimes = 1;

            leds.clear();
            leds.setPixelColor(2,leds.Color(255,255,0));
            leds.setPixelColor(1,leds.Color(255,255,0));
            leds.show();
        }else if (subStep == 3){
            if ((amsStatus == "262" && hwSwitchState == "1") or digitalRead(bufferPin1) == 1){
                statePublish("停止送料");
                mc.stop();
                bambuClient.publish(bambu_topic_publish.c_str(),bambu_done.c_str());
                subStep = 4;

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
        }else if (subStep == 4){
            if (hwSwitchState == "0") {
                statePublish("检测到送料失败，重新送料!");
                mc.backforward();
                delay(2000);
                mc.stop();
                subStep = 2;
                
                leds.setPixelColor(2,leds.Color(255,255,0));
                leds.setPixelColor(1,leds.Color(255,0,0));
                leds.show();
            } else if (hwSwitchState == "1") {
                statePublish("送料成功，等待挤出换料");
                sv.pull(); 
                subStep = 5; // 更新 subStep

                leds.setPixelColor(2,leds.Color(255,255,0));
                leds.setPixelColor(1,leds.Color(0,255,0));
                leds.show();
            }
        }else if (subStep == 5){
            if (printError == "318734343" or printError == "134184967") {
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
                    subStep = AGAIN;
                }
            } else if (amsStatus == "768") {
                statePublish("芜湖~换料完成！");
                ledAll(0,255,0);
                delay(1000);
                bambuClient.publish(bambu_topic_publish.c_str(), bambu_resume.c_str());
                step = 4;
                subStep = 1;
            }
        }else if (subStep == AGAIN){
            if (hwSwitchState == "0"){
                statePublish("尝试重新送料");
                mc.forward();
            }else if (hwSwitchState == "1"){
                statePublish("送料成功!");
                mc.stop();
                sv.pull();
                subStep = 5;
            }
        }
    }else if (step == 4){
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
            statePublish("换色完成！切换上料通道为["+String(nextFilament)+"]");
            step = 1;
            subStep = 1;
            isFirstTime = false;
            lastFilament = nextFilament;
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
    }else if (step == 5){
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
            step = 1;
            subStep = 1;
            ledAll(0,255,0);
        }
    }
}

//定时任务
void bambuTimerCallback() {
    if (debug){Serial.println("bambu定时任务执行！");}
    bambuClient.publish(bambu_topic_publish.c_str(), bambu_status.c_str());
}

void setup() {
    leds.begin();
    Serial.begin(115200);
    Serial1.begin(115200);
    pinMode(2,INPUT);
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
        filamentID = Serial.readString().toInt();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+String(filamentID));
        
        delay(500);
        ledAll(255,0,0);

        Serial.println("7.请输入回抽延时[单位毫秒]:");
        while (!(Serial.available() > 0)){
            delay(100);
        }

        backTime = Serial.readString().toInt();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+String(backTime));
        
        Serial.println("8.请输入本通道耗材温度(后续可更改):");
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
        filamentID = Cdata["filamentID"].as<int>();
        ledBrightness = Cdata["ledBrightness"].as<unsigned int>();
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
    leds.setBrightness(ledBrightness);

    mc.stop();
    sv.pull();
    connectWF(wifiName,wifiKey);

    servo.attach(servoPin,500,2500);
    //servo.write(20);//初始20°方便后续调试

    pinMode(bufferPin1, INPUT_PULLDOWN_16);
    pinMode(bufferPin2, INPUT_PULLDOWN_16);

    bambuWifiClient.setInsecure();
    bambuClient.setServer(bambu_mqtt_broker.c_str(), 8883);
    bambuClient.setCallback(bambuCallback);
    bambuClient.setBufferSize(4096);

    connectBambuMQTT();
}

void loop() {
    if (!bambuClient.connected()) {
        connectBambuMQTT();
    }
    bambuClient.loop();

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

    if (not mc.getStopState()){
        if (digitalRead(bufferPin1) == 1 or digitalRead(bufferPin2) == 1){
        mc.stop();}
        delay(100);
    }

    if (Serial.available()){
        String GETcontent = Serial.readString(); 
        GETcontent.replace("\r", ""); // 移除所有的 \r 字符
        GETcontent.replace("\n", "");
        int msgID;
        String content;
        if (GETcontent.indexOf("|") != -1){
            msgID = GETcontent.substring(0,GETcontent.indexOf("|")).toInt();
            content = GETcontent.substring(GETcontent.indexOf("|") + 1);
        }else{
            msgID = filamentID;
            content = GETcontent;
        }
        if (msgID != filamentID){
            Serial.println(String(msgID)+"|"+content);
        }else{
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
            }
        }
    }    
}
