'''
Python DirectServo (pyds.py)
Tested in python 3.5 and 3.7

'''
import serial, atexit, time

GlobalKillSwitch = False

# Static lifetime RS485 connections are stored in PortMap, which 
# allows many DirectServo class instances to be bound to ports 
# without clashing port connections
PortMap = {}

# the following would be better organized as class static member variables/methods, but 
# it's more performant to avoid dot lookups in this manner
Decimal2HexStr = lambda x: hex(abs(x))[2:].zfill(2).upper()[-2:]
d2hi = {x:int.from_bytes(Decimal2HexStr(x).encode('utf-8'),'big') for x in range(256)}
BroadcastID = 0xFF
BROADCAST      = d2hi[BroadcastID]
CMD_TORQUE     = int.from_bytes(b"T",'big') << 32 # passes torque parameter: [-255 to 255]
CMD_SETID      = int.from_bytes(b"I",'big') << 24 # passes ID parameter: [0 to 255] 0xFF is reserved for broadcast
CMD_CALIBRATE  = int.from_bytes(b"C",'big') << 8 # no passed parameters
CMD_GETANGLE   = int.from_bytes(b"a",'big') << 8 # no passed parameters
CMD_ZEROANGLE  = int.from_bytes(b"Z",'big') << 8 # no passed parameters
CMD_FAILSAFE   = int.from_bytes(b"F",'big') << 24 # passes:{
													#  +/-; uint8_t FailoverTorqueRatio;
													# uint8_t SearchingTorqueRatio;
													# uint8_t FailAtLimitOffsetDeg_MSByte;
													# uint8_t FailAtLimitOffsetDeg_LSByte;}
CMD_RESET      = int.from_bytes(b"R",'big') << 8 # no passed parameters (resets flash)
NEWLINE_BYTE   = int.from_bytes(b'\n','big')
PLUS_SYM_BYTE  = int.from_bytes(b'+','big') << 16
MINUS_SYM_BYTE = int.from_bytes(b'-','big') << 16

def RatioToByte(x):
	return (MINUS_SYM_BYTE if x<0 else PLUS_SYM_BYTE,int((1 if x>1 or x<-1 else abs(x))*0xFF))

def Torque_BYTES(Dest,TorqueRatio):
	tr_prefix,TorqueRatio = RatioToByte(TorqueRatio)
	return ((d2hi.get(Dest,BROADCAST) << 40) | CMD_TORQUE | 
	( (tr_prefix  | d2hi.get(TorqueRatio,0xFF)) << 8) | NEWLINE_BYTE).to_bytes(7,'big')

def SetID_BYTES(Dest,NewID):
	if NewID>0xFF:
		raise ValueError("NewID cannot be larger than 0xFF")
	return ((d2hi.get(Dest,BROADCAST) << 32) | CMD_SETID | ( d2hi[NewID] << 8) | NEWLINE_BYTE).to_bytes(6,'big')

def Calibrate_BYTES(Dest):
	return ((d2hi.get(Dest,BROADCAST) << 16) | CMD_CALIBRATE | NEWLINE_BYTE).to_bytes(4,'big')

def GetState_BYTES(Dest):
	return ((d2hi.get(Dest,BROADCAST) << 16) | CMD_GETANGLE | NEWLINE_BYTE).to_bytes(4,'big')

def SetZeroAngle_BYTES(Dest):
	return ((d2hi.get(Dest,BROADCAST) << 16) | CMD_ZEROANGLE | NEWLINE_BYTE).to_bytes(4,'big')

#FailoverTorqueRatio, torque ratio [-1.0 to 1.0]
#SearchingTorqueRatio, torque ratio search magnitude [0.0 to 1.0]
#FailAtLimitOffsetDeg, offset in degrees to failsafe [0.0 to 360.0]
def Failsafe_BYTES(Dest,FailoverTorqueRatio,SearchingTorqueRatio,FailAtLimitOffsetDeg):
	FailAtLimitOffsetDeg = 0 if FailAtLimitOffsetDeg<0 else int(FailAtLimitOffsetDeg/360.0*0xFFFF)
	if FailAtLimitOffsetDeg>0xFFFF:
		raise ValueError("Offset cannot be larger than 360 degrees")
	tFVprefix,FailoverTorqueRatio = RatioToByte(FailoverTorqueRatio)
	if SearchingTorqueRatio<0:
		SearchingTorqueRatio = 0
	elif SearchingTorqueRatio>1:
		SearchingTorqueRatio = 0xFF
	else:
		SearchingTorqueRatio = int(SearchingTorqueRatio*0xFF)
	return ((d2hi.get(Dest,BROADCAST) << 32) | CMD_FAILSAFE
	| (tFVprefix  | d2hi[ FailoverTorqueRatio])).to_bytes(6,'big') + \
	( (d2hi[SearchingTorqueRatio] << 40)
	| (d2hi[FailAtLimitOffsetDeg>>8] << 24)
	| (d2hi[FailAtLimitOffsetDeg & 0xFF] << 8)
	| NEWLINE_BYTE).to_bytes(7,'big')

def Reset_BYTES(Dest):
	return ((d2hi.get(Dest,BROADCAST) << 16) | CMD_RESET | NEWLINE_BYTE).to_bytes(4,'big')

class DirectServo():
	"""A helper class for controlling the DirectServo, it is not built for efficiency,
	(use C/C++ if you need that) but instead from a desire to mess around with the 
	controller easily via a python interface.
	
	Examples:

		YourServo = DirectServo(ID = 23) # initialize a communication object to ID 23
		YourServo.SetID()                # send a broadcast signal to set ID = 23
		YourServo.Calibrate()            # calibrate servo with ID = 23
		Angle = YourServo.GetAngle()     # get the angle in degrees
		Angle = YourServo.Torque(0.25)   # set torque to 25% and return the angle
		Angle = YourServo.SetZeroAngle() # set the angle tracked by the motor driver to zero at its current position
		Angle = YourServo.SetFailsafe()  # turns on a failsafe feature, and passes optional settings:
		                                 # 'failsafe':-1.0 to 1.0, 'search':0.0 to 1.0, 'offset': value in degrees from searched limit to set failsafe at [0 to 360]
		Angle = YourServo.Reset()        # resets the board and clears the flash memory to default settings
	
	"""

	def __init__(self, ID = BroadcastID, Port = None, BaudRate = 115200, Timeout = 0.5):
		if Port is None:
			raise RuntimeError("Must specify a serial port when creating a DirectServo object. Example: DirectServo(Port = 'COM1')")
		self.ID = ID
		self.RS485 = PortMap.get(Port)
		if self.RS485 is None:
			self.RS485 = serial.Serial(port = Port, baudrate = BaudRate, timeout = Timeout)
			PortMap[Port] = self.RS485
			if self.RS485.isOpen():
				self.RS485.flushInput()
				self.RS485.flushOutput()
		if not self.RS485.isOpen():
			raise RuntimeError("{}.RS485({}) connection not open".format(self,self.ID))
	
	def _Read(self):
		Response = {"ID":BroadcastID, "Angle":None, "Accel":None, "Code":"No Response"}
		try:
			rxline = self.RS485.readline()
		except serial.SerialTimeoutException:
			return Response
		try:
			rxline = rxline.decode()[:-1] # decode and remove newline
			Response["ID"] = int.from_bytes(bytes.fromhex(rxline[:2]), byteorder = 'big')
			Accel = int.from_bytes(bytes.fromhex(rxline[2:4]),'big')
			Info = rxline[4:]
			if Info!='error':
				Response["Angle"] = 360.0*(int.from_bytes(bytes.fromhex(Info), byteorder = 'big', signed = True) / float(0xFFFF))
				Response["Accel"] = Accel
				Response["Code"] = "OK"
			else:
				Response["Code"] = Info
		except KeyboardInterrupt:
			raise
		except Exception as e:
			Response["Code"] = rxline
			print('Received unrecognized response: "{}" due to: {}'.format(rxline,e))
			self.RS485.flushInput()
			self.RS485.flushOutput()
		return Response

	def _WaitForRead(self,Timeout):
		EndTime = Timeout+time.time()
		response = self._Read()
		while response["Code"] == "No Response" and time.time()<EndTime:
			response = self._Read()
		return response

	def Torque(self, TorqueRatio):
		self.RS485.write(Torque_BYTES(self.ID,TorqueRatio))
		return self._Read()

	def SetID(self, IDValue = None, Destination = BroadcastID):
		if IDValue is None:
			IDValue = self.ID
		self.RS485.write(SetID_BYTES(Destination,IDValue))
		return self._Read()
	
	def Calibrate(self,Timeout=10):
		self.RS485.write(Calibrate_BYTES(self.ID))
		return self._WaitForRead(Timeout)
		
	def GetAngle(self):
		self.RS485.write(GetState_BYTES(self.ID))
		return self._Read()["Angle"]

	def GetAccel(self):
		self.RS485.write(GetState_BYTES(self.ID))
		return self._Read()["Accel"]

	def GetState(self):
		self.RS485.write(GetState_BYTES(self.ID))
		return self._Read()
		
	def SetZeroAngle(self):
		self.RS485.write(SetZeroAngle_BYTES(self.ID))
		return self._Read()
	
	def SetFailsafe(self,FailoverTorqueRatio=0,SearchingTorqueRatio=0.4,FailAtLimitOffsetDeg=10,Timeout=15):
		self.RS485.write(Failsafe_BYTES(self.ID,FailoverTorqueRatio,SearchingTorqueRatio,FailAtLimitOffsetDeg))
		return self._WaitForRead(Timeout)

	def Reset(self):
		self.RS485.write(Reset_BYTES(self.ID))
		return self._Read()

UserKillFunction = None
def SetAdditionalKillFunction(NewFunct):
	global UserKillFunction
	UserKillFunction = NewFunct
	
def EnableMotors():
	global GlobalKillSwitch
	GlobalKillSwitch = False
	
#making sure all connected devices are left at zero torque if the user's program fails
def KillMotors(Motors=None,Notify = False):
	global GlobalKillSwitch
	GlobalKillSwitch = True
	response = ''
	if Motors is None:
		try:
			for port in PortMap.keys():
				BroadcastViaPort = DirectServo(Port = port)
				BroadcastViaPort.Torque(0)
				if Notify:
					response += "\nKilled all motors on Port {}.".format(port)
		except Exception as e:
			raise RuntimeError("Could not kill motors. Exception: ",e)
	else:
		for mtr in Motors:
			mtr.Torque(0)
		if Notify:
			response += "\nKilled motors."
	return response

def KillAndNotify():
	global UserKillFunction
	print("")
	print(KillMotors(None,True))
	if not UserKillFunction is None:
		try:
			UserKillFunction()
			print("Executed UserKillFunction()")
		except Exception as e:
			raise RuntimeError("UserKillFunction() Exception: ",e)
	
def KillOnExit():
	atexit.register(KillAndNotify)
