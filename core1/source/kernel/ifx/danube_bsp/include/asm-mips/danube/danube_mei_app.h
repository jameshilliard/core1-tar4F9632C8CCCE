/*
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * danube_mei_driver
 * 25-July-2005: Jin-Sze.Sow@infineon.com
 *
 *
 *
 */
#ifndef       	_DANUBE_MEI_APP_H
#define        	_DANUBE_MEI_APP_H
                //  ioctl control
#define DANUBE_MEI_START 	                       	300
#define DANUBE_MEI_REPLY                           	301
#define DANUBE_MEI_NOREPLY                      	302

#define DANUBE_MEI_RESET				303
#define DANUBE_MEI_REBOOT				304
#define DANUBE_MEI_HALT					305
#define DANUBE_MEI_CMV_WINHOST				306
#define DANUBE_MEI_CMV_READ				307
#define DANUBE_MEI_CMV_WRITE				308
#define DANUBE_MEI_MIB_DAEMON				309
#define DANUBE_MEI_SHOWTIME				310
#define DANUBE_MEI_REMOTE				311
#define DANUBE_MEI_READDEBUG				312
#define DANUBE_MEI_WRITEDEBUG				313
#define DANUBE_MEI_LOP					314

#define DANUBE_MEI_PCM_SETUP				315
#define DANUBE_MEI_PCM_START_TIMER			316
#define DANUBE_MEI_PCM_STOP_TIMER			317
#define DANUBE_MEI_PCM_CHECK				318
#define DANUBE_MEI_GET_EOC_LEN				319
#define DANUBE_MEI_GET_EOC_DATA				320
#define DANUBE_MEI_PCM_GETDATA				321
#define DANUBE_MEI_PCM_GPIO				322
#define DANUBE_MEI_EOC_SEND				323
#define DANUBE_MEI_DOWNLOAD				326
#define DANUBE_MEI_JTAG_ENABLE				327
#define DANUBE_MEI_RUN					328


/* Loop diagnostics mode of the ADSL line related constants */
#define GET_ADSL_LOOP_DIAGNOSTICS_MODE 			330
#define SET_ADSL_LOOP_DIAGNOSTICS_MODE 			331
#define LOOP_DIAGNOSTIC_MODE_COMPLETE			332

/***	Enums    ***/
typedef enum mei_error
{
	MEI_SUCCESS = 0,
	MEI_FAILURE = -1,
	MEI_MAILBOX_FULL = -2,
	MEI_MAILBOX_EMPTY = -3,
        MEI_MAILBOX_TIMEOUT = -4,
}MEI_ERROR;

#endif
