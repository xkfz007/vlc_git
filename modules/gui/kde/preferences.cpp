/*****************************************************************************
 * preferences.cpp: preferences window for the kde gui
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: preferences.cpp,v 1.10 2002/12/17 09:54:32 sam Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no> Mon Aug 12 2002
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <kdialogbase.h>
#include <qmap.h>
#include <qcheckbox.h>
#include <qframe.h>
#include <qgroupbox.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qlistview.h>
#include <qnamespace.h>
#include <qobjectlist.h>
#include <qspinbox.h>
#include <qtooltip.h>
#include <qvbox.h>

#include <kbuttonbox.h>
#include <klineedit.h>
#include <klocale.h>
#include <knuminput.h>
#include <kurlrequester.h>

#include "QConfigItem.h"
#include "pluginsbox.h"
#include "preferences.h"

/*
  construct a new configuration window for the given module
*/
KPreferences::KPreferences(intf_thread_t *p_intf, const char *psz_module_name,
                           QWidget *parent, const QString &caption) :
    KDialogBase ( Tabbed, caption, Ok| Apply|Cancel|User1, Ok, parent,
                  "vlc preferences", true, false, "Save")
{
    module_t *p_parser = NULL;
    vlc_list_t list;
    module_config_t *p_item;
    int i_index;
    QVBox *category_table = NULL;
    QString *category_label;

    this->p_intf = p_intf;

    /* List all modules */
    list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* Look for the selected module */
    for( i_index = 0; i_index < list.i_count; i_index++ )
    {
        p_parser = (module_t *)list.p_values[i_index].p_object ;

        if( psz_module_name
            && !strcmp( psz_module_name, p_parser->psz_object_name ) )
        {
            break;
        }
    }

    if( !p_parser || i_index == list.i_count )
    {
        vlc_list_release( &list );
        return;
    }

    p_item = p_parser->p_config;
    if( p_item ) do
    {
        switch( p_item->i_type )
        {

        case CONFIG_HINT_CATEGORY:
        case CONFIG_HINT_END:

            /*
             * Now we can start taking care of the new category
             */
            if( p_item->i_type == CONFIG_HINT_CATEGORY )
            {
                category_label = new QString( p_item->psz_text );
                QFrame *page = addPage( *category_label );
                QVBoxLayout *toplayout = new QVBoxLayout( page);
                QScrollView *sv = new QScrollView(page);
                sv->setResizePolicy(QScrollView::AutoOneFit);
                sv->setFrameStyle(QScrollView::NoFrame);
                toplayout->addWidget(sv);
                category_table = new QVBox(sv->viewport());
                sv->addChild(category_table);
                toplayout->addStretch(10);
                category_table->setSpacing(spacingHint());
            }

            break;

        case CONFIG_ITEM_MODULE:

        {
            vlc_mutex_lock( p_item->p_lock );
            KPluginsBox *item_frame =
                new KPluginsBox( p_intf, p_item->psz_text,
                                 p_item->psz_value ? p_item->psz_value :"",
                                 category_table,
                                 spacingHint(),
                                 this );
            QConfigItem *ci = new QConfigItem(this,
                                              p_item->psz_name,
                                              p_item->i_type,
                                              p_item->psz_value);
            connect(item_frame, SIGNAL(selectionChanged(const QString &)),
                    ci, SLOT(setValue(const QString &)));


            /* build a list of available plugins */
            for( i_index = 0; i_index < list.i_count; i_index++ )
            {
                p_parser = (module_t *)list.p_values[i_index].p_object ;

                if( !strcmp( p_parser->psz_capability,
                             p_item->psz_type ) )
                {
                    new QListViewItem(item_frame->getListView(),
                                      p_parser->psz_object_name,
                                      p_parser->psz_longname);
                }
            }

            vlc_mutex_unlock( p_item->p_lock );
        }
        break;

        case CONFIG_ITEM_STRING:
        {
            QHBox *hb = new QHBox(category_table);
            hb->setSpacing(spacingHint());
            new QLabel(p_item->psz_text, hb);
            /* add input box with default value */
            vlc_mutex_lock( p_item->p_lock );

            KLineEdit *kl = new KLineEdit( p_item->psz_value ?
                                           p_item->psz_value : "", hb);
            QConfigItem *ci = new QConfigItem(this, p_item->psz_name,
                                              p_item->i_type,
                                              p_item->psz_value ?
                                              p_item->psz_value : "");
            connect(kl, SIGNAL(textChanged ( const QString & )),
                    ci, SLOT(setValue( const QString &)));
            QToolTip::add(kl, p_item->psz_longtext);
            kl->setMaxLength(40);

            vlc_mutex_unlock( p_item->p_lock );
        }
        break;

        case CONFIG_ITEM_FILE:
        {
            QHBox *hb = new QHBox(category_table);
            hb->setSpacing(spacingHint());
            new QLabel(p_item->psz_text, hb);
            /* add input box with default value */
            vlc_mutex_lock( p_item->p_lock );

//            KLineEdit *kl = new KLineEdit( p_item->psz_value ?
//                                           p_item->psz_value : "", hb);
            QConfigItem *ci = new QConfigItem(this, p_item->psz_name,
                                              p_item->i_type,
                                              p_item->psz_value ?
                                              p_item->psz_value : "");
//            QPushButton *bbrowse = new QPushButton( _("Browse"), hb );
            KURLRequester *kfile = new KURLRequester( p_item->psz_value ?
                                                      p_item->psz_value : "",
                                                      hb );
            connect(kfile, SIGNAL(textChanged ( const QString & )),
                    ci, SLOT(setValue( const QString &)));
            QToolTip::add(kfile, p_item->psz_longtext);
            vlc_mutex_unlock( p_item->p_lock );
        }
        break;

        case CONFIG_ITEM_INTEGER:
            /* add input box with default value */
        {
            QHBox *hb = new QHBox(category_table);
            hb->setSpacing(spacingHint());
            new QLabel(p_item->psz_text, hb);
            QSpinBox *item_adj = new QSpinBox(-1, 99999, 1, hb);
            QConfigItem *ci = new QConfigItem(this, p_item->psz_name,
                                              p_item->i_type,
                                              p_item->i_value);
            item_adj->setValue( p_item->i_value );
            connect(item_adj, SIGNAL(valueChanged( int)),
                    ci, SLOT(setValue(int)));
            QToolTip::add(item_adj, p_item->psz_longtext);
        }
        break;

        case CONFIG_ITEM_FLOAT:
        {
            QHBox *hb = new QHBox(category_table);
            hb->setSpacing(spacingHint());
            new QLabel(p_item->psz_text, hb);
            KDoubleNumInput *kdi= new KDoubleNumInput(p_item->f_value, hb);
            kdi->setRange(-1, 99999, 0.01, false);
            QConfigItem *ci = new QConfigItem(this, p_item->psz_name,
                                              p_item->i_type,
                                              p_item->f_value);
            connect(kdi, SIGNAL(valueChanged(double)),
                    ci, SLOT(setValue(double)));
            QToolTip::add(kdi, p_item->psz_longtext);
        }
        break;

        case CONFIG_ITEM_BOOL:

            /* add check button */
        {
            QCheckBox *bool_checkbutton =
                new QCheckBox(QString(p_item->psz_text), category_table);
            QConfigItem *ci = new QConfigItem(this, p_item->psz_name,
                                              p_item->i_type,
                                              p_item->i_value);
            bool_checkbutton->setChecked(p_item->i_value);
            connect(bool_checkbutton, SIGNAL(stateChanged( int)),
                    ci, SLOT(setValue(int)));
            QToolTip::add(bool_checkbutton, p_item->psz_longtext);

        }
        break;

        }

        p_item++;
    }
    while( p_item->i_type != CONFIG_HINT_END );

    vlc_list_release( &list );

    exec();
}

/*
  empty destructor, qt takes care of this (I think)
*/
KPreferences::~KPreferences()
{
}

/*
  return true if the give module is configureable
*/
bool KPreferences::isConfigureable(QString module)
{
    module_t *p_parser;
    vlc_list_t list;
    int i_index;

    list = vlc_list_find( this->p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < list.i_count; i_index++ )
    {
        p_parser = (module_t *)list.p_values[i_index].p_object ;

        if( !module.compare( p_parser->psz_object_name ) )
        {
            bool ret = p_parser->i_config_items != 0;
            vlc_list_release( &list );
            return ret;
        }
    }

    vlc_list_release( &list );
    return false;
}

/*
  run when the Apply button is pressed, and by the methods for the ok
  and save buttons
*/
void KPreferences::slotApply()
{
    QObjectList * l = queryList( "QConfigItem" );
    QObjectListIt it( *l );             // iterate over the config items
    QObject * obj;
    while ( (obj=it.current()) != 0 ) {
        ++it;
        QConfigItem *p_config = (QConfigItem *)obj;
        if ( p_config->changed() )
        {
            msg_Dbg( p_intf, const_cast<char *>(p_config->name()));
            msg_Dbg( p_intf, "%d", p_config->getType());

            switch( p_config->getType() ) {

            case CONFIG_ITEM_STRING:
            case CONFIG_ITEM_FILE:
            case CONFIG_ITEM_MODULE:
                if (p_config->sValue()) {
                    config_PutPsz( p_intf, p_config->name(),
                                   strdup(p_config->sValue().latin1()));
                }
                else {
                    config_PutPsz( p_intf, p_config->name(), NULL );
                }
                break;
            case CONFIG_ITEM_INTEGER:
            case CONFIG_ITEM_BOOL:
                config_PutInt( p_intf, p_config->name(), p_config->iValue() );
                break;
            case CONFIG_ITEM_FLOAT:
                config_PutFloat( p_intf, p_config->name(), p_config->fValue() );
                break;
            }
            p_config->resetChanged();
        }
    }
    delete l;
}

/*
  run when the Ok button is pressed
*/
void KPreferences::slotOk()
{
    slotApply();
    accept();
}

/*
  run when the save button is pressed
*/
void KPreferences::slotUser1()
{
    slotApply();
    config_SaveConfigFile( p_intf, NULL );
}
