/*
 *  einit-gui-gtk.c++
 *  einit
 *
 *  Created by Magnus Deininger on 01/08/2007.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Magnus Deininger
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be
	  used to endorse or promote products derived from this software without
	  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <einit/einit++.h>

#include <gtkmm.h>
#include <iostream>

#include <libnotify/notify.h>

Einit einit;

void einit_feedback_event_handler (struct einit_remote_event *ev) {
 if ((ev->type == einit_feedback_notice) && ev->string) {
  gint to = 3000;
  NotifyNotification *nitem = notify_notification_new("eINIT Notification", ev->string, USHAREDIR "/images/einit.png", NULL);

  if (ev->flag < 4) { notify_notification_set_urgency (nitem, NOTIFY_URGENCY_CRITICAL); to = 60000; }
  else if (ev->flag < 6) { notify_notification_set_urgency (nitem, NOTIFY_URGENCY_NORMAL); to = 20000; }
  else { notify_notification_set_urgency (nitem, NOTIFY_URGENCY_LOW); }

  notify_notification_set_timeout(nitem, to);
  notify_notification_show (nitem, NULL);
 }
}

void einit_core_event_handler (struct einit_remote_event *ev) {
 if ((ev->type == einit_core_service_update) && ev->string) {
  if (ev->status & status_failed) {
   NotifyNotification *nitem = notify_notification_new("eINIT: Service Command Failed", ev->string, USHAREDIR "/images/einit.png", NULL);

   notify_notification_set_urgency (nitem, NOTIFY_URGENCY_CRITICAL);

   notify_notification_set_timeout(nitem, 30000);
   notify_notification_show (nitem, NULL);
  } else if (ev->status & status_enabled) {
   NotifyNotification *nitem = notify_notification_new("eINIT: Module Enabled", ev->string, USHAREDIR "/images/einit.png", NULL);

   notify_notification_set_urgency (nitem, NOTIFY_URGENCY_NORMAL);

   notify_notification_set_timeout(nitem, 30000);
   notify_notification_show (nitem, NULL);
  } else  if (ev->status & status_disabled) {
   NotifyNotification *nitem = notify_notification_new("eINIT: Module Disabled", ev->string, USHAREDIR "/images/einit.png", NULL);

   notify_notification_set_urgency (nitem, NOTIFY_URGENCY_NORMAL);

   notify_notification_set_timeout(nitem, 30000);
   notify_notification_show (nitem, NULL);
  }

 }
}

class EinitGTK : public Gtk::Window {
 public:
  EinitGTK();
  virtual ~EinitGTK();

 protected:
  //Signal handlers:
  static void     on_statusicon_popup  (GtkStatusIcon* widget,
				        guint          button, 
				        guint            time, 
				        gpointer       object);
  static  void on_menuitem_selected (const Glib::ustring& item_name);

  Glib::RefPtr<Gtk::TextBuffer> m_refTextBuffer1;

  Glib::RefPtr<Gtk::StatusIcon> m_refStatusIcon;
  Glib::RefPtr<Gtk::UIManager> m_refUIManager;
};

EinitGTK::EinitGTK() {

 Glib::RefPtr<Gtk::ActionGroup> refActionGroup = Gtk::ActionGroup::create();
 refActionGroup->add( Gtk::Action::create("Power Down", "Power Down"),
	       sigc::bind(sigc::ptr_fun(&EinitGTK::on_menuitem_selected), "Power Down") );
 refActionGroup->add( Gtk::Action::create("Reboot", "Reboot"),
	       sigc::bind(sigc::ptr_fun(&EinitGTK::on_menuitem_selected), "Reboot") );
 refActionGroup->add( Gtk::Action::create("Quit", Gtk::Stock::QUIT),
                      sigc::bind(sigc::ptr_fun(&EinitGTK::on_menuitem_selected), "Quit") );

 m_refUIManager = Gtk::UIManager::create();
 m_refUIManager->insert_action_group(refActionGroup);

 Glib::ustring ui_info =
  "<ui>"
  "  <popup name='Popup'>"
  "    <menuitem action='Power Down' />"
  "    <menuitem action='Reboot' />"
  "    <separator/>"
  "    <menuitem action='Quit' />"
  "  </popup>"
  "</ui>";

 m_refUIManager->add_ui_from_string(ui_info);

 m_refStatusIcon = Gtk::StatusIcon::create(Gdk::Pixbuf::create_from_file(USHAREDIR "/images/einit.png", 80, 80, true));
 m_refStatusIcon->set_tooltip("eINIT");
 GtkStatusIcon* gobj_StatusIcon = m_refStatusIcon->gobj();
 g_signal_connect(G_OBJECT(gobj_StatusIcon), "popup-menu", G_CALLBACK(on_statusicon_popup), this);

 einit.listen (einit_event_subsystem_feedback, einit_feedback_event_handler);
 einit.listen (einit_event_subsystem_core, einit_core_event_handler);
}

void EinitGTK::on_statusicon_popup(GtkStatusIcon* status_icon, guint button,
					guint time, gpointer object)
{
	EinitGTK* win = static_cast<EinitGTK*>(object);
	Gtk::Menu* pMenu = static_cast<Gtk::Menu*>( win->m_refUIManager->get_widget("/Popup") );
  
	if(pMenu)
		pMenu->popup(button, time);
}

void EinitGTK::on_menuitem_selected(const Glib::ustring& item)
{
 if (item == "Quit") {
  Gtk::Main::quit();
 } else if (item == "Power Down") {
  einit.powerDown();
 } else if (item == "Reboot") {
  einit.powerReset();
 } else {
  g_print("Unknown action: %s\n", item.data());
 }
}

EinitGTK::~EinitGTK() {
}

int main (int argc, char *argv[]) {
 notify_init ("eINIT");

 {
  Gtk::Main kit(argc, argv);

  EinitGTK trayicon;
  Gtk::Main::run();
 }

 return 0;
}
