import time
from machine import I2C,Pin
import math
import ustruct


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
    def __init__(self, scl,sda,freq=50):
        # self.i2c = I2C(0, scl=machine.Pin(5), sda=machine.Pin(4))
        self.i2c = I2C(0, scl=Pin(scl), sda=Pin(sda))
        self.pwm = PCA9685(self.i2c)
        self.freq = freq  # PWM频率 
        self.pwm.set_pwm_freq(self.freq) # 设置PWM频率
        self.max_pulse = 4095  # PWM最大脉宽
        self.max_degrees = 180  # 最大旋转角度
        self.min_pulse = 100  # PWM最小脉宽
        self.min_degrees = 0  # 最小旋转角度

    def set_angle(self, channel, degrees):
        """
        设置舵机角度
        :param channel: 舵机连接的PWM通道
        :param degrees: 要设置的角度
        """
        if degrees < self.min_degrees:
            degrees = self.min_degrees
        elif degrees > self.max_degrees:
            degrees = self.max_degrees
        pulse = int((degrees * (self.max_pulse - self.min_pulse) / self.max_degrees) + self.min_pulse)
        self.pwm.duty(channel, pulse)

    def set_speed(self, channel, speed):
        """
        设置电机转速
        :param channel: 电机连接的PWM通道
        :param speed: 要设置的转速
        """
        if speed > 100:
            speed = 100
        elif speed < 0:
            speed = 0
        pulse = int(speed * self.max_pulse / 100)
        self.pwm.duty(channel, pulse)
    


    def stop(self, channel):
        """
        停止电机运行
        :param channel: 电机连接的PWM通道
        """
        self.pwm.duty(channel, 0)

    def __del__(self):
        """
        释放I2C总线资源
        """
        self.i2c.deinit()



if __name__ == '__main__':

    motor = ServoMotor(5,4)
    motor.set_angle(0, 90)
    motor.set_angle(1, 90)
    motor.set_angle(2, 90)
