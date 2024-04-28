import time
import json
import requests
from machine import Pin,Timer
from wifi import connect
from sparkfuc import spark
from file import exist
from umqtt.simple import MQTTClient

from servo import Servo
from machinery import Machinery

#-=-=-=-=-=-=-=-=↓用户配置↓-=-=-=-=-=-=-=-=-=-=
bambu_mqtt_broker = ""#输入拓竹打印机的ip地址
bambu_mqtt_password = ""#拓竹访问码
bambu_device_serial = ""#设备序列号
ha_mqtt_broker = None#homeassistant的mqtt地址,不需要则填None
ha_mqtt_user = None
ha_mqtt_password = None
wifi_name = ""#wifi名
wifi_key = ""#wifi密码
filamentID = 1#!!!非常重要，此项填写通道的id，比如我是第一通道那么就填1
#-=-=-=-=-=-=-=-=↑用户配置↑-=-=-=-=-=-=-=-=-=-=

#-=-=-=-=-=-↓高级配置(最好不要动)↓-=-=-=-=-=-=-=-=-=
bambu_topic_subscribe = f"device/{bambu_device_serial}/report"
bambu_topic_publish = f"device/{bambu_device_serial}/request"
bambu_mqtt_port = 8883#mqtt端口
bambu_mqtt_user = "bblp"#mqtt账号
bambu_client_id = "ams"#mqtt的cid
bambu_keepalive= 60#mqtt心跳
ha_topic_subscribe = "amstest"
ha_err_topic = "ams/err"
ha_mqtt_port = 1883
ha_client_id = 'ams'
ha_keepalive = 60
debug = True
#-=-=-=-=-=-↑系统配置(最好不要动)↑-=-=-=-=-=-=-=-=-=

global data
if not exist("data.json"):
    with open("data.json","w",encoding="utf-8") as f:
        data = {'lastFilament':1,'step':[1],'filamentID':filamentID}
        #初始化一些数据
        #lastFilament代表上一个耗材的通道id(从1开始)
        #step代表本机器执行的步骤数，用于断电保存进度,是一个二维列表，第一个元素是主步骤，第二元素是副步骤
        #主步骤1：准备状态，判断本机是否需要换色等等
        #主步骤2：退料
        #主步骤3：进料
        #副步骤看下面代码吧（
        #filamentID代表本机器对应的通道id(从1开始)
        json.dump(data,f)
else:
    with open("data.json","r",encoding="utf-8") as f:
        data = json.load(f)

sv = Servo(Pin(2),max_us=2500)
mc = Machinery(Pin(4),Pin(5))

wlan = connect(wifi_name,wifi_key)
def callback(topic, msg):
    global data
    if ha_mqtt_broker:
        ha.ping()
    #print(topic.decode(),msg.decode())
    msg = msg.decode()
    jsonData = json.loads(msg)
    if 'print' in jsonData:
        hb = jsonData['print']['sequence_id']
        if ha_mqtt_broker:
            ha.publish('ams/heartbreak',hb)
            #print(f'转发心跳{hb}')
        #print(data['step'])
        if data['step'][0] == 1:
            if "gcode_state" in jsonData["print"] and jsonData["print"]["gcode_state"] == "PAUSE" and "mc_percent" in jsonData["print"] \
               and "mc_remaining_time" in jsonData["print"] and jsonData["print"]["mc_percent"] == 101:
                print('收到换色指令，进入换色准备状态')
                next_filament = jsonData["print"]["mc_remaining_time"] + 1
                print(f'本机通道{data['filamentID']}|上料通道{data['lastFilament']}|下一耗材通道{next_filament}')
                if data['filamentID'] == data['lastFilament']:
                    print(f'本机通道{data['filamentID']}在上料')
                    if next_filament == data['filamentID']:
                        print(f'下一耗材通道与本机通道相同，无需退料，准备送料')
                        data['step'] = [3,0]
                    else:
                        print(f'下一耗材通道与本机通道不同，需要换料，准备退料')
                        data['step'] = [2,0]
                else:
                    print(f'本机通道{data['filamentID']}不在上料')
                    if next_filament == data['filamentID']:
                        print(f'本机通道将要换色，准备送料')
                        data['step'] = [3,0]
                    else:
                        print('本机通道与本次换色无关，无需换色')
                        data['step'] = [4,0]
            
        elif data['step'][0] == 2:
            if data['step'][1] == 0:
                print('进入退料状态')
                client.publish(bambu_topic_publish,'{"print":{"command":"ams_change_filament","curr_temp":220,"sequence_id":"1","tar_temp":220,"target":255},"user_id":"1"}')
                data['step'][1] += 1
            elif data['step'][1] == 1 and 'print_error' in jsonData["print"]:
                if jsonData["print"]['print_error'] == 318750723:
                    print('拔出耗材')
                    sv.push()
                    mc.backforward()
                    data['step'][1] += 1
                elif jsonData["print"]['print_error'] == 318734339:
                    print('拔出耗材')
                    sv.push()
                    mc.backforward()
                    client.publish(bambu_topic_publish,'{"print":{"command":"resume","sequence_id":"1"},"user_id":"1"}')
            elif data['step'][1] == 2 and 'ams_status' in jsonData["print"] and int(jsonData["print"]['ams_status']) == 0:
                print('退料完成,本次换色完成')
                sv.pull()
                mc.stop()
                data['step'] = [4,0]
            
        elif data['step'][0] == 3:
            if data['step'][1] == 0:
                if  'ams_status' in jsonData["print"] and int(jsonData["print"]['ams_status']) == 0:
                    print('进入送料状态')
                    client.publish(bambu_topic_publish,'{"print":{"command":"ams_change_filament","curr_temp":220,"sequence_id":"1","tar_temp":220,"target":254},"user_id":"1"}')
                    data['step'][1] += 1
                else:
                    print('等待耗材退料完成……')
            elif data['step'][1] == 1 and 'print_error' in jsonData["print"] and int(jsonData["print"]['print_error']) == 318750726:
                print('送入耗材')
                sv.push()
                mc.foward()
                data['step'][1] += 1
            elif data['step'][1] == 2 and 'ams_status' in jsonData["print"] and int(jsonData["print"]['ams_status']) == 262\
                and 'hw_switch_state' in jsonData["print"] and int(jsonData["print"]['hw_switch_state']) == 1:
                print('停止送料')
                mc.stop()
                client.publish(bambu_topic_publish,'{"print":{"command":"ams_control","param":"done","sequence_id":"1"},"user_id":"1"}')
                data['step'][1] += 1
            elif data['step'][1] == 3 and 'hw_switch_state' in jsonData["print"]:
                if int(jsonData["print"]['hw_switch_state']) == 0:
                    print('检测到送料失败,重新送料!')
                    mc.backforward()
                    time.sleep(2)
                    mc.stop()
                    data['step'][1] = 0
                elif int(jsonData["print"]['hw_switch_state']) == 1:
                    print('送料成功，等待挤出换料')
                    sv.pull()
                    data['step'][1] += 1
            elif data['step'][1] == 4:
                if 'print_error' in jsonData["print"] and int(jsonData["print"]['print_error']) == 318734343:
                    if 'hw_switch_state' in jsonData["print"] and int(jsonData["print"]['hw_switch_state']) == 1:
                        print('被虚晃一枪!重新点击确认')
                        client.publish(bambu_topic_publish,'{"print":{"command":"ams_control","param":"done","sequence_id":"1"},"user_id":"1"}')
                    elif 'hw_switch_state' in jsonData["print"] and int(jsonData["print"]['hw_switch_state']) == 0:
                        print('检测到送料失败……进入步骤-10重新送料')
                        data['step'][1] = -10
                elif 'ams_status' in jsonData["print"] and int(jsonData["print"]['ams_status']) == 768:
                    print('芜湖~换料完成！')
                    time.sleep(1)
                    client.publish(bambu_topic_publish,'{"print":{"command":"resume","sequence_id":"1"},"user_id":"1"}')
                    data['step'] = [4,0]
            elif data['step'][1] == -10:
                if 'hw_switch_state' in jsonData["print"] and int(jsonData["print"]['hw_switch_state']) == 0:
                    print('尝试重新送料')
                    sv.push()
                    mc.forward()
                elif 'hw_switch_state' in jsonData["print"] and int(jsonData["print"]['hw_switch_state']) == 1:
                    print('送料成功!')
                    mc.stop()
                    sv.pull()
                    data['step'][1] = 4
        elif data['step'][0] == 4:
            print('进入看戏状态,等待换色完成')
            if 'ams_status' in jsonData["print"] and int(jsonData["print"]['ams_status']) == 1280:
                print('换色完成！')
                data['step'] = [1,0]

        with open("data.json","w",encoding="utf-8") as f:
            json.dump(data,f)

    elif debug:
        if ha_mqtt_broker:
            ha.publish('ams/heartbreak',str(jsonData).encode())
            print(f'转发数据包{jsonData}')
        
# 连接到 MQTT 服务器
client = MQTTClient(bambu_client_id,bambu_mqtt_broker,bambu_mqtt_port,bambu_mqtt_user,bambu_mqtt_password,keepalive=bambu_keepalive,ssl = True)
client.set_callback(callback)
client.connect()

def hac(topic, msg):
    print(topic.decode(),msg.decode())
    
if ha_mqtt_broker:
    ha = MQTTClient(ha_client_id,ha_mqtt_broker,ha_mqtt_port,ha_mqtt_user,ha_mqtt_password,keepalive=ha_keepalive)
    ha.connect()
    ha.set_callback(hac)

# 订阅主题
client.subscribe(bambu_topic_subscribe)
if ha_mqtt_broker:
    ha.subscribe(ha_topic_subscribe)

def timer_func(timer):
    print('sleep!')
    time.sleep(2)
    print('wake')
    
#timer = Timer(-1)
#timer.init(period=5000, mode=Timer.PERIODIC, callback=timer_func)
 
print("进入mqtt循环")
while True:
    try:
        client.check_msg()
        if ha_mqtt_broker:
            ha.check_msg()
    except Exception as e:
        if str(e) == '-1':
            print(f"-被动心跳执行-{time.localtime()[3]}:{time.localtime()[4]}:{time.localtime()[5]}-")
            try:
                client.connect()
                ha.connect()
            except:machine.reset()
        else:
            if not exist("err.log"):
                with open("err.log","w",encoding="utf-8") as f:
                    f.write(str(e)+"\n---\n")
            else:
                with open("err.log","a",encoding="utf-8") as f:
                    f.write(str(e)+"\n---\n")
                    
            if ha_mqtt_broker:
                try:ha.publish(ha_err_topic,str(e))
                except:pass
                
            print(f"发生错误{e}")
            try:
                try:
                    print(requests.get("https://quan.suning.com/getSysTime.do").text)
                except:
                    print('wifi已断开，尝试重连wifi')
                    #重连wifi再尝试
                    count = 1
                    while True:
                        if count > 3:
                            machine.reset()
                        try:
                            wlan = connect(wifi_name,wifi_key)
                            print(requests.get("https://quan.suning.com/getSysTime.do").text)
                        except:
                            count += 1
                        else:break
                    client.connect()
                    if ha_mqtt_broker:
                        ha.connect()
                        ha.subscribe(ha_topic_subscribe)
                    client.subscribe(bambu_topic_subscribe)
                    #client.wait_msg()
                else:
                    print('wifi正常，尝试直接重连客户端')
                    client.connect()
                    if ha_mqtt_broker:
                        ha.connect()
                        ha.subscribe(ha_topic_subscribe)
                    client.subscribe(bambu_topic_subscribe)
                    #client.wait_msg()
            except:
                machine.reset()

