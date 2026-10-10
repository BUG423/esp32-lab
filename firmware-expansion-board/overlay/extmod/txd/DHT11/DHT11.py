import machine
import dht

class DHT11:
    def __init__(self, pin):
        self.dht = dht.DHT11(machine.Pin(pin))
        
    def read(self):
        self.dht.measure()
        return self.dht.temperature(), self.dht.humidity()
    
if __name__ == '__main__':
    dht11 = DHT11(4)
    print(dht11.read())
    