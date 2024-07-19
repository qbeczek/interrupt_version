from bmp_280 import BMP280
from time import sleep

bmp = BMP280(port=18, mode=BMP280.FORCED_MODE, oversampling_p=BMP280.OVERSAMPLING_P_x16, oversampling_t=BMP280.OVERSAMPLING_T_x1,
             filter=BMP280.IIR_FILTER_OFF, standby=BMP280.T_STANDBY_1000)

pressure = bmp.read_pressure()
# temp = bmp.read_temperature()

# print("Pressure (hPa): " + str(pressure))
# print("Temperature (°C): " + str(temp))
