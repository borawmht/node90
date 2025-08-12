from aiocoap import *
from cbor2 import loads, dumps
import socket
import time
import random

def get(ip_address,resource,timeout):
    port = 5683
    mtype = CON
    if not resource.startswith('/'):
        resource = '/'+resource
    uri = 'coap://'+ip_address+resource
    print(f"GET {uri}")
    request = Message(code=GET, mtype=mtype, uri=uri, mid=random.randint(0, 65535))
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.settimeout(timeout)
    sock.sendto(request.encode(), (ip_address, port))
    try:
        response = sock.recv(1024)        
        print(response)
        print(response.hex())
    except Exception as e:
        print('Exception',e)
        response = b''
        return None
    sock.close()
    # print(response.decode('utf-8'))
    # print(response.hex())
    #print(loads(response))
    # if len(response)>7:
    #     payload = Message.from_binary(response).payload      
    #     try:
    #         data = loads(payload, str_errors='replace')['e']
    #         return data
    #     except Exception:
    #         try:
    #             payload = response[12:]
    #             data = loads(payload, str_errors='replace')['e']
    #             return data
    #         except Exception:
    #             return None
    return response

def test_error_rate(ip_address,path,timeout=1,repeats=10):
    errors = 0
    for i in range(repeats):
        print(f"{i+1}/{repeats} ",end='')
        response = get(ip_address, path, timeout)
        if response is None:
            errors += 1
        time.sleep(0.5)
    print('Done')
    print(f'Errors: {errors} / {repeats}')

if __name__ == "__main__":
    # time.sleep(0.25)
    # get('192.168.1.29', '/', 1)
    # time.sleep(0.25)
    # get('192.168.1.29', '/status', 1)    
    test_error_rate('192.168.1.29','/',5,1)
    