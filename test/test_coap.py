from aiocoap import *
from cbor2 import loads, dumps
import socket
import time
import random

def get(ip_address, resource, timeout):
    port = 5683
    mtype = CON
    if not resource.startswith('/'):
        resource = '/'+resource
    uri = 'coap://'+ip_address+resource
    print(f"GET {uri}")
    request = Message(code=GET, mtype=mtype, uri=uri, mid=random.randint(0, 65535))
    
    # Create socket without binding to a specific port
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.settimeout(timeout)
    
    try:
        request_data = request.encode()
        print(f"Sending request: {request_data.hex()}")
        sock.sendto(request_data, (ip_address, port))
        
        print("Waiting for response...")
        response_data, addr = sock.recvfrom(1024)
        print(f"Received response from {addr}: {response_data.hex()}")
        
        # Parse the CoAP response
        response_msg = Message.decode(response_data)
        print(f"Response code: {response_msg.code}")
        print(f"Response type: {response_msg.mtype}")
        print(f"Message ID: {response_msg.mid}")
        
        if response_msg.payload:
            try:
                # Try to decode as text first
                payload_text = response_msg.payload.decode('utf-8')
                print(f"Payload (text): {payload_text}")
                return payload_text
            except UnicodeDecodeError:
                # If not text, try CBOR
                try:
                    payload_cbor = loads(response_msg.payload)
                    print(f"Payload (CBOR): {payload_cbor}")
                    return payload_cbor
                except Exception as e:
                    print(f"Payload (raw): {response_msg.payload.hex()}")
                    return response_msg.payload
        else:
            print("No payload")
            return None
            
    except socket.timeout:
        print('Timeout - no response received')
        return None
    except Exception as e:
        print(f'Exception: {e}')
        return None
    finally:
        sock.close()

def test_error_rate(ip_address, path, timeout=1, repeats=10):
    errors = 0
    for i in range(repeats):
        print(f"{i+1}/{repeats} ", end='')
        response = get(ip_address, path, timeout)
        if response is None:
            errors += 1
        time.sleep(0.5)
    print('Done')
    print(f'Errors: {errors} / {repeats}')

if __name__ == "__main__":
    # Test single request first
    print("Testing single request...")
    # result = get('192.168.1.29', '/', 5)
    result = get('192.168.1.29', '/inx/network', 5)
    # result = get('192.168.1.206', '/inx/network', 5)
    print(f"Result: {result}")
    
    # Then test error rate
    # print("\nTesting error rate...")
    # test_error_rate('192.168.1.29', '/', 5, 3)
    