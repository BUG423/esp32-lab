import math
import ustruct
import time
from machine import I2C,Pin

class PCA9685:
    def __init__(self, i2c, address=0x40):
        self.i2c = i2c
        self.address = address
        self.reset()

    def _write(self, address, value):
        self.i2c.writeto_mem(self.address, address, bytearray([value]))

    def _read(self, address):
        return self.i2c.readfrom_mem(self.address, address, 1)[0]

    def reset(self):
        self._write(0x00, 0x00) # Mode1

    def freq(self, freq=None):
        if freq is None:
            return int(25000000.0 / 4096 / (self._read(0xfe) - 0.5))
        prescale = int(25000000.0 / 4096.0 / freq + 0.5)
        old_mode = self._read(0x00) # Mode 1
        self._write(0x00, (old_mode & 0x7F) | 0x10) # Mode 1, sleep
        self._write(0xfe, prescale) # Prescale
        self._write(0x00, old_mode) # Mode 1
        time.sleep_us(5)
        self._write(0x00, old_mode | 0xa1) # Mode 1, autoincrement on

    def pwm(self, index, on=None, off=None):
        if on is None or off is None:
            data = self.i2c.readfrom_mem(self.address, 0x06 + 4 * index, 4)
            return ustruct.unpack('<HH', data)
        data = ustruct.pack('<HH', on, off)
        self.i2c.writeto_mem(self.address, 0x06 + 4 * index,  data)

    def duty(self, index, value=None, invert=False):
        if value is None:
            pwm = self.pwm(index)
            if pwm == (0, 4096):
                value = 0
            elif pwm == (4096, 0):
                value = 4095
            value = pwm[1]
            if invert:
                value = 4095 - value
            return value
        if not 0 <= value <= 4095:
            raise ValueError("Out of range")
        if invert:
            value = 4095 - value
        if value == 0:
            self.pwm(index, 0, 4096)
        elif value == 4095:
            self.pwm(index, 4096, 0)
        else:
            self.pwm(index, 0, value)

class ServoMotor:
    def __init__(self, scl = 25, sda = 26 , address=0x40, freq=50, min_us=600, max_us=2400,
                 degrees=180):
        i2c=I2C(sda=Pin(26),scl=Pin(25),freq=10000)
        self.period = 1000000 / freq
        self.min_duty = self._us2duty(min_us)
        self.max_duty = self._us2duty(max_us)
        self.degrees = degrees
        self.freq = freq
        self.pca9685 = PCA9685(i2c, address)
        self.pca9685.freq(freq)

    def _us2duty(self, value):
        return int(4095 * value / self.period)
    
    def set_angle(self, channel, degrees):
        self.position(channel, degrees=degrees)

    def set_speed(self, channel, speed):
        if speed > 100:
            speed = 100
        elif speed<0:
            speed = 0
        pusle = int(speed * 4095 / 100)
        self.pca9685.duty(channel, pusle)


    def position(self, index, degrees=None, radians=None, us=None, duty=None):
        span = self.max_duty - self.min_duty
        if degrees is not None:
            duty = self.min_duty + span * degrees / self.degrees
        elif radians is not None:
            duty = self.min_duty + span * radians / math.radians(self.degrees)
        elif us is not None:
            duty = self._us2duty(us)
        elif duty is not None:
            pass
        else:
            return self.pca9685.duty(index)
        duty = min(self.max_duty, max(self.min_duty, int(duty)))
        self.pca9685.duty(index, duty)

    def release(self, index):
        self.pca9685.duty(index, 0)

async def turn_motor(servos,j):
    """
    控制一个电机的运行方向和速度
    :param motor: 电机控制引脚，如Pin(0, Pin.OUT)等
    :param direction: 电机运行方向，1为正转，-1为反转
    :param speed: 电机运行速度，0-1023之间的数值
    """
    # 休眠0.1秒
    for i in range(0, 180, 10):
        servos.set_angle(j,i)
        await asyncio.sleep(0.1)

    for i in range(180, 0, -10):
        servos.set_angle(j,i)
        await asyncio.sleep(0.1)
    
        

async def run_motors(servos):
    """
    同时控制多个电机的运行方向和速度
    """

    tasks = [
        asyncio.create_task(turn_motor(servos,0)),
        asyncio.create_task(turn_motor(servos,1))
    ]

    await asyncio.gather(*tasks)

    # 创建事件循环对象
    loop = asyncio.get_event_loop()
    # 运行异步任务
    loop.run_until_complete(run_motors())

if __name__ == '__main__':

    servos=ServoMotor(scl = 25 , sda = 26 ,address=0x40) 
    servos.set_angle(0,180)
    servos.set_speed(0,10)

    import uasyncio as asyncio
    asyncio.run(run_motors())

    
    

    

