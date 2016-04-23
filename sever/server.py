import wave
import struct

waveFile = wave.open('data.wav', 'r')

"""
A simple echo server
"""

import socket

host = '192.168.1.1'
port = 12001
backlog = 5
size = 1024
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind((host,port))
s.listen(backlog)
try:
    while 1:
        print 'waiting for connection...'
        client, address = s.accept()
        print 'connection established to: {0}'. format(address)
        for i in range(waveFile.getnframes()):
            client.send(waveFile.readframes(1))
        client.close()
finally:
    s.close()
