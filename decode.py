"""
Decodes the array of 10 concatenated 16-bit RMS values into two arrays (one for each ADC input)
"""
print("Press ctrl+C to exit")
try:
    while True:
        hex_vals = input("Hex: ")
        for i in range(10):
            label = "ADC1: " if i<5 else "ADC2: "
            label_print = label if i==0 or i==5 else ""
            print(f"{label_print}{int(hex_vals[2+4*i:2+4*i+2], 16)}", end=", " if i!=4 and i!=9 else "\n")
        print("",end="\n")
except KeyboardInterrupt:
    pass