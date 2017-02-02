/*
 * WebFiles.h
 *
 *  Created on: 29 d�c. 2016
 *      Author: fran�ois
 */

#ifndef APP_INCLUDE_WEBFILES_H_
#define APP_INCLUDE_WEBFILES_H_

#include "c_types.h"

struct WEBFILE_INFO {
	const char* filename;
	const uint16_t filesize;
	const char* mimetype;
	const uint8_t* filecontent;
};

#endif /* APP_INCLUDE_WEBFILES_H_ */
