#!/usr/bin/env python
#__author__ == 'Kios'
#__Email__ == 'root@mkernel.com'

import socket

class PoC(object):
    '''
    Redis-server unauthorize vulnerbalitity.
    Education purpose only! Do not use illegal way!
    '''
    def __init__(self, host, port):
        '''
        Initialize
        '''
        self.host = host
        self.port = port
        
    def _createSocket(self):
        '''
        Create socket
        '''
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((self.host, self.port))
            return sock
        except Exception as e:
            print(">>> Error encountered! due to %s"%str(e))
            return None
        
    def run(self):
        '''
        Verify
        '''
        sock = self._createSocket()
        sock.send("INFO\r\n".encode(encoding='utf-8'))
        result = sock.recv(1024)
        
        if "redis_version" in result:
            print("redis-server is vulnerable!")
        else:
            print("redis-server is not vulnerable!")
            
if __name__ == '__main__':
    obj1 = PoC('127.0.0.1', 6379)
    obj1.run()
        