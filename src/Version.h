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
# define MAIN_VERSION	"2.05.1+"
#else
# define MAIN_VERSION	"1.26.1+"
#endif

# define VERSION OMNI_VERSION "(based on " MAIN_VERSION ")"
#endif

#define OMNI_VERSION    "1.17.13"

#ifndef DATE
# define DATE "2020-06-11b1"
#endif

#define AUTHORS "reprappro, dc42, chrishamm, t3p3, dnewman, printm3d, OMNI3D"

#endif /* SRC_VERSION_H_ */
