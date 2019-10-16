#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>

int main(int argc, char *argv[])
{
    /* code for main function */
    ros::init(argc, argv, "talker");
    
    ros::NodeHandle nh;
    ros::Publisher scan_pub = nh.advertise<sensor_msgs::LaserScan>("scan", 50);

    unsigned int num_readings = 100;
    double laser_frequency = 40;
    double ranges[num_readings];
    double intensities[num_readings];

    int count = 0;
    ros::Rate r(1.0);
    while(nh.ok()){
        ROS_INFO("This is the %d scan", count);

        for(unsigned int i=0; i<num_readings; ++i){
            ranges[i] = count;
            intensities[i] = 100 + count;
        }

        ros::Time scan_time = ros::Time::now();

        sensor_msgs::LaserScan scan;
        scan.header.stamp = scan_time;
        scan.header.frame_id = "base_link";
        scan.angle_min = -1.57;
        scan.angle_max = 1.5386;
        scan.angle_increment = 3.14 / num_readings;
        scan.time_increment = (1 / laser_frequency) / (num_readings);
        scan.range_min = 0.0;
        scan.range_max = 100.0;

        scan.ranges.resize(num_readings);
        scan.intensities.resize(num_readings);
        for(unsigned int i=0; i<num_readings; ++i){
            scan.ranges[i] = ranges[i];
            scan.intensities[i] = intensities[i];
        }

        scan_pub.publish(scan);
        ++count;
        r.sleep();
    }

    return 0;
}
