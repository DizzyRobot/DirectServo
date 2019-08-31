from pyds import DirectServo
from pyds import KillOnExit
import time, math, sys

# optionally set any motors in communication with python to be killed when the program exits
KillOnExit()

# Select the serial port you're connected to. Note: Multiple serial ports can be used at the same time.
SerialPortName = 'COM7'

# Example of creating and binding different ID'd servos to their own DirectServo instances.
#     In this way, we can send-to/receive-from RightKneeJoint only, using
#     the RightKneeJoint object, or send-to/receive-from all on the motors on
#     the selected serial port via the Broadcast object because it didn't specify an ID
Broadcast      = DirectServo(Port = SerialPortName)
RightHipJoint  = DirectServo(Port = SerialPortName, ID = 1)
RightKneeJoint = DirectServo(Port = SerialPortName, ID = 2)
LeftHipJoint   = DirectServo(Port = SerialPortName, ID = 3)
LeftKneeJoint  = DirectServo(Port = SerialPortName, ID = 4)

# Example of setting an id and then if successful: calibrating that newly ID'd servo
if False:
	if Broadcast.SetID( RightHipJoint.ID )["Code"] == "OK":
		print(RightHipJoint.Calibrate()) # responds when calibration command is accepted

# Example of reading the angles for 5 seconds
if False:
	start = time.time()
	Angles = [0,0,0,0]
	while time.time()-start < 5:
		Angles[0] = RightHipJoint.GetAngle()
		Angles[1] = RightKneeJoint.GetAngle()
		Angles[2] = LeftHipJoint.GetAngle()
		Angles[3] = LeftKneeJoint.GetAngle()
		print(Angles)
		sys.stdout.flush() # to print the results in real time

# Example setting the motor's failsafe torques, and initiation a search for its angular limits
if False:
	print(Broadcast.SetFailsafe(failsafe = 0 , search = 0.7, offset = 180))

# Example of resetting all attached servos
if False:
	print(Broadcast.Reset())

# Example o f setting the torque to 25% for 3 seconds
if False:
	print(Broadcast.Torque(0.25))
	sys.stdout.flush() # to print the results in real time
	time.sleep(3)
		
# Example of sweeping the torque through a sine wave, while printing fps performance info
if False:
	dt_avg = 0.01
	SinStep = 0.3
	RepeatCount = 7
	timestep_old = time.time()
	for _ in range(RepeatCount):
		SinInput = -3.14
		while SinInput<3.14:
			timestep = time.time()
			dt_avg = dt_avg*0.99 + (timestep - timestep_old)*0.01 # rolling average of dt
			timestep_old = timestep
			SinInput += SinStep
			print('fps = {:.3}'.format(1./dt_avg),Broadcast.Torque(math.sin(SinInput)*0.9,ResonseTimeout=0.001))
			sys.stdout.flush() # to print the results in real time

# Example of real-time plotting of a PID loop position controller
# Warning: This is NOT pre-tuned for your motor!
if False:
	import numpy as np
	import matplotlib.pyplot as plt
	import matplotlib.animation as anim
	
	PID_Controlled_Motor = LeftHipJoint
	
	# First select PID parameters
	
	# Using Ziegler–Nichols methods:
	# Ku = 0.1 # Ziegler–Nichols method's ultimate gain
	# Tu = 0.3 # Ziegler–Nichols method's oscillation period
	# PID = (0.60*Ku, 1.2*Ku/Tu, 3.0*Ku*Tu/40.0) # classic Ziegler–Nichols
	# PID = (7.0*Ku/10.0, 1.75*Ku/Tu, 21.0*Ku*Tu/200.0) # Pessen's method
	# PID = (Ku/3.0, 0.666*Ku/Tu, Ku*Tu/9.0) # some overshoot
	# PID = (Ku/5.0, 2.0/5.0*Ku/Tu, Ku*Tu/15.0) # no overshoot
	
	# Or, manual tunning:
	PID = (0.4, 0.1, 0.0025)

	# You can pretend that the motor spins more easily in one direction by adding a non-zero bias 
	# offset and seeing how the integral term will "learn" that offset and compensate for it :)
	SimulatedTorqueBias = 0.0
	
	# Reducing the integral decay ratio to less than 1.0 will result in a reduction in the 
	# influence of long-term errors, this may or not be a good thing depending on bias magnitudes
	IntegralDecay = 1.0
	
	# Select target position profile
	AnglesToDriveTo = [460, 215, -184, 215, 92, -460] # in degrees
	AnglesToDriveTo_Period = 5 # in seconds per angle
	
	# initialize some parameters
	timestep_old = 0
	error_old = 0.0
	angle_old = PID_Controlled_Motor.GetAngle()
	error_integral = 0.0
	NormalizedTorque = 0
	dtavg = 0.1
	start0 = time.time()
	
	# Create figure for plotting
	fig = plt.figure()
	
	# add AngleHistory plot
	Tx = np.linspace(0, AnglesToDriveTo_Period*len(AnglesToDriveTo), num=len(AnglesToDriveTo)+1)
	Ty = [AnglesToDriveTo[0]]+AnglesToDriveTo
	angle_buffer = max(AnglesToDriveTo)*0.2
	ax = fig.add_subplot(2, 1, 1, ylim=(min(AnglesToDriveTo)-angle_buffer, max(AnglesToDriveTo)+angle_buffer), xlim=(0, Tx[-1]))
	TargetHistory, = ax.step(Tx,Ty,'b-')
	xs = [0]
	ys = [angle_old]
	AngleHistory, = ax.plot(xs, ys,'o-',lw=2,color='k')
	ax.grid()
	timetext = ax.text(0.9, 0.9,'0 seconds', horizontalalignment='center', verticalalignment='center', transform = ax.transAxes)
	plt.title('Measured and Targeted Positions')
	plt.ylabel('Degrees')
	plt.xticks(np.arange(0, Tx[-1], step=AnglesToDriveTo_Period/2))
	
	# add PID Control Contributions plots
	axb = fig.add_subplot(2, 1, 2, ylim=(-0.75, 0.75), xlim=(0, Tx[-1]))
	xpe = [0]
	ype = [0.0]
	xie = [0]
	yie = [0.0]
	xde = [0]
	yde = [0.0]
	xt = [0]
	yt = [0.0]
	PHistory, = axb.plot(xpe, ype,'-',lw=2,color='r')
	IHistory, = axb.plot(xie, yie,'-',lw=2,color='b')
	DHistory, = axb.plot(xde, yde,'-',lw=2,color='g')
	THistory, = axb.plot(xt, yt,'--',lw=2,color='k')
	axb.grid()
	plt.title("Control Signals (Kp={0[0]}, Ki={0[1]}, Kd={0[2]})".format(PID))
	plt.ylabel('Normalized Torque')
	plt.legend((PHistory, IHistory, DHistory, THistory), ('P', 'I', 'D', 'Applied'))
	plt.xlabel('Seconds')
	plt.xticks(np.arange(0, Tx[-1], step=AnglesToDriveTo_Period/2))
	ended = False
	# This function is called periodically from FuncAnimation
	def PID_Controller(i):
		global ended, timetext, dtavg, AnglesToDriveTo_Period, error_integral, timestep_old, error_old, angle_old, NormalizedTorque
		timestep = time.time() - start0
		indx = math.floor(timestep/AnglesToDriveTo_Period)
		if indx<len(AnglesToDriveTo):
		
			dt = timestep - timestep_old
			dtavg = dtavg*0.9 + dt*0.1 # rolling average of dt
			desiredAngle = AnglesToDriveTo[indx]
			
			# Apply the Torque control signal and get the measured angle
			measured_angle = PID_Controlled_Motor.Torque(NormalizedTorque).get("Angle")
			if measured_angle is None:
				raise RuntimeError("bad response")
			
			# Degrade the error_integral term slightly, so that long term 
			# bias degrades in favor of more recent bias accumulation
			# note that this will have false positives in some scenarios
			error_integral *= IntegralDecay
			
			# calculate the error and PID components
			error_proportional = (desiredAngle-measured_angle) / 180.0 # normalized error
			error_integral += error_proportional * dt
			error_derivative = (error_proportional - error_old) / dt
			
			# apply the output torque and get the current position
			NormalizedTorque = error_proportional*PID[0] + error_integral*PID[1] + error_derivative*PID[2] + SimulatedTorqueBias
			
			# record historical measurements
			error_old = error_proportional
			angle_old = measured_angle
			timestep_old = timestep
			
			# record and set plot related data
			xs.append(timestep)
			ys.append(measured_angle)
			AngleHistory.set_data(xs, ys)
	
			xpe.append(timestep)
			ype.append(error_proportional*PID[0])
			PHistory.set_data(xpe,ype)
			
			xie.append(timestep)
			yie.append(error_integral*PID[1])
			IHistory.set_data(xie,yie)
			
			xde.append(timestep)
			yde.append(error_derivative*PID[2])
			DHistory.set_data(xde,yde)
			
			xt.append(timestep)
			yt.append(NormalizedTorque)
			THistory.set_data(xt,yt)
			
			timetext.set_text('fps {:0.4}'.format(1/dtavg))
			
		else:
			if not ended:
				# turn off motor current when finished and continue to print response details
				ended = True
				print("Ended at:",PID_Controlled_Motor.Torque(0))
				sys.stdout.flush() # to print the results in real time
		
		# return any graphical objects modified by the function
		return AngleHistory,TargetHistory,PHistory,IHistory,DHistory,THistory,timetext,
	
	# Run and show the function animation
	PID_Animation = anim.FuncAnimation( fig, PID_Controller, interval=0, blit=True )
	plt.show()

