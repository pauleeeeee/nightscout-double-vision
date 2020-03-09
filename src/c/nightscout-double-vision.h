#pragma once
#include <pebble.h>

//messaging keys
#define PersonID 0
#define PersonName 1
#define SGV 2
#define Delta 3
#define IOB 4
#define Battery 5
#define Direction 6
#define MinutesAgo 7
#define IntervalTime 50

//declare reousrce IDs in this array for easy access when drawing bitmap arrows
static const uint32_t ICONS[] = {
	RESOURCE_ID_IMAGE_NONE, //0						
	RESOURCE_ID_IMAGE_UPUP,  //1
	RESOURCE_ID_IMAGE_UP,			 //2
	RESOURCE_ID_IMAGE_UP45,					 //3
	RESOURCE_ID_IMAGE_FLAT, //4
	RESOURCE_ID_IMAGE_DOWN45,   //5
	RESOURCE_ID_IMAGE_DOWN,	 //6
	RESOURCE_ID_IMAGE_DOWNDOWN  //7
};