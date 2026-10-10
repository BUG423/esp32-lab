# Freeze every runtime module below extmod/txd into the txd package. Passing
# an empty file tuple here used to exclude all helpers and produced a firmware
# in which `from txd import ...` could not work. Keep this manifest itself out
# of the device package.
package(
    "txd",
    files=(
        "__init__.py",
        "DHT11/DHT11.py",
        "DHT11/__init__.py",
        "RGBs/RGBs.py",
        "RGBs/__init__.py",
        "SoilMoistureSensor/SoilMoistureSensor.py",
        "SoilMoistureSensor/__init__.py",
        "Ultrasonic/Ultrasonic.py",
        "Ultrasonic/__init__.py",
        "motor/PCA9685.py",
        "motor/__init__.py",
        "motor/motor.py",
        "motor/servor.py",
        "oled/__init__.py",
        "oled/oled.py",
        "oled/oled_2.py",
        "oled/sh1106.py",
        "oled/ssd1306.py",
        "rgb/__init__.py",
        "rgb/rgb.py",
    ),
    base_path="..",
    opt=3,
)
