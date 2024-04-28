import time
import network

def connect(ssid,password):
    # 创建 WIFI 连接对象
    wlan = network.WLAN(network.STA_IF)
    # 激活 wlan 接口
    wlan.active(True)
    # 扫描允许访问的 WiFi
    print('扫描周围信号源：', wlan.scan())

    print("正在连接 WiFi 中", end="")
    # 
    wlan.connect(ssid, password)

    # 如果一直没有连接成功，则每隔 0.1s 在命令号中打印一个 .
    count = 1
    while not wlan.isconnected():
      count += 1
      print(".", end="")
      time.sleep(0.1)
      if count == 200:
          count = 1
          print(f"重新扫描{wlan.scan()}")

    # 连接成功之后，打印出 IP、子网掩码(netmask)、网关(gw)、DNS 地址
    print(f"\n{wlan.ifconfig()}")


