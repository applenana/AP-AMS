class Machinery:
    def __init__(self,Pin1,Pin2):
        self.p1 = Pin1
        self.p2 = Pin2 

    def foward(self):
        self.p1.value(1)
        self.p2.value(0)

    def backforward(self):
        self.p1.value(0)
        self.p2.value(1)

    def stop(self):
        self.p1.value(0)
        self.p2.value(0)
        
    