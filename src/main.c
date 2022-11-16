/*==========================================================================
syslogd-lite
Copyright (c)2019-20 Kevin Boone
Distributed under the terms of the GPL v3.0
==========================================================================*/

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <getopt.h>

#ifdef __APPLE__
#include <spawn.h>
#endif


// The syslog socket file, normally /dev/log
#define SOCKET_FILE "/dev/log2"

// Where the log will be flushed to
#define LOG_FILE "/var/log/messages"

// The default maximum number of log entries to be stored
#define DEF_MAX_ENTRIES 100 

// The default interval at which to flush the log to file
#define DEF_INTERVAL 60

// The list of messages
char **list;

// Maximum number of messages in the list
int max_entries = DEF_MAX_ENTRIES;

// The current number of messages in the list. In practice, this will
//  be max_entries most of the time
int list_size = 0;

// Interval at which to flush the log messages to file
int interval = DEF_INTERVAL;

pthread_mutex_t list_mutex;

typedef int BOOL;
#define TRUE 1
#define FALSE 0


/*==========================================================================
dump_list()
Dumps the entire log message list to the specified file pointer
==========================================================================*/
void dump_list (FILE *f)
  {
  pthread_mutex_lock (&list_mutex);
  for (int i = 0; i < list_size; i++)
    {
    fprintf (f, "%s\n", list[i]);
    }
  pthread_mutex_unlock (&list_mutex);
  }

/*==========================================================================
dump_list_to_log()
Dumps the entire log message list to the logfile 
==========================================================================*/
void dump_list_to_log (void)
  {
  FILE *f = fopen (LOG_FILE, "w"); // Truncate log on opening
  dump_list (f);
  fclose (f);
  }


/*==========================================================================
sigalrm()
==========================================================================*/
void sigalarm (int dummy)
  {
  dump_list_to_log ();
  alarm (interval);
  }


/*==========================================================================
cleanup()
==========================================================================*/
void cleanup (void)
  {
  // Tidy up memory -- do not attempt to wait for the mutex here --
  //   program is shutting down, and we don't want to get stuck
  for (int i = 0; i < list_size; i++)
    free (list[i]);
  free (list);
  }

/*==========================================================================
sigquit()
==========================================================================*/
void sigquit (int dummy)
  {
  cleanup();
  exit (0);
  }


/*==========================================================================
sigquit()
==========================================================================*/
void sigusr1 (int dummy)
  {
  dump_list_to_log ();
  }


/*==========================================================================
main()
==========================================================================*/
int main (int argc, char **argv)
  {
  BOOL show_version = FALSE;
  BOOL show_usage = FALSE;
  BOOL debug = FALSE;
#ifdef __APPLE__
  pid_t pid = 0;
  char **environ;
#endif

  // Parse the command line

  static struct option long_options[] =
    {
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {"debug", no_argument, NULL, 'd'},
      {"lines", required_argument, NULL, 'l'},
      {"interval", required_argument, NULL, 'i'},
      {0, 0, 0, 0}
    };

   int opt;
   while (1)
     {
     int option_index = 0;
     opt = getopt_long (argc, argv, "hvdl:i:",
     long_options, &option_index);

     if (opt == -1) break;

     switch (opt)
       {
       case 0:
         if (strcmp (long_options[option_index].name, "help") == 0)
           show_usage = TRUE;
         else if (strcmp (long_options[option_index].name, "version") == 0)
           show_version = TRUE;
         else if (strcmp (long_options[option_index].name, "debug") == 0)
           debug = TRUE;
         else if (strcmp (long_options[option_index].name, "lines") == 0)
           max_entries = atoi (optarg); 
         else if (strcmp (long_options[option_index].name, "interval") == 0)
           interval = atoi (optarg); 
         else
           exit (-1);
         break;

       case 'h': case '?': show_usage = TRUE; break;
       case 'v': show_version = TRUE; break;
       case 'd': debug = TRUE; break;
       case 'l': max_entries = atoi (optarg); break;
       case 'i': interval = atoi (optarg); break;
       default:
         exit(-1);
       }
    }

  // Check for terminating arguments like -h before opening any sockets

  if (show_version)
    {
    printf ("%s: %s version %s\n", argv[0], NAME, VERSION);
    printf ("Copyright (c)2019-2020 Kevin Boone\n");
    printf ("Distributed under the terms of the GPL v3.0\n");
    exit (0);
    }

  if (show_usage)
    {
    printf ("Usage: %s [options]\n", argv[0]);
    printf ("  -d,--debug              stay in foreground\n");
    printf ("  -h,--help               show this message\n");
    printf ("  -l,--lines=N            retain N log lines (100)\n");
    printf ("  -i,--interval=N         seconds between log dumps (60)\n");
    printf ("  -v,--version            show version\n");
    exit (0);
    }

  // Set up an empty log message list
  list = malloc (max_entries * sizeof (char *));
  for (int i = 0; i < max_entries; i++)
    list[i] = NULL;

  // Open the log socket, usually /dev/log
  int listen_fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if(listen_fd == -1)
     {
     fprintf (stderr, "%s: Can't open logger socket: %s\n", 
       argv[0], strerror (errno));
     exit(-1);
     }

  struct sockaddr saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sa_family = AF_UNIX;
  strcpy(saddr.sa_data, SOCKET_FILE);

  socklen_t saddrlen = sizeof(struct sockaddr) + 6;
  if (bind (listen_fd, &saddr, saddrlen) < 0)
     {
     fprintf (stderr, "%s: Can't bind logger socket: %s\n", 
       argv[0], strerror (errno));
     exit(-1);
     }

  // Make sure non-root processes can log
  chmod (SOCKET_FILE, 666);

  // Clear log file, and report if it is not writable
  FILE *f = fopen (LOG_FILE, "w"); 
  if (!f)
     {
     fprintf (stderr, "%s: Can't open %s for writing: %s\n", 
       argv[0], LOG_FILE, strerror (errno));
     exit(-1);
     }
  fclose (f);

  // Put this program into the background, stifling stdout/stderr,
  //   unless we are in debug mode
  if (!debug)
#ifdef __APPLE__
    posix_spawn(&pid, argv[0], NULL, NULL, argv, environ);
    waitpid(pid, NULL, 0);
#else
    daemon (0, 0);
#endif

  // Set up signals. The alarm signal will be used for timed dumps
  //  of the log messages to file. 
  alarm (interval);
  signal (SIGALRM, sigalarm);
  signal (SIGQUIT, sigquit);
  signal (SIGINT, sigquit);
  signal (SIGUSR1, sigusr1);

  pthread_mutex_init  (&list_mutex, NULL);

  listen (listen_fd, 10);

  while (1)
    {
    int conn_fd = accept (listen_fd, NULL, NULL);

    char buff[2048];
    int n = read (conn_fd, buff, sizeof (buff) - 1);
    if (n > 0)
      {
      buff[n] = 0;
      
      // Strip LF if present
      int l = strlen (buff);
      if (buff[l-1] == 10) buff[l-1] = 0;

      if (debug) 
        printf ("%s\n", buff); 

      pthread_mutex_lock (&list_mutex);

      // Is list full?
      if (list_size == max_entries)
        {
	// If list is full...
	// Free the oldest entry
	free (list[0]);
        // Then shift all entries down, to make room at the top
	// list_size will not change after this point
        for (int i = 1; i < list_size; i++)
	  {
	  list[i-1] = list[i];
	  }
	list_size--;
	}

      // Add new message to the head of the list
      list[list_size] = strdup (buff);
      list_size++;

      pthread_mutex_unlock (&list_mutex);
      }
    close (conn_fd);
    }
  // We never get here -- the program terminates in the signal handler
  return 0;
  }


