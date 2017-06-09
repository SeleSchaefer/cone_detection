#include <planning/car_model.hpp>
#include <planning/planner.hpp>
#include <planning/pure_pursuit.hpp>
#include <planning/tools.hpp>

#include <cone_detection/tools.hpp>

#include <ros/ros.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/TwistStamped.h>
#include <nav_msgs/OccupancyGrid.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/Imu.h>
#include <std_msgs/Bool.h>
#include <std_msgs/Float64.h>
#include <std_msgs/Float32MultiArray.h>
#include <cone_detection/Path.h>

//Publisher.& Subscriber.
ros::Publisher car_model_pub;
ros::Publisher car_model_pose;
ros::Publisher controls_pub;
ros::Publisher path_pub;
ros::Subscriber gridmap_sub;
ros::Subscriber imu_time_sub;
ros::Subscriber pose_sub;
ros::Subscriber steering_sub;
ros::Subscriber wheel_left_sub;
ros::Subscriber wheel_right_sub;
//Init classes.
CarModel car_model;
PurePursuit pure_pursuit;
Planner planner;
//Parameter.
Control control;
Erod erod;
Planning planning;
//Decleration of functions.
void gridmapCallback(const nav_msgs::OccupancyGrid::ConstPtr& grid);
void imuTimeCallback(const sensor_msgs::Imu::ConstPtr& msg);
void poseCallback(const geometry_msgs::Pose::ConstPtr& msg);
void steeringCallback(const std_msgs::Float64::ConstPtr& msg);
void wheelLeftCallback(const std_msgs::Float64::ConstPtr& msg);
void wheelRightCallback(const std_msgs::Float64::ConstPtr& msg);
void gettingParameter(ros::NodeHandle* node, Control* control, Erod* erod, Planning* planning);

int main(int argc, char** argv){
	ros::init(argc, argv, "path_planner");
	ros::NodeHandle node;
	//Getting parameter.
	gettingParameter(&node,&control,&erod,&planning);
	//Init classes.
	car_model.init(erod);
	pure_pursuit.init(control,erod);
	planner.init(planning);
	//Init pubs & subs.
	car_model_pub = node.advertise<geometry_msgs::TwistStamped>("/car_model_velocity", 1);
	controls_pub = node.advertise<std_msgs::Float32MultiArray>("/stellgroessen", 1);
	path_pub = node.advertise<std_msgs::Float32MultiArray>("/path", 10);
	gridmap_sub = node.subscribe("/cones_grid", 1, gridmapCallback);
	imu_time_sub = node.subscribe("/imu0", 1, imuTimeCallback);
	pose_sub = node.subscribe("/car_pose", 1, poseCallback);
	steering_sub = node.subscribe("/state_steering_angle", 1, steeringCallback);
	wheel_left_sub = node.subscribe("/wheel_rear_left", 1, wheelLeftCallback);
	wheel_right_sub = node.subscribe("/wheel_rear_right", 1, wheelRightCallback);
  	//Spinning.
  	ros::Rate rate(10);
  	while(ros::ok()){
  		ros::spinOnce();
  		//Update and publish car model.
  		car_model_pub.publish(car_model.getTwistMsg());
  		//Pure pursuit update.
		pure_pursuit.setVelocity(car_model.getVelocity().norm());
		rate.sleep();
  	}
	return 0;
}

void gridmapCallback(const nav_msgs::OccupancyGrid::ConstPtr& grid){
	//Path planning.
	planner.gridAnalyser(*grid);
	std::vector<Eigen::Vector2d> path = planner.getGlobalPath();
	if(path.size() > 0){
		//Calculate controls.
		AckermannControl stellgroessen = pure_pursuit.calculateControls(path);
		//Visualisation.
		std_msgs::Float32MultiArray global_path_msg = planner.getGlobalPathMsg();
		path_pub.publish(global_path_msg);
		std_msgs::Float32MultiArray control_msg = pure_pursuit.getControlsMsg();
		controls_pub.publish(control_msg);
	}	
}

void imuTimeCallback(const sensor_msgs::Imu::ConstPtr& msg){
  	car_model.setTimeStamp(msg->header.stamp);
}

void poseCallback(const geometry_msgs::Pose::ConstPtr& msg){
	Pose pose;
	pose.position = Eigen::Vector2d(msg->position.x, msg->position.y);
	pose.orientation = Eigen::Vector4d(msg->orientation.x, msg->orientation.y, 
									   msg->orientation.z, msg->orientation.w);
	pure_pursuit.setPose(pose);
}

void steeringCallback(const std_msgs::Float64::ConstPtr& msg){
	car_model.setSteeringAngle(msg->data);
}

void wheelLeftCallback(const std_msgs::Float64::ConstPtr& msg){
	car_model.setRearLeftWheelVel(msg->data);
}
void wheelRightCallback(const std_msgs::Float64::ConstPtr& msg){
	car_model.setRearRightWheelVel(msg->data);
}

void gettingParameter(ros::NodeHandle* node, Control* control, Erod* erod, Planning* planning){
	//Get control parameter.
	node->getParam("/control/distance_interpolation", control->distance_interpolation);
	node->getParam("/control/K1_LAD_S", control->k1_lad_s);
	node->getParam("/control/K2_LAD_S", control->k2_lad_s);
	node->getParam("/control/K1_LAD_V", control->k1_lad_v);
	node->getParam("/control/K2_LAD_V", control->k2_lad_v);
	node->getParam("/control/upperbound_lad_s", control->upperbound_lad_s);
	node->getParam("/control/lowerbound_lad_s", control->lowerbound_lad_s);
	node->getParam("/control/max_absolute_velocity", control->max_absolute_velocity);
	node->getParam("/control/max_lateral_acceleration", control->max_lateral_acceleration);
	node->getParam("/control/max_steering_angle", control->max_steering_angle);
	//Get erod parameter.
	node->getParam("/erod/length_laser_to_VI", erod->length_laser_to_VI);
	node->getParam("/erod/distance_wheel_axis", erod->distance_wheel_axis);
	node->getParam("/erod/height_laser", erod->height_laser);
	//Get planning parameter.
	node->getParam("/path_planning/distance_cone", planning->distance_cone);
	node->getParam("/path_planning/number_points_x_axis", planning->number_points_x_axis);
	node->getParam("/detection/searching_length", planning->searching_length);
	node->getParam("/detection/searching_width", planning->searching_width);
}
