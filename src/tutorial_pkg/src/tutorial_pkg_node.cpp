#include "ros/ros.h"
#include "geometry_msgs/Twist.h"
#include "nav_msgs/Odometry.h"
#include "sensor_msgs/LaserScan.h"

#include <fstream>
#include <time.h>
#include <iomanip>
#include <iostream>
#include <cmath> 
#include <chrono>
#include <vector>
#include <utility>

using namespace std;
using namespace std::chrono;

struct EulerAngles{
    double roll, pitch, yaw;
}; // yaw is what you want, i.e. Th

struct Quaternion{
    double w, x, y, z;
}; 

EulerAngles ToEulerAngles(Quaternion q) {
    EulerAngles angles;
    // roll (x-axis rotation)
    double sinr_cosp = +2.0 * (q.w * q.x + q.y * q.z);
    double cosr_cosp = +1.0 - 2.0 * (q.x * q.x + q.y * q.y);
    angles.roll = atan2(sinr_cosp, cosr_cosp);
    // pitch (y-axis rotation)
    double sinp = +2.0 * (q.w * q.y - q.z * q.x);
    if (fabs(sinp) >= 1)
        angles.pitch = copysign(M_PI/2, sinp); // use 90 degrees if out of range
    else
        angles.pitch = asin(sinp);
    // yaw (z-axis rotation)
    double siny_cosp = +2.0 * (q.w * q.z + q.x * q.y);
    double cosy_cosp = +1.0 - 2.0 * (q.y * q.y + q.z * q.z); 
    angles.yaw = atan2(siny_cosp, cosy_cosp);
    return angles;
}

class RobotMove { //main class
public:
    // Tunable parameters
    constexpr const static double FORWARD_SPEED_LOW = 0.1;
    constexpr const static double FORWARD_SPEED_MIDDLE = 0.3;
    constexpr const static double FORWARD_SPEED_HIGH = 0.5;
    // constexpr const static double FORWARD_SPEED_SHIGH = 0.4;
    constexpr const static double FORWARD_SPEED_STOP = 0;
    constexpr const static double TURN_LEFT_SPEED_HIGH = 1.0;
    constexpr const static double TURN_LEFT_SPEED_MIDDLE = 0.6;
    constexpr const static double TURN_LEFT_SPEED_LOW = 0.3;
    constexpr const static double TURN_RIGHT_SPEED_HIGH = -1.0; 
    constexpr const static double TURN_RIGHT_SPEED_MIDDLE = -0.59; //-0.6
    constexpr const static double TURN_RIGHT_SPEED_LOW = -0.3;

    RobotMove();
    void startMoving();
    void moveForward(double forwardSpeed);
    void moveStop(); 
    void moveRight(double turn_right_speed = TURN_RIGHT_SPEED_HIGH);
    void moveForwardRight(double forwardSpeed, double turn_right_speed);
    void odomCallback(const nav_msgs::Odometry::ConstPtr& odomMsg);
    void scanCallback(const sensor_msgs::LaserScan::ConstPtr& scan); // lab5
    void transformMapPoint(ofstream& fp, double laserRange, double laserTh);
    void PID_wallFollowing(double moveSpeed, double laserData);

private:
    ros::NodeHandle node;
    ros::Publisher commandPub; // Publisher to the robot's velocity command topic
    ros::Subscriber odomSub; // Subscriber to the odometry topic
    ros::Subscriber laserSub; // Subscriber to robot’s laser topic
    ros::Time current_time;
    ros::Duration real_time;
    double PositionX = 0.3, PositionY = 0.3, landmark1 = 1.15, landmark2 = 0.9;
    double landmark3 = 1.2, landmark4 = 1.83, landmark5 = 2.25;
    double homeX = 0.3, homeY = 0.3;
    double robVelocity = 0.0; // Robot's velocity

    double frontRange=0,mleftRange=0,leftRange=0,rightRange=0,mrightRange=0;
    double backRange=0, backleftRange=0, backrightRange=0, laserData[36];
    double frontAngle=0, mleftAngle=M_PI/4, leftAngle=M_PI/2;
    double rightAngle=-M_PI/2, mrightAngle=-M_PI/4;
    double backAngle=M_PI, backleftAngle=3*M_PI/4, backrightAngle=-3*M_PI/4;
    double kp1 = 0.01, ki1 = 0.001, kd1 = 0.001, ei_pre1 = 0;
    double ed_pre1 = 0, landmark1_toWall = 0.3, Max_PID_outout = 0.6;

    Quaternion robotQuat;
    EulerAngles robotAngles; 
    double robotHeadAngle;
};

ofstream openFile(const string& name) { 
    string homedir = getenv("HOME"); 
    ostringstream path; 
    path << homedir << "/ros_workspace/src/tutorial_pkg/" << name; 
    cout << "File path: " << path.str() << endl; // Print the path for debugging
    ofstream file(path.str());
    if (!file.is_open()) {
        cerr << "Failed to open file: " << path.str() << endl;
    }
    return file;
};

RobotMove::RobotMove() {
    // Advertise a new publisher for the simulated robot's velocity command topic at 10Hz
    commandPub = node.advertise<geometry_msgs::Twist>("cmd_vel", 10);
    odomSub = node.subscribe("odom", 20, &RobotMove::odomCallback, this); // Subscriber to the odometry topic
    laserSub = node.subscribe("scan", 1, &RobotMove::scanCallback, this);
}

// Send a velocity command
void RobotMove::moveForward(double forwardSpeed) {
    geometry_msgs::Twist msg; // The default constructor to set all commands to 0
    msg.linear.x = forwardSpeed; // Drive forward at a given speed along the x-axis.
    commandPub.publish(msg);
}

void RobotMove::moveStop() {
    geometry_msgs::Twist msg; 
    msg.linear.x = FORWARD_SPEED_STOP;
    commandPub.publish(msg);
}

void RobotMove::moveRight(double turn_right_speed) {
    geometry_msgs::Twist msg; 
    msg.angular.z = turn_right_speed;
    commandPub.publish(msg);
}

void RobotMove::moveForwardRight(double forwardSpeed, double turn_right_speed) { 
    // Move forward and right at the same time 
    geometry_msgs::Twist msg; 
    msg.linear.x = forwardSpeed; 
    msg.angular.z = turn_right_speed; 
    commandPub.publish(msg); 
}

// Callback function to determine the robot position and velocity
void RobotMove::odomCallback(const nav_msgs::Odometry::ConstPtr& odomMsg) {
    PositionX = odomMsg->pose.pose.position.x + homeX;
    PositionY = odomMsg->pose.pose.position.y + homeY;
    robVelocity = odomMsg->twist.twist.linear.x; // Get the linear velocity
    ROS_INFO_STREAM("PositionX: " << PositionX << " PositionY: " << PositionY << " Velocity: " << robVelocity); // Debugging
    robotAngles = ToEulerAngles(robotQuat);
    robotHeadAngle = robotAngles.yaw;
    robotQuat.x = odomMsg->pose.pose.orientation.x;
    robotQuat.y = odomMsg->pose.pose.orientation.y;
    robotQuat.z = odomMsg->pose.pose.orientation.z;
    robotQuat.w = odomMsg->pose.pose.orientation.w;
}  

void RobotMove::scanCallback(const sensor_msgs::LaserScan::ConstPtr& scan){
    // collect 36 laser readings every 360 degrees scan
    for(int i=0; i<36; i++) // to get 36 laser readings over 360 degrees
        laserData[i] = scan->ranges[i*10]; // to get laser readings every 10 degrees
    // the following code for the control purpose
    frontRange = scan->ranges[0]; // get the range reading at 0 radians
    mleftRange = scan->ranges[89]; // get the range reading at -π/4 radians
    leftRange = scan->ranges[179]; // get the range reading at -π/2 radians
    rightRange = scan->ranges[539]; // get the range reading at π/2 radians
    mrightRange = scan->ranges[629]; // get the range reading at π/4 radians
    backRange = scan->ranges[359]; // get the range reading at π radians
    backleftRange = scan->ranges[269]; // get the range reading at π/2 radians
    backrightRange = scan->ranges[449]; // get the range reading at π/4 radians
}

void RobotMove::transformMapPoint(ofstream& fp, double laserRange, double laserTh)
{
    double localX, localY, globalX, globalY;
    localX = laserRange * cos(laserTh);
    localY = laserRange * sin(laserTh);
    globalX =(localX*cos(robotHeadAngle)-localY*sin(robotHeadAngle))+ PositionX;
    globalY = (localX*sin(robotHeadAngle)+localY*cos(robotHeadAngle))+ PositionY;
    if (globalX < 0) globalX = 0; else if (globalX > 2.5) globalX = 2.5;
    if (globalY < 0) globalY = 0; else if (globalY > 2.5) globalY = 2.5;
    fp << globalX << " " << globalY << endl;
}

void RobotMove::PID_wallFollowing(double moveSpeed, double laserData)
{
    double ei, ed, err, output;
    err = laserData - landmark1_toWall;
    ei = ei_pre1 + err; ed = err - ed_pre1;
    ei_pre1 = ei; ed_pre1 = ed;
    output = kp1*err + ki1*ei + kd1*ed;
    if (output > Max_PID_outout ) 
        output = Max_PID_outout; 
    else 
        if(output < -Max_PID_outout) 
            output = -Max_PID_outout;
    geometry_msgs::Twist msg;
    msg.linear.x = moveSpeed;
    msg.angular.z = output;
    commandPub.publish(msg);
}

void RobotMove::startMoving() { 
    ofstream odomTrajFile = openFile("odomTrajData.csv");
    ofstream odomVelFile = openFile("odomVelData.csv");
    ofstream laserFile = openFile("laserData.csv");
    ofstream laserMapFile = openFile("laserMapData.csv");
    if (!odomTrajFile.is_open() || !odomVelFile.is_open() || !laserFile.is_open() || !laserMapFile.is_open()) {
        ROS_ERROR("Failed to open file for writing");
        return;
    }
    auto startTime = high_resolution_clock::now();
    int stage = 1; 
    ros::Rate rate(20); // Define rate for repeatable operations. 
    ROS_INFO("Start moving");
    while (ros::ok()) { // Keep spinning loop until user presses Ctrl+C 
        switch(stage) {
            case 1: // The robot moves forward from home 
                if (PositionY < landmark1) 
                    PID_wallFollowing(FORWARD_SPEED_HIGH, rightRange); // create “the PID-based wall following behaviour”
                else 
                    stage = 2;
                break;
            case 2: // The robot turns right toward the 1st gap 
                if (PositionX < landmark2) 
                    PID_wallFollowing(FORWARD_SPEED_MIDDLE, rightRange); // create “PID-based going through the 1st gap behaviour”
                else 
                    stage = 3;
                break;
            case 3: // The robot stops at the middle of the 1st gap 
                if (PositionX < landmark3)
                    PID_wallFollowing(FORWARD_SPEED_MIDDLE, rightRange); // create “PID-based moving toward the 2nd gap behaviour”
                else 
                    stage = 4;
                break;
            case 4: // The robot moves and turns right slowly
                if (PositionX < landmark4) 
                    PID_wallFollowing(FORWARD_SPEED_LOW, rightRange); // create “PID-based going through the 2nd gap behaviour”
                else 
                    stage = 5; 
                break;
            case 5: // The robot moves towards the charger
                if (PositionX < landmark5)
                    PID_wallFollowing(FORWARD_SPEED_LOW, frontRange); // create “PID-based reaching goal behaviour”
                else 
                    stage = 6;
                break;
            case 6: // Stop at the charger position
                moveStop(); 
                break;
        } 
        auto currentTime = high_resolution_clock::now(); 
        duration<double> runTime = currentTime - startTime;
        odomVelFile << runTime.count() << " " << robVelocity << endl;
        odomTrajFile << PositionX << " " << PositionY << endl;
        ROS_INFO_STREAM("Writing to file: " << PositionX << " " << PositionY); // Debugging

        for(int i=0; i<36; i++) // save laser data for view and check
            laserFile << i << " " << laserData[i];
        laserFile << endl;

        transformMapPoint(laserMapFile, frontRange, frontAngle);
        transformMapPoint(laserMapFile, leftRange, leftAngle);
        transformMapPoint(laserMapFile, rightRange, rightAngle);
        transformMapPoint(laserMapFile, mleftRange, mleftAngle);
        transformMapPoint(laserMapFile, mrightRange, mrightAngle);
        ros::spinOnce(); // Allow ROS to process incoming messages 
        rate.sleep(); // Wait until defined time passes.
    }
    odomTrajFile.close();
    odomVelFile.close();
    laserFile.close();
    laserMapFile.close();
    ROS_INFO("Closing files");
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "RobotMove"); // Initiate new ROS node named "RobotMove"
    RobotMove robotMove; // Create new RobotMove object 
    robotMove.startMoving(); // Start the movement
    return 0;
}
