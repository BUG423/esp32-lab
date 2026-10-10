import machine,neopixel
import time

class RGB:
    def __init__(self, pin = 14):
        self.n = 1
        self.p = pin
        self.np = neopixel.NeoPixel(machine.Pin(self.p),self.n)
    
    def set_color(self, r, g, b):
        self.np[0] = (r, g, b)
        self.np.write()
    
    def fade_in_out(self, color, wait):
        for i in range(0, 4 * 256, 8):
            for j in range(self.n):
                if (i // 256) % 2 == 0:
                    val = i & 0xff
                else:
                    val = 255 - (i & 0xff)
                    if color == 'red':
                        self.np[j] = (val, 0, 0)
                    elif color == 'green':
                        self.np[j] = (0, val, 0)
                    elif color == 'blue':
                        self.np[j] = (0, 0, val)
                    elif color == 'purple':
                        self.np[j] = (val, 0, val)
                    elif color == 'yellow':
                        self.np[j] = (val, val, 0)
                    elif color == 'teal':
                        self.np[j] = (0, val, val)
                    elif color == 'white':
                        self.np[j] = (val, val, val)
                    self.np.write()
            time.sleep_ms(wait)



# 测试代码
# rgb = RGB(14)
# rgb.set_color(255, 0, 0)
# time.sleep(1)
# rgb.set_color(0, 255, 0)
# time.sleep(1)
# rgb.set_color(0, 0, 255)
# time.sleep(1)
# rgb.set_color(255, 255, 255)
# time.sleep(1)
# rgb.set_color(0, 0, 0)
# time.sleep(1)
# rgb.fade_in_out('red', 10)
# time.sleep(1)
# rgb.fade_in_out('green', 10)
# time.sleep(1)
# rgb.fade_in_out('blue', 10)
