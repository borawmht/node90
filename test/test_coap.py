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
    request = Message(code=GET, mtype=mtype, uri=uri, mid=random.randint(0, 65535))
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.settimeout(timeout)
    sock.sendto(request.encode(), (ip_address, port))
    try:
        response = sock.recv(1518)
        print(response)
        print(response.hex())
    except Exception:
        response = b''
    sock.close()
    print(response)
    #print(response.hex())
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
    return None


if __name__ == "__main__":
    # get('192.168.1.29', '/', 1)
    time.sleep(1)
    get('192.168.1.29', '/status', 1)
    time.sleep(1)
    