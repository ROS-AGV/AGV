

#include <ros/ros.h>
#include <serial/serial.h>
#include <std_msgs/String.h>
#include <std_msgs/Empty.h>
#include <tf/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Twist.h>
#include <iostream>
#include <math.h>
#include <sensor_msgs/LaserScan.h>
#include <fstream>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <unistd.h>
#include "stateimage.h"
#include "udpstateimage.h"



serial::Serial serial_port;
const double diameter = 0.153;
const double a_PARAMETER = 0.311;
const double b_PARAMETER = 0.3075;
const int counts_per_second = 1800;
int linearx_temp=0,angular_temp=0,lineary_temp=0,angular,speedx,speedy;

#define SCANDATA_SIZE	4096
#define CORRECT_DATA_SIZE 2700
#define MAX_BEAM 2699
#define GMAPPING_DATA_SIZE 1350							// Adaption for gmapping
#define BLOCK_NUM	4
#define PI 3.1415926535897932384626433832795

RSL400::Beam mBeams[SCANDATA_SIZE];

const float angleMin = -45.0 * (PI / 180.0);       	//angle correspond to FIRST beam in scan ( in rad)
const float angleMax= (225.0 - 0.1) * (PI / 180.0);     //angle correspond to LAST beam in scan ( in rad)
const float angleIncrement = 0.2 * (PI / 180.0); 		// 应用gmapping，抽稀雷达扫瞄线

const float rangeMin =0 ;   							// in meters
const float rangeMax = 20.0;  							// scan can measure upto this range

const float scan_time = 0.080;							// time between scans [seconds]

unsigned int mCurrentScanNo = 0;
const int scaleToMap = 128;								//1024*1024的图像，假设为8*8米
const int maxBlockNo = 5;


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

int main(int argc, char* argv[]){
    ros::init(argc, argv, "laser_scan");
    ros::NodeHandle n;
	
    ros::Publisher scan_publisher = n.advertise<sensor_msgs::LaserScan>("base_scan", 50);
    tf::TransformBroadcaster broadcaster;
    tf::Transform laser_transform;

    memset(mBeams, 0, sizeof(RSL400::Beam) * SCANDATA_SIZE);
    std::ofstream outf;
    outf.open("./data_y.txt");

    int receiverSocket;
    struct sockaddr_in receiverAddr;
    int port = 9999;
    int  receiveBufferLen = 2048000;
    char *receiveBuffer = new char[receiveBufferLen];
    struct sockaddr_in senderAddr;
    int senderAddrSize = sizeof(senderAddr);

    RSL400::Beam *ptr;										//拷贝数据起始位置
    int numReadings = 0;
    int preBlockNo = 0;
    bool correctOrderFlag = true;							//判断之前扫描BlockNo是否乱序


    //-----------------------------------------------
    // 创建一个接收器socket来接收数据报
    receiverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    //-----------------------------------------------
    // 将套接字绑定到任何地址和指定的端口
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(port);
    receiverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(receiverSocket, (const struct sockaddr*)&receiverAddr, sizeof(receiverAddr));

    //-----------------------------------------------
    //调用recvfrom函数以接收绑定套接字上的数据报
    ROS_INFO("Receiving datagrams...");

    char key;

    // 接收100个数据包
    while(key != 'q' && key != 27) {
    	auto beginTime = std::chrono::system_clock::now();

		int received = recvfrom(receiverSocket,
			        			receiveBuffer,
			        			receiveBufferLen,
			        			0,
			        			(struct sockaddr*)&senderAddr,
			        			(socklen_t*)&senderAddrSize);

		//-----------------------------------------------
		// 在发生错误时中止
		if (received == -1) {
	    	printf("WSAGetLastError: %d\r\n", received);
	    	break;
		} else {
	    	//fwrite(receiveBuffer, received, 1, fp);
		}

		RSL400::PUdpTelegramType udpTelegramType = (RSL400::PUdpTelegramType)receiveBuffer;

		//printf("--> Telegram Type: %d\r\n", udpTelegramType->Id);	

		if (udpTelegramType->Id == 6) {
	    	//cout << "type is 6\r\n" << endl;
	    	RSL400::PUdpBeamPacket t3 = (RSL400::PUdpBeamPacket)receiveBuffer;

	    	if (t3->BlockNo == 0) {
				// Draw current image 
				//DrawScanData(figure, mBeams, numReadings, key);
			
				mCurrentScanNo = t3->ScanNo;
				int size = (t3->H1.TotalSize - 20) / 2;
				memcpy(mBeams, t3->Beams, size * sizeof(RSL400::Beam));
				ptr = mBeams + size;

				numReadings = size;
				preBlockNo = 0;
				correctOrderFlag = true;
	    	} else if (mCurrentScanNo == t3->ScanNo && correctOrderFlag) {
				if (t3->BlockNo == preBlockNo + 1) {
		    		int size = (t3->H1.TotalSize - 20) / 2;
		    		memcpy(ptr, t3->Beams, size * sizeof(RSL400::Beam));
		    		ptr += size;

		    		numReadings += size;
		    		preBlockNo = t3->BlockNo;
				} else {
		    		correctOrderFlag = false;
		    		continue;
				}

				// if blockNo is 5 and number of reading is 2700, the data is published
				if (t3->BlockNo == maxBlockNo && numReadings == CORRECT_DATA_SIZE) {
		    		ros::Time scan_time = ros::Time::now();

		    		sensor_msgs::LaserScan scanMsg;
		    		scanMsg.header.stamp = scan_time;
		    		scanMsg.header.seq = mCurrentScanNo;
		    		scanMsg.header.frame_id = "base_laser";

		    		scanMsg.angle_min = angleMin;
   		    		scanMsg.angle_max = angleMax;				
		    		scanMsg.angle_increment = angleIncrement;
   		    		scanMsg.time_increment = 0.08 / GMAPPING_DATA_SIZE;
   		    		scanMsg.range_min = 0.1;
   		    		scanMsg.range_max = 30.0;
   					
		    		scanMsg.ranges.resize(GMAPPING_DATA_SIZE);
   		    		scanMsg.intensities.resize(GMAPPING_DATA_SIZE);

		    		for (unsigned int i = 0; i < numReadings; i+=2) {
    					scanMsg.ranges[i/2] = mBeams[MAX_BEAM - i].Distance / 1000.0;
       					scanMsg.intensities[i/2] = i;
   		    		}

   		    		scan_publisher.publish(scanMsg);

		    		double currentTime = scanMsg.header.stamp.toSec();
		    		std::cout << "scan.header.stamp = " << std::setprecision(20) << currentTime << std::endl;
				}
	    	}

	    	//std::cout << "ScanNo = " << t3->ScanNo << "; BlockNo = " << t3->BlockNo << ";  ";

	    	auto endTime = std::chrono::system_clock::now();
	    	std::chrono::milliseconds timeInterval = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime);
	    	//std::cout << "time cost is " << double(timeInterval.count()) * std::chrono::milliseconds::period::num / 
            //  std::chrono::milliseconds::period::den
	    	// << " seconds" << std::endl;
		}	
    }

    try {
		serial_port.setPort("/dev/ttyUSB0");
		serial_port.setBaudrate(115200);
		serial::Timeout to = serial::Timeout::simpleTimeout(1000);
		serial_port.setTimeout(to);
		serial_port.open();
    }
    catch (serial::IOException& e) {
		ROS_ERROR_STREAM("Unable to open port");
		exit(-1);
    }

    if(serial_port.isOpen()){
		ROS_INFO_STREAM("Serial Port initialized.");
    } else {
		exit(-1);
    }
    
    ros::init(argc, argv, "base_controller_node");
    ros::Rate loop_rate(50);
    ros::Time current_time, last_time = ros::Time::now();

    ros::Subscriber cmd_vel_sub = n.subscribe("cmd_vel", 20, cmd_velCallback);
    ros::Publisher odom_pub = n.advertise<nav_msgs::Odometry>("odom", 50);

    double x = 0, y = 0, th = 0;
    static tf::TransformBroadcaster odom_broadcaster;
    geometry_msgs::TransformStamped odom_trans;
    odom_trans.header.frame_id = "odom";
    odom_trans.child_frame_id = "base_footprint";

    while(ros::ok()) {
		if(serial_port.available()) {
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
					odom_trans.transform.rotation = tf::createQuaternionMsgFromYaw(th);

					odom_broadcaster.sendTransform(odom_trans);
					last_time = current_time;
				}
				catch (serial::IOException& e) {
					ROS_ERROR_STREAM("Unable to read serial port.");
					exit(-1);
				}
					
				/*for(int i=0; i<n; i++){
		    		//std::cout << std::hex << (buffer[i] & 0xff) << " ";
				}

				std::cout << std::endl;*/
	    	}
        }

		ros::spinOnce();
		loop_rate.sleep();
    }

    //-----------------------------------------------
    // 完成接收数据报后关闭socket
    printf("Finished receiving. Closing socket.\n");
    //closesocket(receiverSocket);
    close(receiverSocket);

    ros::shutdown();
    //-----------------------------------------------
    // 清理并退出
    printf("Exiting.\n");	
    delete[] receiveBuffer;
	
    outf.close();

    ros::spin();

    return 0;
}
