import time
from machine import Pin, ADC # 模数转换库


class SoilMoistureSensor:

    def __init__(self, pin = 35, p_in = 35):
        self.pin = pin
        if pin:
            self.pin =  ADC(Pin(pin))
        if p_in:
            self.p_in = Pin(p_in, Pin.IN)
    
    def getMoisAo(self):
        """
            获取土壤湿度值()
        """
        p_in_value = self.p_in.value()
        return p_in_value
    
    def getMoisDo(self):
        """
            获取土壤湿度值
        """
        #标定湿土壤实测最少值3100，空气中值4100，
        return self.pin.read()

# if __name__ == '__main__':
#     # 模拟量
#     soilMoistureSensor = SoilMoistureSensor(pin = 34)
#     # 数字量
#     soilMoistureSensor = SoilMoistureSensor(p_in = 35)

#     for i in range(10):
#         print(soilMoistureSensor.getMoisDo())
#         print(soilMoistureSensor.getMoisAo())
#         time.sleep(1)        