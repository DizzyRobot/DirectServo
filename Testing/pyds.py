'''
Python DirectServo (pyds.py)
Tested in python 3.5 and 3.7

'''
import sys, serial, time, math, threading, queue, atexit

GlobalKillSwitch = False

# Static lifetime RS485 connections are stored in PortMap, which 
# allows many DirectServo class instances to be bound to ports 
# without clashing port connections
PortMap = {}

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
		Angle = YourServo.SetFailsafe()  # turns on a failsafe feature, and passes optional settings:
		                                 # 'failsafe':-1.0 to 1.0, 'search':0.0 to 1.0, 'offset': value in degrees
		Angle = YourServo.Reset()        # resets the board and clears the flash memory to default settings
	
	"""
	Dec2HexByte = lambda _,x: hex(abs(x))[2:].zfill(2).upper()[-2:]
	Broadcast = 0xFF
	Cmd_Torque     = "T" # passes torque parameter: [-255 to 255]
	Cmd_SetID      = "I" # passes ID parameter: [0 to 255] 0xFF is reserved for broadcast
	Cmd_Calibrate  = "C" # no passed parameters
	Cmd_GetAngle   = "a" # no passed parameters
	Cmd_Failsafe   = "F" # passes:{
							# uint8_t torqueFailoverValue;
							# uint8_t torqueSearchValue;
							# uint8_t torqueOffsetMSByte;
							# uint8_t torqueOffsetLSByte;}
	Cmd_Reset      = "R" # no passed parameters (resets flash)
							
	FailsafeDefaults = {
		'failsafe':0, # torque ratio [-1.0 to 1.0]
		'search':0.4, # torque search magnitude [0.0 to 1.0]
		'offset':10 # offset in degrees to failsafe > 0.0
		}
	def _ReceivedThread(self):
		while True:
			if self.RS485.inWaiting():
				Response = self.RS485.readline()
				try:
					Response = Response.decode().replace("\n", '').replace("\r", '')
					ID = int.from_bytes(bytes.fromhex(Response[:2]), byteorder = 'big', signed = False)
					if Response[2:]!='error':
						Angle = 360.0*(int.from_bytes(bytes.fromhex(Response[2:]), byteorder = 'big', signed = True) / float(0xFFFF))
						Response = {"ID":ID, "Angle":Angle, "Code":"OK"}
					else:
						Response = {"ID":ID, "Angle":None,  "Code":Response[2:]}
				except KeyboardInterrupt:
					raise
				except:
					Response = {"ID":255, "Angle":None, "Code":Response}
					print('Received unrecognized response: "{}"'.format(Response))
					self.RS485.flushInput()
					self.RS485.flushOutput()
				if self.RxQueues.get(self.ID) is None:
					self.RxQueues[self.ID] = queue.Queue()
				ReceivedFromID = Response.get("ID",255)
				if self.RxQueues.get(ReceivedFromID) is None:
					self.RxQueues[ReceivedFromID] = queue.Queue()
				self.RxQueues[ReceivedFromID].put(Response)
				if ReceivedFromID != self.Broadcast:
					self.RxQueues[self.Broadcast].put(Response)
		
	def __init__(self, ID = Broadcast, Port = None, BaudRate = 115200):
		if Port is None:
			raise RuntimeError("Must specify a serial port when creating a DirectServo object. Example: DirectServo(Port = 'COM1')")
		self.ID = ID
		self.RS485 = PortMap.get(Port,{}).get('serial')
		self.RxQueues = PortMap.get(Port,{}).get('queues',{})
		self.RxThread = PortMap.get(Port,{}).get('thread')
		if self.RS485 is None:
			self.RS485 = serial.Serial(port = Port, baudrate = BaudRate)
			if self.RxQueues.get(self.ID) is None:
				self.RxQueues[self.ID] = queue.Queue()
			if self.RxQueues.get(self.Broadcast) is None:
				self.RxQueues[self.Broadcast] = queue.Queue()
			self.RxThread = threading.Thread(target=self._ReceivedThread,daemon=True)
			self.RxThread.start()
			PortMap[Port] = {'serial':self.RS485,'queues':self.RxQueues,'thread':self.RxThread}
			if self.RS485.isOpen():
				self.RS485.flushInput()
				self.RS485.flushOutput()
		if not self.RS485.isOpen():
			raise RuntimeError("{}.RS485({{}}) connection not open".format(self,self.ID))
		
	def Read(self, CommandTerm, ResonseTimeout = 0.0):
		if self.RxQueues.get(self.ID) is None:
			self.RxQueues[self.ID] = queue.Queue()
		try:
			respns = self.RxQueues[self.ID].get(True,ResonseTimeout)
			if not respns is None:
				return respns
		except queue.Empty:
			pass
		return {"ID":0xFF, "Angle":None, "Code":"No Response"}
		
	def Write(self, CommandTerm, Parameter = '', Destination = Broadcast, ResonseTimeout = 0.2, OverrideSend = None):
		global GlobalKillSwitch
		if CommandTerm == self.Cmd_Torque:
			if GlobalKillSwitch and not (Parameter == 0 or Parameter == 0.0):
				return {"ID":0xFF, "Angle":None, "Code":"Disallowed cmd during killed"}
			if Parameter<0:
				Parameter = '-'+self.Dec2HexByte(Parameter)
			else:
				Parameter = '+'+self.Dec2HexByte(Parameter)
		elif CommandTerm == self.Cmd_Failsafe:
			if not isinstance(Parameter,dict):
				raise RuntimeError("Must pass a dictionary, with one or more of the following:",FailsafeDefaults)
			Options = {**self.FailsafeDefaults,**Parameter}
			failsafe = Options['failsafe']
			search = Options['search']
			offset = Options['offset']
	
			if failsafe>1.0:
				failsafe = 1
			elif failsafe<-1.0:
				failsafe = -1
			failsafe = int(failsafe*0xFF)
			
			if search<0.0:
				search = 0
			elif search >1.0:
				search = 1
			search = int(search*0xFF)
				
			if offset<0.0:
				offset = 0.0
			offset = int(float(offset)/360.0*float(0xFFFF))
				
			if Options['failsafe']<0:
				Parameter = '-'
			else:
				Parameter = '+'
			Parameter += self.Dec2HexByte(failsafe)
			Parameter += self.Dec2HexByte(search)
			Parameter += self.Dec2HexByte(offset >> 8)
			Parameter += self.Dec2HexByte(offset)
		elif CommandTerm == self.Cmd_SetID:
			Parameter = self.Dec2HexByte(Parameter)
		Destination = self.Dec2HexByte(Destination)
		Sent = OverrideSend
		if Sent is None:
			Sent = "{}{}{}".format(Destination,CommandTerm,Parameter)
		self.RS485.flush() # wait to write if there's something already waiting to go out
		self.RS485.write("{}\n".format(Sent).encode())
		return self.Read(CommandTerm, ResonseTimeout)
	
	def Torque(self, TorqueRatio,**kargs):
		if TorqueRatio>1.0:
			TorqueRatio = 1
		elif TorqueRatio<-1.0:
			TorqueRatio = -1
		return self.Write(self.Cmd_Torque, int(TorqueRatio*0xFF), self.ID,**kargs)
		
	def SetID(self, IDValue = None, Destination = Broadcast,**kargs):
		if IDValue is None:
			IDValue = self.ID
		return self.Write(self.Cmd_SetID, IDValue, Destination,**kargs)
		
	def Calibrate(self,**kargs):
		return self.Write(self.Cmd_Calibrate, Destination = self.ID,**kargs)
		
	def GetAngle(self,**kargs):
		return self.Write(self.Cmd_GetAngle, Destination = self.ID,**kargs).get("Angle")
		
	def SetFailsafe(self, failsafe = FailsafeDefaults['failsafe'],
					search = FailsafeDefaults['search'],
					offset = FailsafeDefaults['offset'],
					ResonseTimeout = 20.0,**kargs):
		return self.Write(self.Cmd_Failsafe, {'failsafe':failsafe,'search':search,'offset':offset}, self.ID, ResonseTimeout = ResonseTimeout,**kargs)
	
	def Reset(self,**kargs):
		return self.Write(self.Cmd_Reset, Destination = self.ID,**kargs)
	
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
