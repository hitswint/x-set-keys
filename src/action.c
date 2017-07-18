/***************************************************************************
 *
 * Copyright (C) 2017 Tomoyuki KAWAO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************/

#include "action.h"
#include "key-combination.h"

void action_free(gpointer action_)
{
  Action *action = action_;
  action->free_data(action->data);
  g_free(action);
}

gint action_compare_key_combination(gconstpointer a,
                                    gconstpointer b,
                                    gpointer user_data)
{
  return key_combination_compare(*(const KeyCombination *)a,
                                 *(const KeyCombination *)b);
}
