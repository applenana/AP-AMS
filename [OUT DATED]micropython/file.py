import os

def exist(filename):
    try:os.stat(filename)
    except:return False
    else:return True