#include <stdio.h>
//#include <stdlib.h>

#include "touch-gestures.h"

#ifdef __cplusplus
using namespace std;
#endif

int main(int argc, char **argv)
{
  touch_gestures_init();
  while(1)
  {
	  touch_gestures_get();
  }
}
