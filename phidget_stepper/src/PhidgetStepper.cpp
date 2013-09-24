#include "ros/ros.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h>

#include "std_msgs/Float64.h"
#include "corobot_msgs/MoveArm.h"
#include "corobot_msgs/PosMsg.h"
#include <phidget21.h>

#include <corobot_diagnostics/diagnostics.h>
#include <diagnostic_updater/diagnostic_updater.h>
#include <diagnostic_updater/publisher.h>
//Enumeration to identify the position of a stepper
typedef enum
{
	armBaseRotation,
    armShoulder,
	armElbow,
	armWrist,
	armGripper,
  leftWheel,
  rightWheel
} stepper_type;


// stepper type that save data about the connected stepper.
typedef struct
{
	stepper_type type;
	int serial; 
	CPhidgetStepperHandle handle;
} stepper;

stepper* steppers = NULL; // list of connected servos
int number_stepper = 0; // THe number of connected stepper motors
int stepperError = 0; // used for diagnostics purpose
ros::Publisher positionPublisher; // Publish the position of the left and right wheel motors, if any is present
ros::Publisher stepperPositionPublisher; // Publish the position of stepper motors present in an arm
int64_t leftPosition = 0;
int64_t rightPosition = 0;

void setStepperPosition(const corobot_msgs::MoveArm &msg)
/**
 * @brief Topic to move the stepper motors of the Corobot
 */ 
{
	int err;
	for(int i; i<number_stepper; i++)
	{

		//We received the indication on which stepper we want to move, so now we are finding the index corresponding to the servo.  
		if(msg.index == steppers[i].type)
		{
		    err = CPhidgetStepper_setTargetPosition(steppers[i].handle, 0, msg.position);	
		    if (err != 0)
		    {
		        stepperError = 2;
		    }
		    else
		    {
		        stepperError = 0;
		    }
		}
	}
}

void publishPosition()
{
  corobot_msgs::PosMsg msg;
  msg.px = leftPosition;
  msg.py = rightPosition;

  positionPublisher.publish(msg);
}

int positionCallback(CPhidgetStepperHandle phid, void *userPtr, int index, __int64 position)
{
  stepper * stepperMoved = (stepper*) userPtr;

  if (stepperMoved->type == leftWheel)
  {
    leftPosition = position;
    publishPosition();
  }
  else if (stepperMoved->type == rightWheel)
  {
    rightPosition = position;
    publishPosition();
  }
  else
  {
    corobot_msgs::MoveArm msg;
    msg.index = stepperMoved->type;
    msg.position = position;

    stepperPositionPublisher.publish(msg);
  } 
}

void init_servos_db(XmlRpc::XmlRpcValue stepper_list, ros::NodeHandle n)
/**
 * @brief initialise the list of servos (variable servos) and the number of servos connected. It reads this from the yaml file. 
 */ 
{
	if(stepper_list.hasMember("leftWheel") && (int)stepper_list["leftWheel"]["serial"] != -1)
	{
		steppers[number_stepper].type = leftWheel;
		steppers[number_stepper].serial = (int) stepper_list["leftWheel"]["serial"];
        steppers[number_stepper].handle = 0;
        
		number_stepper++;
	}
	if(stepper_list.hasMember("rightWheel") && (int)stepper_list["leftWheel"]["serial"] != -1)
	{
		steppers[number_stepper].type = rightWheel;
		steppers[number_stepper].serial = (int) stepper_list["rightWheel"]["serial"];
        steppers[number_stepper].handle = 0;
        
		number_stepper++;
	}
	if(stepper_list.hasMember("armBaseRotation") && (int)stepper_list["leftWheel"]["serial"] != -1)
	{
		steppers[number_stepper].type = armBaseRotation;
		steppers[number_stepper].serial = (int) stepper_list["armBaseRotation"]["serial"];
        steppers[number_stepper].handle = 0;
        
		number_stepper++;
	}
	if(stepper_list.hasMember("armShoulder") && (int)stepper_list["leftWheel"]["serial"] != -1)
	{
		steppers[number_stepper].type = armShoulder;
		steppers[number_stepper].serial = (int) stepper_list["armShoulder"]["serial"];
        steppers[number_stepper].handle = 0;
        
		number_stepper++;
	}
	if(stepper_list.hasMember("armElbow") && (int)stepper_list["leftWheel"]["serial"] != -1)
	{
		steppers[number_stepper].type = armElbow;
		steppers[number_stepper].serial = (int) stepper_list["armElbow"]["serial"];
        steppers[number_stepper].handle = 0;
        
		number_stepper++;
	}
	if(stepper_list.hasMember("armWrist") && (int)stepper_list["leftWheel"]["serial"] != -1)
	{
		steppers[number_stepper].type = armWrist;
		steppers[number_stepper].serial = (int) stepper_list["armWrist"]["serial"];
        steppers[number_stepper].handle = 0;
        
		number_stepper++;
	}
	if(stepper_list.hasMember("armGripper") && (int)stepper_list["leftWheel"]["serial"] != -1)
	{
		steppers[number_stepper].type = armGripper;
		steppers[number_stepper].serial = (int) stepper_list["armGripper"]["serial"];
        steppers[number_stepper].handle = 0;
        
		number_stepper++;
	}
}


void init_stepper_boards()
{
    int err;
    for(int i; i<number_stepper; i++)
	{
	    err = CPhidgetStepper_create (&(steppers[i].handle));
	    if (err != 0)
	    {
		    ROS_ERROR("error create the Phidget stepper controller device of serial number %d", steppers[i].serial);
		    stepperError = 1;
	    }
	    
	    err = CPhidget_open((CPhidgetHandle)steppers[i].handle, steppers[i].serial);
	    if (err != 0)
	    {
		    ROS_ERROR("error opening the Phidget stepper controller device of serial number %d", steppers[i].serial);
		    stepperError = 1;
	    }

	    CPhidgetStepper_set_OnPositionChange_Handler(steppers[i].handle, &positionCallback, (void*) &(steppers[i]));

      if((err = CPhidget_waitForAttachment((CPhidgetHandle)steppers[i].handle, 10000)))
	    {
		    ROS_ERROR("Problem waiting for attachment of Phidget stepper controller of serial number %d", steppers[i].serial);
		    stepperError = 1;
	    }
	    if(err = CPhidgetStepper_setEngaged(steppers[i].handle, 0, 1))
	    {
		    ROS_ERROR("Problem engaging Phidget stepper controller of serial number %d", steppers[i].serial);
		    stepperError = 1;
	    }
	}
}


void stepper_diagnostic (diagnostic_updater::DiagnosticStatusWrapper &stat)
{
  if (stepperError == 0)
  {
    stat.summaryf(diagnostic_msgs::DiagnosticStatus::OK, "initialized");
  }
  else if (stepperError == 1)
  {
    stat.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR, "Phidget Stepper cannot be initialized");
    stat.addf("Recommendation", PHIDGET_STEPPER_ERROR_CONNECTION);
  }
  else if (stepperError == 2)
  {
    stat.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR, "The Phidget Stepper motor cannot be moved");
    stat.addf("Recommendation",ERROR_MOVING_ARM);
  }
}


int main(int argc, char **argv)
{
	ros::init(argc, argv, "phidget_stepper");
	ros::NodeHandle n;
	ros::NodeHandle n_private("~");


	//Read yalm file parameters.
	XmlRpc::XmlRpcValue stepper_list;
   	n_private.param("stepper_list", stepper_list, stepper_list);


	//Read information about the servos
	steppers = (stepper*) malloc(stepper_list.size() * sizeof(stepper));
	init_servos_db(stepper_list, n_private);


  diagnostic_updater::Updater updater;
  updater.setHardwareIDf("Phidget");
  updater.add("Stepper", stepper_diagnostic); // Function executed with updater.update()

    // Initialize all the boards using the serial numbers we got.
    init_stepper_boards();


    //Subscribe to the topic
    ros::Subscriber stepperPos= n.subscribe("stepperPosition", 100, setStepperPosition);
    
    //Advertise topic
    positionPublisher= n.advertise<corobot_msgs::PosMsg>("position_data", 100);
    stepperPositionPublisher = n.advertise<corobot_msgs::MoveArm>("stepper_position",100);
    ros::Rate rate(50);
    
    while(ros::ok())
    {
        updater.update();
        ros::spinOnce();
        rate.sleep();
    }
	
	free(steppers);
	return 0;
}
