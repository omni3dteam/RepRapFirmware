/*
 * Version.h
 *
 *  Created on: 25 Dec 2016
 *      Author: David
 */

#ifndef SRC_VERSION_H_
#define SRC_VERSION_H_


#ifndef VERSION
#ifdef RTOS
# define MAIN_VERSION	"2.05.1"
#else
# define MAIN_VERSION	"1.26.1"
#endif

# define VERSION MAIN_VERSION "-" OMNI_VERSION
#endif

#define OMNI_VERSION    "1.14.9"

#ifndef DATE
# define DATE "2020-02-09b1"
#endif

#define AUTHORS "reprappro, dc42, chrishamm, t3p3, dnewman, printm3d"

#endif /* SRC_VERSION_H_ */
