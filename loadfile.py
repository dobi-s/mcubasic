import serial
import time

ser = serial.Serial('COM5', 115200, timeout=0.1)

ser.write(b"$basic load\n")
time.sleep(0.1)
print(ser.read_all().decode(), end="")

with open("C:\\temp\\test.bas") as file:
  for line in file:
    ser.write(line.encode())
    time.sleep(.1)
    print(ser.read_all().decode(), end="")
ser.write(b"\n\0")
time.sleep(.2)
print(ser.read_all().decode(), end="")
ser.close()
