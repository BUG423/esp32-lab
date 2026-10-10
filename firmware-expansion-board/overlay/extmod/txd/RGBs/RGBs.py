from machine import Pin
from neopixel import NeoPixel
import utime


class RGBs:
    red = (255,0,0)
    blue=(0,0,255)
    green=(0,255,0)

    def __init__(self, pin, num):
        self.np = NeoPixel(Pin(pin), num)
        self.num = num

    def set_color(self, index,r, g, b):
        self.np[index] = (r, g, b)
        self.np.write()
    
    def set_all_color(self, r, g, b):
        for i in range(self.num):
            self.np[i] = (r, g, b)
        self.np.write()

    def show(self, r,g,b, num):
        for i in range(num, self.num):
            self.np[i] =  (r, g, b)
            self.np.write()
            utime.sleep(0.05)

    def show_reverse(self, r,g,b, num):
        for i in range(num, self.num)[::-1]:
            self.np[i] = (r, g, b)
            self.np.write()
            utime.sleep(0.05)


# 测试
# if __name__ == '__main__':

#     rgb = RGBs(15, 8)
#     rgb.show(rgb.red, 0)
#     rgb.show(rgb.green, 0)
#     rgb.show(rgb.blue, 0)
#     rgb.show_reverse(rgb.red, 0)
#     rgb.show_reverse(rgb.green, 0)


    



