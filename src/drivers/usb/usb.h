/*
 * usb.h
 *
 * Public USB driver interface exposed to the driver management layer.
 */

/*
 * Copyright (C) 2001-2004 Kern Sibbald
 * Copyright (C) 2004-2005 Adam Kropelin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General
 * Public License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1335, USA.
 */

#ifndef _USB_H
#define _USB_H

#include "usb_common.h"

class UsbUpsDriver: public UpsDriver
{
public:
   UsbUpsDriver(UPSINFO *ups);
   virtual ~UsbUpsDriver() {}

   static UpsDriver *Factory(UPSINFO *ups);

   // UpsDriver functions impemented in UsbUpsDriver base class
   virtual bool get_capabilities();
   virtual bool read_volatile_data();
   virtual bool read_static_data();
   virtual bool kill_power();
   virtual bool shutdown();
   virtual bool entry_point(int command, void *data);

   // Extra functions exported for use by apctest
   // Implemented by derived XXXUsbUpsDriver class
   virtual int write_int_to_ups(int ci, int value, char const* name) = 0;
   virtual int read_int_from_ups(int ci, int *value) = 0;

protected:

   typedef struct s_usb_value {
      s_usb_value();                /* Constructor */
      int value_type;               /* Type of returned value */
      double dValue;                /* Value if double */
      int iValue;                   /* Integer value */
      const char *UnitName;         /* Name of units */
      char sValue[MAXSTRING];       /* Value if string */
   } USB_VALUE;

   // Helper functions implemented in UsbUpsDriver
   bool usb_get_value(int ci, USB_VALUE *uval);
   bool usb_process_value_bup(int ci, USB_VALUE* uval);
   void usb_process_value(int ci, USB_VALUE* uval);
   bool usb_update_value(int ci);
   bool usb_report_event(int ci, USB_VALUE *uval);
   double pow_ten(int exponent);

   struct s_known_info {
      int ci;                       /* Command index */
      unsigned usage_code;          /* Usage code */
      unsigned physical;            /* Physical usage */
      unsigned logical;             /* Logical usage */
      int data_type;                /* Data type expected */
      bool isvolatile;              /* Volatile data item */
   };

   static const s_known_info _known_info[];

   // Functions implemented in derived XXXUsbUpsDriver class
   virtual bool pusb_ups_get_capabilities() = 0;
   virtual bool pusb_get_value(int ci, USB_VALUE *uval) = 0;

   bool _quirk_old_backups_pro;
   struct timeval _prev_time;
   int _bpcnt;
};

/* Max rate to update volatile data */
#define MAX_VOLATILE_POLL_RATE 5

/* How often to retry the link (seconds) */
#define LINK_RETRY_INTERVAL    5 

/* These are the data_type expected for our know_info */ 
#define T_NONE     0          /* No units */
#define T_INDEX    1          /* String index */
#define T_CAPACITY 2          /* Capacity (usually %) */
#define T_BITS     3          /* Bit field */
#define T_UNITS    4          /* Use units/exponent field */
#define T_DATE     5          /* Date */
#define T_APCDATE  6          /* APC date */

/* These are the resulting value types returned */ 
#define V_DEFAULT  0          /* Unknown type */
#define V_DOUBLE   1          /* Double */ 
#define V_STRING   2          /* String pointer */
#define V_INTEGER  3          /* Integer */

/* These are the desired Physical usage values we want */ 
#define P_ANY     0           /* Any value */
#define P_OUTPUT  0x84001c    /* Output values */
#define P_BATTERY 0x840012    /* Battery values */
#define P_INPUT   0x84001a    /* Input values */
#define P_PWSUM   0x840024    /* Power summary */
#define P_APC1    0xff860007  /* From AP9612 environmental monitor */

/* No Command Index, don't save this value */ 
#define CI_NONE -1

/* Check if the UPS has the given capability */ 
#define UPS_HAS_CAP(ci) (_ups->UPS_Cap[ci])

#endif  /* _USB_H */
