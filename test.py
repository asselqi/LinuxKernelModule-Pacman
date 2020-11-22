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
DEVICE_PATH1 = '/dev/pacman1'
DEVICE_PATH2 = '/dev/pacman2'

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


def test1_moving():
    MY_MAGIC = 'r'
    NEWGAME = _IOW(MY_MAGIC, 0, 'unsigned long')
    GAMESTAT = _IOR(MY_MAGIC, 1, 'unsigned long')
    MAP_SIZE = 3030

    print("****************************************************")
    print("********************Test1 - moving******************")
    print("****************************************************")
    success = True

    f = os.open(DEVICE_PATH, os.O_RDWR)
    fcntl.ioctl(f, NEWGAME, os.path.abspath('./test_map1') + '\0')

    # move up until hit the wall and try to keep moving:
    os.write(f, "UUUUUUU")
    expected1 = open('./test_map1_expected1').read()
    if expected1 == os.read(f, MAP_SIZE):
        print("->success trying to move out of bound")
    else:
        print("->failure trying to move out of bound")
        success = False

    # test score count for the last move and make sure game isn't over:
    gamestat = struct.unpack('I', fcntl.ioctl(f, GAMESTAT, "    "))[0]
    game_over_bit = 1 << 31
    is_over = bool(gamestat & game_over_bit)
    score = gamestat & ~game_over_bit
    if score == 5:
        print("->success counting score")
    else:
        print("->failure counting score")
        success = False
    if not is_over:
        print("->success checking game is not over")
    else:
        print("->failure game over bit should not be 1")
        success = False

    # some more moves including trying to move into the wall:
    os.write(f, "DDDDD")
    os.write(f, "RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR")
    # there are 81 rights but the 81'th one is into the wall
    expected2 = open('./test_map1_expected2').read()
    if expected2 == os.read(f, MAP_SIZE):
        print("->success trying to move into wall")
    else:
        print("->failure trying to move into wall")
        success = False

    # trying to eat the "*" and keep moving on
    os.write(f, "LLDDDRRRRLLLL")
    os.write(f,"UUULLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL")
    os.write(f, "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD")
    os.write(f,"LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL")
    os.write(f, "D")



    # test score count and game over:
    gamestat = struct.unpack('I', fcntl.ioctl(f, GAMESTAT, "    "))[0]
    game_over_bit = 1 << 31
    is_over = bool(gamestat & game_over_bit)
    if is_over:
        print("->failure WHILE game should be over")
        success = False
    else:
        print("->success WHILE game over bit")




    os.write(f, "UUUUUU")
    os.write(f, "RRRR")
    expected3 = open('./test_map1_expected3').read()
    if expected3 == os.read(f, MAP_SIZE):
        print("->success trying to eat")
    else:
        print("->failure trying to eat")
        success = False



    # test score count and game over:
    gamestat = struct.unpack('I', fcntl.ioctl(f, GAMESTAT, "    "))[0]
    game_over_bit = 1 << 31
    is_over = bool(gamestat & game_over_bit)
    if is_over:
        print("->success game over bit END")
    else:
        print("->failure game should be over END")
        success = False



    os.close(f)
    if success:
        print("test 1 passed")
    else:
        print("test 1 failed")
    print("****************************************************")
    print("*********************Test1 - end********************")
    print("****************************************************")


def test2_game_is_over():
    MY_MAGIC = 'r'
    NEWGAME = _IOW(MY_MAGIC, 0, 'unsigned long')
    GAMESTAT = _IOR(MY_MAGIC, 1, 'unsigned long')
    MAP_SIZE = 3030

    print("****************************************************")
    print("******************Test2 - game over*****************")
    print("****************************************************")
    success = True

    f = os.open(DEVICE_PATH, os.O_RDWR)
    fcntl.ioctl(f, NEWGAME, os.path.abspath('./test_map2') + '\0')

    # take last food on the map:
    os.write(f, "RRRRR")
    expected1 = open('./test_map2_expected1').read()
    if expected1 == os.read(f, MAP_SIZE):
        print("->success eating")
    else:
        print("->failure eating")
        success = False

    # test score count and game over:
    gamestat = struct.unpack('I', fcntl.ioctl(f, GAMESTAT, "    "))[0]
    game_over_bit = 1 << 31
    is_over = bool(gamestat & game_over_bit)
    score = gamestat & ~game_over_bit
    if score == 3:
        print("->success counting score")
    else:
        print("->failure counting score")
        success = False
    if is_over:
        print("->success game over bit")
    else:
        print("->failure game should be over")
        success = False

    os.close(f)
    if success:
        print("test 2 passed")
    else:
        print("test 2 failed")
    print("****************************************************")
    print("*********************Test2 - end********************")
    print("****************************************************")


def test3_multipule_games():
    MY_MAGIC = 'r'
    NEWGAME = _IOW(MY_MAGIC, 0, 'unsigned long')
    GAMESTAT = _IOR(MY_MAGIC, 1, 'unsigned long')
    MAP_SIZE = 3030

    print("****************************************************")
    print("******************Test3 - game over*****************")
    print("****************************************************")
    success = True

    f1 = os.open(DEVICE_PATH1, os.O_RDWR)
    fcntl.ioctl(f1, NEWGAME, os.path.abspath('./test_map1') + '\0')
    f1_ref = os.open(DEVICE_PATH1, os.O_RDWR)
    f2 = os.open(DEVICE_PATH2, os.O_RDWR)
    fcntl.ioctl(f2, NEWGAME, os.path.abspath('./test_map1') + '\0')

    os.write(f1, "UUUUU")
    os.write(f2, "DDDDD")

    #print(os.read(f1,MAP_SIZE))
    #print(os.read(f2,MAP_SIZE))
    # check that f1 changed :

    if os.read(f2, MAP_SIZE) != os.read(f1, MAP_SIZE):
        print("->success changing f1 and not f2")
    else:
        print("->failure changing f1 and not f2")
        success = False

    if os.read(f1_ref, MAP_SIZE) == os.read(f1, MAP_SIZE):
        print("->success changing f1_ref by changing f1")
    else:
        print("->failure changing f1_ref by changing f1")
        success = False 

    os.close(f1)
    os.close(f1_ref)
    os.close(f2)

    if success:
        print("test 3 passed")
    else:
        print("test 3 failed")
    print("****************************************************")
    print("*********************Test3 - end********************")
    print("****************************************************")


def main():
    """Test the device driver"""
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
    #print(os.read(f,MAP_SIZE))
    #os.write(f,"RRR")
    #print(os.read(f,MAP_SIZE))
    gamestat = struct.unpack('I', fcntl.ioctl(f, GAMESTAT, "    "))[0]
    game_over_bit = 1 << 31
    is_over = bool(gamestat & game_over_bit)
    score = gamestat & ~game_over_bit
    print(score)
    assert is_over == False
    assert score == 0
    # Finaly close the device file
    os.close(f)

    test1_moving()
    test2_game_is_over()
    test3_multipule_games()

    
if __name__ == '__main__':
    main()

