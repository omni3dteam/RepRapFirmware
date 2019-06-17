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
# define MAIN_VERSION	"2.03"
#else
# define MAIN_VERSION	"1.24"
#endif

# define VERSION MAIN_VERSION "-" OMNI_VERSION
#endif

#define OMNI_VERSION    "1.00RC2"

#ifndef DATE
# define DATE "2019-06-13b2"
#endif

#define AUTHORS "reprappro, dc42, chrishamm, t3p3, dnewman, printm3d"

#endif /* SRC_VERSION_H_ */
