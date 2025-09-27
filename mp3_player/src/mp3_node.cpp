#include "ros/ros.h"
#include "std_msgs/String.h"

//콜백 함수: /mp3_cmd 토픽 구독
void mp3Callback(const std_msgs::String::ConstPtr& msg)
{
    ROS_INFO("MP3 Command Received: %s", msg->data.c_str());
    // TODO : 여기에 UART write 코드 추가
}

int main(int argc, char **argv)
{
    ros::init(argc,argv, "mp3_node");
    
    ros::NodeHandle nh;

    ros::Subscriber sub = nh.subscribe("mp3_cmd", 10, mp3Callback);

    ROS_INFO("MP3 Node Started. Waiting for commands....");

    ros::spin();

    return 0;
}