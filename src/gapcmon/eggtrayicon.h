
/* eggtrayicon.h           serial-0085-0 ***************************************
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 *
 * Modified by James Scott, Jr <skoona@users.sourceforge.net>
 * - To enhance events and size management  4/2006
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __EGG_TRAY_ICON_H__
#define __EGG_TRAY_ICON_H__

#include <gtk/gtkplug.h>
#include <gdk/gdkx.h>
#ifdef G_OS_WIN32
#include <gdk/gdkwin32.h>
#endif                             /* WINDOWS */

G_BEGIN_DECLS
#define EGG_TYPE_TRAY_ICON (egg_tray_icon_get_type ())
#define EGG_TRAY_ICON(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_TRAY_ICON, EggTrayIcon))
#define EGG_TRAY_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_TRAY_ICON, EggTrayIconClass))
#define EGG_IS_TRAY_ICON(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_TRAY_ICON))
#define EGG_IS_TRAY_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_TRAY_ICON))
#define EGG_TRAY_ICON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), EGG_TYPE_TRAY_ICON, EggTrayIconClass))
typedef struct _EggTrayIcon EggTrayIcon;
typedef struct _EggTrayIconClass EggTrayIconClass;

struct _EggTrayIcon {
   GtkPlug parent_instance;

   guint stamp;

#ifdef GDK_WINDOWING_X11
   Atom selection_atom;
   Atom manager_atom;
   Atom system_tray_opcode_atom;
   Atom orientation_atom;
   Window manager_window;
#endif
   GtkOrientation orientation;
};

struct _EggTrayIconClass {
   GtkPlugClass parent_class;
};

GType egg_tray_icon_get_type(void);

EggTrayIcon *egg_tray_icon_new_for_screen(GdkScreen * screen, const gchar * name);

EggTrayIcon *egg_tray_icon_new(const gchar * name);

guint egg_tray_icon_send_message(EggTrayIcon * icon,
   gint timeout, const char *message, gint len);
void egg_tray_icon_cancel_message(EggTrayIcon * icon, guint id);

GtkOrientation egg_tray_icon_get_orientation(EggTrayIcon * icon);

G_END_DECLS
#endif                             /* __EGG_TRAY_ICON_H__ */
