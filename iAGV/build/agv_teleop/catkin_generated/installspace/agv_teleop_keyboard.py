#!/usr/bin/env python2

import roslib
import rospy

from geometry_msgs.msg import Twist

import sys, select, termios, tty

msg = """
"""

moveBindings = {
                'd' : (10, 0, 0),
		'a' : (-10, 0, 0), 
		'w' : (0, 10, 0), 
		's' : (0, -10, 0),
		'f': (0, 0, 10),
		'g': (0, 0, -10),
		}

def getKey():
    tty.setraw(sys.stdin.fileno())
    rlist, _, _ = select.select([sys.stdin], [], [], 0.1)

    if rlist:
	key = sys.stdin.read(1)
    else:
	key = ''

    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
    return key

if  __name__=="__main__":
    settings = termios.tcgetattr(sys.stdin)

    rospy.init_node('keyboard')
    pub = rospy.Publisher('cmd_vel', Twist, queue_size=5)

    speed_x = 0
    speed_y = 0
    speed_th = 0
    count = 0

    try:
	
	while(1):
	    key = getKey()


 	    if key in moveBindings.keys():
		speed_x = moveBindings[key][0]
		speed_y = moveBindings[key][1]
		speed_th = moveBindings[key][2]
		count = 0
	    elif key == ' ' or key == 'q':
		speed_x = 0
		speed_y = 0
		speed_th = 0

	    else:
		   
		count = count + 1
 		if count > 4:
                    speed_x = 0
		    speed_y = 0
		    speed_th = 0 
		if (key == '0x03'):
		    break
			
 	    twist = Twist()
	    twist.linear.x = speed_x; twist.linear.y = speed_y; twist.linear.z = 0
	    twist.angular.x = 0; twist.angular.y = 0; twist.angular.z = speed_th 
	    pub.publish(twist)

    finally:
	twist = Twist()
	twist.linear.x = 0; twgist.linear.y = 0; twist.linear.z = 0
	twist.angular.x = 0; twist.angular.y = 0; twist.angular.z = 0 
	pub.publish(twist)

    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
