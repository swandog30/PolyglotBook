#ifndef _WIN32

// engine.cpp

// includes

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>

#include "engine.h"
#include "io.h"
#include "option.h"
#include "util.h"


// constants

static const unsigned int StringSize = 4096;

// variables

engine_t Engine[1];

// prototypes

static void my_close (int fd);
static void my_dup2  (int old_fd, int new_fd);

// functions

// engine_is_ok()

bool engine_is_ok(const engine_t * engine) {

   if (engine == NULL) return false;

   if (!io_is_ok(engine->io)) return false;

   return true;
}

// engine_open()

void engine_open(engine_t * engine) {

   const char * dir, * command;
   char string[StringSize];
   int argc;
   char * ptr;
   char * argv[256];
   int from_engine[2], to_engine[2];
   pid_t pid;

   ASSERT(engine!=NULL);

   // init

   dir = option_get_string("EngineDir");
   my_log("POLYGLOT Dir \"%s\"\n",dir);

   command = option_get_string("EngineCommand");
   my_log("POLYGLOT Command \"%s\"\n",command);

   // parse the command line and create the argument list

   if (strlen(command) >= StringSize) my_fatal("engine_open(): buffer overflow\n");
   strcpy(string,command);

   argc = 0;

   for (ptr = strtok(string," "); ptr != NULL; ptr = strtok(NULL," ")) {
      argv[argc++] = ptr;
   }

   argv[argc] = NULL;

   // create the pipes

   if (pipe(from_engine) == -1) {
      my_fatal("engine_open(): pipe(): %s\n",strerror(errno));
   }

   if (pipe(to_engine) == -1) {
      my_fatal("engine_open(): pipe(): %s\n",strerror(errno));
   }

   // create the child process

   pid = fork();

   if (pid == -1) {

      my_fatal("engine_open(): fork(): %s\n",strerror(errno));

   } else if (pid == 0) {

      // child = engine

      // close unused pipe descriptors to avoid deadlocks

      my_close(from_engine[0]);
      my_close(to_engine[1]);

      // attach the pipe to standard input

      my_dup2(to_engine[0],STDIN_FILENO);
      my_close(to_engine[0]);

      // attach the pipe to standard output

      my_dup2(from_engine[1],STDOUT_FILENO);
      my_close(from_engine[1]);

      // attach standard error to standard output

      my_dup2(STDOUT_FILENO,STDERR_FILENO);

      // set a low priority

      if (option_get_bool("UseNice")) {
          my_log("POLYGLOT Adjust Engine Piority");
          nice(option_get_int("NiceValue"));
      }

      // change the current directory

      if (dir[0] != '\0' && chdir(dir) == -1) {
         my_fatal("engine_open(): chdir(): %s\n",strerror(errno));
      }

      // launch the new executable file

      execvp(argv[0],&argv[0]);

      // execvp() only returns when an error has occured

      my_fatal("engine_open(): execvp(): %s\n",strerror(errno));

   } else { // pid > 0

      ASSERT(pid>0);

      // parent = PolyGlot

      // close unused pipe descriptors to avoid deadlocks

      my_close(from_engine[1]);
      my_close(to_engine[0]);

      // fill in the engine struct

      engine->io->in_fd = from_engine[0];
      engine->io->out_fd = to_engine[1];
      engine->io->name = "Engine";
      engine->pid=pid;
      engine->state|=ENGINE_ACTIVE; // can we test if this really true?

      io_init(engine->io);
   }
}

// engine_active

bool engine_active(engine_t *engine){
    return (engine->state & ENGINE_ACTIVE)!=0;
}

// engine_eof

bool engine_eof(engine_t *engine){
    return (engine->state & ENGINE_EOF)!=0;
}

// engine_set_nice_value()

void engine_set_nice_value(engine_t * engine, int value){
    setpriority(PRIO_PROCESS,engine->pid,value);
}


// engine_close()

void engine_close(engine_t * engine) {

   ASSERT(engine_is_ok(engine));

   char string[StringSize];
   io_close(engine->io);
       // TODO: timeout
   while (!engine_eof(engine)) {
       engine_get(engine,string,StringSize); 
   }

}

// engine_get_non_blocking()

bool engine_get_non_blocking(engine_t * engine, char string[], int size){
    if(io_line_ready(engine->io)){
        engine_get(engine,string,StringSize);
        return true;
    }else{
        string[0]='\0';
        return false;
    }
}

// engine_get()

void engine_get(engine_t * engine, char string[], int size) {

   ASSERT(engine_is_ok(engine));
   ASSERT(string!=NULL);
   ASSERT(size>=256);

   while (!io_line_ready(engine->io)) {
      io_get_update(engine->io);
   }

   if (!io_get_line(engine->io,string,size)) { // EOF
       engine->state|=ENGINE_EOF;
   }
}

// engine_send()

void engine_send(engine_t * engine, const char format[], ...) {

   va_list arg_list;
   char string[StringSize];

   ASSERT(engine_is_ok(engine));
   ASSERT(format!=NULL);

   // format

   va_start(arg_list,format);
   vsprintf(string,format,arg_list);
   va_end(arg_list);

   // send

   io_send(engine->io,"%s",string);
}

// engine_send_queue()

void engine_send_queue(engine_t * engine, const char format[], ...) {

   va_list arg_list;
   char string[StringSize];

   ASSERT(engine_is_ok(engine));
   ASSERT(format!=NULL);

   // format

   va_start(arg_list,format);
   vsprintf(string,format,arg_list);
   va_end(arg_list);

   // send

   io_send_queue(engine->io,"%s",string);
}

// my_close()

static void my_close(int fd) {

   ASSERT(fd>=0);

   if (close(fd) == -1) my_fatal("my_close(): close(): %s\n",strerror(errno));
}

// my_dup2()

static void my_dup2(int old_fd, int new_fd) {

   ASSERT(old_fd>=0);
   ASSERT(new_fd>=0);

   if (dup2(old_fd,new_fd) == -1) my_fatal("my_dup2(): dup2(): %s\n",strerror(errno));
}

// end of posix part
#else

// WIN32 part

// includes

#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <direct.h>



#include "engine.h"
#include "option.h"
#include "pipe.h"
#include "posix.h"

// constants

static const int StringSize = 4096;

// variables

static int nQueuePtr = 0;
static char szQueueString[StringSize];
engine_t Engine[1];

// functions

void set_affinity(engine_t *engine, int affin){
	if(affin==-1) return;

    typedef void (WINAPI *SPAM)(HANDLE, int);
    SPAM pSPAM;
    pSPAM = (SPAM) GetProcAddress(
        GetModuleHandle(TEXT("kernel32.dll")), 
        "SetProcessAffinityMask");
    if(NULL != pSPAM){
            // [HGM] avoid crash on Win95 by first checking if API call exists
        my_log("POLYGLOT Setting process affinity to %d\n",affin);
        pSPAM((engine->io).hProcess,affin);
    }else{
        my_log("POLYGLOT API call \"SetProcessAffinityMask\" not available\n");
    }
}

DWORD GetWin32Priority(int nice)
{
/*
REALTIME_PRIORITY_CLASS     0x00000100
HIGH_PRIORITY_CLASS         0x00000080
ABOVE_NORMAL_PRIORITY_CLASS 0x00008000
NORMAL_PRIORITY_CLASS       0x00000020
BELOW_NORMAL_PRIORITY_CLASS 0x00004000
IDLE_PRIORITY_CLASS         0x00000040
*/
	if (nice < -15) return 0x00000080;
	if (nice < 0)   return 0x00008000;
	if (nice == 0)  return 0x00000020;
	if (nice < 15)  return 0x00004000;
	return 0x00000040;
}

void engine_set_nice_value(engine_t *engine, int value){
    SetPriorityClass((engine->io).hProcess,
                     GetWin32Priority(value));
}

void engine_send_queue(engine_t * engine,const char *szFormat, ...) {
    nQueuePtr += vsprintf(szQueueString + nQueuePtr, szFormat, (va_list) (&szFormat + 1));
}

void engine_send(engine_t * engine, const char *szFormat, ...) {
    vsprintf(szQueueString + nQueuePtr, szFormat, (va_list) (&szFormat + 1));
    (engine->io).LineOutput(szQueueString);
    my_log("Adapter->Engine: %s\n",szQueueString);
    nQueuePtr = 0;
}

void engine_close(engine_t * engine){
    char string[StringSize];
    (engine->io).Close();
        // TODO: Timeout
    while (!engine_eof(engine)) { 
      engine_get(Engine,string,StringSize);
    }
    (engine->io).Kill();
}

void engine_open(engine_t * engine){
    int affinity;
    char *my_dir;
    engine->state=0;
    if( (my_dir = _getcwd( NULL, 0 )) == NULL )
        my_fatal("Can't build path: %s\n",strerror(errno));
    SetCurrentDirectory(option_get_string("EngineDir"));
    (engine->io).Open(option_get_string("EngineCommand"));
    if((engine->io).Active()){
        engine->state|=ENGINE_ACTIVE;
            //play with affinity (bad idea)
        affinity=option_get_int("Affinity");
        if(affinity!=-1) set_affinity(engine,affinity); //AAA
            //lets go back
        SetCurrentDirectory(my_dir);
            // set a low priority
        if (option_get_bool("UseNice")){
            my_log("POLYGLOT Adjust Engine Piority\n");
            engine_set_nice_value(engine, option_get_int("NiceValue"));
        }
    }
    
}

bool engine_active(engine_t *engine){
    return (engine->state & ENGINE_ACTIVE)!=0;
}

bool engine_eof(engine_t *engine){
    return (engine->state & ENGINE_EOF)!=0;
}

bool engine_get_non_blocking(engine_t * engine, char *szLineStr, int size){
    if(engine_eof(engine)){ return false;}
    if ((engine->io).GetBuffer(szLineStr)) {
        my_log("Engine->Adapter: %s\n",szLineStr);
        return true;
    } else {
        szLineStr[0]='\0';
        if(engine->io.EOF_()){
            engine->state|=ENGINE_EOF;
	    my_log("POLYGLOT *** EOF from Engine ***\n");
        }
        return false;
    }
}

void engine_get(engine_t * engine, char *szLineStr, int size){
    (engine->io).LineInput(szLineStr);
    if(engine->io.EOF_()){
      engine->state|=ENGINE_EOF;
      my_log("POLYGLOT *** EOF from Engine ***\n");
    }else{
      my_log("Engine->Adapter: %s\n",szLineStr);
    }
}


#endif
