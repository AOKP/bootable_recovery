/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "adb_install.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>

#include "common.h"
#include "fuse_sideload.h"
#include "install.h"
#include "ui.h"

static void set_usb_driver(bool enabled) {
  // USB configfs doesn't use /s/c/a/a/enable.
  if (android::base::GetBoolProperty("sys.usb.configfs", false)) {
    return;
  }

  static constexpr const char* USB_DRIVER_CONTROL = "/sys/class/android_usb/android0/enable";
  android::base::unique_fd fd(open(USB_DRIVER_CONTROL, O_WRONLY));
  if (fd == -1) {
    PLOG(ERROR) << "Failed to open driver control";
    return;
  }
  // Not using android::base::WriteStringToFile since that will open with O_CREAT and give EPERM
  // when USB_DRIVER_CONTROL doesn't exist. When it gives EPERM, we don't know whether that's due
  // to non-existent USB_DRIVER_CONTROL or indeed a permission issue.
  if (!android::base::WriteStringToFd(enabled ? "1" : "0", fd)) {
    PLOG(ERROR) << "Failed to set driver control";
  }
}

static void stop_adbd() {
  ui->Print("Stopping adbd...\n");
  android::base::SetProperty("ctl.stop", "adbd");
  set_usb_driver(false);
}

static void maybe_restart_adbd() {
  if (is_ro_debuggable()) {
    ui->Print("Restarting adbd...\n");
    set_usb_driver(true);
    android::base::SetProperty("ctl.start", "adbd");
  }
}

static pthread_t sideload_thread;
static pid_t     sideload_adb_pid;
static bool      sideload_cancelled;
static bool      sideload_started;

void *adb_sideload_thread(void* v) {
  time_t start_time = time(nullptr);
  time_t now = start_time;

  // How long (in seconds) we wait for the host to start sending us a package, before timing out.
  static constexpr int ADB_INSTALL_TIMEOUT = 300;

  // FUSE_SIDELOAD_HOST_PATHNAME will start to exist once the host connects and starts serving a
  // package. Poll for its appearance. (Note that inotify doesn't work with FUSE.)
  int status = -1;
  while (now - start_time < ADB_INSTALL_TIMEOUT) {
    // Exit if either:
    //  - The adb child process dies, or
    //  - The ui tells us to cancel
    if (kill(sideload_adb_pid, 0) != 0) {
      break;
    }
    if (sideload_cancelled) {
      break;
    }

    struct stat st;
    status = stat(FUSE_SIDELOAD_HOST_PATHNAME, &st);
    if (status == 0) {
      break;
    }
    if (errno != ENOENT && errno != ENOTCONN) {
      ui->Print("\nError %s waiting for package\n\n", strerror(errno));
      break;
    }

    sleep(1);
    now = time(nullptr);
  }

  if (status == 0) {
    sideload_started = true;
    // Signal UI thread that sideload has started
    ui->CancelWaitKey();
  }

  return nullptr;
}

void sideload_start() {
  stop_adbd();
  set_usb_driver(true);

  if ((sideload_adb_pid = fork()) == 0) {
    execl("/sbin/recovery", "recovery", "--adbd", nullptr);
    _exit(EXIT_FAILURE);
  }

  ui->Print("\n\nNow send the package you want to apply\n"
            "to the device with \"adb sideload <filename>\"...\n");

  sideload_cancelled = false;
  sideload_started = false;

  pthread_create(&sideload_thread, nullptr, &adb_sideload_thread, nullptr);
}

void sideload_wait(bool cancel) {
  if (cancel) {
    sideload_cancelled = true;
  }
  pthread_join(sideload_thread, nullptr);
}

int sideload_install(bool* wipe_cache, const char* install_file, bool verify) {
  int result = INSTALL_ERROR;
  if (sideload_started) {
    modified_flash = true;

    set_perf_mode(true);

    result = install_package(FUSE_SIDELOAD_HOST_PATHNAME,
                             wipe_cache,
                             install_file,
                             false, 0, verify);

    set_perf_mode(false);
  }

  return result;
}

void sideload_stop() {
  // Ensure adb exits
  int status;
  kill(sideload_adb_pid, SIGTERM);
  waitpid(sideload_adb_pid, &status, 0);

  sideload_started = false;

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    if (WEXITSTATUS(status) == 3) {
      ui->Print("\nYou need adb 1.0.32 or newer to sideload\nto this device.\n\n");
    } else if (!WIFSIGNALED(status)) {
      ui->Print("\n(adbd status %d)\n", WEXITSTATUS(status));
    }
  }

  ui->FlushKeys();

  maybe_restart_adbd();
}
