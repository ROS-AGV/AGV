#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>
#include <tf/transform_broadcaster.h>
#include <fstream>
#include <chrono>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <unistd.h>
#include "stateimage.h"
#include "udpstateimage.h"

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



int main(int argc, char *argv[])
{
    /* code for main function */
    ros::init(argc, argv, "laser_scan");
    ros::NodeHandle nh;
    ros::Publisher scan_publisher = nh.advertise<sensor_msgs::LaserScan>("base_scan", 50);
    tf::TransformBroadcaster broadcaster;
    tf::Transform laser_transform;

    memset(mBeams, 0, sizeof(RSL400::Beam) * SCANDATA_SIZE);
    std::ofstream outf;
    outf.open("../data.txt");

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
