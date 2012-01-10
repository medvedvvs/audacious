/*
 * list.h
 * Copyright 2011 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#ifndef AUDGUI_LIST_H
#define AUDGUI_LIST_H

#include <gtk/gtk.h>
#include <libaudcore/core.h>

typedef struct {
    void (* get_value) (void * user, int row, int column, GValue * value);

    /* selection (optional) */
    bool_t (* get_selected) (void * user, int row);
    void (* set_selected) (void * user, int row, bool_t selected);
    void (* select_all) (void * user, bool_t selected);

    void (* activate_row) (void * user, int row); /* optional */
    void (* right_click) (void * user, GdkEventButton * event); /* optional */
    void (* shift_rows) (void * user, int row, int before); /* optional */

    void (* mouse_motion) (void * user, GdkEventMotion * event, int row); /* optional */
    void (* mouse_leave) (void * user, GdkEventMotion * event, int row); /* optional */

    /* cross-widget drag and drop (optional) */
    const char * data_type;
    void (* get_data) (void * user, void * * data, int * length); /* data will
     be freed */
    void (* receive_data) (void * user, int row, const void * data, int length);
} AudguiListCallbacks;

GtkWidget * audgui_list_new (const AudguiListCallbacks * cbs, void * user,
 int rows);
void * audgui_list_get_user (GtkWidget * list);
void audgui_list_add_column (GtkWidget * list, const char * title,
 int column, GType type, int width);

int audgui_list_row_count (GtkWidget * list);
void audgui_list_insert_rows (GtkWidget * list, int at, int rows);
void audgui_list_update_rows (GtkWidget * list, int at, int rows);
void audgui_list_delete_rows (GtkWidget * list, int at, int rows);
void audgui_list_update_selection (GtkWidget * list, int at, int rows);

int audgui_list_get_highlight (GtkWidget * list);
void audgui_list_set_highlight (GtkWidget * list, int row);
int audgui_list_get_focus (GtkWidget * list);
void audgui_list_set_focus (GtkWidget * list, int row);

int audgui_list_row_at_point (GtkWidget * list, int x, int y);

#endif
