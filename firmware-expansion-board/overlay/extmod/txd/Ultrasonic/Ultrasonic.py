from machine import Pin, time_pulse_us
import time

class Ultrasonic:
    def __init__(self, trig_pin, echo_pin):
        self.trig = Pin(trig_pin, Pin.OUT)
        self.echo = Pin(echo_pin, Pin.IN)

    def distance(self):
        self.trig.value(0)
        # 使触发信号保持至少10微秒以上
        time.sleep_us(10)
        self.trig.value(1)
        # 等待触发信号超过10微秒，然后再把触发信号设置为低电平
        time.sleep_us(10)
        self.trig.value(0)
        # 等待接收到回声信号，计算回声持续时间并返回距离
        duration = time_pulse_us(self.echo, 1, 5000)  # 超时时间为5000微秒
        distance = duration / 58.0  # 根据回声持续时间计算距离（单位：厘米）
        return int(distance)

# if __name__ == '__main__':
#     ultrasonic = Ultrasonic(5, 4)
#     for i in range(10):
#         print(ultrasonic.distance())
#         time.sleep(1)
