
//===========================================================================
//
// log2pg - File forwarder to Postgresql database
// Copyright (C) 2018 Gerard Torrent
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//===========================================================================

#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>
#include <syslog.h>
#include <pthread.h>
#include <assert.h>
#include "log.h"
#include "config.h"
#include "vector.h"
#include "mqueue.h"
#include "entities.h"
#include "map.h"
#include "witem.h"
#include "monitor.h"
#include "processor.h"
#include "database.h"

#define PACKAGE_NAME "log2pg"
#define PACKAGE_VERSION "0.1.0"
#define DEFAULT_CONFIG_FILE "/etc/" PACKAGE_NAME ".conf"
#define QUEUE2_MAX_CAPACITY 32000
#define UNUSED(x) (void)(x)

// public variables
volatile sig_atomic_t keep_running = 1;
volatile pthread_t *thread1 = NULL;

/**************************************************************************//**
 * @brief Force exit sending a signal to monitor thread.
 * @details Linked to catchsignal.
 */
void terminate(void)
{
  if (keep_running > 0 && thread1 != NULL) {
    pthread_kill(*thread1, SIGINT);
  }
}

/**************************************************************************//**
 * @details Signal handler that simply resets a flag to cause termination.
 * @param[in] signum Signal number.
 */
void catchsignal(int signum)
{
  keep_running = 0;
  syslog(LOG_INFO, "system interrupted (%d)", signum);
}

/**************************************************************************//**
 * @brief Set signal handlers.
 * @see Pthreads Programming (Bradford Nichols, Dick Buttlar and Jacqueline Proulx Farrell)
 * @see http://maxim.int.ru/bookshelf/PthreadsProgram/htm/r_1.html
 */
void set_signal_handlers(void)
{

  // defines process signal handlers
  struct sigaction sa;
  sa.sa_handler = catchsignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  // disable signals for the main thread (and descendants)
  sigset_t signals_to_block = {0};
  sigaddset(&signals_to_block, SIGINT);
  sigaddset(&signals_to_block, SIGABRT);
  sigaddset(&signals_to_block, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &signals_to_block, NULL);
}

/**************************************************************************//**
 * @brief Displays program help.
 * @details Follows POSIX guidelines. You can create man pages using help2man.
 * @see http://www.gnu.org/prep/standards/standards.html#Command_002dLine-Interfaces
 * @see http://www.gnu.org/software/help2man/
 */
void help(void)
{
  fprintf(stdout,
    "Usage: " PACKAGE_NAME " [OPTION]...\n"
    "\n"
    "File forwarder to Postgresql database.\n"
    "\n"
    "Mandatory arguments to long options are mandatory for short options too.\n"
    "  -f, --file=CONFIG   Set configuration file (default = " DEFAULT_CONFIG_FILE ").\n"
    "  -h, --help          Show this message and exit.\n"
    "      --version       Show version info and exit.\n"
    "\n"
    "Exit status:\n"
    "  0   finished without errors.\n"
    "  1   finished with errors.\n"
  );
}

/**************************************************************************//**
 * @brief Displays program version.
 * @details Follows POSIX guidelines. You can create man pages using help2man.
 * @see http://www.gnu.org/prep/standards/standards.html#Command_002dLine-Interfaces
 * @see http://www.gnu.org/software/help2man/
 */
void version(void)
{
  fprintf(stdout,
    PACKAGE_NAME " " PACKAGE_VERSION "\n"
    "Copyright (c) 2018 Gerard Torrent.\n"
    "License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl-2.0.html>.\n"
    "This program is distributed in the hope that it will be useful, but WITHOUT ANY\n"
    "WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A\n"
    "PARTICULAR PURPOSE. See the GNU General Public License for more details.\n"
  );
}

/**************************************************************************//**
 * @brief Executes parsing + monitoring + writing.
 * @param[in] filename Configuration filename.
 * @return 0=OK, otherwise=KO.
 */
int run(const char *filename)
{
  int rc = EXIT_SUCCESS;
  char *syslog_tag = NULL;
  config_t cfg = {0};
  vector_t formats = {0};
  vector_t tables = {0};
  vector_t dirs = {0};
  vector_t witems = {0};
  mqueue_t mqueue1 = {0};
  mqueue_t mqueue2 = {0};
  map_t dict = {0};
  monitor_t monitor = {0};
  processor_t processor = {0};
  database_t database = {0};
  pthread_t thread_monitor;
  pthread_t thread_processor;
  pthread_t thread_database;

  // read configuration file
  rc = init_config(&cfg, filename);
  if (rc != EXIT_SUCCESS) {
    goto run_exit;
  }

  // init log
  init_syslog(&syslog_tag, &cfg);
  syslog(LOG_INFO, "log2pg started");

  // initializations
  rc |= formats_init(&formats, &cfg);
  rc |= tables_init(&tables, &cfg);
  rc |= dirs_init(&dirs, &cfg, &formats, &tables);
  rc |= database_init(&database, &cfg, &tables);
  config_destroy(&cfg);
  if (rc != EXIT_SUCCESS) {
    goto run_exit;
  }

  // initialize message queue between monitor-processor
  rc = mqueue_init(&mqueue1, "mqueue1", 0);
  if (rc != EXIT_SUCCESS) {
    syslog(LOG_CRIT, "error creating message queue monitor->processor (%d)", rc);
    goto run_exit;
  }

  // initialize message queue between processor-database
  rc = mqueue_init(&mqueue2, "mqueue2", QUEUE2_MAX_CAPACITY);
  if (rc != EXIT_SUCCESS) {
    syslog(LOG_CRIT, "error creating message queue processor->database (%d)", rc);
    goto run_exit;
  }

  // initialize data
  monitor.ifd = -1;
  monitor.witems = &witems;
  monitor.mqueue = &mqueue1;
  monitor.dict = &dict;
  processor.mqueue1 = &mqueue1;
  processor.mqueue2 = &mqueue2;
  database.mqueue = &mqueue2;

  // initialize files/dirs to monitor
  rc = monitor_init(&dirs, &monitor);
  if (rc != EXIT_SUCCESS) {
    syslog(LOG_CRIT, "error creating mqueue");
    goto run_exit;
  }

  // catching interruptions like ctrl-C
  set_signal_handlers();

  rc = pthread_create(&thread_database, NULL, database_run, &database);
  if (rc != EXIT_SUCCESS) {
    syslog(LOG_ERR, "Error creating database thread");
    goto run_exit;
  }

  rc = pthread_create(&thread_processor, NULL, processor_run, &processor);
  if (rc != EXIT_SUCCESS) {
    syslog(LOG_ERR, "Error creating processor thread");
    goto run_exit;
  }

  rc = pthread_create(&thread_monitor, NULL, monitor_run, &monitor);
  if (rc != EXIT_SUCCESS) {
    syslog(LOG_ERR, "Error creating monitor thread");
    goto run_exit;
  }
  thread1 = &thread_monitor;

  pthread_join(thread_database, NULL);
  pthread_join(thread_processor, NULL);
  pthread_join(thread_monitor, NULL);
  thread1 = NULL;

run_exit:
  config_destroy(&cfg);
  database_reset(&database);
  mqueue_reset(&mqueue1, NULL);
  mqueue_reset(&mqueue2, free);
  map_reset(&dict, NULL);
  vector_reset(&witems, witem_free);
  vector_reset(&dirs, dir_free);
  vector_reset(&formats, format_free);
  vector_reset(&tables, table_free);
  syslog(LOG_INFO, "log2pg ended (rc=%d)", rc);
  free(syslog_tag);
  return(rc);
}

/**************************************************************************//**
 * @brief Main procedure.
 * @param[in] argc Number of arguments.
 * @param[in] argv List of arguments.
 * @return 0=OK, otherwise=KO.
 */
int main(int argc, char *argv[])
{
  // return code
  int rc = EXIT_SUCCESS;
  // config filename
  char *filename = NULL;
  // short options
  char* const options1 = "hf:" ;
  // long options (name + has_arg + flag + val)
  const struct option options2[] = {
      { "file",         1,  NULL,  'f' },
      { "help",         0,  NULL,  'h' },
      { "version",      0,  NULL,  301 },
      { NULL,           0,  NULL,   0  }
  };

  // parsing options
  while (1)
  {
    int curropt = getopt_long(argc, argv, options1, options2, NULL);
    if (curropt == -1) {
      break;
    }

    switch(curropt)
    {
      case 'f': // -f FILE or --file=FILE (set config file)
        if (filename != NULL) {
          fprintf(stderr, "Error: configuration file defined twice.\n");
          fprintf(stderr, "Try \"" PACKAGE_NAME " --help\" for more information.\n");
          rc = EXIT_FAILURE;
          goto main_exit;
        }
        filename = strdup(optarg);
        break;

      case 'h': // -h or --help (show help and exit)
          help();
          rc = EXIT_SUCCESS;
          goto main_exit;
          break;

      case 301: // --version (show version and exit)
          version();
          rc = EXIT_SUCCESS;
          goto main_exit;
          break;

      default: // invalid option
          fprintf(stderr, "Try \"" PACKAGE_NAME " --help\" for more information.\n");
          rc = EXIT_FAILURE;
          goto main_exit;
          break;
    }
  }

  // check remaining arguments
  if (argc != optind) {
    fprintf(stderr, "Error: unexpected argument found.\n");
    fprintf(stderr, "Try \"" PACKAGE_NAME " --help\" for more information.\n");
    rc = EXIT_FAILURE;
    goto main_exit;
  }

  // setting default config file
  if (filename == NULL) {
    filename = strdup(DEFAULT_CONFIG_FILE);
  }

  // running simulation
  rc = run(filename);

main_exit:
  free(filename);
  closelog();
  return(rc==0?EXIT_SUCCESS:EXIT_FAILURE);
}

