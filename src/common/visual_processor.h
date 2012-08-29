/* *****************************************************************************
 * A.L.E (Atari 2600 Learning Environment)
 * Copyright (c) 2009-2010 by Yavar Naddaf
 * Released under GNU General Public License www.gnu.org/licenses/gpl-3.0.txt
 *
 * Based on: Stella  --  "An Atari 2600 VCS Emulator"
 * Copyright (c) 1995-2007 by Bradford W. Mott and the Stella team
 * *****************************************************************************/

#ifndef VISUAL_PROCESSOR_H
#define VISUAL_PROCESSOR_H

#include <deque>
#include "Constants.h"
#include "display_screen.h"
#include "../games/RomSettings.hpp"
#include "../emucore/OSystem.hxx"
#include "../emucore/MediaSrc.hxx"
#include <boost/unordered_set.hpp>
#include <boost/filesystem.hpp>
#include <set>
#include <map>


// Search a map for a key and returns default value if not found
template <typename K, typename V>
    V GetWithDef(const std::map <K,V> & m, const K & key, const V & defval ) {
    typename std::map<K,V>::const_iterator it = m.find( key );
    if (it == m.end()) {
        return defval;
    }
    else {
        return it->second;
    }
};

// Calculates the information entropy of a random variable whose values and
// frequencies are provided by the map m.
template <typename K>
float compute_entropy(const std::map <K,int> & m, int count_sum) {
    float velocity_entropy = 0;
    typename std::map<K,int>::const_iterator it;
    for (it=m.begin(); it!=m.end(); ++it) {
        int count = it->second;
        float p_x = count / (float) count_sum;
        velocity_entropy -= p_x * log(p_x);
    }
    return velocity_entropy;
};

// Counts the number of 1 bits in a char. Taken from stack overflow
const int oneBits[] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
static int count_ones(unsigned char x) {
    int results;
    results = oneBits[x&0x0f];
    results += oneBits[x>>4];
    return results;
};

struct point {
    int x, y;

    point(int _x, int _y) { x = _x; y = _y; };

    bool operator< (const point& other) const {
        return x < other.x || (x == other.x && y < other.y);
    };
};

/*|------- A swath of color. Used as an intermediate data struct in blob detection --------|*/
struct swath {
    int color;

    // Bounding box of swath
    int x_min, x_max, y_min, y_max; 

    // x,y locations for pixels it contains
    vector<int> x;
    vector<int> y;

    swath* parent;

    swath(int _color, int _x, int _y);

    void update_bounding_box(int x, int y);

    void update_bounding_box(swath& other);
};

/*|------------ A b&w grid that is used to represent a group of pixels ------------|*/
struct PixelMask {
    vector<char> pixel_mask; // Pixel mask -- we may want an array here
    int width, height, size; // Width and height and number of active pixels of the mask

    PixelMask();
    PixelMask(int width, int height);
    PixelMask(const string& filename);

    // Sets pixel at location (x, y) to value val and updates size
    void add_pixel(int x, int y);

    // Gets the value of the pixel at location (x, y)
    bool get_pixel(int x, int y);

    // Checks if two pixel_masks are exact matches of each other
    bool equals(const PixelMask& other);

    // Returns the number of matching pixels between the two masks
    int get_pixel_overlap(const PixelMask& other);

    // Returns the percentage of pixels that overlap between the two masks
    float get_pixel_overlap_percent(const PixelMask& other);

    // Resets state and variables of this pixel mask
    void clear();

    // Saves this mask to a file
    void save(const string& filename);

    // Loads a mask from a file
    void load(const string& filename);

    void to_string();
};

/*|------------ The blob is a region of contiguous color found in the game screen ------------|*/
struct Blob {
    long id;                        // Used for the comparator function. Should be unique.
    int color;                      // Color of this blob
    set<long> neighbors;            // Neighboring blob ids
    int x_min, x_max, y_min, y_max; // Bounding box of blob region
    PixelMask mask;                 // Pixel mask
    int x_velocity, y_velocity;     // Velocity of the blob
    long parent_id; long child_id;  // Pointers to ourself in the last and next timestep

    Blob();
    Blob(int _color, long _id, int _x_min, int _x_max, int _y_min, int _y_max);

    // Adds a blob's id to the list of neighbors
    void add_neighbor(long neighbor_id);
    
    // Returns the centroid of this blob
    point get_centroid();

    // Computes our velocity relative to a given blob. Velocity is based on
    // the centroid distance of both blobs.
    void compute_velocity(const Blob& other);

    // Computes the euclidean distance between the blobs centroids
    float get_centroid_dist(const Blob& other);

    // Spits out an all things considered blob match. Takes into account:
    // 1. color 2. distance 3. overlap 4. area 5. density
    float get_aggregate_blob_match(const Blob& other);

    // Find the blob that most closely resembles this blob. Do not consider
    // any of the blobs in the excluded set.
    long find_matching_blob(map<long,Blob>& blobs);

    // Prints the blob and its velocity history
    void to_string(bool verbose=false, deque<map<long,Blob> >* blob_hist=NULL);

    bool operator< (const Blob& other) const {
        return id < other.id;
    };
};

/*|------------ A composite object is an object composed of blobs ------------|*/
struct CompositeObject {
    long id;                        // Unique identifier
    set<long> blob_ids;             // Set of blob ids composing this object
    PixelMask mask;                 // Pixel mask of the object
    int x_velocity, y_velocity;     // Object velocity
    int x_min, x_max, y_min, y_max; // Bounding box
    int frames_since_last_movement; // Number of frames since object last moved
    int age;                        // Number of frames since this object was discovered

    CompositeObject();
    CompositeObject(int x_vel, int y_vel, long id);

    // Resets the state and variables of this object
    void clear();
    
    // Updates the bounding box from a blob
    void update_bounding_box(const Blob& b);

    // Adds a blob to this object and updates the bounding box
    void add_blob(const Blob& b);

    // Calculates the centroid of the object
    point get_centroid() { return point((x_max+x_min)/2,(y_max+y_min)/2); };

    // Attempts to expand the composite object by looking for any blobs who are
    // connected and have the same velocity
    void expand(map<long,Blob>& blob_map);

    // Builds the mask from the current set of blobs
    void computeMask(map<long,Blob>& blob_map);

    void to_string(bool verbose=false);
};

/*|------------ A prototype represents a class of objects ------------|*/
struct Prototype {
    long id;                 // Unique identifier
    set<long> obj_ids;       // Set of objects ids belonging to this class
    vector<PixelMask> masks; // List of different pixel masks this object class assumes
    long seen_count;         // Counts the number of times this prototype has been seen
    int frames_since_last_seen; // Number of frames since prototype was last seen
    int times_seen_this_frame;  // Number of times this prototype was seen in last frame
    bool is_valid;              // Specifies if the prototype is valid or not
    float self_likelihood, alpha; // How likely is the prototype to be part of the "self"?
  
    Prototype();

    // Constructs a prototype from a specified object
    Prototype(CompositeObject& obj, long id);

    // Find the best matching mask to a given object. Sets the overlap score and the best
    // masks index. 
    void get_pixel_match(const CompositeObject& obj, float& overlap, int& mask_indx);

    // Populates the obj_ids field by looking for objects in the map which match or exceed
    // the specified similarity threshold.
    void find_matching_objects(float similarity_threshold, map<long,CompositeObject>& obj_map);

    void to_string(bool verbose=false);
};


class VisualProcessor : public SDLEventHandler {
public:
    VisualProcessor(OSystem* _osystem, string myRomFile);
    ~VisualProcessor() {};
        
    void process_image(const MediaSource& mediaSrc, Action a);

    // Given a screen it will do object detection on that screen and
    // return the results.
    void process_image(const IntMatrix* screen_matrix, Action a);

    // Blob Detection
    void find_connected_components(const IntMatrix& screen_matrix, map<long,Blob>& blobs);

    // Matches blobs found in the current timestep with those from
    // the last time step
    void find_blob_matches(map<long,Blob>& blobs);

    // Merges blobs together into composite objects
    void merge_blobs(map<long,Blob>& blobs);

    // Updates objects when a new timestep arrives
    void update_existing_objs();

    // Sanitize objects based on size and velocity
    void sanitize_objects();

    // Merges objects together into classes of objects
    void merge_objects(float similarity_threshold);

    // Looks through objects attempting to find one that we are controlling
    void identify_self();

    // Gives a point corresponding to the location of the self on the screen.
    // Assumes identify self has already been called.
    point get_self_centroid();

    // Returns true if a self object has been located.
    bool found_self();

    // Methods for the SDLEventHandler class
    bool handleSDLEvent(const SDL_Event& event);
    void display_screen(IntMatrix& screen_matrix, int screen_width, int screen_height);
    void usage();

    // Plotting methods
    void plot_blobs(IntMatrix& screen_matrix);      // Plots the blobs on screen
    void plot_objects(IntMatrix& screen_matrix);    // Plots the objects on screen
    void plot_prototypes(IntMatrix& screen_matrix); // Plots the prototypes on screen
    void plot_self(IntMatrix& screen_matrix);       // Plots the self blob
    void box_blob(Blob& b, IntMatrix& screen_matrix, int color); // Draws a box around a blob  
    void box_object(CompositeObject& obj, IntMatrix& screen_matrix, int color); 

    void printVelHistory(CompositeObject& obj);

    // Saves an image of the currently selected object. Returns true if successful, false otherwise.
    bool saveSelection();
    // Loads masks of images. Assumes files have format prefix + image_num + suffix.
    void loadPrototype(boost::filesystem::path p, const string& prefix, const string& suffix,
                       Prototype& proto);
    
public:
    OSystem* p_osystem;
    RomSettings* game_settings;
    int screen_width, screen_height;
    IntMatrix screen_matrix;

    // History of past screens, actions, and blobs
    int max_history_len;
    deque<IntMatrix>        screen_hist;
    deque<Action>           action_hist;
    deque<map<long,Blob> >  blob_hist;

    // Used to generate new IDs
    long blob_ids, obj_ids, proto_ids;

    map<long,Blob>            curr_blobs;      // Map of blob ids to blobs for the current frame
    map<long,CompositeObject> composite_objs;  // Map of obj ids to objs for the current frame
    vector<Prototype>         obj_classes;     // Classes of objects

    long self_id; // ID of the object which corresponds to the "self"

    // Prototypes which are manually identified. These are loaded up from saved files of the game
    Prototype manual_self;
    vector<Prototype> manual_obj_classes;

    // Graphical display variables
    long focused_entity_id; // The focused object is selected by a click
    int focus_level;        // Are we focusing on a blob/object/prototype?
    int display_mode;       // Which graphical representation should we display?
    bool display_self;      // Should the results of self detection be displayed?
    int proto_indx;         // Indicates which prototype we are saving masks for
};

#endif

