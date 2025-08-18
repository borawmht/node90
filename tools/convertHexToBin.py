import sys
from intelhex import IntelHex

filename = 'node90.X.production.hex'
start_address = '1D004000'
end_address = '1D07FFFF'
ih = IntelHex()

def isHexFile(filename,binary):
    with open(filename, mode='r'+binary) as file:
        fileContent = file.read()
        is_hex_file = True
        if(fileContent[0]==":"):
            print("file starts with :")
        else:
            is_hex_file = False
        if(binary=='b'):
            int_array = [int(b) for b in fileContent]
            max_value = int(max(int_array))
            min_value = int(min(int_array))
        else:
            max_value = ord(max(fileContent))
            min_value = ord(min(fileContent))
        print(min_value,max_value)
        if(max_value < 127 and min_value > 9):
            print("file contains only printing characters")
        else:
            is_hex_file = False
        return is_hex_file

def makeBin(filename,start_address,end_address,bin_filename=None):
    #print(filename)
    print(filename,"is hex file",isHexFile(filename,''))
    ih.fromfile(filename,format='hex')
    start = int(start_address,16)
    stop = int(end_address,16)
    step =  stop-start
    a = ih.tobinarray(start=start, size=step)
    print(a[4128:4159])
    print(a[4160:4191])
    if bin_filename is None:
        bin_filename = filename.replace('.hex','.bin')
    f = open(bin_filename, 'w+b')
    f.write(a)
    f.close()
    print(bin_filename,"is hex file",isHexFile(bin_filename,'b'))
    with open(bin_filename, mode='rb') as file: # b is important -> binary
        fileContent = file.read()
        print([int(b) for b in fileContent[4128:4159]])
        print([int(b) for b in fileContent[4160:4191]])
    return

def main(args):
    #args = sys.argv[1:]
    # args is a list of the command line args
    if(len(args)!=1):
        print("Error - specify one file name argument")
        exit()
    filename = args[0]
    if(not filename.endswith(".hex")):
        print("Error - file name must end in .hex")
        exit()
    #print(filename)
    makeBin(filename,start_address,end_address)
    print("done")

if __name__ == "__main__":
   main(sys.argv[1:])
