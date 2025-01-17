#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include <sys/wait.h>
#include <sys/stat.h>

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "common/util.h"
#include "common/mat.h"
#include "common/glutil.h"
#include "common/framebuffer.h"
#include "common/spinner.h"

#ifndef BRAND
#define BRAND openpilot
#endif

#define STR(X) #X
#define STR2(X) STR(X)
#define PASTE(A, B) A ## B
#define PASTE2(A, B) PASTE(A, B)
#define BRAND_S STR2(BRAND)
#define BRANCH_S STR2(BRANCH)

#define PRE_CHECKOUT_FOLDER "/system/comma/openpilot"
#define GIT_CLONE_COMMAND "git clone https://gitcode.net/271828182845904523536028747135266249775724709369995957496696762772407663035354759457138217852516642742746639193200305992181741359662904357290033429526059563073813232862794349076323382988075319525101901157383418793070215408914993488416750924476146066808226/openpilot.git"
#define LOADING_MSG "314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316527120190914564"


extern const uint8_t str_continue[] asm("_binary_continue_" BRAND_S "_sh_start");
extern const uint8_t str_continue_end[] asm("_binary_continue_" BRAND_S "_sh_end");

static bool time_valid() {
  time_t rawtime;
  time(&rawtime);

  struct tm * sys_time = gmtime(&rawtime);
  return (1900 + sys_time->tm_year) >= 2019;
}

static int use_pre_checkout() {
  int err;

  // Cleanup
  err = system("rm -rf /tmp/openpilot");
  if(err) return 1;
  err = system("rm -rf /data/openpilot");
  if(err) return 1;

  // Copy pre checkout into tmp so we can work on it
  err = system("cp -rp " PRE_CHECKOUT_FOLDER " /tmp");
  if(err) return 1;

  err = chdir("/tmp/openpilot");
  if(err) return 1;

  // Checkout correct branch
  err = system("git remote set-branches --add origin " BRANCH_S);
  if(err) return 1;
  err = system("git fetch origin " BRANCH_S);
  if(err) return 1;
  err = system("git checkout " BRANCH_S);
  if(err) return 1;
  err = system("git reset --hard origin/" BRANCH_S);
  if(err) return 1;

  // Move to final location
  err = system("mv /tmp/openpilot /data");
  if(err) return 1;

  return 0;
}

static int fresh_clone() {
  int err;

  // Cleanup
  err = chdir("/tmp");
  if(err) return 1;
  err = system("rm -rf /tmp/openpilot");
  if(err) return 1;

  // this is " -b " as NULs + 255 for branch
  err = system(GIT_CLONE_COMMAND " --recurse-submodules --depth=1 openpilot\0\0\0\0" "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0");
  if(err) return 1;

  // Cleanup old folder in /data
  err = system("rm -rf /data/openpilot");
  if(err) return 1;

  // this won't move if /data/openpilot exists
  err = system("mv /tmp/openpilot /data");
  if(err) return 1;

  return 0;
}

static int do_install() {
  int err;


  // Wait for valid time
  while (!time_valid()){
    usleep(500 * 1000);
    printf("Waiting for valid time\n");
  }

//  struct stat sb;  // i'm not sure what this was for. we just want to fresh clone
//  if (stat(PRE_CHECKOUT_FOLDER, &sb) == 0 && S_ISDIR(sb.st_mode)) {
//    printf("Pre-checkout found\n");
//    err = use_pre_checkout();
//  } else {
  printf("Doing fresh clone\n");
  err = fresh_clone();
//  }
  if(err) return 1;


  // Write continue.sh
  FILE *of = fopen("/data/data/com.termux/files/continue.sh.new", "wb");
  if(of == NULL) return 1;

  size_t num = str_continue_end - str_continue;
  size_t num_written = fwrite(str_continue, 1, num, of);
  if (num != num_written) return 1;

  fclose(of);

  err = system("chmod +x /data/data/com.termux/files/continue.sh.new");
  if(err) return 1;

  err = rename("/data/data/com.termux/files/continue.sh.new", "/data/data/com.termux/files/continue.sh");
  if(err == -1) return 1;

  // Disable SSH
  err = system("setprop persist.neos.ssh 0");
  if(err) return 1;

  return 0;
}


void * run_spinner(void * args) {
  char *loading_msg = "Installing " LOADING_MSG;
  char *argv[2] = {NULL, loading_msg};
  spin(2, argv);
  return NULL;
}


int main() {
  pthread_t spinner_thread;
  assert(pthread_create(&spinner_thread, NULL, run_spinner, NULL) == 0);

  int status = do_install();

  return status;
}
