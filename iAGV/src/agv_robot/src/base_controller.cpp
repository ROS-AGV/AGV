//touch *

#include <ros/ros.h>
#include <serial/serial.h>
#include <std_msgs/String.h>
#include <std_msgs/Empty.h>
#include <tf/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Twist.h>
#include <iostream>
#include <math.h>
#include "JY901.h"

serial::Serial serial_port;
serial::Serial serial_port_IMU;

const double diameter = 0.153;
const double a_PARAMETER = 0.311;
const double b_PARAMETER = 0.3075;
const int counts_per_second = 1800;
int linearx_temp=0,angular_temp=0,lineary_temp=0,angular,speedx,speedy;


uint8_t speed_data[10];  


void cmd_velCallback(const geometry_msgs::Twist& cmd_vel)
{
    ROS_INFO("Received a /cmd_vel message.");
    ROS_INFO("Linear Components:[%f, %f, %f]", cmd_vel.linear.x, cmd_vel.linear.y, cmd_vel.linear.z);
    ROS_INFO("Angular Components:[%f, %f, %f]", cmd_vel.angular.x, cmd_vel.angular.y, cmd_vel.angular.z);

	int speed_x = abs(static_cast<int>(cmd_vel.linear.x));
	int speed_y = abs(static_cast<int>(cmd_vel.linear.y));
	int speed_th = abs(static_cast<int>(cmd_vel.angular.z));
	
	unsigned char txbuf[10];
	txbuf[0] = 0xff;
	txbuf[1] = 0xfe;
	txbuf[2] = 0x01;
	txbuf[3] = speed_x / 256;
	txbuf[4] = speed_x % 256;
	txbuf[5] = speed_y / 256;
	txbuf[6] = speed_y % 256;
	txbuf[7] = speed_th / 256;
	txbuf[8] = speed_th % 256;
	txbuf[9] = 0x00;

	if (cmd_vel.linear.x < 0)
		txbuf[9] |= 0x04;
	else if(cmd_vel.linear.y < 0)
		txbuf[9] |= 0x02;
	else if(cmd_vel.angular.z < 0)
		txbuf[9] |= 0x01;

	try {
    	serial_port.write(txbuf, 10);
	}
	catch (serial::IOException& e){
		ROS_ERROR_STREAM("Error in write data to serial port.");
		exit(-1);
	}
}

int main(int argc, char* argv[])
{    
    ros::init(argc, argv, "base_controller_node");
    ros::NodeHandle nh;


    try {
		serial_port.setPort("/dev/ttyUSB0");
		serial_port.setBaudrate(115200);
		serial::Timeout to = serial::Timeout::simpleTimeout(1000);
		serial_port.setTimeout(to);
		serial_port.open();
    }
    catch (serial::IOException& e) {
		ROS_ERROR_STREAM("Unable to open port");
                printf("Unable to open port.");
		exit(-1);
    }

    if(serial_port.isOpen()){
		ROS_INFO_STREAM("Serial Port initialized.");
		printf("Serial Port initialized.");
    } else {
		exit(-1);
    }


    ros::Rate loop_rate(50);
	ros::Time current_time, last_time = ros::Time::now();

    ros::Subscriber cmd_vel_sub = nh.subscribe("cmd_vel", 20, cmd_velCallback);
    ros::Publisher odom_pub = nh.advertise<nav_msgs::Odometry>("odom", 50);

    double x = 0, y = 0, th = 0;
    static tf::TransformBroadcaster odom_broadcaster;
    geometry_msgs::TransformStamped odom_trans;
    odom_trans.header.frame_id = "odom";
	odom_trans.child_frame_id = "base_footprint";

    
    //IMU data 
    char chrBuffer[2000];//buffer for IMU Y901
    unsigned short usLength=0,usCnt=0;
    float yaw=0.0f;

    try {
		serial_port_IMU.setPort("/dev/ttyUSB1");
		serial_port_IMU.setBaudrate(115200);
		serial::Timeout to = serial::Timeout::simpleTimeout(1000);
		serial_port_IMU.setTimeout(to);
		serial_port_IMU.open();
    }
    catch (serial::IOException& e) {
		ROS_ERROR_STREAM("Unable to open IMU port");
                printf("Unable to open IMU port.");
		exit(-1);
    }

    if(serial_port_IMU.isOpen()){
		ROS_INFO_STREAM("IMU Serial Port initialized.");
		printf("IMU Serial Port initialized.");
    } else {
		exit(-1);
    }


    while(ros::ok()) {		
		if(serial_port_IMU.isOpen() && serial_port_IMU.available()) 
		{
	    		unsigned short usLength = serial_port_IMU.available();
	    		if(usLength != 0) {
				usLength = serial_port_IMU.read((uint8_t *)chrBuffer, usLength);
				if (usLength>0)
				{
					JY901.CopeSerialData(chrBuffer,usLength);
				}
				if (usCnt++>=0)
				{
					usCnt=0;
					//printf("Time:20%d-%d-%d %d:%d:%.3f\r\n",(short)JY901.stcTime.ucYear,(short)JY901.stcTime.ucMonth,
					//		(short)JY901.stcTime.ucDay,(short)JY901.stcTime.ucHour,(short)JY901.stcTime.ucMinute,(float)JY901.stcTime.ucSecond+(float)JY901.stcTime.usMiliSecond/1000);

					printf("Acc:%.3f %.3f %.3f\r\n",(float)JY901.stcAcc.a[0]/32768*16,(float)JY901.stcAcc.a[1]/32768*16,(float)JY901.stcAcc.a[2]/32768*16);

					printf("Gyro:%.3f %.3f %.3f\r\n",(float)JY901.stcGyro.w[0]/32768*2000,(float)JY901.stcGyro.w[1]/32768*2000,(float)JY901.stcGyro.w[2]/32768*2000);

					printf("Angle:%.3f %.3f %.3f\r\n",(float)JY901.stcAngle.Angle[0]/32768*180,(float)JY901.stcAngle.Angle[1]/32768*180,(float)JY901.stcAngle.Angle[2]/32768*180);
					yaw = (float)JY901.stcAngle.Angle[2]/32768*180;

					//printf("Mag:%d %d %d\r\n",JY901.stcMag.h[0],JY901.stcMag.h[1],JY901.stcMag.h[2]);

					//printf("Pressure:%lx Height%.2f\r\n",JY901.stcPress.lPressure,(float)JY901.stcPress.lAltitude/100);

					//printf("DStatus:%d %d %d %d\r\n",JY901.stcDStatus.sDStatus[0],JY901.stcDStatus.sDStatus[1],JY901.stcDStatus.sDStatus[2],JY901.stcDStatus.sDStatus[3]);

					//printf("Longitude:%ldDeg%.5fm Lattitude:%ldDeg%.5fm\r\n",JY901.stcLonLat.lLon/10000000,(double)(JY901.stcLonLat.lLon % 10000000)/1e5,JY901.stcLonLat.lLat/10000000,(double)(JY901.stcLonLat.lLat % 10000000)/1e5);

					//printf("GPSHeight:%.1fm GPSYaw:%.1fDeg GPSV:%.3fkm/h\r\n\r\n",(float)JY901.stcGPSV.sGPSHeight/10,(float)JY901.stcGPSV.sGPSYaw/10,(float)JY901.stcGPSV.lGPSVelocity/1000);
				}
			}

		}

		if(serial_port.isOpen() && serial_port.available()) {
	    	size_t n = serial_port.available();
	    	if(n != 0) {
	    		uint8_t rxbuf[1024];

			try {
				n = serial_port.read(rxbuf, n);
				//ROS_INFO("Read %d bytes from serial port.\n", (int)n);
				//
				if (n != 12 || rxbuf[0] != 0xff || rxbuf[1] != 0xfe)
					continue;

				for(int i=0; i<n; i++){
                    			std::cout << std::hex << (rxbuf[i] & 0xff) << " ";
                		}

                		std::cout << std::endl;

				int encoder_A = (1 - rxbuf[3]) * rxbuf[2];
				int encoder_B = (1 - rxbuf[5]) * rxbuf[4];
				int encoder_C = (1 - rxbuf[7]) * rxbuf[6];
				int encoder_D = (1 - rxbuf[9]) * rxbuf[8];

				ros::Time current_time = ros::Time::now();
				double dt = (current_time - last_time).toSec();
				double delta_A = encoder_A * M_PI * diameter / counts_per_second;
			   	double delta_B = encoder_B * M_PI * diameter / counts_per_second;
				double delta_C = encoder_C * M_PI * diameter / counts_per_second;
				double delta_D = encoder_D * M_PI * diameter / counts_per_second;	

				double l = 4.0 / (a_PARAMETER + b_PARAMETER);
				double y = (delta_A + delta_B + delta_C + delta_D) / 4.0;
				double x = (delta_B - delta_A + delta_D - delta_C) / 4.0; 
				double th = (delta_C - delta_A + delta_D - delta_B) * l; 

				odom_trans.header.stamp = current_time;
				odom_trans.transform.translation.x = x;
				odom_trans.transform.translation.y = y;
				odom_trans.transform.translation.z = 0.0;

				odom_trans.transform.rotation = tf::createQuaternionMsgFromYaw(yaw*0.017453293);
				printf("Yaw:%.3f %.3f \r\n",(float)th,yaw*0.017453293);

				odom_broadcaster.sendTransform(odom_trans);
				last_time = current_time;
			}
			catch (serial::IOException& e) {
				ROS_ERROR_STREAM("Unable to read serial port.");
				exit(-1);
			}
				
	    	}
        }

		ros::spinOnce();
		loop_rate.sleep();
    }
	if(serial_port_IMU.isOpen() ){

	    serial_port_IMU.close();
	}
	if(serial_port.isOpen() ){

	    serial_port.close();
	}

    return 0;
}
