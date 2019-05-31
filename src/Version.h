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
# define MAIN_VERSION	"2.03RC3+1"
#else
# define MAIN_VERSION	"1.24RC3+1"
#endif

# define VERSION MAIN_VERSION "-" OMNI_VERSION
#endif

#define OMNI_VERSION    "1.00RC1"

#ifndef DATE
# define DATE "2019-05-28b1"
#endif

#define AUTHORS "reprappro, dc42, chrishamm, t3p3, dnewman, printm3d"

#endif /* SRC_VERSION_H_ */
