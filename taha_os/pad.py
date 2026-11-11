input_file = "kernel.bin"
output_file = "kernel-padded.bin"
target_size = 65536  # Target size in bytes

with open(input_file, "rb") as f:
    data = f.read()

data += b'\x00' * (target_size - len(data))

with open(output_file, "wb") as f:
    f.write(data)