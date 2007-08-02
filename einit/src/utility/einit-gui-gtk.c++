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

class EinitGTK : public Gtk::Window {
 public:
  EinitGTK();
  virtual ~EinitGTK();

  Einit einit;

 protected:
  virtual void updateInformation();

  //Signal handlers:
  virtual void on_button_quit();
  virtual void on_button_buffer1();
  static void     on_statusicon_popup  (GtkStatusIcon* widget,
				        guint          button, 
				        guint            time, 
				        gpointer       object);
  static  void on_menuitem_selected (const Glib::ustring& item_name);

  //Child widgets:
  Gtk::VBox m_VBox;

  Gtk::ScrolledWindow m_ScrolledWindow;
  Gtk::TextView m_TextView;

  Glib::RefPtr<Gtk::TextBuffer> m_refTextBuffer1;

  Gtk::HButtonBox m_ButtonBox;
  Gtk::Button m_Button_Quit, m_Button_Buffer1;

  Glib::RefPtr<Gtk::StatusIcon> m_refStatusIcon;
  Glib::RefPtr<Gtk::UIManager> m_refUIManager;
};

EinitGTK::EinitGTK(): m_Button_Quit(Gtk::Stock::QUIT), m_Button_Buffer1("Update"), einit() {
 set_title("eINIT GTK GUI");
 set_border_width(5);
 set_default_size(400, 200);


 add(m_VBox);

  //Add the TreeView, inside a ScrolledWindow, with the button underneath:
 m_ScrolledWindow.add(m_TextView);

  //Only show the scrollbars when they are necessary:
 m_ScrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

 m_VBox.pack_start(m_ScrolledWindow);

  //Add buttons:
 m_VBox.pack_start(m_ButtonBox, Gtk::PACK_SHRINK);

 m_ButtonBox.pack_start(m_Button_Buffer1, Gtk::PACK_SHRINK);
 m_ButtonBox.pack_start(m_Button_Quit, Gtk::PACK_SHRINK);
 m_ButtonBox.set_border_width(5);
 m_ButtonBox.set_spacing(5);
 m_ButtonBox.set_layout(Gtk::BUTTONBOX_END);

  //Connect signals:
 m_Button_Quit.signal_clicked().connect(sigc::mem_fun(*this,
 &EinitGTK::on_button_quit) );
 m_Button_Buffer1.signal_clicked().connect(sigc::mem_fun(*this,
 &EinitGTK::on_button_buffer1) );

 m_refTextBuffer1 = Gtk::TextBuffer::create();

 Glib::RefPtr<Gtk::ActionGroup> refActionGroup = Gtk::ActionGroup::create();
 refActionGroup->add( Gtk::ToggleAction::create("Toggle0", "enable something", "", true) );
 refActionGroup->add( Gtk::ToggleAction::create("Toggle1", "enable something else") );
 refActionGroup->add( Gtk::Action::create("Preferences", Gtk::Stock::PREFERENCES),
	       sigc::bind(sigc::ptr_fun(&EinitGTK::on_menuitem_selected),
			  "Preferences") );
 refActionGroup->add( Gtk::Action::create("Info", Gtk::Stock::INFO),
	       sigc::bind(sigc::ptr_fun(&EinitGTK::on_menuitem_selected),
			  "Information") );
 refActionGroup->add( Gtk::Action::create("Help", Gtk::Stock::HELP),
	       sigc::bind(sigc::ptr_fun(&EinitGTK::on_menuitem_selected),
			  "Help") );
 refActionGroup->add( Gtk::Action::create("Quit", Gtk::Stock::QUIT),
	       sigc::mem_fun(*this, &EinitGTK::hide) );

 m_refUIManager = Gtk::UIManager::create();
 m_refUIManager->insert_action_group(refActionGroup);

 Glib::ustring ui_info =
  "<ui>"
  "  <popup name='Popup'>"
  "    <menuitem action='Toggle0' />"
  "    <menuitem action='Toggle1' />"
  "    <menuitem action='Preferences' />"
  "    <separator/>"
  "    <menuitem action='Info' />"
  "    <menuitem action='Help' />"
  "    <separator/>"
  "    <menuitem action='Quit' />"
  "  </popup>"
  "</ui>";

 m_refUIManager->add_ui_from_string(ui_info);

 m_refStatusIcon = Gtk::StatusIcon::create(Gtk::Stock::HOME);
 m_refStatusIcon->set_tooltip("eINIT gtk++ gui");
 GtkStatusIcon* gobj_StatusIcon = m_refStatusIcon->gobj();
 g_signal_connect(G_OBJECT(gobj_StatusIcon), "popup-menu", G_CALLBACK(on_statusicon_popup), this);
 updateInformation();

 show_all_children();
}

void EinitGTK::on_statusicon_popup(GtkStatusIcon* status_icon, guint button,
					guint time, gpointer object)
{
	EinitGTK* win = static_cast<EinitGTK*>(object);
	Gtk::Menu* pMenu = static_cast<Gtk::Menu*>( win->m_refUIManager->get_widget("/Popup") );
  
	if(pMenu)
		pMenu->popup(button, time);
}

void EinitGTK::on_menuitem_selected(const Glib::ustring& item) //static
{
  g_print("You have selected `%s'\n", item.data());
}

void EinitGTK::updateInformation() {
 this->einit.update();
 einit_connect();
 char *services = einit_ipc ("list services");

 if (services) {
  m_refTextBuffer1->set_text(services);
 } else {
  m_refTextBuffer1->set_text("Connection Failed");
}

 m_TextView.set_buffer(m_refTextBuffer1);
}

EinitGTK::~EinitGTK() {
}

void EinitGTK::on_button_quit() {
 hide();
}

void EinitGTK::on_button_buffer1() {
 this->updateInformation();
}

int main (int argc, char *argv[]) {
 Gtk::Main kit(argc, argv);

 EinitGTK helloworld;
 Gtk::Main::run(helloworld);

 return 0;
}
