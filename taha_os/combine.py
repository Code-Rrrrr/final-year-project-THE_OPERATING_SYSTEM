with open("stage_one_bootloader.bin", "rb") as f1:
    data1 = f1.read()

with open("stage_two_bootloader.bin", "rb") as f2:
    data2 = f2.read()

with open("kernel-padded.bin", "rb") as f3:
    data3 = f3.read()

combined = data1 + data2 + data3

with open("os.bin", "wb") as out:
    out.write(combined)