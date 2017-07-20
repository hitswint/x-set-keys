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

#include "key-code-array.h"

gboolean key_code_array_remove(KeyCodeArray *array, KeyCode key_code)
{
  guint index;

  for (index = 0; index < key_code_array_get_length(array); index++) {
    if (key_code_array_get_at(array, index) == key_code) {
      g_array_remove_index(array, index);
      return TRUE;
    }
  }
  return FALSE;
}

gboolean key_code_array_contains(const KeyCodeArray *array, KeyCode key_code)
{
  KeyCode *pointer;

  for (pointer = &key_code_array_get_at(array, 0); *pointer; pointer++) {
    if (*pointer == key_code) {
      return TRUE;
    }
  }
  return FALSE;
}
