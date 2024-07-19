import smbus
import time

# Adres I2C czujnika BMP280
BMP280_I2C_ADDRESS = 0x76

# Rejestry BMP280
BMP280_REG_DIG_T1 = 0x88
BMP280_REG_DIG_T2 = 0x8A
BMP280_REG_DIG_T3 = 0x8C
BMP280_REG_DIG_P1 = 0x8E
BMP280_REG_DIG_P2 = 0x90
BMP280_REG_DIG_P3 = 0x92
BMP280_REG_DIG_P4 = 0x94
BMP280_REG_DIG_P5 = 0x96
BMP280_REG_DIG_P6 = 0x98
BMP280_REG_DIG_P7 = 0x9A
BMP280_REG_DIG_P8 = 0x9C
BMP280_REG_DIG_P9 = 0x9E
BMP280_REG_CONTROL = 0xF4
BMP280_REG_CONFIG = 0xF5
BMP280_REG_PRESSURE_DATA = 0xF7
BMP280_REG_TEMP_DATA = 0xFA

# Inicjalizacja magistrali I2C
bus = smbus.SMBus(18)

# Funkcja do odczytu 16-bitowej wartości z dwóch rejestrów


def read_16bit(register):
    data = bus.read_i2c_block_data(BMP280_I2C_ADDRESS, register, 2)
    return data[0] + (data[1] << 8)

# Funkcja do odczytu 16-bitowej wartości podpisanej


def read_signed_16bit(register):
    value = read_16bit(register)
    if value > 32767:
        value -= 65536
    return value


# Odczytanie kalibracji
dig_T1 = read_16bit(BMP280_REG_DIG_T1)
# dig_T2 = read_signed_16bit(BMP280_REG_DIG_T2)
# dig_T3 = read_signed_16bit(BMP280_REG_DIG_T3)
# dig_P1 = read_16bit(BMP280_REG_DIG_P1)
# dig_P2 = read_signed_16bit(BMP280_REG_DIG_P2)
# dig_P3 = read_signed_16bit(BMP280_REG_DIG_P3)
# dig_P4 = read_signed_16bit(BMP280_REG_DIG_P4)
# dig_P5 = read_signed_16bit(BMP280_REG_DIG_P5)
# dig_P6 = read_signed_16bit(BMP280_REG_DIG_P6)
# dig_P7 = read_signed_16bit(BMP280_REG_DIG_P7)
# dig_P8 = read_signed_16bit(BMP280_REG_DIG_P8)
# dig_P9 = read_signed_16bit(BMP280_REG_DIG_P9)

# # Funkcja do zapisu rejestru


def write_register(register, value):
    bus.write_byte_data(BMP280_I2C_ADDRESS, register, value)


# Ustawienia czujnika (tryb normalny, oversampling x1)
# write_register(BMP280_REG_CONTROL, 0x3F)
# write_register(BMP280_REG_CONFIG, 0xA0)

# Funkcja do odczytu temperatury i ciśnienia


# def read_bmp280():
# # Odczyt surowych danych temperatury
# data = bus.read_i2c_block_data(BMP280_I2C_ADDRESS, BMP280_REG_TEMP_DATA, 3)
# adc_T = (data[0] << 16) | (data[1] << 8) | data[2]
# adc_T >>= 4

# # Odczyt surowych danych ciśnienia
# data = bus.read_i2c_block_data(
#     BMP280_I2C_ADDRESS, BMP280_REG_PRESSURE_DATA, 3)
# adc_P = (data[0] << 16) | (data[1] << 8) | data[2]
# adc_P >>= 4

# # Obliczenia temperatury
# var1 = (((adc_T >> 3) - (dig_T1 << 1)) * dig_T2) >> 11
# var2 = (((((adc_T >> 4) - dig_T1) * ((adc_T >> 4) - dig_T1)) >> 12) * dig_T3) >> 14
# t_fine = var1 + var2
# temperature = (t_fine * 5 + 128) >> 8

# # Obliczenia ciśnienia
# var1 = t_fine - 128000
# var2 = var1 * var1 * dig_P6
# var2 = var2 + ((var1 * dig_P5) << 17)
# var2 = var2 + (dig_P4 << 35)
# var1 = ((var1 * var1 * dig_P3) >> 8) + ((var1 * dig_P2) << 12)
# var1 = (((1 << 47) + var1) * dig_P1) >> 33

# if var1 == 0:
#     return None, None  # unikanie dzielenia przez zero

# p = 1048576 - adc_P
# p = (((p << 31) - var2) * 3125) // var1
# var1 = (dig_P9 * (p >> 13) * (p >> 13)) >> 25
# var2 = (dig_P8 * p) >> 19
# pressure = ((p + var1 + var2) >> 8) + (dig_P7 << 4)

# return temperature / 100.0, pressure / 25600.0

# temperature, pressure = read_bmp280()

# if temperature is not None and pressure is not None:
#     print(f"Temperature: {temperature:.2f} °C")
#     print(f"Pressure: {pressure:.2f} hPa")
