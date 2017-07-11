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

#include <unistd.h>
#include <linux/uinput.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"
#include "uinput-device.h"
#include "keyboard-device.h"

static gint _open_uinput_device();
static gboolean _create_uinput_device(XSetKeys *xsk, gint fd);
static gboolean _write(gint fd, gconstpointer buffer, gsize length);
static gboolean _handle_event(gpointer user_data);

Device *ud_initialize(XSetKeys *xsk)
{
  gint fd = _open_uinput_device();

  if (fd < 0) {
    g_critical("Failed to open uinput device."
               " Maybe uinput module is not loaded");
    return NULL;
  }
  if (!_create_uinput_device(xsk, fd)) {
    close(fd);
    return NULL;
  }
  return device_initialize(fd, "uinput device", _handle_event, xsk);
}

void ud_finalize(XSetKeys *xsk)
{
  Device *device = xsk_get_uinput_device(xsk);

  if (ioctl(device_get_fd(device), UI_DEV_DESTROY) < 0) {
    print_error("Failed to destroy uinput device");
  }
  device_finalize(device);
}

gboolean ud_send_key_event(XSetKeys *xsk, KeyCode key_cord, gboolean is_press)
{
  struct input_event event;

  event.type = EV_KEY;
  event.code = key_cord;
  event.value = is_press ? 1 : 0;
  if (!ud_send_event(xsk, &event)) {
    return FALSE;
  }

  event.type = EV_SYN;
  event.code = SYN_REPORT;
  event.value = 0;
  if (!ud_send_event(xsk, &event)) {
    return FALSE;
  }

  return TRUE;
}

gboolean ud_send_event(XSetKeys *xsk, struct input_event *event)
{
  Device *device = xsk_get_uinput_device(xsk);

  gettimeofday(&event->time, NULL);
  debug_print("Write to uinput : type=%02x code=%d value=%d",
              event->type,
              event->code,
              event->value);
  return device_write(device, event, sizeof (*event));
}

static gint _open_uinput_device()
{
  const char* filepath[] = { "/dev/input/uinput", "/dev/uinput" };
  gint fd;
  gint index;

  for (index = 0; index < array_num(filepath); index++) {
    fd = open(filepath[index], O_RDWR);
    if (fd >= 0) {
      break;
    }
  }
  return fd;
}

static gboolean _create_uinput_device(XSetKeys *xsk, gint fd)
{
  struct uinput_user_dev user_dev = { { 0 } };
  uint8_t ev_bits[KD_EV_BITS_LENGTH] = { 0 };
  uint8_t key_bits[KD_KEY_BITS_LENGTH] = { 0 };
  gint index = 0;

  user_dev.id.bustype = BUS_VIRTUAL;
  strcpy(user_dev.name, "x-set-keys");
  user_dev.id.vendor = 1;
  user_dev.id.product = 1;
  user_dev.id.version = 1;
  if (!_write(fd, &user_dev, sizeof (user_dev))) {
    print_error("Failed to write uinput user device");
    return FALSE;
  }

  if (!kd_get_ev_bits(xsk, ev_bits)) {
    print_error("Failed to get ev bits from keyboard device");
    return FALSE;
  }
  for (index = 0; index < EV_CNT; index++) {
    if (kd_test_bit(ev_bits, index)) {
      if (ioctl(fd, UI_SET_EVBIT, index) < 0) {
        print_error("Failed to set ev bit %02x to uinput device", index);
        return FALSE;
      }
      debug_print("UI_SET_EVBIT : %02x", index);
    }
  }

  if (!kd_get_key_bits(xsk, key_bits)) {
    print_error("Failed to get key bits from keyboard device");
    return FALSE;
  }
  for (index = 0; index < KEY_CNT; index++) {
    if (kd_test_bit(key_bits, index)) {
      if (ioctl(fd, UI_SET_KEYBIT, index) < 0) {
        print_error("Failed to set key bit %d to uinput device", index);
        return FALSE;
      }
      debug_print("UI_SET_KEYBIT : %d", index);
    }
  }

  if (ioctl(fd, UI_DEV_CREATE) < 0) {
    print_error("Failed to create uinput device");
    return FALSE;
  }
  return TRUE;
}

static gboolean _write(gint fd, gconstpointer buffer, gsize length)
{
  gssize rest = length;

  while (rest > 0) {
    gssize written = write(fd, buffer, rest);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return FALSE;
    }
    rest -= written;
    buffer += written;
  }
  return TRUE;
}

static gboolean _handle_event(gpointer user_data)
{
  XSetKeys *xsk;
  Device *device;
  gssize length;
  struct input_event event;

  xsk = user_data;
  device = xsk_get_uinput_device(xsk);

  length = device_read(device, &event, sizeof (event));
  if (length < 0) {
    return FALSE;
  }
  debug_print("Read from uinput : length=%ld", length);
  return kd_write(xsk, &event, length);
}
