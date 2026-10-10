from micropython import const
import utime
import framebuf
from machine import Pin,I2C
import time

# register definitions

SET_SCAN_DIR = const(0xc0)
LOW_COLUMN_ADDRESS = const(0x00)
HIGH_COLUMN_ADDRESS = const(0x10)
SET_PAGE_ADDRESS = const(0xB0)
SET_CONTRAST = const(0x81)
SET_ENTIRE_ON = const(0xa4)
SET_NORM_INV = const(0xa6)
SET_DISP = const(0xae)
SET_MEM_ADDR = const(0x20)
SET_COL_ADDR = const(0x21)
SET_PAGE_ADDR = const(0x22)
SET_DISP_START_LINE = const(0x40)
SET_SEG_REMAP = const(0xa0)
SET_MUX_RATIO = const(0xa8)
SET_COM_OUT_DIR = const(0xc0)
SET_DISP_OFFSET = const(0xd3)
SET_COM_PIN_CFG = const(0xda)
SET_DISP_CLK_DIV = const(0xd5)
SET_PRECHARGE = const(0xd9)
SET_VCOM_DESEL = const(0xdb)
SET_CHARGE_PUMP = const(0x8d)


class SH1106:
    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.pages = self.height // 8
        self.buffer = bytearray(self.pages * self.width)
        fb = framebuf.FrameBuffer(
            memoryview(self.buffer)[1:], self.width, self.height, framebuf.MVLSB)
        self.framebuf = fb
# set shortcuts for the methods of framebuf
        self.fill = fb.fill
        self.fill_rect = fb.fill_rect
        self.hline = fb.hline
        self.vline = fb.vline
        self.line = fb.line
        self.rect = fb.rect
        self.pixel = fb.pixel
        self.scroll = fb.scroll
        self.text = fb.text
        self.blit = fb.blit

        # print("init done")

        self.init_display()

    def init_display(self):
        self.reset()
        for cmd in (
            SET_DISP | 0x00,    # 关闭显示
            SET_DISP_CLK_DIV, 0x80,  # 设置时钟分频因子
            SET_MUX_RATIO, self.height - 1,  # 设置驱动路数 路数默认0x3F（1/64）
            SET_DISP_OFFSET, 0x00,  # 设置显示偏移 偏移默认为0
            SET_DISP_START_LINE | 0x00,  # 设置显示开始行[5:0]
            SET_CHARGE_PUMP, 0x14,  # 电荷泵设置 bit2，开启/关闭
            # 设置内存地址模式 [1:0],00，列地址模式;01，行地址模式;10,页地址模式;默认10;
            SET_MEM_ADDR, 0x02,
            SET_SEG_REMAP | 0x01,  # 段重定义设置,bit0:0,0->0;1,0->127;
            # 设置COM扫描方向;bit3:0,普通模式;1,重定义模式 COM[N-1]->COM0;N:驱动路数
            SET_COM_OUT_DIR | 0x08,
            SET_COM_PIN_CFG, 0x12,  # 设置COM硬件引脚配置   [5:4]配置
            SET_PRECHARGE, 0xf1,  # 设置预充电周期 [3:0],PHASE 1;[7:4],PHASE 2;
            # 设置VCOMH 电压倍率 [6:4] 000,0.65*vcc;001,0.77*vcc;011,0.83*vcc;
            SET_VCOM_DESEL, 0x30,
            SET_CONTRAST, 0xff,  # 对比度设置 默认0x7F（范围1~255，越大越亮）
            SET_ENTIRE_ON,  # 全局显示开启;bit0:1,开启;0,关闭;(白屏/黑屏)
            SET_NORM_INV,  # 设置显示方式;bit0:1,反相显示;0,正常显示
                SET_DISP | 0x01):  # 开启显示
            self.write_cmd(cmd)
        self.fill(1)
        self.show()

    def poweroff(self):
        self.write_cmd(SET_DISP | 0x00)

    def poweron(self):
        self.write_cmd(SET_DISP | 0x01)

    def rotate(self, flag, update=True):
        if flag:
            self.write_cmd(SET_SEG_REMAP | 0x01)  # mirror display vertically
            self.write_cmd(SET_SCAN_DIR | 0x08)  # mirror display hor.
        else:
            self.write_cmd(SET_SEG_REMAP | 0x00)
            self.write_cmd(SET_SCAN_DIR | 0x00)
        if update:
            self.show()

    def sleep(self, value):
        self.write_cmd(SET_DISP | (not value))

    def contrast(self, contrast):
        self.write_cmd(SET_CONTRAST)
        self.write_cmd(contrast)

    def invert(self, invert):
        self.write_cmd(SET_NORM_INV | (invert & 1))

    def show(self):
        for page in range(self.height // 8):
            self.write_cmd(SET_PAGE_ADDRESS | page)
            self.write_cmd(LOW_COLUMN_ADDRESS)
            self.write_cmd(HIGH_COLUMN_ADDRESS)
            page_buffer = bytearray(self.width)
            for i in range(self.width):
                page_buffer[i] = self.buffer[self.width * page + i]
            self.write_data(page_buffer)

    def set_buffer(self, buffer):
        for i in range(len(buffer)):
            self.buffer[i] = buffer[i]

    def draw_XBM(self, x, y, w, h, bitmap):
        x_byte = (w//8) + (w % 8 != 0)
        for nbyte in range(len(bitmap)):
            for bit in range(8):
                if(bitmap[nbyte] & (0b10000000 >> bit)):
                    p_x = (nbyte % x_byte)*8+bit
                    p_y = nbyte//x_byte
                    self.pixel(x + p_x, y + p_y, 1)

    # 以屏幕GRAM的原始制式去填充Buffer
    def draw_buffer(self, x, y, w, h, bitmap):
        y_byte = (h//8) + (h % 8 != 0)
        for nbyte in range(len(bitmap)):
            for bit in range(8):
                if(bitmap[nbyte] & (1 << bit)):
                    p_y = (nbyte % y_byte)*8+bit
                    p_x = nbyte//y_byte
                    self.pixel(x + p_x, y + p_y, 1)

    def fill_rect(self, x, y, w, h, c):
        self.fill_rect(x, y, w, h, c)

    def fill_circle(self, x0, y0, r, c):
        x = 0
        y = r
        deltax = 3
        deltay = 2 - r - r
        d = 1 - r
        #print(x)
        #print(y)
        #print(deltax)
        #print(deltay)
        #print(d)
        self.pixel(x + x0, y + y0, c)
        self.pixel(x + x0, -y + y0, c)
        for i in range(-r + x0, r + x0):
            self.pixel(i, y0, c)
        while x < y:
            if d < 0:
                d += deltax
                deltax += 2
                x = x +1
            else:
                d += (deltax + deltay)
                deltax += 2
                deltay += 2
                x = x +1
                y = y -1
            for i in range(-x + x0, x + x0):
                self.pixel(i, -y + y0, c)
                self.pixel(i, y + y0, c)
            for i in range(-y + x0, y + x0):
                self.pixel(i, -x + y0, c)
                self.pixel(i, x + y0, c)

    def reset(self, res):
        if res is not None:
            res.write(1)
            utime.sleep_ms(1)
            res.write(0)
            utime.sleep_ms(20)
            res.write(1)
            utime.sleep_ms(20)


class SH1106_I2C(SH1106):
    def __init__(self, width, height, i2c, res=None, addr=0x3c):
        self.i2c = i2c
        self.addr = addr
        self.res = res
        self.temp = bytearray(2)
        super().__init__(width, height)

    def write_cmd(self, cmd):
        self.temp[0] = 0x80  # Co=1, D/C#=0
        self.temp[1] = cmd
        self.i2c.writeto(self.addr, self.temp)

    def write_data(self, buf):
        send_buf = bytearray(1 + len(buf))
        send_buf[0] = 0x40
        for i in range(len(buf)):
            send_buf[i+1] = buf[i]
#        print(send_buf)
        self.i2c.writeto(self.addr, send_buf)

    def reset(self):
        super().reset(self.res)


class Oled:
    def __init__(self,scl=18, sda=19):
        self.width = 128
        self.height = 64
        self.hcur = 0
        self.wcur = 0
        self.hline = 8
        self.maxh = self.height / self.hline # 每一行最多占用像素
        self.oled = SH1106_I2C(128, 64, I2C(scl=Pin(scl), sda=Pin(sda)))
        self.oled.fill(0)
        #self.oled.text("Hello World", 0, 0, 1)
        self.oled.show()

    def __del__(self):
        self.fill(0)
        self.show()

    def show_text(self, text, lno=None):
        
        if self.oled :
            if lno != None and lno <= self.maxh:
                show_hcur = lno * self.hline
                self.clean_line(lno)
                self.oled.text(text, self.wcur, show_hcur)
                if show_hcur == self.hcur:
                    self.hcur += self.hline
            else:
                if self.hcur + self.hline > self.height:
                    self.scroll_line()
                self.oled.text(text, self.wcur, self.hcur)
                self.hcur += self.hline
            self.oled.show()
        else:
            print("Oled.print(): oled is not initialized")

    def scroll(self, dx, dy):
        if self.oled:
            self.oled.scroll(dx, dy)
            self.oled.show()
        else:
            print("Oled.scroll(): oled is not initialized")

    def scroll_line(self, line_num=1):
        if self.oled:
            self.oled.scroll(0, line_num * self.hline)
            self.oled.show()
        else:
            print("Oled.scroll_one_line(): oled is not initialized")

    def draw_line(self, x0, y0, x1, y1, col=1):
        if self.oled:
            self.oled.line(x0, y0, x1, y1, col)
            self.oled.show()
        else:
            print("Oled.draw_line(): oled is not initialized")
    
    def draw_rect(self, x, y, w, h, col=1):
        if self.oled:
            self.oled.rect(x, y, w, h, col)
            self.oled.show()
        else:
            print("Oled.draw_rect(): oled is not initialized")

    def draw_circle(self, x0, y0, r, w, c=1):
        if self.oled:
            self.oled.draw_circle(x0, y0, r, c)
            self.oled.draw_circle(x0, y0, r -w, 0)
            self.oled.show()
        else:
            print("Oled.draw_circle(): oled is not initialized")        
    
    def clean_line(self, lno):
        if self.oled:
            self.oled.fill_rect(0, lno * self.hline, self.width, self.hline, 0)
            #self.oled.framebuf.fill_rect(0, lno * self.hline, self.width, self.hline, 0)
            self.oled.show()
        else:
            print("Oled.clean_line(): oled is not initialized")

    def clear_all(self):
        if self.oled:
            self.oled.fill(0)
            self.oled.show()
        else:
            print("Oled.clear_all(): oled is not initialized")


# # 以下为测试代码
# if __name__ == "__main__":

#     oled = Oled(18,19)
#     oled.show_text("123123")
#     oled.show_text("123123",1)
#     oled.show_text("123123",2)

#     oled.scroll(0, 8)
#     time.sleep(1)
#     oled.scroll(0, -8)
#     time.sleep(1)
#     oled.scroll_line()
#     time.sleep(1)
#     oled.scroll_line(-1)
#     time.sleep(1)
#     oled.draw_line(0, 0, 127, 63)
#     time.sleep(1)
#     oled.draw_line(0, 63, 127, 0)
#     time.sleep(1)
#     oled.draw_rect(0, 0, 127, 63)
#     time.sleep(1)
#     oled.draw_rect(0, 0, 127, 63, 0)
#     time.sleep(1)
#     oled.draw_circle(64, 32, 32, 1, 1)
#     time.sleep(1)
#     oled.draw_circle(64, 32, 32, 1, 0)
#     time.sleep(1)
#     oled.clean_line(0)
#     time.sleep(1)
#     oled.clean_line(1)
#     time.sleep(1)
#     oled.clean_line(2)
#     time.sleep(1)
#     oled.clear_all()

    


