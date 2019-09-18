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
# define MAIN_VERSION	"2.04RC2"
#else
# define MAIN_VERSION	"1.25RC2"
#endif

# define VERSION MAIN_VERSION "-" OMNI_VERSION
#endif

#define OMNI_VERSION    "1.06.4Alpha"

#ifndef DATE
# define DATE "2019-08-05b1"
#endif

#define AUTHORS "reprappro, dc42, chrishamm, t3p3, dnewman, printm3d"

#endif /* SRC_VERSION_H_ */
