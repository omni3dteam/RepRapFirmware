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
# define MAIN_VERSION	"2.04RC4"
#else
# define MAIN_VERSION	"1.25RC4"
#endif

# define VERSION MAIN_VERSION "-" OMNI_VERSION
#endif

#define OMNI_VERSION    "1.08.1Alpha"

#ifndef DATE
# define DATE "2019-10-18b1"
#endif

#define AUTHORS "reprappro, dc42, chrishamm, t3p3, dnewman, printm3d"

#endif /* SRC_VERSION_H_ */
