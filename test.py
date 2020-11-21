#!/usr/bin/python

from __future__ import division
from struct import *
import fcntl
import struct
import os

#
# Globals
#
DEVICE_PATH = '/dev/pacman'

#
# Utilities for calculating the IOCTL command codes.
#
sizeof = {
    'byte': calcsize('c'),
    'signed byte': calcsize('b'),
    'unsigned byte': calcsize('B'),
    'short': calcsize('h'),
    'unsigned short': calcsize('H'),
    'int': calcsize('i'),
    'unsigned int': calcsize('I'),
    'long': calcsize('l'),
    'unsigned long': calcsize('L'),
    'long long': calcsize('q'),
    'unsigned long long': calcsize('Q')
}

_IOC_NRBITS = 8
_IOC_TYPEBITS = 8
_IOC_SIZEBITS = 14
_IOC_DIRBITS = 2

_IOC_NRMASK = ((1 << _IOC_NRBITS)-1)
_IOC_TYPEMASK = ((1 << _IOC_TYPEBITS)-1)
_IOC_SIZEMASK = ((1 << _IOC_SIZEBITS)-1)
_IOC_DIRMASK = ((1 << _IOC_DIRBITS)-1)

_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = (_IOC_NRSHIFT+_IOC_NRBITS)
_IOC_SIZESHIFT = (_IOC_TYPESHIFT+_IOC_TYPEBITS)
_IOC_DIRSHIFT = (_IOC_SIZESHIFT+_IOC_SIZEBITS)

_IOC_NONE = 0
_IOC_WRITE = 1
_IOC_READ = 2

def _IOC(dir, _type, nr, size):
    if type(_type) == str:
        _type = ord(_type)
        
    cmd_number = (((dir)  << _IOC_DIRSHIFT) | \
        ((_type) << _IOC_TYPESHIFT) | \
        ((nr)   << _IOC_NRSHIFT) | \
        ((size) << _IOC_SIZESHIFT))

    return cmd_number

def _IO(_type, nr):
    return _IOC(_IOC_NONE, _type, nr, 0)

def _IOR(_type, nr, size):
    return _IOC(_IOC_READ, _type, nr, sizeof[size])

def _IOW(_type, nr, size):
    return _IOC(_IOC_WRITE, _type, nr, sizeof[size])


def main():
    """Test the device driver"""
    
    #
    # Calculate the ioctl cmd number
    #
    MY_MAGIC = 'r'
    NEWGAME = _IOW(MY_MAGIC, 0, 'unsigned long')
    GAMESTAT = _IOR(MY_MAGIC, 1, 'unsigned long')
       
    MAP_SIZE = 3030
    #
    # Open the device file
    #
    f = os.open(DEVICE_PATH, os.O_RDWR)
    
    #
    # Test new game operation
    #
    fcntl.ioctl(f, NEWGAME, os.path.abspath('./map1')+'\0')
    
    #
    # Make sure we can read the expected map
    #
    map1 = open('./map1').read()
    assert (map1 == os.read(f,MAP_SIZE))

    #
    # Test that game and score are correct
    #
    gamestat = struct.unpack('I', fcntl.ioctl(f, GAMESTAT, "    "))[0]
    game_over_bit = 1 << 31
    is_over = bool(gamestat & game_over_bit)
    score = gamestat & ~game_over_bit
    assert is_over == False
    assert score == 0


    #
    # Finaly close the device file
    #
    os.close(f)

    
if __name__ == '__main__':
    main()
    
