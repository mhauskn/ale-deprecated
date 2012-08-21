#ifndef ALE_INTERFACE_H
#define ALE_INTERFACE_H

#include <cstdlib>
#include <ctime>
#include "emucore/m6502/src/bspf/src/bspf.hxx"
#include "emucore/Console.hxx"
#include "emucore/Event.hxx"
#include "emucore/PropsSet.hxx"
#include "emucore/Settings.hxx"
#include "emucore/FSNode.hxx"
#include "emucore/OSystem.hxx"
#include "os_dependent/SettingsUNIX.hxx"
#include "os_dependent/OSystemUNIX.hxx"
#include "control/fifo_controller.h"
#include "control/internal_controller.h"
#include "common/Constants.h"
#include "common/Defaults.hpp"
#include "common/visual_processor.h"
#include "games/RomSettings.hpp"
#include "games/Roms.hpp"
#include "agents/PlayerAgent.hpp"

static const std::string Version = "0.3";

/* display welcome message */
static std::string welcomeMessage() {

    // ALE welcome message
    std::ostringstream oss;

    oss << "A.L.E: Arcade Learning Environment (version "
        << Version << ")\n" 
        << "[Powered by Stella]\n"
        << "Use -help for help screen.";

    return oss.str();
}


/**
   This class interfaces ALE with external code for controlling agents.
 */
class ALEInterface
{
public:
    OSystem* theOSystem;
    InternalController* game_controller;
    MediaSource *mediasrc;
    System* emulator_system;
    RomSettings* game_settings;
    VisualProcessor* visProc;

    int screen_width, screen_height;  // Dimensions of the screen
    IntMatrix screen_matrix;     // This contains the raw pixel representation of the screen
    IntVect ram_content;         // This contains the ram content of the Atari

    int frame;                   // Current frame number
    int max_num_frames;          // Maximum number of frames allowed in this episode
    float game_score;            // Score accumulated throughout the course of a game
    ActionVect allowed_actions;  // Vector of allowed actions for this game
    Action last_action;          // Always stores the latest action taken
    time_t time_start, time_end; // Used to keep track of fps
    bool display_active;         // Should the screen be displayed or not
    bool process_screen;         // Should visual processing be performed or not

public:
    ALEInterface(): theOSystem(NULL), game_controller(NULL), mediasrc(NULL), emulator_system(NULL),
                    game_settings(NULL), frame(0), max_num_frames(-1),
                    game_score(0), display_active(false) {
    }

    ~ALEInterface() {
        if (theOSystem) delete theOSystem;
        if (game_controller) delete game_controller;
    }

    // Loads and initializes a game. After this call the game should be ready to play.
    bool loadROM(string rom_file, bool display_screen, bool process_screen) {
        display_active = display_screen;
        this->process_screen = process_screen;
        int argc = 8;
        char** argv = new char*[argc];
        for (int i=0; i<=argc; i++) {
            argv[i] = new char[200];
        }
        strcpy(argv[0],"./ale");
        strcpy(argv[1],"-player_agent");
        strcpy(argv[2],"random_agent");
        strcpy(argv[3],"-display_screen");
        if (display_screen) strcpy(argv[4],"true");
        else                strcpy(argv[4],"false");
        strcpy(argv[5],"-process_screen");
        if (process_screen) strcpy(argv[6],"true");
        else                strcpy(argv[6],"false");
        strcpy(argv[7],rom_file.c_str());  

        std::cerr << welcomeMessage() << endl;
    
        if (theOSystem) delete theOSystem;

#ifdef WIN32
        theOSystem = new OSystemWin32();
        SettingsWin32 settings(theOSystem);
#else
        theOSystem = new OSystemUNIX();
        SettingsUNIX settings(theOSystem);
#endif

        setDefaultSettings(theOSystem->settings());

        theOSystem->settings().loadConfig();

        // process commandline arguments, which over-ride all possible config file settings
        string romfile = theOSystem->settings().loadCommandLine(argc, argv);

        // Load the configuration from a config file (passed on the command
        //  line), if provided
        string configFile = theOSystem->settings().getString("config", false);
   
        if (!configFile.empty())
            theOSystem->settings().loadConfig(configFile.c_str());

        theOSystem->settings().validate();
        theOSystem->create();
  
        //// Main loop ////
        // First we check if a ROM is specified on the commandline.  If so, and if
        //   the ROM actually exists, use it to create a new console.
        // If not, use the built-in ROM launcher.  In this case, we enter 'launcher'
        //   mode and let the main event loop take care of opening a new console/ROM.
        if(argc == 1 || romfile == "" || !FilesystemNode::fileExists(romfile)) {
            printf("No ROM File specified or the ROM file was not found.\n");
            return false;
        } else if(theOSystem->createConsole(romfile)) 	{
            printf("Running ROM file...\n");
            theOSystem->settings().setString("rom_file", romfile);
        } else {
            printf("Unable to create console from ROM file.\n");
            return false;
        }

        // Seed the Random number generator
        if (theOSystem->settings().getString("random_seed") == "time") {
            cout << "Random Seed: Time" << endl;
            srand((unsigned)time(0)); 
            //srand48((unsigned)time(0));
        } else {
            int seed = theOSystem->settings().getInt("random_seed");
            assert(seed >= 0);
            cout << "Random Seed: " << seed << endl;
            srand((unsigned)seed); 
            //srand48((unsigned)seed);
        }

        // Generate the GameController
        if (game_controller) delete game_controller;
        game_controller = new InternalController(theOSystem);
        theOSystem->setGameController(game_controller);

        // Set the Pallete 
        theOSystem->console().setPalette("standard");

        // Setup the screen representation
        mediasrc = &theOSystem->console().mediaSource();
        screen_width = mediasrc->width();
        screen_height = mediasrc->height();
        for (int i=0; i<screen_height; ++i) { // Initialize our screen matrix
            IntVect row;
            for (int j=0; j<screen_width; ++j)
                row.push_back(-1);
            screen_matrix.push_back(row);
        }

        // Intialize the ram array
        for (int i=0; i<RAM_LENGTH; i++)
            ram_content.push_back(0);

        emulator_system = &theOSystem->console().system();
        game_settings = buildRomRLWrapper(theOSystem->romFile());
        visProc = theOSystem->p_vis_proc;
        allowed_actions = game_settings->getAvailableActions();
    
        reset_game();

        return true;
    }

    // Resets the game
    void reset_game() {
        game_controller->systemReset();
        
        game_settings->step(*emulator_system);
        
        // Get the first screen
        mediasrc->update();
        int ind_i, ind_j;
        uInt8* pi_curr_frame_buffer = mediasrc->currentFrameBuffer();
        for (int i = 0; i < screen_width * screen_height; i++) {
            uInt8 v = pi_curr_frame_buffer[i];
            ind_i = i / screen_width;
            ind_j = i - (ind_i * screen_width);
            screen_matrix[ind_i][ind_j] = v;
        }

        // Get the first ram content
        for(int i = 0; i<RAM_LENGTH; i++) {
            int offset = i;
            offset &= 0x7f; // there are only 128 bytes
            ram_content[i] = emulator_system->peek(offset + 0x80);
        }

        // Record the starting time of this game
        time_start = time(NULL);
    }

    // Indicates if the game has ended
    bool game_over() {
        return game_settings->isTerminal();
    }

    // Applies an action to the game and returns the reward. It is the user's responsibility
    // to check if the game has ended and reset when necessary -- this method will keep pressing
    // buttons on the game over screen.
    float act(Action action) {
        frame++;
        float action_reward = 0;
            
        game_settings->step(*emulator_system);

        // Apply action to simulator and update the simulator
        game_controller->getState()->apply_action(action, PLAYER_B_NOOP);

        // Get the latest screen
        mediasrc->update();
        int ind_i, ind_j;
        uInt8* pi_curr_frame_buffer = mediasrc->currentFrameBuffer();
        for (int i = 0; i < screen_width * screen_height; i++) {
            uInt8 v = pi_curr_frame_buffer[i];
            ind_i = i / screen_width;
            ind_j = i - (ind_i * screen_width);
            screen_matrix[ind_i][ind_j] = v;
        }

        // Get the latest ram content
        for(int i = 0; i<RAM_LENGTH; i++) {
            int offset = i;
            offset &= 0x7f; // there are only 128 bytes
            ram_content[i] = emulator_system->peek(offset + 0x80);
        }

        // Get the reward
        action_reward += game_settings->getReward();

        if (frame % 1000 == 0) {
            time_end = time(NULL);
            double avg = ((double)frame)/(time_end - time_start);
            cerr << "Average main loop iterations per sec = " << avg << endl;
        }

        // Display the screen
        if (display_active) {
            theOSystem->p_display_screen->display_screen(screen_matrix, screen_width, screen_height);
            //theOSystem->p_display_screen->display_screen(*mediasrc);            
        }
        if (process_screen) {
            theOSystem->p_vis_proc->process_image(*mediasrc, action);
        }

        game_score += action_reward;
        last_action = action;
        return action_reward;
    }

    //****************** Visual Processing Methods ********************//
    // These are only active if the process_screen variable is set to
    // true when the load_rom method is invoked. For detail info see
    // src/common/visual_processor.h
    //*****************************************************************//    
    
    // Returns the (x,y) pixel location of the centroid of the self object
    // or (-1,-1) if the self is not detected.
    point getSelfLocation() {
        assert(process_screen && visProc);
        return visProc->get_self_centroid();
    };

    // Returns a pointer to the vector of object classes. Each object
    // class may or may not contain objects on screen. See Prototype
    // class in visual_processor.h
    vector<Prototype>* getDetectedObjectClasses() {
        assert(process_screen && visProc);
        return &(visProc->manual_obj_classes);        
    };

    // Returns a vector of (x,y) pixel locations of the centroids of any
    // objects of the given object class on the current screen.
    vector<point> getObjLocations(const Prototype& proto) {
        assert(process_screen && visProc);
        vector<point> v;
        for (set<long>::iterator it=proto.obj_ids.begin(); it!=proto.obj_ids.end(); it++) {
            long obj_id = *it;
            assert(visProc->composite_objs.find(obj_id) != visProc->composite_objs.end());
            point obj_centroid = visProc->composite_objs[obj_id].get_centroid();
            v.push_back(obj_centroid);
        }
        return v;
    };
};

#endif
