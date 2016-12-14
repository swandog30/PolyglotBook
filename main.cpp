
// main.cpp

// includes

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "attack.h"
#include "board.h"
#include "book.h"
#include "book_make.h"
#include "book_merge.h"
#include "engine.h"
#include "epd.h"
#include "fen.h"
#include "gui.h"
#include "hash.h"
#include "list.h"
#include "main.h"
#include "mainloop.h"
#include "move.h"
#include "move_gen.h"
#include "option.h"
#include "piece.h"
#include "search.h"
#include "square.h"
#include "uci.h"
#include "util.h"
#include "xboard2uci.h"
#include "uci2uci.h"

// constants


static const char * const Version = "1.4b27";
static const char * const HelpMessage = "\
SYNTAX\n\
* polyglot [configfile]\n\
* polyglot -ec enginecommand\n\
* polyglot make-book [-pgn inputfile] [-bin outputfile] [-max-ply ply] [-min-game games] [-min-score score] [-only-white] [-only-black] [-uniform]\n\
* polyglot merge-book -in1 inputfile1 -in2 inputfile2 [-out outputfile]\n\
* polyglot info-book [-bin inputfile] [-exact]\n\
* polyglot dumb-book [-bin inputfile] -color color [-out outputfile]\n\
* polyglot [configfile] epd-test [-epd inputfile] [-min-depth depth] [-max-depth depth] [-min-time time] [-max-time time] [-depth-delta delta]\n\
* polyglot perft [-fen fen] [-max-depth depth]\
";

static const int SearchDepth = 63;
static const double SearchTime = 3600.0;
static const int StringSize = 4096;

// variables

static bool Init;

// prototypes

static void parse_option ();
static void init_book ();
static bool parse_line   (char line[], char * * name_ptr, char * * value_ptr);
static void stop_search  ();

// functions

// main()

int main(int argc, char * argv[]) {

//   board_t board[1];

    if(argc>=2 && ((my_string_case_equal(argv[1],"help")) || (my_string_case_equal(argv[1],"-help")) || (my_string_case_equal(argv[1],"--help")) ||  (my_string_case_equal(argv[1],"-h")) ||  my_string_case_equal(argv[1],"/?"))){
        printf("%s\n",HelpMessage);
        return EXIT_SUCCESS;
    }
   // init

    Init = false;

    util_init();
    printf("PolyGlot %s by Fabien Letouzey\n",Version);
    
    option_init();
    
    square_init();
    piece_init();
    attack_init();
    
    hash_init();
    
    my_random_init();

        // build book
    
    if (argc >= 2 && my_string_equal(argv[1],"make-book")) {
        book_make(argc,argv);
        return EXIT_SUCCESS;
    }
    
    if (argc >= 2 && my_string_equal(argv[1],"merge-book")) {
        book_merge(argc,argv);
        return EXIT_SUCCESS;
    }

       if (argc >= 2 && my_string_equal(argv[1],"merge-book")) {
      book_merge(argc,argv);
      return EXIT_SUCCESS;
   }

   if (argc >= 2 && my_string_equal(argv[1],"dump-book")) {
      book_dump(argc,argv);
      return EXIT_SUCCESS;
   }

   if (argc >= 2 && my_string_equal(argv[1],"info-book")) {
      book_info(argc,argv);
      return EXIT_SUCCESS;
   }
    
    if (argc >= 2 && my_string_equal(argv[1],"perft")) {
        do_perft(argc,argv);
        return EXIT_SUCCESS;
    }
    
    if (argc >= 3 && my_string_equal(argv[1],"-ec")) {
        option_set("EngineCommand",argv[2]);
        engine_open(Engine);
        if(!engine_active(Engine)){
            my_fatal("Could not start \"%s\"\n",option_get("EngineCommand"));
        }
        Init=true;
        gui_init(GUI);
        uci_open(Uci,Engine);
        if (my_string_equal(option_get_string("EngineName"),"<empty>")) {
            option_set("EngineName",Uci->name);
        }
        mainloop();
        return EXIT_SUCCESS; 
    }
    
        // read options
    
    if (argc == 2) option_set("OptionFile",argv[1]); // HACK for compatibility

    parse_option(); // HACK: also launches the engine
    
        // EPD test
    
    if (argc >= 2 && my_string_equal(argv[1],"epd-test")){
        epd_test(argc,argv);
        return EXIT_SUCCESS;
    }else if(argc >= 3 && my_string_equal(argv[2],"epd-test")){
        epd_test(argc-1,argv+1);
        return EXIT_SUCCESS;
    }
    
    if (argc >= 3) my_fatal("Too many arguments\n");


    init_book();
    gui_init(GUI);
    mainloop();
    return EXIT_SUCCESS; 
}

// polyglot_set_option

void polyglot_set_option(char *name, char *value){ // this must be cleaned up!
    option_set(name,value);
    if(option_get_bool("Book")&&(my_string_case_equal(name,"BookFile")||my_string_case_equal(name,"Book"))){
        my_log("POLYGLOT *** SETTING BOOK ***\n");
        my_log("POLYGLOT BOOK \"%s\"\n",option_get_string("BookFile"));
        book_close();
        book_clear();
        book_open(option_get_string("BookFile"));
        if(!book_is_open()){
            my_log("POLYGLOT Unable to open book \"%s\"\n",option_get_string("BookFile"));
        }
    }else if(option_get_bool("Log")&&(my_string_case_equal(name,"LogFile") ||my_string_case_equal(name,"Log"))){
        my_log("POLYGLOT *** SETTING LOGFILE ***\n");
        my_log("POLYGLOT LOGFILE \"%s\"\n",option_get_string("LogFile"));
        my_log_close();
        my_log_open(option_get_string("LogFile"));
    }else if(option_get_bool("UseNice") &&(my_string_case_equal(name,"NiceValue")||my_string_case_equal(name,"UseNice"))){
        my_log("POLYGLOT Adjust Engine Piority\n");
        engine_set_nice_value(Engine,atoi(option_get_string("NiceValue")));
    }else if(my_string_case_equal(name,"Book") && !option_get_bool("Book")){
        book_close();
        book_clear();
    }else if(my_string_case_equal(name,"UseNice") && !option_get_bool("UseNice")){
        my_log("POLYGLOT Adjust Engine Piority\n");
        engine_set_nice_value(Engine,0);
    }else if(my_string_case_equal(name,"Log") && !option_get_bool("Log")){
        my_log("POLYGLOT QUIT LOGGING\n");
        my_log_close();
    }
}


// init_book()

static void init_book(){
    book_clear();
    if (option_get_bool("Book")){
        my_log("POLYGLOT *** SETTING BOOK ***\n");
        my_log("POLYGLOT BOOK \"%s\"\n",option_get_string("BookFile"));
        book_open(option_get_string("BookFile"));
        if(!book_is_open()){
            my_log("POLYGLOT Unable to open book \"%s\"\n",
                   option_get_string("BookFile"));
        }
    }
}

// parse_option()

static void parse_option() {

    const char * file_name;
    FILE * file;
    char line[256];
    char * name, * value;
    file_name = option_get_string("OptionFile");
    
    file = fopen(file_name,"r");
    if (file == NULL) {
        my_fatal("Can't open file \"%s\": %s\n",file_name,strerror(errno));
    }
    
        // PolyGlot options (assumed first)
    
   while (true) {
       
       if (!my_file_read_line(file,line,256)) {
           my_fatal("parse_option(): missing [Engine] section\n");
       }
       
       if (my_string_case_equal(line,"[engine]")) break;
       
       if (parse_line(line,&name,&value)) {
           option_set(name,value);
           option_set_default(name,value);
       }
   }
   
   if (option_get_bool("Log")) {
       my_log_open(option_get_string("LogFile"));
   }
   
   my_log("POLYGLOT *** START ***\n");
   my_log("POLYGLOT INI file \"%s\"\n",file_name);
   engine_open(Engine);
   if(!engine_active(Engine)){
       my_fatal("Could not start \"%s\"\n",option_get("EngineCommand"));
   }

   if (option_get_bool("UCI")) {
       my_log("POLYGLOT *** Switching to UCI mode ***\n");
   }
   uci_open(Uci,Engine);
   Init = true;
   while (my_file_read_line(file,line,256)) {
       if (line[0] == '[') my_fatal("parse_option(): unknown section %s\n",line);
       if (parse_line(line,&name,&value)) {
           uci_send_option(Uci,name,"%s",value);
               //to get a decent display in winboard_x we need to now if an engine really is doing multipv analysis
               // "multipv 1" in the pv is meaningless,f.i. toga sends that all the time
               //therefore check if MultiPV is set to a decent value in the polyglot ini file
           if(my_string_case_equal(name,"MultiPV") && atoi(value)>1)  Uci->multipv_mode=true;
       }
   }
   if (my_string_equal(option_get_string("EngineName"),"<empty>")) {
       option_set("EngineName",Uci->name);
   }
   
   fclose(file);
}

// parse_line()

static bool parse_line(char line[], char * * name_ptr, char * * value_ptr) {
    
    char * ptr;
    char * name, * value;
    
    ASSERT(line!=NULL);
    ASSERT(name_ptr!=NULL);
    ASSERT(value_ptr!=NULL);
    
        // remove comments
    
    ptr = strchr(line,';');
    if (ptr != NULL) *ptr = '\0';
    
    ptr = strchr(line,'#');
    if (ptr != NULL) *ptr = '\0';
    
        // split at '='
    
    ptr = strchr(line,'=');
    if (ptr == NULL) return false;
    
    name = line;
    value = ptr+1;
   
        // cleanup name
    
    while (*name == ' ') name++; // remove leading spaces
    
    while (ptr > name && ptr[-1] == ' ') ptr--; // remove trailing spaces
    *ptr = '\0';
    
    if (*name == '\0') return false;
    
        // cleanup value
    
    ptr = &value[strlen(value)]; // pointer to string terminator
    
    while (*value == ' ') value++; // remove leading spaces
    
    while (ptr > value && ptr[-1] == ' ') ptr--; // remove trailing spaces
    *ptr = '\0';
    
    if (*value == '\0') return false;
   
        // end
    
    *name_ptr = name;
    *value_ptr = value;
    
    return true;
}

// quit()

void quit() {

    my_log("POLYGLOT *** QUIT ***\n");
    
    if (Init) {
        
        stop_search();
        engine_send(Engine,"quit");
        my_log("POLYGLOT Closing engine\n");
        engine_close(Engine);
        
    }
    my_log("POLYGLOT Calling exit\n");
    exit(EXIT_SUCCESS);
}

// stop_search()

static void stop_search() {
    
    if (Init && Uci->searching) {
        
        ASSERT(Uci->searching);
        ASSERT(Uci->pending_nb>=1);
        
        my_log("POLYGLOT STOP SEARCH\n");
        
        if (option_get_bool("SyncStop")) {
            uci_send_stop_sync(Uci);
        } else {
            uci_send_stop(Uci);
        }
    }
}


// end of main.cpp

