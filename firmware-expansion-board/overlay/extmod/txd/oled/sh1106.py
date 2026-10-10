from micropython import const
import utime as time
import framebuf
from machine import Pin,I2C

# a few register definitions
_SET_CONTrAST        = const(0x81)
_SET_NOrM_INV        = const(0xa6)
_SET_DISP            = const(0xae)
_SET_SCAN_DIr        = const(0xc0)
_SET_SEG_rEMAP       = const(0xa0)
_LOW_COLUMN_ADDrESS  = const(0x00)
_HIGH_COLUMN_ADDrESS = const(0x10)
_SET_PAGE_ADDrESS    = const(0xB0)


class SH1106(framebuf.FrameBuffer):
    
    def __init__(self, width, height, external_vcc, rotate=0):
        self.width = width
        self.height = height
        self.external_vcc = external_vcc
        self.flip_en = rotate == 180 or rotate == 270
        self.rotate90 = rotate == 90 or rotate == 270
        self.pages = self.height // 8
        self.bufsize = self.pages * self.width
        self.renderbuf = bytearray(self.bufsize)
        self.pages_to_update = 0
       
        if self.rotate90:
            self.displaybuf = bytearray(self.bufsize)
            # HMSB is required to keep the bit order in the render buffer
            # compatible with byte-for-byte remapping to the display buffer,
            # which is in VLSB. Else we'd have to copy bit-by-bit!
            super().__init__(self.renderbuf, self.height, self.width,
                             framebuf.MONO_HMSB)
        else:
            self.displaybuf = self.renderbuf
            super().__init__(self.renderbuf, self.width, self.height,
                             framebuf.MONO_VLSB)

        self.rotate = self.flip
        self.init_display()

    def init_display(self):
        self.reset()
        self.fill(0)
        self.show()
        self.poweron()
        # rotate90 requires a call to flip() for setting up.
        self.flip(self.flip_en)

    def poweroff(self):
        self.write_cmd(_SET_DISP | 0x00)

    def poweron(self):
        self.write_cmd(_SET_DISP | 0x01)
        if self.delay:
            time.sleep_ms(self.delay)

    def flip(self, flag=None, update=True):
        if flag is None:
            flag = not self.flip_en
        mir_v = flag ^ self.rotate90
        mir_h = flag
        self.write_cmd(_SET_SEG_rEMAP | (0x01 if mir_v else 0x00))
        self.write_cmd(_SET_SCAN_DIr | (0x08 if mir_h else 0x00))
        self.flip_en = flag
        if update:
            self.show(True) # full update

    def sleep(self, value):
        self.write_cmd(_SET_DISP | (not value))

    def contrast(self, contrast):
        self.write_cmd(_SET_CONTrAST)
        self.write_cmd(contrast)

    def invert(self, invert):
        self.write_cmd(_SET_NOrM_INV | (invert & 1))

    def show(self, full_update = False):
        # self.* lookups in loops take significant time (~4fps).
        (w, p, db, rb) = (self.width, self.pages,
                          self.displaybuf, self.renderbuf)
        if self.rotate90:
            for i in range(self.bufsize):
                db[w * (i % p) + (i // p)] = rb[i]
        if full_update:
            pages_to_update = (1 << self.pages) - 1
        else:
            pages_to_update = self.pages_to_update
        #print("Updating pages: {:08b}".format(pages_to_update))
        for page in range(self.pages):
            if (pages_to_update & (1 << page)):
                self.write_cmd(_SET_PAGE_ADDrESS | page)
                self.write_cmd(_LOW_COLUMN_ADDrESS | 2)
                self.write_cmd(_HIGH_COLUMN_ADDrESS | 0)
                self.write_data(db[(w*page):(w*page+w)])
        self.pages_to_update = 0


    def pixel(self, x, y, color=None):
        if color is None:
            return super().pixel(x, y)
        else:
            super().pixel(x, y , color)
            page = y // 8
            self.pages_to_update |= 1 << page

    def text(self, text, x, y, color=1):
        super().text(text, x, y, color)
        self.register_updates(y, y+7)

    def line(self, x0, y0, x1, y1, color):
        super().line(x0, y0, x1, y1, color)
        self.register_updates(y0, y1)

    def hline(self, x, y, w, color):
        super().hline(x, y, w, color)
        self.register_updates(y)

    def vline(self, x, y, h, color):
        super().vline(x, y, h, color)
        self.register_updates(y, y+h-1)

    def fill(self, color):
        super().fill(color)
        self.pages_to_update = (1 << self.pages) - 1

    def blit(self, fbuf, x, y, key=-1, palette=None):
        super().blit(fbuf, x, y, key, palette)
        self.register_updates(y, y+self.height)

    def scroll(self, x, y):
        # my understanding is that scroll() does a full screen change
        super().scroll(x, y)
        self.pages_to_update =  (1 << self.pages) - 1

    def fill_rect(self, x, y, w, h, color):
        super().fill_rect(x, y, w, h, color)
        self.register_updates(y, y+h-1)

    def draw_rect(self, x, y, w, h, color):
        super().rect(x, y, w, h, color)
        self.register_updates(y, y+h-1)
    
    #画椭圆，参数：起始圆心坐标，x半径，y半径，颜色默认为亮，是否填充，显示象限（0-15的数字）
    def ellipse(self,x,y,xr,yr,c=1,f=False,m=15):
        self.framebuf.ellipse(x,y,xr,yr,c,f,m)
    def draw_circle(self, x0, y0, r, color=1):
        x = r - 1
        y = 0
        dx = 1
        dy = 1
        err = dx - (r << 1)

        while x >= y:
            self.pixel(x0 + x, y0 + y, color)
            self.pixel(x0 + y, y0 + x, color)
            self.pixel(x0 - y, y0 + x, color)
            self.pixel(x0 - x, y0 + y, color)
            self.pixel(x0 - x, y0 - y, color)
            self.pixel(x0 - y, y0 - x, color)
            self.pixel(x0 + y, y0 - x, color)
            self.pixel(x0 + x, y0 - y, color)

            if err <= 0:
                y += 1
                err += dy
                dy += 2

            if err > 0:
                x -= 1
                dx += 2
                err += dx - (r << 1)

    #画立方体，左上前点的坐标，边长
    def cube(self,x,y,l):
        self.rect(x,y,l,l)
        self.rect(x+int(0.5*l),int(y-0.5*l),l,l) 
        self.line(x,y,int(x+0.5*l),int(y-0.5*l))  #感觉画的是左边下面的一条边
        self.line(x+l-1,y,int(x+1.5*l-1),int(y-0.5*l))  #这里可能是右边底面的一条边，因为x加上了边长
        self.line(x-1,y+l,int(x+0.5*l),int(y+0.5*l-1))  #这里的y加上了边长，可能是左上的边
        self.line(x+l-1,y+l-1,int(x+1.5*l-1),int(y+0.5*l-1))  #右上
    
    #画8*8的图，列行
    def p8(self,page,x,y):
        for e in range(8):#遍历每一个page
            byte=bin(page[e]).replace('0b','')
            while len(byte)<8: #当长度小于8时用0填充到8位
                byte='0'+byte
            for i in range(8):#遍历page中的每一个bit
                if byte[i]=='1':  #如果对应的位置为1就将对应的像素点点亮
                    self.pixel(x+e,y+i,int(byte[i]))  #xy就是左上角
    
    #画16*16的图，列行
    def p16(self,page,x,y):
        for e in range(32):
            byte=bin(page[e]).replace('0b','')
            while len(byte)<8:
                byte='0'+byte
            for i in range(8):
                if byte[i] and e<16:
                    self.pixel(x+e,y+i,int(byte[i]))  #有一个关键的问题，显示器左上角为0,0，右下角为127,63,所以这里先画了图像的上半部分
                elif byte[i] and e>=16:
                    self.pixel(x-16+e,y+8+i,int(byte[i])) #再画图像的下半部分
    
    #画32*32的图，列行
    def p32(self,page,x,y):
        for e in range(128):
            byte=bin(page[e]).replace('0b','')
            while len(byte)<8:
                byte='0'+byte
            for i in range(8):
                if byte[i] and e<32:    #从上到下，从左往右
                    self.pixel(x+e,y+i,int(byte[i]))
                elif byte[i] and 32<=e<64:
                    self.pixel(x+e-32,y+8+i,int(byte[i]))
                elif byte[i] and 64<=e<96:
                    self.pixel(x+e-64,y+16+i,int(byte[i]))
                elif byte[i] and 96<=e<128:
                    self.pixel(x+e-96,y+24+i,int(byte[i]))
    
    #画64*64的图，列行
    def p64(self,page,x,y):
        for e in range(512):
            byte=bin(page[e]).replace('0b','')
            while len(byte)<8:
                byte='0'+byte
            for i in range(8):
                if byte[i] and e<64:    #从上到下，从左往右
                    self.pixel(x+e,y+i,int(byte[i]))
                elif byte[i] and 64<=e<128:
                    self.pixel(x+e-64,y+8+i,int(byte[i]))
                elif byte[i] and 128<=e<192:
                    self.pixel(x+e-128,y+16+i,int(byte[i]))
                elif byte[i] and 192<=e<256:
                    self.pixel(x+e-192,y+24+i,int(byte[i]))
                elif byte[i] and 256<=e<320:
                    self.pixel(x+e-256,y+32+i,int(byte[i]))
                elif byte[i] and 320<=e<384:
                    self.pixel(x+e-320,y+40+i,int(byte[i]))
                elif byte[i] and 384<=e<448:
                    self.pixel(x+e-384,y+48+i,int(byte[i]))
                elif byte[i] and 448<=e<512:
                    self.pixel(x+e-448,y+56+i,int(byte[i]))

    def register_updates(self, y0, y1=None):
        # this function takes the top and optional bottom address of the changes made
        # and updates the pages_to_change list with any changed pages
        # that are not yet on the list
        start_page = max(0, y0 // 8)
        end_page = max(0, y1 // 8) if y1 is not None else start_page
        # rearrange start_page and end_page if coordinates were given from bottom to top
        if start_page > end_page:
            start_page, end_page = end_page, start_page
        for page in range(start_page, end_page+1):
            self.pages_to_update |= 1 << page

    def reset(self, res):
        if res is not None:
            res(1)
            time.sleep_ms(1)
            res(0)
            time.sleep_ms(20)
            res(1)
            time.sleep_ms(20)


class SH1106_I2C(SH1106):
    def __init__(self, width, height, i2c, res=None, addr=0x3c,
                 rotate=0, external_vcc=False, delay=0):
        self.i2c = i2c
        self.addr = addr
        self.res = res
        self.temp = bytearray(2)
        self.delay = delay
        if res is not None:
            res.init(res.OUT, value=1)
        super().__init__(width, height, external_vcc, rotate)

    def write_cmd(self, cmd):
        self.temp[0] = 0x80  # Co=1, D/C#=0
        self.temp[1] = cmd
        self.i2c.writeto(self.addr, self.temp)

    def write_data(self, buf):
        self.i2c.writeto(self.addr, b'\x40'+buf)

    def reset(self):
        super().reset(self.res)



class Oled:
    def __init__(self,scl=18, sda=19, rotate=180):
        self.width = 128
        self.height = 64
        self.hcur = 0
        self.wcur = 0
        self.hline = 8
        self.rotate = rotate
        self.maxh = self.height / self.hline # 每一行最多占用像素
        self.sh1106 = SH1106_I2C(128, 64,I2C(scl=Pin(scl), sda=Pin(sda)),rotate = self.rotate)
        self.sh1106.fill(0)
        #self.sh1106.text("Hello World", 0, 0, 1)
        self.sh1106.show()

    def __del__(self):
        self.fill(0)
        self.show()

    def scroll(self, x, y):
        # my understanding is that scroll() does a full screen change
        super().scroll(x, y)
        self.pages_to_update =  (1 << self.pages) - 1
 
    def scroll_line(self, line_num=1):
        if self.sh1106:
            self.sh1106.scroll(0, line_num * self.hline)
            self.sh1106.show()
        else:
            print("Oled.scroll_one_line(): sh1106 is not initialized")
    def show(self, text, row, color=1): 
        if self.sh1106:
         x = 0 # 固定在行的起始位置
         y = row * 8 # 每行高度为8像素 
         self.sh1106.text(text, x, y, color)
         self.sh1106.show()
        else:
            print("Oled.show(): sh1106 is not initialized")

    def draw_line(self, x0, y0, x1, y1, color=1):
        if self.sh1106:
            self.sh1106.line(x0, y0, x1, y1, color)
            self.sh1106.show()

    def draw_rect(self, x, y, w, h, color=1):
        if self.sh1106:
           self.sh1106.draw_rect(x, y, w, h, color)
           self.sh1106.show()
        else:
            print("Oled.draw_circle(): sh1106 is not initialized")

    def draw_circle(self, x0, y0, r, color=1):
        if self.sh1106:
            self.sh1106.draw_circle(x0, y0, r, color)
            self.sh1106.show()
        else:
            print("Oled.draw_circle(): sh1106 is not initialized")

    def clean_line(self, lno):
        if self.sh1106:
            self.sh1106.fill_rect(0, lno * self.hline, self.width, self.hline, 0)
            #self.sh1106.framebuf.fill_rect(0, lno * self.hline, self.width, self.hline, 0)
            self.sh1106.show()
        else:
            print("Oled.clean_line(): sh1106 is not initialized")

    def clear_all(self):
        if self.sh1106:
            self.sh1106.fill(0)
            self.sh1106.show()
        else:
            print("Oled.clear_all(): sh1106 is not initialized")
