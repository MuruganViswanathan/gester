/*
 * touch-gestures.cpp
 *
 *  Created on: Apr 23, 2020
 *      Author: murugan
 */

#include <stdio.h>
//#include <stdlib.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <math.h>
#include <fstream>

#include "config.h"
#include "touch-gestures.h"

#ifdef __cplusplus
using namespace std;
#endif

int touch_device;
struct input_event ev;

/* how many fingers are touching the screen */
int touch_num_fingers = 0;

/* which finger is being tracked */
int finger = 0;

/*fix rotations with wide spread fingers*/
int swipesuccess = 0;

/*placeholder to help switching xmax and ymax*/
int yrotmax = xmax;
int xrotmax = ymax;

/*get info from accelerometer script*/
double accelscaling = 0.000009806;

std::ifstream xacceldata;
std::ifstream yacceldata;

int accelxraw = 0;
int accelyraw = 0;
double accelx = 0;
double accely = 0;
double gthresh = 7.0;
int orientation = 0;
/*conversion of the angles from radians to degree*/
double radtoang = 360 / 6.28318531;

/*two arrays per finger to track*/
std::vector<int> finger1x;
std::vector<int> finger1y;
std::vector<int> finger2x;
std::vector<int> finger2y;
std::vector<int> finger3x;
std::vector<int> finger3y;
std::vector<int> finger4x;
std::vector<int> finger4y;
std::vector<int> finger5x;
std::vector<int> finger5y;

std::string touch_input_device;

int
touch_gestures_init ()
{
	if(getenv ("TOUCH_INPUT_DEVICE"))
	{
		touch_input_device = std::string(getenv ("TOUCH_INPUT_DEVICE"));
	}
	//printf ("touch_gestures_init() - touch_input_device : %s\n", touch_input_device.c_str());

	if(!touch_input_device.empty())
	{
		touch_device = open (touch_input_device.c_str(), O_RDONLY);
		if (touch_device == -1)
		{
			printf ("Error: Could not open input device : %s\n", touch_input_device.c_str());
			return TOUCH_GESTURE_EVENT_NONE;
		}
		else
		{
			printf ("Touch Gestures from device: %s\n", touch_input_device.c_str());
		}
	}
	else
	{
		touch_device = open (devname, O_RDONLY);
		if (touch_device == -1)
		{
			printf ("Error: Could not open default input device : %s\n", devname);
			return TOUCH_GESTURE_EVENT_NONE;
		}
		else
		{
			printf ("Touch Gestures from default device: %s\n", devname);
		}
	}

	std::string xaccelpathfull = accelpath + "iio:device0/" + xrawdata;
	std::string yaccelpathfull = accelpath + "iio:device0/" + yrawdata;

	xacceldata.open (xaccelpathfull);
	yacceldata.open (yaccelpathfull);

	if (xacceldata.good () != true)
	{
		xaccelpathfull = accelpath + "iio:device1/" + xrawdata;
		yaccelpathfull = accelpath + "iio:device1/" + yrawdata;
		xacceldata.open (xaccelpathfull);
		yacceldata.open (yaccelpathfull);
	}

	return 0;
}

int
touch_gestures_exit ()
{
	touch_input_device.clear();
	return close(touch_device);
}

enum touch_gestures_event_type
touch_gestures_get ()
{
	int gesture_complete = 0;
	while (!gesture_complete)
	{
		int n = read (touch_device, &ev, sizeof(ev));
		if (n < 0)
		{
			printf ("Read input_event failed from device: %s ! \n", devname);
			return TOUCH_GESTURE_EVENT_NONE;
		}
		else if (n == 0)
		{
			printf ("No data on port\n");
		}

		//printf("type: %d\t code(Key): %d\t value(State): %d\n", ev.type, ev.code, ev.value);

		/*count the number of fingers*/
		//ABS_MT_TRACKING_ID	0x39 (57) - Unique ID of initiated contact
		//printf("ABS_MT_TRACKING_ID: %d\n", ABS_MT_TRACKING_ID);
		if (ev.code == ABS_MT_TRACKING_ID && ev.value > 0)
		{
			touch_num_fingers += 1;
			printf ("Fingers detected: %d\n", touch_num_fingers);
		}

		//ABS_MT_POSITION_X: 53 , ABS_MT_POSITION_Y: 54
		if ((ev.code == ABS_MT_POSITION_X || ev.code == ABS_MT_POSITION_Y)
				&& ev.value > 0)
		{
			static int xval;
			static int yval;
			if (ev.code == ABS_MT_POSITION_X)
				xval = ev.value;
			if (ev.code == ABS_MT_POSITION_Y)
				yval = ev.value;
			//printf("(%d, %d)\n", xval, yval);
		}

		/*determine which finger's coordinates will be incoming*/
		//ABS_MT_SLOT	0x2f (47)  - Multi Touch slot being modified
		if (ev.code == ABS_MT_SLOT)
		{
			finger = ev.value;
		}

		/*here we need to read the accelerometers and then assign data to the arrays according to the orientation*/
		xacceldata >> accelxraw;
		yacceldata >> accelyraw;
		//printf("accelxraw: %i accelyraw: %i ",accelxraw,accelyraw);
		accelx = accelxraw * accelscaling;
		accely = accelyraw * accelscaling;
		//printf("accelx: %f accely: %f\n",accelx,accely);

		if (accelx < gthresh && accely < -gthresh)
		{
			orientation = 0; /* 0 is normal orientation */
			printf ("Setting orientation to normal\n");
		}
		else if (accelx > gthresh && accely < gthresh)
		{
			orientation = 1; /* 1 is rotated right*/
			printf ("Setting orientation to right\n");
		}
		else if (accelx < gthresh && accely > gthresh)
		{
			orientation = 2; /* 2 is upside down*/
			printf ("Setting orientation to upside down\n");
		}
		else if (accelx < -gthresh && accely < gthresh)
		{
			orientation = 3; /* 3 is rotated left*/
			printf ("Setting orientation to left\n");
		}
		else
		{
			//printf("could not find accelerometer data, setting orientation to normal\n");
			orientation = 0;
		}

		/*actually put the data into the arrays*/
		// ABS_MT_POSITION_X	0x35 - 53	/* Center X touch position */
		// ABS_MT_POSITION_Y	0x36 - 54	/* Center Y touch position */
		if (orientation == 0)
		{
			//printf("normal orientation\n");
			if (finger == 0)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger1x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger1y.push_back (ev.value);
				}
			}
			else if (finger == 1)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger2x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger2y.push_back (ev.value);
				}
			}
			else if (finger == 2)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger3x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger3y.push_back (ev.value);
				}
			}
			else if (finger == 3)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger4x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger4y.push_back (ev.value);
				}
			}
			else if (finger == 4)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger5x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger5y.push_back (ev.value);
				}
			}
			else
			{
				printf ("Currently not supported for more than 5 fingers!.\n");
			}
		}
		else if (orientation == 1)
		{
			//printf("rotated right\n");
			xmax = xrotmax;
			ymax = yrotmax;
			if (finger == 0)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger1x.push_back (xmax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger1y.push_back (ev.value);
				}
			}
			else if (finger == 1)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger2x.push_back (ymax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger2y.push_back (ev.value);
				}
			}
			else if (finger == 2)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger3x.push_back (ymax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger3y.push_back (ev.value);
				}
			}
			else if (finger == 3)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger4x.push_back (ymax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger4y.push_back (ev.value);
				}
			}
			else if (finger == 4)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger5x.push_back (ymax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger5y.push_back (ev.value);
				}
			}
			else
			{
				printf ("Not designed for more than 5 fingers.\n");
			}
		}
		else if (orientation == 2)
		{
			//printf("upside down\n");
			if (finger == 0)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger1x.push_back (xmax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger1y.push_back (ymax - ev.value);
				}
			}
			else if (finger == 1)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger2x.push_back (xmax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger2y.push_back (ymax - ev.value);
				}
			}
			else if (finger == 2)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger3x.push_back (xmax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger3y.push_back (ymax - ev.value);
				}
			}
			else if (finger == 3)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger4x.push_back (xmax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger4y.push_back (ymax - ev.value);
				}
			}
			else if (finger == 4)
			{
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger5x.push_back (xmax - ev.value);
				}
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger5y.push_back (ymax - ev.value);
				}
			}
			else
			{
				printf ("Currently not supported for more than 5 fingers!.\n");
			}
		}
		else if (orientation == 3)
		{
			//printf("rotated left\n");
			xmax = xrotmax;
			ymax = yrotmax;
			if (finger == 0)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger1x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger1y.push_back (ymax - ev.value);
				}
			}
			else if (finger == 1)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger2x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger2y.push_back (ymax - ev.value);
				}
			}
			else if (finger == 2)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger3x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger3y.push_back (ymax - ev.value);
				}
			}
			else if (finger == 3)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger4x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger4y.push_back (ymax - ev.value);
				}
			}
			else if (finger == 4)
			{
				if (ev.code == ABS_MT_POSITION_Y)
				{
					finger5x.push_back (ev.value);
				}
				if (ev.code == ABS_MT_POSITION_X)
				{
					finger5y.push_back (ymax - ev.value);
				}
			}
			else
			{
				printf ("Currently not supported for more than 5 fingers!.\n");
			}
		}
		//printf("array sizes %i,%i,%i,%i,%i\n",finger1x.size(),finger2x.size(),finger3x.size(),finger4x.size(),finger5x.size());

		if (ev.code == ABS_MT_TRACKING_ID && ev.value == -1) /* this code+value means that a finger left the screen and I think that is when the gesture should end, not at the last finger.*/
		{
			/*stuff to calculate per finger*/
			double x0first = finger1x[0];
			double x0last = finger1x[finger1x.size () - 1];
			double x0len = ((x0last - x0first) / xmax);
			double y0first = finger1y[0];
			double y0last = finger1y[finger1y.size () - 1];
			double y0len = ((y0last - y0first) / ymax);
			double finger1directionality = abs (x0len / y0len);
			//printf("directionality: %f\n",finger1directionality);
			double x1first, x1last, x1len, y1first, y1last, y1len,
					finger2directionality;
			double x2first, x2last, x2len, y2first, y2last, y2len,
					finger3directionality;
			double x3first, x3last, x3len, y3first, y3last, y3len,
					finger4directionality;
			double x4first, x4last, x4len, y4first, y4last, y4len,
					finger5directionality;

			/*only try reading from vectors that are not empty*/
			if (finger2x.size () != 0)
			{
				x1first = finger2x[0];
				x1last = finger2x[finger2x.size () - 1];
				x1len = ((x1last - x1first) / xmax);
				y1first = finger2y[0];
				y1last = finger2y[finger2y.size () - 1];
				y1len = ((y1last - y1first) / ymax);
				finger2directionality = abs (x1len / y1len);
			}
			if (finger3x.size () != 0)
			{
				x2first = finger3x[0];
				x2last = finger3x[finger3x.size () - 1];
				x2len = ((x2last - x2first) / xmax);
				y2first = finger3y[0];
				y2last = finger3y[finger3y.size () - 1];
				y2len = ((y2last - y2first) / ymax);
				finger3directionality = abs (x2len / y2len);
			}
			if (finger4x.size () != 0)
			{
				x3first = finger4x[0];
				x3last = finger4x[finger4x.size () - 1];
				x3len = ((x3last - x3first) / xmax);
				y3first = finger4y[0];
				y3last = finger4y[finger4y.size () - 1];
				y3len = ((y3last - y3first) / ymax);
				finger4directionality = abs (x3len / y3len);
			}
			if (finger5x.size () != 0)
			{
				x4first = finger5x[0];
				x4last = finger5x[finger5x.size () - 1];
				x4len = ((x4last - x4first) / xmax);
				y4first = finger5y[0];
				y4last = finger5y[finger5y.size () - 1];
				y4len = ((y4last - y4first) / ymax);
				finger5directionality = abs (x4len / y4len);
			}
			//printf("touch_num_fingers: %i\n",touch_num_fingers);

			if (touch_num_fingers == 1)
			{
				/*one finger screen edge gestures*/
				if (y0first >= (ymax - offsetbottom)
						&& finger1directionality < 1)
				{
					printf ("(1 finger) TOUCH_GESTURE_EVENT_1FINGER_SWIPE_UP - from bottom edge\n");
					//system(commands[0]);
				}
				if (y0first <= offsettop && finger1directionality < 1)
				{
					printf ("(1 finger) TOUCH_GESTURE_EVENT_1FINGER_SWIPE_DOWN - from top edge\n");
					//system(commands[1]);
				}
				if (x0first >= (xmax - offsetright)
						&& finger1directionality > 1)
				{
					printf ("(1 finger) TOUCH_GESTURE_EVENT_1FINGER_SWIPE_LEFT - from right edge\n");
					//system(commands[2]);
				}
				if (x0first <= offsetleft && finger1directionality > 1)
				{
					printf ("(1 finger) TOUCH_GESTURE_EVENT_1FINGER_SWIPE_RIGHT - from left edge\n");
					//system(commands[3]);
				}

				if (abs (x0len) > swipetolerance || abs (y0len) > swipetolerance)
				{
				  printf("1 finger swipe!\n");
				  //system(commands[3]);
				}
				if (abs (x0len) <= taptolerance && abs (y0len) <= taptolerance)
				{
				  printf("(1 finger) TOUCH_GESTURE_EVENT_1FINGER_TAP\n");
				  //system(commands[3]);
				}

			}
			else if (touch_num_fingers == 2)
			{
				/*calculating values for 2 finger rotations*/
				double xfirstcom = (x0first + x1first) / 2;
				double yfirstcom = (y0first + y1first) / 2;
				double xlastcom = (x0last + x1last) / 2;
				double ylastcom = (y0last + y1last) / 2;
				double comdist = sqrt (
						(xfirstcom - xlastcom) * (xfirstcom - xlastcom)
								+ (yfirstcom - ylastcom)
										* (yfirstcom - ylastcom));
				//printf("common distance: %f\n", comdist);
				double vec0xfirst = x0first - xfirstcom;
				double vec0yfirst = y0first - yfirstcom;
				double vec1xfirst = x1first - xfirstcom;
				double vec1yfirst = y1first - yfirstcom;
				double vec0xlast = x0last - xlastcom;
				double vec0ylast = y0last - ylastcom;
				double vec1xlast = x1last - xlastcom;
				double vec1ylast = y1last - ylastcom;
				double phi0 = atan2 (
						(vec0xfirst * vec0ylast - vec0yfirst * vec0xlast),
						(vec0xfirst * vec0xlast + vec0yfirst * vec0ylast))
						* radtoang;
				double phi1 = atan2 (
						(vec1xfirst * vec1ylast - vec1yfirst * vec1xlast),
						(vec1xfirst * vec1xlast + vec1yfirst * vec1ylast))
						* radtoang;
				int angleavg = round ((phi0 + phi1) / 2);
				int anglescaled = (angleavg / anglescaling)
						- (angleavg / anglescaling) % anglestepping;
				//printf("phi0: %f phi1: %f angle avg: %i angle scaled: %i\n",phi0,phi1,angleavg,anglescaled);

				/*conditions for 2 finger gestures*/
				//printf("swipetolerance = %f\n", swipetolerance);
				//printf("abs(x0len) = %f\n", abs(x0len));
				//printf("abs(x1len) = %f\n", abs(x1len));
				//printf("abs(y0len) = %f\n", abs(y0len));
				//printf("abs(y1len) = %f\n", abs(y1len));
				if ((abs (x0len) > swipetolerance
						&& abs (x1len) > swipetolerance)
						|| (abs (y0len) > swipetolerance
								&& abs (y1len) > swipetolerance))
				{
					//printf("conditions for 2 finger gestures met\n");
					/*2 finger edge swipes*/
					if (y0first >= (ymax - offsetbottom)
							&& y1first >= (ymax - offsetbottom)
							&& finger1directionality < 1
							&& finger2directionality < 1)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_SWIPE_UP - from bottom edge\n");
						//system(commands[4]);
						swipesuccess = 1;
					}
					else if (y0first <= offsettop && y1first <= offsettop
							&& finger1directionality < 1
							&& finger2directionality < 1)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_SWIPE_DOWN - from top edge\n");
						//system(commands[5]);
						swipesuccess = 1;
					}
					else if (x0first >= (xmax - offsetright)
							&& x1first >= (xmax - offsetright)
							&& finger1directionality > 1
							&& finger2directionality > 1)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_SWIPE_LEFT - from right edge\n");
						//system(commands[6]);
						swipesuccess = 1;
					}
					else if (x0first <= offsetleft && x1first <= offsetleft
							&& finger1directionality > 1
							&& finger2directionality > 1)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_SWIPE_RIGHT - from left edge\n");
						//system(commands[7]);
						swipesuccess = 1;
					}
					else if (finger1directionality < 1
							&& finger2directionality < 1 && y0last > y0first
							&& y1last > y1first)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_SWIPE_DOWN\n");
						//system(commands[8]);
						swipesuccess = 1;
					}
					else if (finger1directionality < 1
							&& finger2directionality < 1 && y0last < y0first
							&& y1last < y1first)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_SWIPE_UP\n");
						//system(commands[9]);
						swipesuccess = 1;
					}
					else if (finger1directionality > 1
							&& finger2directionality > 1 && x0last > x0first
							&& x1last > x1first)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_SWIPE_RIGHT\n");
						//system(commands[10]);
						swipesuccess = 1;
					}
					else if (finger1directionality > 1
							&& finger2directionality > 1 && x0last < x0first
							&& x1last < x1first)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_SWIPE_LEFT\n");
						//system(commands[11]);
						swipesuccess = 1;
					}
				}
				else if ((abs (x0len) > pinchtolerance
						|| abs (x1len) > pinchtolerance)
						|| (abs (y0len) > pinchtolerance
								|| abs (y1len) > pinchtolerance))
				{
					if ((((x0first - x1first) * (x0first - x1first)
							+ (y0first - y1first) * (y0first - y1first))
							- ((x0last - x1last) * (x0last - x1last)
									+ (y0last - y1last) * (y0last - y1last)))
							> 0)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_PINCH_IN\n");
						//system(commands[11]);
						swipesuccess = 1;
					}
					else if ((((x0last - x1last) * (x0last - x1last)
							+ (y0last - y1last) * (y0last - y1last))
							- ((x0first - x1first) * (x0first - x1first)
									+ (y0first - y1first) * (y0first - y1first)))
							> 0)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_PINCH_OUT\n");
						//system(commands[11]);
						swipesuccess = 1;
					}
				}
				else if (abs (x0len) <= taptolerance && abs (x1len) <= taptolerance
					    && abs (y0len) <= taptolerance && abs (y1len) <= taptolerance)
				{
				  printf("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_TAP\n");
				  //system(commands[11]);
				  swipesuccess = 1;
				}

				if (comdist < comdisttolerance && swipesuccess == 0)
				{
					if (angleavg < 0)
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_ROTATE_ANTICLOCKWISE - angle: %i degree, scaled to %i\n",
								angleavg, anglescaled);
						//std::string tmpcommand = commands[12]
						//+ std::to_string (anglescaled);
						//system(tmpcommand.c_str ());
					}
					else
					{
						printf ("(2 fingers) TOUCH_GESTURE_EVENT_2FINGER_ROTATE_CLOCKWISE - angle: %i degree, scaled to %i\n",
								angleavg, anglescaled);
						//std::string tmpcommand = commands[13]
						//+ std::to_string (anglescaled);
						//system(tmpcommand.c_str ());
					}
				}
			}
			else if (touch_num_fingers == 3)
			{
				/*calculating values for 3 finger rotations*/
				double xfirstcom = (x0first + x1first + x2first) / 3;
				double yfirstcom = (y0first + y1first + y2first) / 3;
				double xlastcom = (x0last + x1last + x2last) / 3;
				double ylastcom = (y0last + y1last + y2last) / 3;
				double comdist = sqrt (
						(xfirstcom - xlastcom) * (xfirstcom - xlastcom)
								+ (yfirstcom - ylastcom)
										* (yfirstcom - ylastcom));
				//printf("comdist: %f\n",comdist);
				double vec0xfirst = x0first - xfirstcom;
				double vec0yfirst = y0first - yfirstcom;
				double vec1xfirst = x1first - xfirstcom;
				double vec1yfirst = y1first - yfirstcom;
				double vec2xfirst = x2first - xfirstcom;
				double vec2yfirst = y2first - yfirstcom;
				double vec0xlast = x0last - xlastcom;
				double vec0ylast = y0last - ylastcom;
				double vec1xlast = x1last - xlastcom;
				double vec1ylast = y1last - ylastcom;
				double vec2xlast = x2last - xlastcom;
				double vec2ylast = y2last - ylastcom;
				double phi0 = atan2 (
						(vec0xfirst * vec0ylast - vec0yfirst * vec0xlast),
						(vec0xfirst * vec0xlast + vec0yfirst * vec0ylast))
						* radtoang;
				double phi1 = atan2 (
						(vec1xfirst * vec1ylast - vec1yfirst * vec1xlast),
						(vec1xfirst * vec1xlast + vec1yfirst * vec1ylast))
						* radtoang;
				double phi2 = atan2 (
						(vec2xfirst * vec2ylast - vec2yfirst * vec2xlast),
						(vec2xfirst * vec2xlast + vec2yfirst * vec2ylast))
						* radtoang;
				int angleavg = round ((phi0 + phi1 + phi2) / 3);
				int anglescaled = (angleavg / anglescaling)
						- (angleavg / anglescaling) % anglestepping;
				//printf("phi0: %f phi1: %f phi2: %f angleavg: %i anglescaled: %i\n",phi0,phi1,phi2,angleavg,anglescaled);

				if (((abs (x0len) > swipetolerance
						&& abs (x1len) > swipetolerance
						&& abs (x2len) > swipetolerance)
						|| (abs (y0len) > swipetolerance
								&& abs (y1len) > swipetolerance
								&& abs (y2len) > swipetolerance)))
				{
					if (y0first >= (ymax - offsetbottom)
							&& y1first >= (ymax - offsetbottom)
							&& y2first >= (ymax - offsetbottom)
							&& finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1)
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_SWIPE_UP - from bottom edge\n");
						//system(commands[14]);
						swipesuccess = 1;
					}
					else if (y0first <= offsettop && y1first <= offsettop
							&& y2first <= offsettop && finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1)
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_SWIPE_DOWN - from top edge\n");
						//system(commands[15]);
						swipesuccess = 1;
					}
					else if (x0first >= (xmax - offsetright)
							&& x1first >= (xmax - offsetright)
							&& x2first >= (xmax - offsetright)
							&& finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1)
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_SWIPE_LEFT - from right edge\n");
						//system(commands[16]);
						swipesuccess = 1;
					}
					else if (x0first <= offsetleft && x1first <= offsetleft
							&& x2first <= offsetleft
							&& finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1)
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_SWIPE_RIGHT - from left edge\n");
						//system(commands[17]);
						swipesuccess = 1;
					}
					else if (finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1 && y0last > y0first
							&& y1last > y1first && y2last > y2first)
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_SWIPE_DOWN\n");
						//system(commands[18]);
						swipesuccess = 1;
					}
					else if (finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1 && y0last < y0first
							&& y1last < y1first && y2last < y2first)
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_SWIPE_UP\n");
						//system(commands[19]);
						swipesuccess = 1;
					}
					else if (finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1 && x0last > x0first
							&& x1last > x1first && x2last > x2first)
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_SWIPE_RIGHT\n");
						//system(commands[20]);
						swipesuccess = 1;
					}
					else if (finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1 && x0last < x0first
							&& x1last < x1first && x2last < x2first)
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_SWIPE_LEFT\n");
						//system(commands[21]);
						swipesuccess = 1;
					}
				}
				if (comdist < comdisttolerance && swipesuccess == 0)
				{
					if (angleavg < 0)
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_ROTATE_ANTICLOCKWISE - angle: %i degree, scaled to %i\n",
								angleavg, anglescaled);
						//std::string tmpcommand = commands[22]
						//+ std::to_string (anglescaled);
						//system(tmpcommand.c_str ());
					}
					else
					{
						printf ("(3 fingers) TOUCH_GESTURE_EVENT_3FINGER_ROTATE_CLOCKWISE - angle: %i degree, scaled to %i\n",
								angleavg, anglescaled);
						//std::string tmpcommand = commands[23]
						//+ std::to_string (anglescaled);
						//system(tmpcommand.c_str ());
					}
				}
			}
			else if (touch_num_fingers == 4)
			{
				/*calculating values for 4 finger rotations*/
				double xfirstcom = (x0first + x1first + x2first + x3first) / 4;
				double yfirstcom = (y0first + y1first + y2first + y3first) / 4;
				double xlastcom = (x0last + x1last + x2last + x3last) / 4;
				double ylastcom = (y0last + y1last + y2last + y3last) / 4;
				double comdist = sqrt (
						(xfirstcom - xlastcom) * (xfirstcom - xlastcom)
								+ (yfirstcom - ylastcom)
										* (yfirstcom - ylastcom));
				//printf("comdist: %f\n",comdist);
				double vec0xfirst = x0first - xfirstcom;
				double vec0yfirst = y0first - yfirstcom;
				double vec1xfirst = x1first - xfirstcom;
				double vec1yfirst = y1first - yfirstcom;
				double vec2xfirst = x2first - xfirstcom;
				double vec2yfirst = y2first - yfirstcom;
				double vec3xfirst = x3first - xfirstcom;
				double vec3yfirst = y3first - yfirstcom;
				double vec0xlast = x0last - xlastcom;
				double vec0ylast = y0last - ylastcom;
				double vec1xlast = x1last - xlastcom;
				double vec1ylast = y1last - ylastcom;
				double vec2xlast = x2last - xlastcom;
				double vec2ylast = y2last - ylastcom;
				double vec3xlast = x3last - xlastcom;
				double vec3ylast = y3last - ylastcom;
				double phi0 = atan2 (
						(vec0xfirst * vec0ylast - vec0yfirst * vec0xlast),
						(vec0xfirst * vec0xlast + vec0yfirst * vec0ylast))
						* radtoang;
				double phi1 = atan2 (
						(vec1xfirst * vec1ylast - vec1yfirst * vec1xlast),
						(vec1xfirst * vec1xlast + vec1yfirst * vec1ylast))
						* radtoang;
				double phi2 = atan2 (
						(vec2xfirst * vec2ylast - vec2yfirst * vec2xlast),
						(vec2xfirst * vec2xlast + vec2yfirst * vec2ylast))
						* radtoang;
				double phi3 = atan2 (
						(vec3xfirst * vec3ylast - vec3yfirst * vec3xlast),
						(vec3xfirst * vec3xlast + vec3yfirst * vec3ylast))
						* radtoang;
				int angleavg = round ((phi0 + phi1 + phi2 + phi3) / 4);
				int anglescaled = (angleavg / anglescaling)
						- (angleavg / anglescaling) % anglestepping;

				if ((abs (x0len) > swipetolerance
						&& abs (x1len) > swipetolerance
						&& abs (x2len) > swipetolerance
						&& abs (x3len) > swipetolerance)
						|| (abs (y0len) > swipetolerance
								&& abs (y1len) > swipetolerance
								&& abs (y2len) > swipetolerance
								&& abs (y3len) > swipetolerance))
				{
					if (y0first >= (ymax - offsetbottom)
							&& y1first >= (ymax - offsetbottom)
							&& y2first >= (ymax - offsetbottom)
							&& y3first >= (ymax - offsetbottom)
							&& finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1
							&& finger4directionality < 1)
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_SWIPE_UP from bottom edge\n");
						//system(commands[24]);
						swipesuccess = 1;
					}
					else if (y0first <= offsettop && y1first <= offsettop
							&& y2first <= offsettop && y3first <= offsettop
							&& finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1
							&& finger4directionality < 1)
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_SWIPE_DOWN from top edge\n");
						//system(commands[25]);
						swipesuccess = 1;
					}
					else if (x0first >= (xmax - offsetright)
							&& x1first >= (xmax - offsetright)
							&& x2first >= (xmax - offsetright)
							&& x3first >= (xmax - offsetright)
							&& finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1
							&& finger4directionality > 1)
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_SWIPE_LEFT from right edge\n");
						//system(commands[26]);
						swipesuccess = 1;
					}
					else if (x0first <= offsetleft && x1first <= offsetleft
							&& x2first <= offsetleft && x3first <= offsetleft
							&& finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1
							&& finger4directionality > 1)
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_SWIPE_RIGHT from left edge\n");
						//system(commands[27]);
						swipesuccess = 1;
					}
					else if (finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1
							&& finger4directionality < 1 && y0last > y0first
							&& y1last > y1first && y2last > y2first
							&& y3last > y3first)
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_SWIPE_DOWN\n");
						//system(commands[28]);
						swipesuccess = 1;
					}
					else if (finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1
							&& finger4directionality < 1 && y0last < y0first
							&& y1last < y1first && y2last < y2first
							&& y3last < y3first)
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_SWIPE_UP\n");
						//system(commands[29]);
						swipesuccess = 1;
					}
					else if (finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1
							&& finger4directionality > 1 && x0last > x0first
							&& x1last > x1first && x2last > x2first
							&& x3last > x3first)
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_SWIPE_RIGHT\n");
						//system(commands[30]);
						swipesuccess = 1;
					}
					else if (finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1
							&& finger4directionality > 1 && x0last < x0first
							&& x1last < x1first && x2last < x2first
							&& x3last < x3first)
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_SWIPE_LEFT\n");
						//system(commands[31]);
						swipesuccess = 1;
					}
				}
				if (comdist < comdisttolerance && swipesuccess == 0)
				{
					if (angleavg < 0)
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_ROTATE_ANTICLOCKWISE - angle: %i degree, scaled to %i\n",
								angleavg, anglescaled);
						//std::string tmpcommand = commands[32]
						//+ std::to_string (anglescaled);
						//system(tmpcommand.c_str ());
					}
					else
					{
						printf ("(4 fingers) TOUCH_GESTURE_EVENT_4FINGER_ROTATE_CLOCKWISE - angle: %i degree, scaled to %i\n",
								angleavg, anglescaled);
						//std::string tmpcommand = commands[33]
						//+ std::to_string (anglescaled);
						//system(tmpcommand.c_str ());
					}
				}
			}
			else if (touch_num_fingers == 5)
			{
				/*calculating values for 3 finger rotations*/
				double xfirstcom = (x0first + x1first + x2first + x3first
						+ x4first) / 5;
				double yfirstcom = (y0first + y1first + y2first + y3first
						+ y4first) / 5;
				double xlastcom = (x0last + x1last + x2last + x3last + x4last)
						/ 5;
				double ylastcom = (y0last + y1last + y2last + y3last + y4last)
						/ 5;
				double comdist = sqrt (
						(xfirstcom - xlastcom) * (xfirstcom - xlastcom)
								+ (yfirstcom - ylastcom)
										* (yfirstcom - ylastcom));
				//printf("comdist: %f\n",comdist);
				double vec0xfirst = x0first - xfirstcom;
				double vec0yfirst = y0first - yfirstcom;
				double vec1xfirst = x1first - xfirstcom;
				double vec1yfirst = y1first - yfirstcom;
				double vec2xfirst = x2first - xfirstcom;
				double vec2yfirst = y2first - yfirstcom;
				double vec3xfirst = x3first - xfirstcom;
				double vec3yfirst = y3first - yfirstcom;
				double vec4xfirst = x4first - xfirstcom;
				double vec4yfirst = y4first - yfirstcom;
				double vec0xlast = x0last - xlastcom;
				double vec0ylast = y0last - ylastcom;
				double vec1xlast = x1last - xlastcom;
				double vec1ylast = y1last - ylastcom;
				double vec2xlast = x2last - xlastcom;
				double vec2ylast = y2last - ylastcom;
				double vec3xlast = x3last - xlastcom;
				double vec3ylast = y3last - ylastcom;
				double vec4xlast = x4last - xlastcom;
				double vec4ylast = y4last - ylastcom;
				double phi0 = atan2 (
						(vec0xfirst * vec0ylast - vec0yfirst * vec0xlast),
						(vec0xfirst * vec0xlast + vec0yfirst * vec0ylast))
						* radtoang;
				double phi1 = atan2 (
						(vec1xfirst * vec1ylast - vec1yfirst * vec1xlast),
						(vec1xfirst * vec1xlast + vec1yfirst * vec1ylast))
						* radtoang;
				double phi2 = atan2 (
						(vec2xfirst * vec2ylast - vec2yfirst * vec2xlast),
						(vec2xfirst * vec2xlast + vec2yfirst * vec2ylast))
						* radtoang;
				double phi3 = atan2 (
						(vec3xfirst * vec3ylast - vec3yfirst * vec3xlast),
						(vec3xfirst * vec3xlast + vec3yfirst * vec3ylast))
						* radtoang;
				double phi4 = atan2 (
						(vec4xfirst * vec4ylast - vec4yfirst * vec4xlast),
						(vec4xfirst * vec4xlast + vec4yfirst * vec4ylast))
						* radtoang;
				int angleavg = round ((phi0 + phi1 + phi2 + phi3 + phi4) / 5);
				int anglescaled = (angleavg / anglescaling)
						- (angleavg / anglescaling) % anglestepping;

				if ((abs (x0len) > swipetolerance
						&& abs (x1len) > swipetolerance
						&& abs (x2len) > swipetolerance
						&& abs (x3len) > swipetolerance
						&& abs (x4len) > swipetolerance)
						|| (abs (y0len) > swipetolerance
								&& abs (y1len) > swipetolerance
								&& abs (y2len) > swipetolerance
								&& abs (y3len) > swipetolerance
								&& abs (y4len) > swipetolerance))
				{
					if (y0first >= (ymax - offsetbottom)
							&& y1first >= (ymax - offsetbottom)
							&& y2first >= (ymax - offsetbottom)
							&& y3first >= (ymax - offsetbottom)
							&& finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1
							&& finger4directionality < 1)
					{
						printf ("(5 fingers) swipe from bottom edge!\n");
						//system(commands[34]);
						swipesuccess = 1;
					}
					else if (y0first <= offsettop && y1first <= offsettop
							&& y2first <= offsettop && y3first <= offsettop
							&& finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1
							&& finger4directionality < 1)
					{
						printf ("(5 fingers) swipe from top edge!\n");
						//system(commands[35]);
						swipesuccess = 1;
					}
					else if (x0first >= (xmax - offsetright)
							&& x1first >= (xmax - offsetright)
							&& x2first >= (xmax - offsetright)
							&& x3first >= (xmax - offsetright)
							&& finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1
							&& finger4directionality > 1)
					{
						printf ("(5 fingers) swipe from right edge!\n");
						//system(commands[36]);
						swipesuccess = 1;
					}
					else if (x0first <= offsetleft && x1first <= offsetleft
							&& x2first <= offsetleft && x3first <= offsetleft
							&& finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1
							&& finger4directionality > 1)
					{
						printf ("(5 fingers) swipe from left edge!\n");
						//system(commands[37]);
						swipesuccess = 1;
					}
					else if (finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1
							&& finger4directionality < 1 && y0last > y0first
							&& y1last > y1first && y2last > y2first
							&& y3last > y3first)
					{
						printf ("(5 fingers) swipe down!\n");
						//system(commands[38]);
						swipesuccess = 1;
					}
					else if (finger1directionality < 1
							&& finger2directionality < 1
							&& finger3directionality < 1
							&& finger4directionality < 1 && y0last < y0first
							&& y1last < y1first && y2last < y2first
							&& y3last < y3first)
					{
						printf ("(5 fingers) swipe up!\n");
						//system(commands[39]);
						swipesuccess = 1;
					}
					else if (finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1
							&& finger4directionality > 1 && x0last > x0first
							&& x1last > x1first && x2last > x2first
							&& x3last > x3first)
					{
						printf ("(5 fingers) swipe right!\n");
						//system(commands[40]);
						swipesuccess = 1;
					}
					else if (finger1directionality > 1
							&& finger2directionality > 1
							&& finger3directionality > 1
							&& finger4directionality > 1 && x0last < x0first
							&& x1last < x1first && x2last < x2first
							&& x3last < x3first)
					{
						printf ("(5 fingers) swipe left!\n");
						//system(commands[41]);
						swipesuccess = 1;
					}
				}
				if (comdist < comdisttolerance && swipesuccess == 0)
				{
					printf ("(5 fingers) rotation with an angle of %i degree scaled to %i\n",
							angleavg, anglescaled);
					if (angleavg < 0)
					{
						//std::string tmpcommand = commands[42]
						//+ std::to_string (anglescaled);
						//system(tmpcommand.c_str ());
					}
					else
					{
						//std::string tmpcommand = commands[43]
						//+ std::to_string (anglescaled);
						//system(tmpcommand.c_str ());
					}
				}
			}

			//printf("arrays before clear: %i %i %i %i %i\n",finger1x.size(),finger2x.size(),finger3x.size(),finger4x.size(),finger5x.size());

			/*empty the vectors*/
			finger1x.clear ();
			finger1y.clear ();
			finger2x.clear ();
			finger2y.clear ();
			finger3x.clear ();
			finger3y.clear ();
			finger4x.clear ();
			finger4y.clear ();
			finger5x.clear ();
			finger5y.clear ();

			x0first = 0;
			x0last = 0;
			x0len = 0;
			y0first = 0;
			y0last = 0;
			y0len = 0;
			finger1directionality = 0;
			x1first = 0;
			x1last = 0;
			x1len = 0;
			y1first = 0;
			y1last = 0;
			y1len = 0;
			finger2directionality = 0;
			x2first = 0;
			x2last = 0;
			x2len = 0;
			y2first = 0;
			y2last = 0;
			y2len = 0;
			finger3directionality = 0;
			x3first = 0;
			x3last = 0;
			x3len = 0;
			y3first = 0;
			y3last = 0;
			y3len = 0;
			finger4directionality = 0;
			x4first = 0;
			x4last = 0;
			x4len = 0;
			y4first = 0;
			y4last = 0;
			y4len = 0;
			finger5directionality = 0;

			//printf("arrays after clear: %i %i %i %i %i\n",finger1x.size(),finger2x.size(),finger3x.size(),finger4x.size(),finger5x.size());
			swipesuccess = 0;
			touch_num_fingers = 0;
			gesture_complete = 1;
		}
	}

	return TOUCH_GESTURE_EVENT_NONE;
}

int
touch_gestures_get_finger_count (void)
{
	return touch_num_fingers;
}

int
touch_gestures_set_touch_device(const char* sysname)
{
	//printf("touch_gestures_set_touch_device() - sysname: %s\n", sysname);
	if(sysname)
	{
		touch_input_device = "/dev/input/" + std::string(sysname);
		return 0;
	}
	return -1;
}
