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
//-=-=-=-=-=-=-=-=↑用户配置↑-=-=-=-=-=-=-=-=-=-=

//-=-=-=-=-=-↓系统配置↓-=-=-=-=-=-=-=-=-=
bool debug = false;
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
unsigned int renewTime = 1250;//定时任务刷新时间
int backTime = 1000;
unsigned int ledBrightness;//led默认亮度
#define ledPin 12//led引脚
#define ledPixels 3//led数量
//-=-=-=-=-=-↑系统配置↑-=-=-=-=-=-=-=-=-=

//-=-=-=-=-=-mqtt回调逻辑需要的变量-=-=-=-=-=-=
bool unloadMsg;
bool completeMSG;
//-=-=-=-=-=-=end

unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
WiFiClientSecure espClient;
PubSubClient client(espClient);
Adafruit_NeoPixel leds(ledPixels, ledPin, NEO_GRB + NEO_KHZ800);

unsigned long lastTime = millis();
unsigned long checkTime = millis();//mqtt定时任务
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
        ledAll(255,0,0);
    }
}

//获取配置数据
JsonDocument getCData(){
    File file = LittleFS.open("/config.json", "r");
    JsonDocument Cdata;
    deserializeJson(Cdata, file);
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
    }

    void backforward() {
      digitalWrite(pin1, LOW);
      digitalWrite(pin2, HIGH);
      isStop = false;
    }

    void stop() {
      digitalWrite(pin1, HIGH);
      digitalWrite(pin2, HIGH);
      isStop = true;
    }

    bool getStopState() {
        return isStop;
    }
};
class ServoControl {
  public:
    ServoControl(){
    }
    void push() {
        servo.write(0);
    }
    void pull() {
        servo.write(180);
    }
    void writeAngle(int angle) {
        servo.write(angle);
    }
};
//定义电机舵机变量
ServoControl sv;
Machinery mc(motorPin1, motorPin2);

//连接mqtt
void connectMQTT() {
    int count = 1;
    while (!client.connected()) {
        count ++;
        Serial.println("尝试连接拓竹mqtt|"+bambu_mqtt_id+"|"+bambu_mqtt_user+"|"+bambu_mqtt_password);
        if (client.connect(bambu_mqtt_id.c_str(), bambu_mqtt_user.c_str(), bambu_mqtt_password.c_str())) {
            Serial.println("连接成功!");
            Serial.println(bambu_topic_subscribe);
            client.subscribe(bambu_topic_subscribe.c_str());
            ledAll(0,0,255);
        } else {
            Serial.print("连接失败，失败原因:");
            Serial.print(client.state());
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

//mqtt回调
void callback(char* topic, byte* payload, unsigned int length) {
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
            Serial.println(sequenceId+"|ams["+amsStatus+"]"+"|err["+printError+"]"+"|hw["+hwSwitchState+"]"+"|gcode["+gcodeState+"]"+"|mcper["+mcPercent+"]"+"|mcrtime["+mcRemainingTime+"]");
            Serial.print("Free memory: ");
            Serial.print(ESP.getFreeHeap());
            Serial.println(" bytes");
            Serial.println("-=-=-=-=-");}
        lastTime = millis();
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
    if (Pdata["step"] == "1"){
        if (gcodeState == "PAUSE" and mcPercent.toInt() > 100){
            Serial.println("收到换色指令，进入换色准备状态");
            leds.setPixelColor(2,leds.Color(255,0,0));
            leds.setPixelColor(1,leds.Color(0,255,0));
            leds.setPixelColor(0,leds.Color(0,0,255));
            leds.show();
            String nextFilament = String(mcPercent.toInt() - 110 + 1);
            Pdata["nextFilament"] = nextFilament;
            unloadMsg = false;
            completeMSG = false;
            sv.pull();
            mc.stop();
            Serial.println("本机通道"+String(Pdata["filamentID"])+"|上料通道"+String(Pdata["lastFilament"])+"|下一耗材通道"+nextFilament);

            if (Pdata["filamentID"] == Pdata["lastFilament"]){
                Serial.println("本机通道["+String(Pdata["filamentID"])+"]在上料");//如果处于上料状态，那么对于这个换色单元来说，接下来只能退料或者继续打印（不退料）
                if (nextFilament == Pdata["filamentID"]){
                    Serial.println("本机通道,上料通道,下一耗材通道全部相同!无需换色!");
                    Pdata["step"] = "5";
                    Pdata["subStep"] = "1";
                }else{
                    Serial.println("下一耗材通道与本机通道不同，需要换料，准备退料");
                    Pdata["step"] = "2";
                    Pdata["subStep"] = "1";
                }
            }else{
                Serial.println("本机通道["+String(Pdata["filamentID"])+"]不在上料");//如果本换色单元不在上料，那么又两个可能，要么本次换色与自己无关，要么就是要准备进料
                if (nextFilament == Pdata["filamentID"]){
                    Serial.println("本机通道将要换色，准备送料");
                    Pdata["step"] = String("3");
                    Pdata["subStep"] = String("1");
                }else{
                    Serial.println("本机通道与本次换色无关，无需换色");
                    Pdata["step"] = String("4");
                    Pdata["subStep"] = String("1");
                }
            }
        }else{ledAll(0,0,255);}
    }else if (Pdata["step"] == "2"){
        if (Pdata["subStep"] == "1"){
            Serial.println("进入退料状态");
            leds.clear();
            leds.setPixelColor(2,leds.Color(255,255,255));
            leds.show();
            client.publish(bambu_topic_publish.c_str(),bambu_unload.c_str());
            Pdata["subStep"] = "2";
        }else if (Pdata["subStep"] == "2"){
            leds.setPixelColor(1,leds.Color(255,255,255));
            leds.show();
            if (printError == "318750723") {
                Serial.println("拔出耗材");
                sv.push();
                mc.backforward();
                Pdata["subStep"] = "3";
            } else if (printError == "318734339") {
                Serial.println("拔出耗材");
                sv.push();
                mc.backforward();
                client.publish(bambu_topic_publish.c_str(), bambu_resume.c_str());
                Pdata["subStep"] = "3";
            }
        }else if (Pdata["subStep"] == "3" && amsStatus == "0"){
            Serial.println("退料完成，本次换色完成");
            leds.setPixelColor(2,leds.Color(255,255,255));
            leds.show();
            delay(backTime);
            sv.pull();
            mc.stop();
            Pdata["step"] = "4";
            Pdata["subStep"] = "1";
        }
    }else if (Pdata["step"] == "3"){
        if (Pdata["subStep"] == "1"){
            if (amsStatus == "0") {
                Serial.println("进入送料状态");
                leds.clear();
                leds.setPixelColor(2,leds.Color(255,255,0));
                leds.show();
                client.publish(bambu_topic_publish.c_str(), bambu_load.c_str());
                Pdata["subStep"] = "2"; // 更新 subStep
            } else {
                if (!unloadMsg){
                    Serial.println("等待耗材退料完成……");
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
        }else if (Pdata["subStep"] == "2" && printError == "318750726"){
            Serial.println("送入耗材");
            sv.push();
            mc.forward();
            Pdata["subStep"] = "3";

            leds.clear();
            leds.setPixelColor(2,leds.Color(255,255,0));
            leds.setPixelColor(1,leds.Color(255,255,0));
            leds.show();
        }else if ((Pdata["subStep"] == "3" && amsStatus == "262" && hwSwitchState == "1") or digitalRead(bufferPin1) == 1){
            Serial.println("停止送料");
            mc.stop();
            client.publish(bambu_topic_publish.c_str(),bambu_done.c_str());
            Pdata["subStep"] = "4";

            leds.setPixelColor(0,leds.Color(255,255,0));
            leds.show();
        }else if (Pdata["subStep"] == "4"){
            if (hwSwitchState == "0") {
                Serial.println("检测到送料失败，重新送料!");
                mc.backforward();
                delay(2000);
                mc.stop();
                Pdata["subStep"] = "2";
                
                leds.setPixelColor(2,leds.Color(255,255,0));
                leds.setPixelColor(1,leds.Color(255,0,0));
                leds.show();
            } else if (hwSwitchState == "1") {
                Serial.println("送料成功，等待挤出换料");
                sv.pull();
                Pdata["subStep"] = "5"; // 更新 subStep

                leds.setPixelColor(2,leds.Color(255,255,0));
                leds.setPixelColor(1,leds.Color(0,255,0));
                leds.show();
            }
        }else if (Pdata["subStep"] == "5"){
            if (printError == "318734343") {
                if (hwSwitchState == "1"){
                    Serial.println("被虚晃一枪！重新点击确认");
                    client.publish(bambu_topic_publish.c_str(), bambu_done.c_str());
                    leds.setPixelColor(2,leds.Color(255,255,0));
                    leds.setPixelColor(1,leds.Color(0,255,0));
                    leds.setPixelColor(0,leds.Color(0,255,0));
                    leds.show();
                }else if (hwSwitchState == "0"){
                    Serial.println("检测到送料失败……进入步骤AGAIN重新送料");
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
                Serial.println("芜湖~换料完成！");
                ledAll(0,255,0);
                delay(1000);
                client.publish(bambu_topic_publish.c_str(), bambu_resume.c_str());
                Pdata["step"] = "4";
                Pdata["subStep"] = "1";
            }
        }else if (Pdata["subStep"] == "AGAIN"){
            if (hwSwitchState == "0"){
                Serial.println("尝试重新送料");
                mc.forward();
            }else if (hwSwitchState == "1"){
                Serial.println("送料成功!");
                mc.stop();
                sv.pull();
                Pdata["subStep"] = "5";
            }
        }
    }else if (Pdata["step"] == "4"){
        if (!completeMSG){
            Serial.println("进入看戏状态,等待换色完成");
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
            String nextFilament = Pdata["nextFilament"].as<String>();
            Serial.println("换色完成！切换上料通道为["+nextFilament+"]");
            Pdata["step"] = "1";
            Pdata["subStep"] = "1";
            Pdata["lastFilament"] = nextFilament;
            ledAll(0,255,0);
        }
    }else if (Pdata["step"] == "5"){
        client.publish(bambu_topic_publish.c_str(),bambu_resume.c_str());
        if (!completeMSG){
            Serial.println("发送继续指令");
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
            Serial.println("完成!");
            Pdata["step"] = "1";
            Pdata["subStep"] = "1";
            ledAll(0,255,0);
        }
    }
    
    writePData(Pdata);
}

//定时任务
void timerCallback() {
    if (debug){Serial.println("定时任务执行！");}
    client.publish(bambu_topic_publish.c_str(), bambu_status.c_str());
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
    
        Serial.println("7.请输入回抽延时[单位毫秒]:");
        while (!(Serial.available() > 0)){
            delay(100);
        }

        backTime = Serial.readString().toInt();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+String(backTime));

        JsonDocument Cdata;
        Cdata["wifiName"] = wifiName;
        Cdata["wifiKey"] = wifiKey;
        Cdata["bambu_mqtt_broker"] = bambu_mqtt_broker;
        Cdata["bambu_mqtt_password"] = bambu_mqtt_password;
        Cdata["bambu_device_serial"] = bambu_device_serial;
        Cdata["filamentID"] = filamentID;
        Cdata["ledBrightness"] = 100;
        Cdata["backTime"] = backTime;
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
        backTime = Cdata["backTime"].as<int>();
        ledAll(0,255,0);
    }
    bambu_topic_subscribe = "device/" + String(bambu_device_serial) + "/report";
    bambu_topic_publish = "device/" + String(bambu_device_serial) + "/request";
    leds.setBrightness(ledBrightness);

    connectWF(wifiName,wifiKey);

    servo.attach(servoPin,500,2500);
    //servo.write(20);//初始20°方便后续调试

    pinMode(bufferPin1, INPUT_PULLDOWN_16);
    pinMode(bufferPin2, INPUT_PULLDOWN_16);

    espClient.setInsecure();
    client.setServer(bambu_mqtt_broker.c_str(), 8883);
    client.setCallback(callback);
    client.setBufferSize(4096);
    
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

    //[因为与pwm冲突已弃用]布置定时任务，向拓竹索要当前情况
    //timer1_attachInterrupt(timerCallback);  // 绑定定时器中断服务程序
    //timer1_enable(TIM_DIV256, TIM_EDGE, TIM_LOOP); // 设置定时器分频和模式
    //timer1_write(1000000);  // 设置定时器触发周期，单位为微秒，这里设置为 1 秒
}

void loop() {
    if (!client.connected()) {
        connectMQTT();
    }

    client.loop();

    if (millis()-lastTime > renewTime and millis()-checkTime > renewTime*0.8){
        timerCallback();
        checkTime = millis();
        leds.setPixelColor(0,leds.Color(10,255,10));
        leds.show();
        delay(10);
        leds.setPixelColor(0,leds.Color(0,0,0));
        leds.show();
    }

    if (not mc.getStopState()){
        if (digitalRead(bufferPin1) == 1 or digitalRead(bufferPin2) == 1){
        mc.stop();}
        delay(500);
    }

    if (Serial.available()>0){
        String content = Serial.readString();
        if (content=="delet config"){
            if(LittleFS.remove("/config.json")){Serial.println("SUCCESS!");ESP.restart();}
        }else if (content == "delet data")
        {
            if(LittleFS.remove("/data.json")){Serial.println("SUCCESS!");ESP.restart();}
        }else if (content == "confirm")
        {
            client.publish(bambu_topic_publish.c_str(),bambu_done.c_str());
            Serial.println("confirm SEND!");
        }else if (content == "resume")
        {
            client.publish(bambu_topic_publish.c_str(),bambu_resume.c_str());
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
            renewTime = number;
            Serial.println("["+numberString+"]COMPLETE");
        } else if (content.indexOf("ledbright") != -1 or content.indexOf("lb") != -1)        {
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
        }  else if (content == "rgb"){
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